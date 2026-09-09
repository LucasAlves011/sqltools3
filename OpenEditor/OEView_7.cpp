/* 
    Copyright (C) 2018-2026 Aleksey Kochetov

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
*/

#include "stdafx.h"
#include "OpenEditor/OEView.h"
#include <COMMON/AppGlobal.h>
#include <COMMON/ErrorDlg.h>
#include "OpenEditor/OEPlsSqlParser.h"
#include "OpenEditor/OEPlsSqlFormatter.h"
#include "OpenEditor/OEDocument.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

    using namespace std;
    using namespace OpenEditor;
    using namespace Common::PlsSql;

    // FMT_ formatter settings are read from GetSettings() inside OnEditFormatSql().
    // FMT_INDENT_SIZE is sourced from GetIndentSpacing() (ClassSettings::IndentSpacing).

#define RETURN_IF_LOCKED { if (!m_bAttached) return; if (IsLocked()) { Global::SetStatusText("The content is locked and cannot be modified!"); MessageBeep((UINT)-1); return; } } 

// ============================================================================
// verifyFormatterLosslessness
//
// Reparses the formatted buffer, then compares its non-whitespace token stream
// against the original one.  Reports up to 50 mismatches with line numbers.
// ============================================================================
static void verifyFormatterLosslessness (
    const std::vector<OpenEditor::Token>& original,
    const std::wstring&                   formatted)
{
    // --- reparse the formatted buffer ---
    OpenEditor::SimpleSyntaxAnalyser analyser2;
    OpenEditor::PlSqlParserEx        parser2(analyser2);

    // split formatted into lines and feed them one by one
    int lineNo = (original.size() > 0) ? original.begin()->line : 0;
    std::wstring::size_type pos = 0;
    while (pos <= formatted.size())
    {
        auto nl = formatted.find(L'\n', pos);
        std::wstring::size_type end = (nl == std::wstring::npos) ? formatted.size() : nl;
        parser2.PutLine(lineNo, formatted.c_str() + pos, (int)(end - pos));
        ++lineNo;
        if (nl == std::wstring::npos) break;
        pos = nl + 1;
    }

    const auto& reparsed = analyser2.GetTokens();

    // --- compare ignoring etEOL / etWHITESPACE ---
    auto isSpace = [](const OpenEditor::Token& t)
    {
        return t.token == etEOL || t.token == etWHITESPACE;
    };

    auto skipSpace = [&](const std::vector<OpenEditor::Token>& v,
                         size_t i) -> size_t
    {
        while (i < v.size() && isSpace(v[i])) ++i;
        return i;
    };

    // Normalize a token value for comparison: for multiline comments, strip
    // leading whitespace from every line after the first so that alignment
    // differences introduced by align_comment() are ignored.
    auto normalizeValue = [](const OpenEditor::Token& t) -> std::wstring
    {
        if (t.token != etCOMMENT || t.value.find(L'\n') == std::wstring::npos)
            return t.value;
        std::wstring result;
        result.reserve(t.value.size());
        bool afterNewline = false;
        for (wchar_t ch : t.value)
        {
            if (ch == L'\n') { result += ch; afterNewline = true; continue; }
            if (afterNewline && (ch == L' ' || ch == L'\t')) continue;
            afterNewline = false;
            result += ch;
        }
        return result;
    };

    const int maxErrors = 10;
    int errorCount = 0;
    std::string errorLog;

    size_t oi = 0, ri = 0;
    oi = skipSpace(original,  oi);
    ri = skipSpace(reparsed,  ri);

    while (oi < original.size() && ri < reparsed.size())
    {
        const auto& ot = original[oi];
        const auto& rt = reparsed[ri];

        if (ot.token != rt.token || normalizeValue(ot) != normalizeValue(rt))
        {
            wchar_t msg[512];
            auto ov = ot.value.length() < 20 ? ot.value : (ot.value.substr(0, 17) + L"...");
            auto rv = rt.value.length() < 20 ? rt.value : (rt.value.substr(0, 17) + L"...");
            swprintf_s(msg, _countof(msg),
                L"Original  line %d: token=%d len=%d value=\"%s\"\n"
                L"Formatted line %d: token=%d len=%d value=\"%s\"\n\n",
                ot.line + 1, (int)ot.token, (int)ot.value.length(), ov.c_str(),
                rt.line + 1, (int)rt.token, (int)rt.value.length(), rv.c_str());
            TRACE(L"%s", msg);
            errorLog += CW2A(msg);
            if (++errorCount >= maxErrors)
            {
                errorLog += "(further errors suppressed)\n";
                break;
            }
        }

        oi = skipSpace(original, oi + 1);
        ri = skipSpace(reparsed, ri + 1);
    }

    // skip trailing spaces in both
    oi = skipSpace(original, oi);
    ri = skipSpace(reparsed, ri);

    if (errorCount == 0)
    {
        if (oi < original.size())
        {
            wchar_t msg[256];
            swprintf_s(msg, _countof(msg),
                L"Formatted output is missing tokens starting at original line %d: \"%s\"\n",
                original[oi].line + 1, original[oi].value.c_str());
            TRACE(L"%s", msg);
            errorLog += CW2A(msg);
        }
        else if (ri < reparsed.size())
        {
            wchar_t msg[256];
            swprintf_s(msg, _countof(msg),
                L"Formatted output has extra tokens starting at formatted line %d: \"%s\"\n",
                reparsed[ri].line + 1, reparsed[ri].value.c_str());
            TRACE(L"%s", msg);
            errorLog += CW2A(msg);
        }
    }

    if (!errorLog.empty())
    {
        errorLog = "Formatter losslessness check FAILED!\n\n" + errorLog;
        CErrorDlg(errorLog.c_str(), "FormatterLosslessnessCheck").DoModal();
    }
    else
    {
        TRACE(L"Formatter losslessness check passed.\n");
    }
}

