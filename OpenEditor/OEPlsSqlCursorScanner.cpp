#include "stdafx.h"
#include "OpenEditor/OEPlsSqlCursorScanner.h"
#include "OpenEditor/OEView.h"
#include <cwctype>
#include <algorithm>

namespace OpenEditor
{

static bool is_ident_char (wchar_t ch)
{
    return iswalnum(ch) || ch == L'_' || ch == L'$' || ch == L'#';
}

static bool iequals (const std::wstring& a, const std::wstring& b)
{
    if (a.length() != b.length())
        return false;
    return _wcsicmp(a.c_str(), b.c_str()) == 0;
}

bool PlSqlCursorScanner::FindCursorByName (COEditorView* pEditor, const std::wstring& cursorName, PlSqlCursorInfo& outInfo)
{
    if (!pEditor || cursorName.empty())
        return false;

    return ScanDocument(pEditor, cursorName, &outInfo, nullptr);
}

bool PlSqlCursorScanner::FindLocalObjectByName (COEditorView* pEditor, const std::wstring& objectName, PlSqlObjectInfo& outInfo)
{
    if (!pEditor || objectName.empty())
        return false;

    if (FindCursorByName(pEditor, objectName, outInfo))
        return true;

    return ScanLocalRoutines(pEditor, objectName, &outInfo);
}

std::vector<PlSqlCursorInfo> PlSqlCursorScanner::FindAllCursors (COEditorView* pEditor)
{
    std::vector<PlSqlCursorInfo> list;
    if (pEditor)
        ScanDocument(pEditor, L"", nullptr, &list);
    return list;
}

bool PlSqlCursorScanner::IsCursorUnderPosition (COEditorView* pEditor, Position pos, PlSqlCursorInfo& outInfo)
{
    return IsObjectUnderPosition(pEditor, pos, outInfo);
}

bool PlSqlCursorScanner::IsObjectUnderPosition (COEditorView* pEditor, Position pos, PlSqlObjectInfo& outInfo)
{
    if (!pEditor)
        return false;

    Square sqr;
    if (pEditor->WordFromPoint(pos, sqr))
    {
        std::wstring word;
        pEditor->GetBlock(word, &sqr);

        while (!word.empty() && iswspace(word.front())) word.erase(word.begin());
        while (!word.empty() && iswspace(word.back())) word.pop_back();

        if (!word.empty() && FindLocalObjectByName(pEditor, word, outInfo))
            return true;
    }
    return false;
}

bool PlSqlCursorScanner::ScanDocument (COEditorView* pEditor, const std::wstring& targetName, PlSqlCursorInfo* pOutSingle, std::vector<PlSqlCursorInfo>* pOutList)
{
    int lineCount = pEditor->GetLineCount();
    if (lineCount <= 0)
        return false;

    bool inBlockComment = false;

    for (int line = 0; line < lineCount; ++line)
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
                break;

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

            if ((i == 0 || !is_ident_char(str[i - 1])) &&
                (i + 6 <= len) &&
                _wcsnicmp(&str[i], L"CURSOR", 6) == 0 &&
                (i + 6 == len || !is_ident_char(str[i + 6])))
            {
                PlSqlCursorInfo info;
                info.declLineStart = line;
                info.declStart.line = line;
                info.declStart.column = pEditor->InxToPos(line, i);

                int curLine = line;
                int curInx = i + 6;

                auto advanceToNextNonSpace = [&](int& cL, int& cI) -> bool {
                    while (cL < lineCount)
                    {
                        OEStringW cLineBuff;
                        pEditor->GetLineW(cL, cLineBuff);
                        const wchar_t* cStr = cLineBuff.data();
                        int cLen = cLineBuff.length();

                        while (cI < cLen)
                        {
                            if (inBlockComment)
                            {
                                if (cI + 1 < cLen && cStr[cI] == L'*' && cStr[cI + 1] == L'/')
                                {
                                    inBlockComment = false;
                                    cI += 2;
                                    continue;
                                }
                                ++cI;
                                continue;
                            }
                            if (cI + 1 < cLen && cStr[cI] == L'/' && cStr[cI + 1] == L'*')
                            {
                                inBlockComment = true;
                                cI += 2;
                                continue;
                            }
                            if (cI + 1 < cLen && cStr[cI] == L'-' && cStr[cI + 1] == L'-')
                            {
                                cI = cLen;
                                break;
                            }
                            if (!iswspace(cStr[cI]))
                                return true;
                            ++cI;
                        }
                        ++cL;
                        cI = 0;
                    }
                    return false;
                };

                if (!advanceToNextNonSpace(curLine, curInx))
                    break;

                std::wstring cursorName;
                {
                    OEStringW cLineBuff;
                    pEditor->GetLineW(curLine, cLineBuff);
                    const wchar_t* cStr = cLineBuff.data();
                    int cLen = cLineBuff.length();

                    while (curInx < cLen && is_ident_char(cStr[curInx]))
                    {
                        cursorName += cStr[curInx];
                        ++curInx;
                    }
                }

                if (cursorName.empty())
                    continue;

                info.type = eObjCursor;
                info.name = cursorName;
                info.title = L"Cursor: " + cursorName;

                if (!advanceToNextNonSpace(curLine, curInx))
                    continue;

                {
                    OEStringW cLineBuff;
                    pEditor->GetLineW(curLine, cLineBuff);
                    const wchar_t* cStr = cLineBuff.data();
                    int cLen = cLineBuff.length();

                    if (curInx < cLen && cStr[curInx] == L'(')
                    {
                        int parenDepth = 0;
                        while (curLine < lineCount)
                        {
                            OEStringW pLineBuff;
                            pEditor->GetLineW(curLine, pLineBuff);
                            const wchar_t* pStr = pLineBuff.data();
                            int pLen = pLineBuff.length();

                            while (curInx < pLen)
                            {
                                wchar_t pch = pStr[curInx];
                                info.params += pch;
                                if (pch == L'(') parenDepth++;
                                else if (pch == L')')
                                {
                                    parenDepth--;
                                    if (parenDepth == 0)
                                    {
                                        ++curInx;
                                        break;
                                    }
                                }
                                ++curInx;
                            }
                            if (parenDepth == 0) break;
                            ++curLine;
                            curInx = 0;
                        }
                    }
                }

                bool isFound = false;
                while (advanceToNextNonSpace(curLine, curInx))
                {
                    OEStringW cLineBuff;
                    pEditor->GetLineW(curLine, cLineBuff);
                    const wchar_t* cStr = cLineBuff.data();
                    int cLen = cLineBuff.length();

                    if (cStr[curInx] == L';')
                        break;

                    if ((curInx == 0 || !is_ident_char(cStr[curInx - 1])) &&
                        (curInx + 2 <= cLen) &&
                        _wcsnicmp(&cStr[curInx], L"IS", 2) == 0 &&
                        (curInx + 2 == cLen || !is_ident_char(cStr[curInx + 2])))
                    {
                        curInx += 2;
                        isFound = true;
                        break;
                    }
                    ++curInx;
                }

                if (!isFound)
                    continue;

                if (!advanceToNextNonSpace(curLine, curInx))
                    continue;

                info.selectStart.line = curLine;
                info.selectStart.column = pEditor->InxToPos(curLine, curInx);

                int selectEndLine = curLine;
                int selectEndInx = curInx;
                bool terminated = false;
                int parenDepth = 0;

                while (curLine < lineCount)
                {
                    OEStringW cLineBuff;
                    pEditor->GetLineW(curLine, cLineBuff);
                    const wchar_t* cStr = cLineBuff.data();
                    int cLen = cLineBuff.length();

                    while (curInx < cLen)
                    {
                        if (inBlockComment)
                        {
                            if (curInx + 1 < cLen && cStr[curInx] == L'*' && cStr[curInx + 1] == L'/')
                            {
                                inBlockComment = false;
                                curInx += 2;
                                continue;
                            }
                            ++curInx;
                            continue;
                        }

                        if (curInx + 1 < cLen && cStr[curInx] == L'/' && cStr[curInx + 1] == L'*')
                        {
                            inBlockComment = true;
                            curInx += 2;
                            continue;
                        }

                        if (curInx + 1 < cLen && cStr[curInx] == L'-' && cStr[curInx + 1] == L'-')
                        {
                            curInx = cLen;
                            break;
                        }

                        if (cStr[curInx] == L'\'')
                        {
                            ++curInx;
                            while (curInx < cLen)
                            {
                                if (cStr[curInx] == L'\'')
                                {
                                    if (curInx + 1 < cLen && cStr[curInx + 1] == L'\'')
                                        curInx += 2;
                                    else
                                        break;
                                }
                                else
                                    ++curInx;
                            }
                            ++curInx;
                            continue;
                        }

                        if (cStr[curInx] == L'(') parenDepth++;
                        else if (cStr[curInx] == L')') { if (parenDepth > 0) parenDepth--; }
                        else if (cStr[curInx] == L';' && parenDepth == 0)
                        {
                            selectEndLine = curLine;
                            selectEndInx = curInx;
                            terminated = true;
                            break;
                        }

                        ++curInx;
                    }

                    if (terminated)
                        break;

                    ++curLine;
                    curInx = 0;
                }

                if (terminated)
                {
                    info.declLineEnd = selectEndLine;
                    info.declEnd.line = selectEndLine;
                    info.declEnd.column = pEditor->InxToPos(selectEndLine, selectEndInx + 1);

                    info.selectEnd.line = selectEndLine;
                    info.selectEnd.column = pEditor->InxToPos(selectEndLine, selectEndInx);

                    Square selSquare;
                    selSquare.start = info.selectStart;
                    selSquare.end = info.selectEnd;
                    pEditor->GetBlock(info.selectQuery, &selSquare);

                    while (!info.selectQuery.empty() && iswspace(info.selectQuery.back()))
                        info.selectQuery.pop_back();

                    if (!targetName.empty())
                    {
                        if (iequals(info.name, targetName))
                        {
                            if (pOutSingle)
                                *pOutSingle = info;
                            return true;
                        }
                    }
                    else if (pOutList)
                    {
                        pOutList->push_back(info);
                    }
                }
            }
        }
    }

    return false;
}

