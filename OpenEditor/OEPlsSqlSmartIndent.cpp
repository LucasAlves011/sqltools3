#include "stdafx.h"
#include "OpenEditor/OEPlsSqlSmartIndent.h"
#include "OpenEditor/OEPlsSqlBlockMatcher.h"
#include "OpenEditor/OEView.h"
#include <cwctype>
#include <algorithm>

namespace OpenEditor
{

static bool is_ident_char (wchar_t ch)
{
    return iswalnum(ch) || ch == L'_' || ch == L'$' || ch == L'#';
}

static std::wstring GetTrimmedLine (COEditorView* pEditor, int line)
{
    OEStringW lineBuff;
    pEditor->GetLineW(line, lineBuff);
    const wchar_t* str = lineBuff.data();
    int len = lineBuff.length();

    bool inString = false;
    int commentStart = len;
    for (int i = 0; i < len; ++i)
    {
        if (str[i] == L'\'')
        {
            if (i + 1 < len && str[i + 1] == L'\'')
                ++i;
            else
                inString = !inString;
        }
        else if (!inString && i + 1 < len && str[i] == L'-' && str[i + 1] == L'-')
        {
            commentStart = i;
            break;
        }
    }

    std::wstring result(str, commentStart);
    while (!result.empty() && iswspace(result.back()))
        result.pop_back();

    return result;
}

static bool EndsWithWord (const std::wstring& s, const wchar_t* word)
{
    int wlen = (int)wcslen(word);
    int slen = (int)s.length();
    if (slen < wlen) return false;

    if (_wcsnicmp(&s[slen - wlen], word, wlen) != 0)
        return false;

    if (slen > wlen && is_ident_char(s[slen - wlen - 1]))
        return false;

    return true;
}

static bool StartsWithWord (const std::wstring& s, const wchar_t* word)
{
    int i = 0;
    int slen = (int)s.length();
    while (i < slen && iswspace(s[i])) ++i;

    int wlen = (int)wcslen(word);
    if (slen - i < wlen) return false;

    if (_wcsnicmp(&s[i], word, wlen) != 0)
        return false;

    if (i + wlen < slen && is_ident_char(s[i + wlen]))
        return false;

    return true;
}

int PlSqlSmartIndent::GetLineIndentColumn (COEditorView* pEditor, int line)
{
    if (!pEditor || line < 0 || line >= pEditor->GetLineCount())
        return 0;

    OEStringW lineBuff;
    pEditor->GetLineW(line, lineBuff);
    const wchar_t* str = lineBuff.data();
    int len = lineBuff.length();

    int i = 0;
    while (i < len && iswspace(str[i]))
        ++i;

    return pEditor->InxToPos(line, i);
}

void PlSqlSmartIndent::SetLineIndentColumn (COEditorView* pEditor, int line, int targetCol)
{
    if (!pEditor || line < 0 || line >= pEditor->GetLineCount())
        return;

    targetCol = std::max(0, targetCol);

    OEStringW lineBuff;
    pEditor->GetLineW(line, lineBuff);
    const wchar_t* str = lineBuff.data();
    int len = lineBuff.length();

    int i = 0;
    while (i < len && iswspace(str[i]))
        ++i;

    int currentCol = pEditor->InxToPos(line, i);
    if (currentCol == targetCol)
        return;

    Square sel;
    sel.start.line = line;
    sel.start.column = 0;
    sel.end.line = line;
    sel.end.column = currentCol;

    std::wstring buff(targetCol, L' ');

    pEditor->SetSelection(sel);
    pEditor->InsertBlock(buff.c_str(), true, true);
}

int PlSqlSmartIndent::CalculateNewLineColumn (COEditorView* pEditor, int prevLine, int baseColumn, int indentSpacing)
{
    if (!pEditor || prevLine < 0 || prevLine >= pEditor->GetLineCount())
        return baseColumn;

    std::wstring trimmed = GetTrimmedLine(pEditor, prevLine);
    if (trimmed.empty())
        return baseColumn;

    if (EndsWithWord(trimmed, L"THEN") ||
        EndsWithWord(trimmed, L"LOOP") ||
        EndsWithWord(trimmed, L"BEGIN") ||
        EndsWithWord(trimmed, L"DECLARE") ||
        EndsWithWord(trimmed, L"EXCEPTION") ||
        EndsWithWord(trimmed, L"ELSE") ||
        EndsWithWord(trimmed, L"IS") ||
        EndsWithWord(trimmed, L"AS") ||
        EndsWithWord(trimmed, L"CASE"))
    {
        return baseColumn + indentSpacing;
    }

    return baseColumn;
}

bool PlSqlSmartIndent::AdjustLineIndentIfClosing (COEditorView* pEditor, int line, int indentSpacing)
{
    if (!pEditor || line < 0 || line >= pEditor->GetLineCount())
        return false;

    std::wstring trimmed = GetTrimmedLine(pEditor, line);
    if (trimmed.empty())
        return false;

    bool isClosing = false;
    if (StartsWithWord(trimmed, L"END IF") ||
        StartsWithWord(trimmed, L"END LOOP") ||
        StartsWithWord(trimmed, L"END CASE") ||
        StartsWithWord(trimmed, L"END") ||
        StartsWithWord(trimmed, L"ELSE") ||
        StartsWithWord(trimmed, L"ELSIF") ||
        StartsWithWord(trimmed, L"EXCEPTION"))
    {
        isClosing = true;
    }

    if (!isClosing)
        return false;

    int charInx = 0;
    OEStringW lineBuff;
    pEditor->GetLineW(line, lineBuff);
    const wchar_t* pStr = lineBuff.data();
    while (charInx < lineBuff.length() && iswspace(pStr[charInx]))
        ++charInx;

    Position pos;
    pos.line = line;
    pos.column = pEditor->InxToPos(line, charInx);

    LanguageSupport::Match match;
    if (PlSqlBlockMatcher::FindMatchingBlock(pEditor, pos, match) && match.found)
    {
        int openerLine = match.line[0];
        if (openerLine != line)
        {
            int openerCol = GetLineIndentColumn(pEditor, openerLine);
            SetLineIndentColumn(pEditor, line, openerCol);
            return true;
        }
    }

    int curCol = GetLineIndentColumn(pEditor, line);
    SetLineIndentColumn(pEditor, line, std::max(0, curCol - indentSpacing));
    return true;
}

void PlSqlSmartIndent::ReindentRange (COEditorView* pEditor, int startLine, int endLine, int indentSpacing)
{
    if (!pEditor)
        return;

    startLine = std::max(0, startLine);
    endLine = std::min(pEditor->GetLineCount() - 1, endLine);
    if (startLine > endLine)
        return;

    int level = GetLineIndentColumn(pEditor, startLine) / (indentSpacing > 0 ? indentSpacing : 2);

    for (int line = startLine; line <= endLine; ++line)
    {
        std::wstring trimmed = GetTrimmedLine(pEditor, line);
        if (trimmed.empty())
            continue;

        bool isCloser = (StartsWithWord(trimmed, L"END IF") ||
                         StartsWithWord(trimmed, L"END LOOP") ||
                         StartsWithWord(trimmed, L"END CASE") ||
                         StartsWithWord(trimmed, L"END") ||
                         StartsWithWord(trimmed, L"ELSE") ||
                         StartsWithWord(trimmed, L"ELSIF") ||
                         StartsWithWord(trimmed, L"EXCEPTION"));

        if (isCloser && level > 0)
            --level;

        SetLineIndentColumn(pEditor, line, level * indentSpacing);

        bool isOpener = (EndsWithWord(trimmed, L"THEN") ||
                         EndsWithWord(trimmed, L"LOOP") ||
                         EndsWithWord(trimmed, L"BEGIN") ||
                         EndsWithWord(trimmed, L"DECLARE") ||
                         EndsWithWord(trimmed, L"EXCEPTION") ||
                         EndsWithWord(trimmed, L"ELSE") ||
                         EndsWithWord(trimmed, L"IS") ||
                         EndsWithWord(trimmed, L"AS") ||
                         EndsWithWord(trimmed, L"CASE"));

        if (isOpener)
            ++level;
    }
}

} // namespace OpenEditor