void COEditorView::OnEditFormatSqlWithSettings ()
{
    if (!IsSelectionEmpty())
        GetDocument()->ShowFormatSqlSettings(this);
}

void COEditorView::OnEditFormatSql ()
{
    RETURN_IF_LOCKED

    if (!IsSelectionEmpty())
    {
        if (GetBlockMode() != EBlockMode::ebtStream)
        {
            AfxMessageBox(L"SQL Format is supported on steam selection only!", MB_OK | MB_ICONSTOP);
            AfxThrowUserException();
        }

        OpenEditor::SimpleSyntaxAnalyser analyser;
        OpenEditor::PlSqlParserEx parser(analyser);

        OpenEditor::Square selection;
        GetSelection(selection);
        selection.normalize();

        // convert positions to indexes (because of tabs)
        selection.start.column = PosToInx(selection.start.line, selection.start.column);
        selection.end.column   = PosToInx(selection.end.line, selection.end.column);


        int line   = selection.start.line;
        int offset = selection.start.column;
        int nlines = GetLineCount();

        for (; line < nlines && line <= selection.end.line; line++)
        {
            OEStringW lineBuff;
            GetLineW(line, lineBuff);
            const wchar_t* str = lineBuff.data();
            int length = (int)lineBuff.length();

            if (line == selection.end.line) 
                length = min(length, selection.end.column);

            parser.PutLine(line, str + offset, length - offset);
            offset = 0;
        }

        auto tokens = analyser.GetTokens();

        wstring buffer = formatSQL(tokens, GetSettings());

        verifyFormatterLosslessness(tokens, buffer);

        UndoGroup undoGroup(*this);
        PushInUndoStack(GetPosition());
        DeleteBlock(true);

        size_t end = buffer.find_last_not_of(L" \n\r");
        if (end != std::string::npos) {
            buffer.erase(end + 1);
        }
        // TODO: add switch to align formatted text by first keyword (token)
        if (selection.start.column == 0)
        {
            Position pos = { InxToPos(tokens.begin()->line, tokens.begin()->offset), tokens.begin()->line };
            MoveTo(pos, true);
        }
        AlignCodeFragment(buffer.c_str(), buffer);

        if (!selection.end.column && selection.end.line < nlines)
            buffer += L'\n';

        InsertBlock(buffer.c_str(), false, true);
    }
}
