#include "stdafx.h"
#include "OpenEditor/OEPlsSqlBlockMatcher.h"
#include "OpenEditor/OEView.h"
#include <vector>
#include <cwctype>
#include <algorithm>

namespace OpenEditor
{

enum EBlockTok
{
    btNone,
    btIF,
    btTHEN,
    btELSIF,
    btELSE,
    btEND_IF,
    btFOR,
    btWHILE,
    btLOOP,
    btEND_LOOP,
    btBEGIN,
    btEXCEPTION,
    btEND,
    btCASE,
    btEND_CASE
};

struct BlockToken
{
    EBlockTok type;
    int line;
    int colInx;
    int length;
};

static bool is_ident_char (wchar_t ch)
{
    return iswalnum(ch) || ch == L'_' || ch == L'$' || ch == L'#';
}

static std::vector<BlockToken> TokenizeBlocks (COEditorView* pEditor, int maxLine)
{
    std::vector<BlockToken> tokens;
    if (!pEditor) return tokens;

    int totalLines = std::min(pEditor->GetLineCount(), maxLine + 1);
    bool inBlockComment = false;

    for (int line = 0; line < totalLines; ++line)
    {
        OEStringW lineBuff;
        pEditor->GetLineW(line, lineBuff);
        const wchar_t* str = lineBuff.data();
        int len = lineBuff.length();

        for (int i = 0; i < len; ++i)
        {
            if (inBlockComment)
            {
                if (i + 1 < len && str[i] == L'*' && str[i + 1] == L'/')
                {
                    inBlockComment = false;
                    ++i;
                }
                continue;
            }

            if (i + 1 < len && str[i] == L'/' && str[i + 1] == L'*')
            {
                inBlockComment = true;
                ++i;
                continue;
            }

            if (i + 1 < len && str[i] == L'-' && str[i + 1] == L'-')
                break; // comentário de linha

            if (str[i] == L'\'')
            {
                ++i;
                while (i < len)
                {
                    if (str[i] == L'\'')
                    {
                        if (i + 1 < len && str[i + 1] == L'\'')
                            i += 2;
                        else
                            break;
                    }
                    else
                        ++i;
                }
                continue;
            }

            // Ler identificadores ou palavras-chave
            if (!is_ident_char(str[i]))
                continue;

            int startInx = i;
            while (i < len && is_ident_char(str[i]))
                ++i;

            int wordLen = i - startInx;
            const wchar_t* word = &str[startInx];

            auto matches = [&](const wchar_t* kw) {
                return (int)wcslen(kw) == wordLen && _wcsnicmp(word, kw, wordLen) == 0;
            };

            if (matches(L"IF"))
            {
                tokens.push_back({ btIF, line, startInx, wordLen });
            }
            else if (matches(L"THEN"))
            {
                tokens.push_back({ btTHEN, line, startInx, wordLen });
            }
            else if (matches(L"ELSIF"))
            {
                tokens.push_back({ btELSIF, line, startInx, wordLen });
            }
            else if (matches(L"ELSE"))
            {
                tokens.push_back({ btELSE, line, startInx, wordLen });
            }
            else if (matches(L"FOR"))
            {
                tokens.push_back({ btFOR, line, startInx, wordLen });
            }
            else if (matches(L"WHILE"))
            {
                tokens.push_back({ btWHILE, line, startInx, wordLen });
            }
            else if (matches(L"LOOP"))
            {
                tokens.push_back({ btLOOP, line, startInx, wordLen });
            }
            else if (matches(L"BEGIN"))
            {
                tokens.push_back({ btBEGIN, line, startInx, wordLen });
            }
            else if (matches(L"CASE"))
            {
                tokens.push_back({ btCASE, line, startInx, wordLen });
            }
            else if (matches(L"END"))
            {
                int nextInx = i;
                while (nextInx < len && iswspace(str[nextInx]))
                    ++nextInx;

                if (nextInx + 2 <= len && _wcsnicmp(&str[nextInx], L"IF", 2) == 0 &&
                    (nextInx + 2 == len || !is_ident_char(str[nextInx + 2])))
                {
                    int totalLen = (nextInx + 2) - startInx;
                    i = nextInx + 2;
                    tokens.push_back({ btEND_IF, line, startInx, totalLen });
                }
                else if (nextInx + 4 <= len && _wcsnicmp(&str[nextInx], L"LOOP", 4) == 0 &&
                         (nextInx + 4 == len || !is_ident_char(str[nextInx + 4])))
                {
                    int totalLen = (nextInx + 4) - startInx;
                    i = nextInx + 4;
                    tokens.push_back({ btEND_LOOP, line, startInx, totalLen });
                }
                else if (nextInx + 4 <= len && _wcsnicmp(&str[nextInx], L"CASE", 4) == 0 &&
                         (nextInx + 4 == len || !is_ident_char(str[nextInx + 4])))
                {
                    int totalLen = (nextInx + 4) - startInx;
                    i = nextInx + 4;
                    tokens.push_back({ btEND_CASE, line, startInx, totalLen });
                }
                else
                {
                    tokens.push_back({ btEND, line, startInx, wordLen });
                }
            }
        }
    }

    return tokens;
}

bool PlSqlBlockMatcher::FindMatchingBlock (COEditorView* pEditor, Position curPos, LanguageSupport::Match& outMatch)
{
    if (!pEditor || curPos.line < 0 || curPos.line >= pEditor->GetLineCount())
        return false;

    std::vector<BlockToken> tokens = TokenizeBlocks(pEditor, pEditor->GetLineCount());
    if (tokens.empty())
        return false;

    int charInx = pEditor->pos2inx(curPos.line, curPos.column);

    int targetIdx = -1;
    for (size_t i = 0; i < tokens.size(); ++i)
    {
        if (tokens[i].line == curPos.line &&
            tokens[i].colInx <= charInx && charInx <= tokens[i].colInx + tokens[i].length)
        {
            targetIdx = (int)i;
            break;
        }
    }

    if (targetIdx == -1 && charInx > 0)
    {
        for (size_t i = 0; i < tokens.size(); ++i)
        {
            if (tokens[i].line == curPos.line &&
                tokens[i].colInx <= (charInx - 1) && (charInx - 1) <= tokens[i].colInx + tokens[i].length)
            {
                targetIdx = (int)i;
                break;
            }
        }
    }

    if (targetIdx == -1)
        return false;

    const BlockToken& curTok = tokens[targetIdx];
    int partnerIdx = -1;
    int thenIdx = -1;

    // Se estiver em THEN: achar o IF/ELSIF anterior no mesmo nível
    if (curTok.type == btTHEN)
    {
        thenIdx = targetIdx;
        int depth = 0;
        int ifIdx = -1;
        for (int i = targetIdx - 1; i >= 0; --i)
        {
            if (tokens[i].type == btTHEN || tokens[i].type == btEND_IF)
                ++depth;
            else if (tokens[i].type == btIF || tokens[i].type == btELSIF)
            {
                if (depth == 0)
                {
                    ifIdx = i;
                    break;
                }
                --depth;
            }
        }

        if (ifIdx != -1)
        {
            int depthFwd = 0;
            for (size_t i = ifIdx + 1; i < tokens.size(); ++i)
            {
                if (tokens[i].type == btIF)
                    ++depthFwd;
                else if (tokens[i].type == btEND_IF)
                {
                    if (depthFwd == 0)
                    {
                        partnerIdx = (int)i;
                        break;
                    }
                    --depthFwd;
                }
            }
            targetIdx = ifIdx; // O bloco sempre tem como âncora inicial o IF
        }
    }
    // IF / ELSIF / ELSE -> achar o THEN correspondente e o END IF
    else if (curTok.type == btIF || curTok.type == btELSIF || curTok.type == btELSE)
    {
        int depth = 0;
        for (size_t i = targetIdx + 1; i < tokens.size(); ++i)
        {
            if (tokens[i].type == btIF)
                ++depth;
            else if (tokens[i].type == btTHEN && depth == 0 && thenIdx == -1)
            {
                thenIdx = (int)i;
            }
            else if (tokens[i].type == btEND_IF)
            {
                if (depth == 0)
                {
                    partnerIdx = (int)i;
                    break;
                }
                --depth;
            }
        }
    }
    // END IF -> achar o IF e o THEN correspondente
    else if (curTok.type == btEND_IF)
    {
        int depth = 0;
        int ifIdx = -1;
        for (int i = targetIdx - 1; i >= 0; --i)
        {
            if (tokens[i].type == btEND_IF)
                ++depth;
            else if (tokens[i].type == btIF)
            {
                if (depth == 0)
                {
                    ifIdx = i;
                    partnerIdx = targetIdx;
                    targetIdx = i;
                    break;
                }
                --depth;
            }
        }
        if (ifIdx != -1)
        {
            int depthThen = 0;
            for (size_t i = ifIdx + 1; i < (size_t)partnerIdx; ++i)
            {
                if (tokens[i].type == btIF) ++depthThen;
                else if (tokens[i].type == btTHEN && depthThen == 0) { thenIdx = (int)i; break; }
                else if (tokens[i].type == btEND_IF) --depthThen;
            }
        }
    }
    // FOR / WHILE -> achar LOOP e END LOOP
    else if (curTok.type == btFOR || curTok.type == btWHILE)
    {
        for (size_t i = targetIdx + 1; i < tokens.size(); ++i)
        {
            if (tokens[i].type == btLOOP)
            {
                thenIdx = (int)i;
                break;
            }
            if (tokens[i].type == btEND_LOOP || tokens[i].type == btEND)
                break;
        }

        if (thenIdx != -1)
        {
            int depth = 0;
            for (size_t i = thenIdx + 1; i < tokens.size(); ++i)
            {
                if (tokens[i].type == btLOOP)
                    ++depth;
                else if (tokens[i].type == btEND_LOOP)
                {
                    if (depth == 0)
                    {
                        partnerIdx = (int)i;
                        break;
                    }
                    --depth;
                }
            }
        }
    }
    // LOOP -> END LOOP (e verificar se pertence a um FOR/WHILE)
    else if (curTok.type == btLOOP)
    {
        int forDepth = 0;
        int forIdx = -1;
        for (int i = targetIdx - 1; i >= 0; --i)
        {
            if (tokens[i].type == btEND_LOOP)
                ++forDepth;
            else if (tokens[i].type == btLOOP)
            {
                if (forDepth == 0)
                    break;
                --forDepth;
            }
            else if (tokens[i].type == btFOR || tokens[i].type == btWHILE)
            {
                if (forDepth == 0)
                {
                    forIdx = i;
                    break;
                }
            }
        }

        int loopPos = targetIdx;
        if (forIdx != -1)
        {
            thenIdx = targetIdx;
            targetIdx = forIdx;
        }

        int depth = 0;
        for (size_t i = loopPos + 1; i < tokens.size(); ++i)
        {
            if (tokens[i].type == btLOOP)
                ++depth;
            else if (tokens[i].type == btEND_LOOP)
            {
                if (depth == 0)
                {
                    partnerIdx = (int)i;
                    break;
                }
                --depth;
            }
        }
    }
    // END LOOP -> LOOP (e verificar se pertence a um FOR/WHILE)
    else if (curTok.type == btEND_LOOP)
    {
        int depth = 0;
        int loopIdx = -1;
        for (int i = targetIdx - 1; i >= 0; --i)
        {
            if (tokens[i].type == btEND_LOOP)
                ++depth;
            else if (tokens[i].type == btLOOP)
            {
                if (depth == 0)
                {
                    loopIdx = i;
                    break;
                }
                --depth;
            }
        }

        if (loopIdx != -1)
        {
            partnerIdx = targetIdx;
            targetIdx = loopIdx;

            int forDepth = 0;
            for (int i = loopIdx - 1; i >= 0; --i)
            {
                if (tokens[i].type == btEND_LOOP)
                    ++forDepth;
                else if (tokens[i].type == btLOOP)
                {
                    if (forDepth == 0)
                        break;
                    --forDepth;
                }
                else if (tokens[i].type == btFOR || tokens[i].type == btWHILE)
                {
                    if (forDepth == 0)
                    {
                        targetIdx = i;
                        thenIdx = loopIdx;
                        break;
                    }
                }
            }
        }
    }
    // BEGIN -> END
    else if (curTok.type == btBEGIN)
    {
        int depth = 0;
        for (size_t i = targetIdx + 1; i < tokens.size(); ++i)
        {
            if (tokens[i].type == btBEGIN)
                ++depth;
            else if (tokens[i].type == btEND)
            {
                if (depth == 0)
                {
                    partnerIdx = (int)i;
                    break;
                }
                --depth;
            }
        }
    }
    // END -> BEGIN
    else if (curTok.type == btEND)
    {
        int depth = 0;
        for (int i = targetIdx - 1; i >= 0; --i)
        {
            if (tokens[i].type == btEND)
                ++depth;
            else if (tokens[i].type == btBEGIN)
            {
                if (depth == 0)
                {
                    partnerIdx = i;
                    break;
                }
                --depth;
            }
        }
    }
    // CASE -> END CASE
    else if (curTok.type == btCASE)
    {
        int depth = 0;
        for (size_t i = targetIdx + 1; i < tokens.size(); ++i)
        {
            if (tokens[i].type == btCASE)
                ++depth;
            else if (tokens[i].type == btEND_CASE || tokens[i].type == btEND)
            {
                if (depth == 0)
                {
                    partnerIdx = (int)i;
                    break;
                }
                --depth;
            }
        }
    }
    // END CASE -> CASE
    else if (curTok.type == btEND_CASE)
    {
        int depth = 0;
        for (int i = targetIdx - 1; i >= 0; --i)
        {
            if (tokens[i].type == btEND_CASE || tokens[i].type == btEND)
                ++depth;
            else if (tokens[i].type == btCASE)
            {
                if (depth == 0)
                {
                    partnerIdx = i;
                    break;
                }
                --depth;
            }
        }
    }

    if (partnerIdx != -1)
    {
        outMatch.found   = true;
        outMatch.broken  = false;
        outMatch.partial = false;

        int firstIdx = std::min(targetIdx, partnerIdx);
        int secondIdx = std::max(targetIdx, partnerIdx);

        outMatch.line[0]   = tokens[firstIdx].line;
        outMatch.offset[0] = pEditor->InxToPos(tokens[firstIdx].line, tokens[firstIdx].colInx);
        outMatch.length[0] = tokens[firstIdx].length;

        outMatch.line[1]   = tokens[secondIdx].line;
        outMatch.offset[1] = pEditor->InxToPos(tokens[secondIdx].line, tokens[secondIdx].colInx);
        outMatch.length[1] = tokens[secondIdx].length;

        if (thenIdx != -1)
        {
            outMatch.exprLine   = tokens[thenIdx].line;
            outMatch.exprOffset = pEditor->InxToPos(tokens[thenIdx].line, tokens[thenIdx].colInx);
            outMatch.exprLength = tokens[thenIdx].length;
        }
        else
        {
            outMatch.exprLine = outMatch.exprOffset = outMatch.exprLength = -1;
        }

        return true;
    }

    return false;
}

bool PlSqlBlockMatcher::GetEnclosingBlockInfo (COEditorView* pEditor, Position curPos, std::wstring& outScopeDesc)
{
    if (!pEditor || curPos.line < 0 || curPos.line >= pEditor->GetLineCount())
        return false;

    std::vector<BlockToken> tokens = TokenizeBlocks(pEditor, curPos.line);
    if (tokens.empty())
        return false;

    std::vector<BlockToken> stack;
    int curCharInx = pEditor->pos2inx(curPos.line, curPos.column);

    for (const auto& tok : tokens)
    {
        if (tok.line > curPos.line || (tok.line == curPos.line && tok.colInx > curCharInx))
            break;

        if (tok.type == btIF || tok.type == btBEGIN || tok.type == btCASE)
        {
            stack.push_back(tok);
        }
        else if (tok.type == btFOR || tok.type == btWHILE)
        {
            stack.push_back(tok);
        }
        else if (tok.type == btLOOP)
        {
            if (stack.empty() || (stack.back().type != btFOR && stack.back().type != btWHILE))
                stack.push_back(tok);
        }
        else if (tok.type == btEND_IF)
        {
            if (!stack.empty() && stack.back().type == btIF)
                stack.pop_back();
        }
        else if (tok.type == btEND_LOOP)
        {
            if (!stack.empty() && (stack.back().type == btLOOP || stack.back().type == btFOR || stack.back().type == btWHILE))
                stack.pop_back();
        }
        else if (tok.type == btEND)
        {
            if (!stack.empty() && stack.back().type == btBEGIN)
                stack.pop_back();
        }
        else if (tok.type == btEND_CASE)
        {
            if (!stack.empty() && stack.back().type == btCASE)
                stack.pop_back();
        }
    }

    if (stack.empty())
        return false;

    std::wstring desc = L"Bloco: ";
    for (size_t i = 0; i < stack.size(); ++i)
    {
        if (i > 0) desc += L" > ";
        switch (stack[i].type)
        {
        case btIF:
            desc += L"IF (L." + std::to_wstring(stack[i].line + 1) + L")";
            break;
        case btFOR:
            desc += L"FOR LOOP (L." + std::to_wstring(stack[i].line + 1) + L")";
            break;
        case btWHILE:
            desc += L"WHILE LOOP (L." + std::to_wstring(stack[i].line + 1) + L")";
            break;
        case btLOOP:
            desc += L"LOOP (L." + std::to_wstring(stack[i].line + 1) + L")";
            break;
        case btBEGIN:
            desc += L"BEGIN (L." + std::to_wstring(stack[i].line + 1) + L")";
            break;
        case btCASE:
            desc += L"CASE (L." + std::to_wstring(stack[i].line + 1) + L")";
            break;
        default:
            break;
        }
    }

    outScopeDesc = desc;
    return true;
}

} // namespace OpenEditor