bool PlSqlCursorScanner::ScanLocalRoutines (COEditorView* pEditor, const std::wstring& targetName, PlSqlObjectInfo* pOutSingle)
{
    int lineCount = pEditor->GetLineCount();
    if (lineCount <= 0 || targetName.empty())
        return false;

    bool inBlockComment = false;

    for (int line = 0; line < lineCount; ++line)
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
                break;

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

            bool isProc = false;
            bool isFunc = false;
            int kwLen = 0;

            if ((i == 0 || !is_ident_char(str[i - 1])))
            {
                if ((i + 9 <= len) && _wcsnicmp(&str[i], L"PROCEDURE", 9) == 0 && (i + 9 == len || !is_ident_char(str[i + 9])))
                {
                    isProc = true;
                    kwLen = 9;
                }
                else if ((i + 8 <= len) && _wcsnicmp(&str[i], L"FUNCTION", 8) == 0 && (i + 8 == len || !is_ident_char(str[i + 8])))
                {
                    isFunc = true;
                    kwLen = 8;
                }
            }

            if (isProc || isFunc)
            {
                // Verifica se NÃO é CREATE OR REPLACE (procedure/função de nível de arquivo)
                bool isCreate = false;
                for (int p = 0; p < i; ++p)
                {
                    if ((p == 0 || !is_ident_char(str[p - 1])) &&
                        (p + 6 <= i) && _wcsnicmp(&str[p], L"CREATE", 6) == 0 &&
                        (p + 6 == i || !is_ident_char(str[p + 6])))
                    {
                        isCreate = true;
                        break;
                    }
                }
                if (isCreate)
                    continue;

                PlSqlObjectInfo info;
                info.type = isFunc ? eObjFunction : eObjProcedure;
                info.declLineStart = line;
                info.declStart.line = line;
                info.declStart.column = pEditor->InxToPos(line, i);

                int curLine = line;
                int curInx = i + kwLen;

                auto advance = [&](int& cL, int& cI) -> bool {
                    while (cL < lineCount)
                    {
                        OEStringW b;
                        pEditor->GetLineW(cL, b);
                        const wchar_t* s = b.data();
                        int l = b.length();
                        while (cI < l)
                        {
                            if (inBlockComment)
                            {
                                if (cI + 1 < l && s[cI] == L'*' && s[cI + 1] == L'/') { inBlockComment = false; cI += 2; continue; }
                                ++cI;
                                continue;
                            }
                            if (cI + 1 < l && s[cI] == L'/' && s[cI + 1] == L'*') { inBlockComment = true; cI += 2; continue; }
                            if (cI + 1 < l && s[cI] == L'-' && s[cI + 1] == L'-') { cI = l; break; }
                            if (!iswspace(s[cI])) return true;
                            ++cI;
                        }
                        ++cL;
                        cI = 0;
                    }
                    return false;
                };

                if (!advance(curLine, curInx))
                    continue;

                std::wstring routineName;
                {
                    OEStringW b;
                    pEditor->GetLineW(curLine, b);
                    const wchar_t* s = b.data();
                    int l = b.length();
                    while (curInx < l && is_ident_char(s[curInx]))
                    {
                        routineName += s[curInx];
                        ++curInx;
                    }
                }

                if (routineName.empty() || !iequals(routineName, targetName))
                    continue;

                info.name = routineName;
                info.title = (isFunc ? L"Função: " : L"Procedure: ") + routineName;

                bool foundIsAs = false;
                while (advance(curLine, curInx))
                {
                    OEStringW b;
                    pEditor->GetLineW(curLine, b);
                    const wchar_t* s = b.data();
                    int l = b.length();

                    if (s[curInx] == L';')
                        break;

                    if ((curInx == 0 || !is_ident_char(s[curInx - 1])) &&
                        (curInx + 2 <= l) &&
                        (_wcsnicmp(&s[curInx], L"IS", 2) == 0 || _wcsnicmp(&s[curInx], L"AS", 2) == 0) &&
                        (curInx + 2 == l || !is_ident_char(s[curInx + 2])))
                    {
                        curInx += 2;
                        foundIsAs = true;
                        break;
                    }
                    ++curInx;
                }

                if (!foundIsAs)
                    continue;

                int blockDepth = 0;
                bool bodyFound = false;
                int endLine = curLine;
                int endInx = curInx;

                while (advance(curLine, curInx))
                {
                    OEStringW b;
                    pEditor->GetLineW(curLine, b);
                    const wchar_t* s = b.data();
                    int l = b.length();

                    if ((curInx == 0 || !is_ident_char(s[curInx - 1])))
                    {
                        if ((curInx + 5 <= l) && _wcsnicmp(&s[curInx], L"BEGIN", 5) == 0 && (curInx + 5 == l || !is_ident_char(s[curInx + 5])))
                        {
                            blockDepth++;
                            curInx += 5;
                            continue;
                        }
                        else if ((curInx + 4 <= l) && _wcsnicmp(&s[curInx], L"CASE", 4) == 0 && (curInx + 4 == l || !is_ident_char(s[curInx + 4])))
                        {
                            blockDepth++;
                            curInx += 4;
                            continue;
                        }
                        else if ((curInx + 4 <= l) && _wcsnicmp(&s[curInx], L"LOOP", 4) == 0 && (curInx + 4 == l || !is_ident_char(s[curInx + 4])))
                        {
                            blockDepth++;
                            curInx += 4;
                            continue;
                        }
                        else if ((curInx + 3 <= l) && _wcsnicmp(&s[curInx], L"END", 3) == 0 && (curInx + 3 == l || !is_ident_char(s[curInx + 3])))
                        {
                            curInx += 3;
                            if (advance(curLine, curInx))
                            {
                                OEStringW b2;
                                pEditor->GetLineW(curLine, b2);
                                const wchar_t* s2 = b2.data();
                                int l2 = b2.length();

                                if ((curInx + 2 <= l2) && _wcsnicmp(&s2[curInx], L"IF", 2) == 0 && (curInx + 2 == l2 || !is_ident_char(s2[curInx + 2])))
                                {
                                    curInx += 2;
                                }
                                else if ((curInx + 4 <= l2) && _wcsnicmp(&s2[curInx], L"LOOP", 4) == 0 && (curInx + 4 == l2 || !is_ident_char(s2[curInx + 4])))
                                {
                                    curInx += 4;
                                    blockDepth--;
                                }
                                else if ((curInx + 4 <= l2) && _wcsnicmp(&s2[curInx], L"CASE", 4) == 0 && (curInx + 4 == l2 || !is_ident_char(s2[curInx + 4])))
                                {
                                    curInx += 4;
                                    blockDepth--;
                                }
                                else
                                {
                                    blockDepth--;
                                    if (blockDepth <= 0)
                                    {
                                        while (advance(curLine, curInx))
                                        {
                                            OEStringW b3;
                                            pEditor->GetLineW(curLine, b3);
                                            if (b3.data()[curInx] == L';')
                                            {
                                                endLine = curLine;
                                                endInx = curInx;
                                                bodyFound = true;
                                                break;
                                            }
                                            ++curInx;
                                        }
                                        break;
                                    }
                                }
                            }
                            continue;
                        }
                    }
                    ++curInx;
                }

                if (bodyFound)
                {
                    info.declLineEnd = endLine;
                    info.declEnd.line = endLine;
                    info.declEnd.column = pEditor->InxToPos(endLine, endInx + 1);

                    info.selectStart = info.declStart;
                    info.selectEnd = info.declEnd;

                    Square selSquare;
                    selSquare.start = info.selectStart;
                    selSquare.end = info.selectEnd;
                    pEditor->GetBlock(info.selectQuery, &selSquare);

                    while (!info.selectQuery.empty() && iswspace(info.selectQuery.back()))
                        info.selectQuery.pop_back();

                    if (pOutSingle)
                        *pOutSingle = info;
                    return true;
                }
            }
        }
    }

    return false;
}

} // namespace OpenEditor
