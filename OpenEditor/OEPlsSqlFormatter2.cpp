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
#include "OpenEditor/OEPlsSqlFormatter.h"
#include "OpenEditor/OESettings.h"
#include <algorithm>
#include <climits>
#include <sstream>
#include <stack>

namespace OpenEditor {

    using namespace std;
    using namespace Common::PlsSql;

    /*
        POSSIBLY:
        introduce popSpecialIndent( to FROM or to WHERE);
        incorparate COEditorView::PreNormalizeOnChar
        add select the current statement Ctl+Shift+F10
        brick formatting for column list when
          number of columns > 40-60
          columns are relativly short w/o complex expressions
          no comments, at least no end-line comments
    */

    enum IndentType {
        eitRoot,
        eitBlock,
        eitSpecial,
    };

    struct IndentCtx
    {
        IndentCtx(IndentType type, int size = 2)
            : type(type),
            size(size),
            spacer(size, L' ')
        {
        }

        IndentCtx(IndentType type, const IndentCtx& prev, int size = 2)
            : type(type),
            size(prev.size + size),
            spacer(prev.spacer.size() + size, L' ')
        {
        }

        IndentType type;
        int size;
        wstring spacer;
    };

    struct IndentCtxStack : std::stack<IndentCtx>
    {
        IndentCtxStack(int size)
            : indentSize(size),
            currentLineTokenCounter(0)
        {
            push(IndentCtx(eitRoot, 0));
        }

        void pushSpecialIndent(bool resetCounter = true)
        {
            push(IndentCtx(eitSpecial, top(), indentSize));

            if (resetCounter)
                resetCurrentLineTokenCounter();
        }

        void popSpecialIndent(bool resetCounter = true)
        {
            if (top().type == eitSpecial)
                pop();

            if (resetCounter)
                resetCurrentLineTokenCounter();
        }

        void pushBlockIndent()
        {
            push(IndentCtx(eitBlock, top(), indentSize));
            resetCurrentLineTokenCounter();
        }

        void popBlockIndent()
        {
            while (top().type == eitSpecial)
                pop();

            if (top().type == eitBlock)
                pop();

            resetCurrentLineTokenCounter();
        }

        int  getCurrentLineTokenCounter() const { return currentLineTokenCounter; }
        void incrementCurrentLineTokenCounter() { ++currentLineTokenCounter; }
        void resetCurrentLineTokenCounter() { currentLineTokenCounter = 0; }

        const wstring& getSpacer() const { return top().spacer; }

        int indentSize;
        int currentLineTokenCounter;
    };

    // Returns true for a -- end-line comment (value always starts with "--").
    static
    bool is_end_line_comment(const OpenEditor::Token& tk)
    {
        return tk.token == etCOMMENT
            && tk.value.size() >= 2
            && tk.value[0] == L'-'
            && tk.value[1] == L'-';
    }

    // Forward declaration — defined after OutputTokenStream.
    static void align_comment(Token& tk, const wstring& spacer);
    static void realign_comment(Token& tk, int delta);

    struct OutputTokenStream
    {
        std::vector<OpenEditor::Token> tokens;

        bool endsWithEol() const
        {
            for (auto it = tokens.rbegin(); it != tokens.rend(); ++it)
                if (it->token != etWHITESPACE)
                    return it->token == etEOL;
            return true; // empty stream — treat as already on a fresh line
        }

        void  putSpace(const wstring& val = wstring(1, L' '))
        {
            OpenEditor::Token tk;
            tk.token = etWHITESPACE;
            tk.value = val;
            tk.length = static_cast<OpenEditor::Token::size_pos>(tk.value.length());
            tokens.push_back(tk);
        }

        void  putEol()
        {
            OpenEditor::Token tk;
            tk.token = etEOL;
            tk.value = L"\n";
            tk.length = static_cast<OpenEditor::Token::size_pos>(tk.value.length());
            tokens.push_back(tk);
        }

        void  putToken(const OpenEditor::Token& tk /*, const wchar_t* str, int len*/)
        {
            tokens.push_back(tk);
            //tokens.back().value = wstring(str, len);
        }

        void flush(std::wostream& out)
        {
            for (auto it = tokens.begin(); it != tokens.end(); ++it)
                out << it->value;
        }

        int position() const { return tokens.size(); }

        /*
        * alignStatementComments scans the token range [from, end)
        * formatted statement — and aligns every end-line "--" comment to the same
        * column.  Two sub-passes: measure the widest content-before-comment width,
        * then set each preceding whitespace token to reach that column + 2 spaces.
        *
        * Hint-comment exception: a line where the last non-whitespace token before
        * the "--" is SELECT is a query hint (e.g. SELECT --+hint).  Such lines are
        * excluded from both measurement and gap adjustment.  The check is per-line
        * so it works at any subquery nesting depth.
        */
        void alignStatementComments(int from)
        {
            if (from < 0 || from >= (int)tokens.size()) return;

            auto stmtBegin = tokens.begin() + from;
            auto stmtEnd = tokens.end();

            // Returns true when the last non-whitespace token on [lineBegin, commentIt)
            // is SELECT — i.e. this "--" comment is a query hint, not a column comment.
            auto isHintComment =
                [&](decltype(stmtBegin) lineBegin, decltype(stmtBegin) commentIt) -> bool
                {
                    for (auto r = commentIt; r != lineBegin; )
                    {
                        --r;
                        if (r->token == etWHITESPACE) continue;
                        return r->token == etSELECT || r->token == etDELETE 
                            || r->token == etUPDATE || r->token == etINSERT
                            || r->token == etMERGE;
                    }
                    return false;
                };

            // Width of a token on the current visual line.
            // A multi-line block comment token contains '\n'; only the portion after
            // the last '\n' is on the current output line.  Earlier lines inside the
            // comment do not contribute to the current line's visual width, so bufLen
            // is reset to that last-line length rather than accumulating the full value.
            auto accumWidth = [](size_t& bufLen, const OpenEditor::Token& tk)
                {
                    auto pos = tk.value.rfind(L'\n');
                    if (pos == wstring::npos)
                        bufLen += tk.value.size();
                    else
                        bufLen = tk.value.size() - pos - 1; // reset to last-line width
                };

            // sub-pass a: find max content width.
            size_t maxContentWidth = 0;
            {
                for (auto it = stmtBegin; it != stmtEnd; ++it)
                {
                    auto lineBegin = it;
                    auto prevIt = stmtEnd;
                    size_t bufLen = 0;
                    bool hadComment = false;

                    for (; it != stmtEnd && it->token != etEOL; ++it)
                    {
                        if (is_end_line_comment(*it))
                        {
                            hadComment = true;
                            if (!isHintComment(lineBegin, it))
                            {
                                size_t wsLen = (prevIt != stmtEnd
                                    && prevIt->token == etWHITESPACE)
                                    ? prevIt->value.length() : 0;
                                maxContentWidth = std::max(maxContentWidth, bufLen - wsLen);
                            }
                            while (it != stmtEnd && it->token != etEOL) ++it;
                            break;
                        }
                        prevIt = it;
                        accumWidth(bufLen, *it);
                    }

                    if (!hadComment)
                    {
                        size_t wsLen = (prevIt != stmtEnd
                            && prevIt->token == etWHITESPACE)
                            ? prevIt->value.length() : 0;
                        maxContentWidth = std::max(maxContentWidth, bufLen - wsLen);
                    }

                    if (it == stmtEnd) break;
                }
            }

            // sub-pass b: adjust the whitespace gap before each comment
            if (maxContentWidth > 0)
            {
                const size_t commentCol = maxContentWidth + 1;

                for (auto it = stmtBegin; it != stmtEnd; ++it)
                {
                    auto lineBegin = it;
                    auto prevIt = stmtEnd;
                    size_t bufLen = 0;

                    for (; it != stmtEnd && it->token != etEOL; ++it)
                    {
                        if (is_end_line_comment(*it))
                        {
                            if (!isHintComment(lineBegin, it)
                                && prevIt != stmtEnd
                                && prevIt->token == etWHITESPACE)
                            {
                                size_t contentWidth = bufLen - prevIt->value.length();
                                if (commentCol >= contentWidth)
                                    prevIt->value = wstring(commentCol - contentWidth, L' ');
                            }
                            while (it != stmtEnd && it->token != etEOL) ++it;
                            break;
                        }
                        prevIt = it;
                        accumWidth(bufLen, *it);
                    }

                    if (it == stmtEnd) break;
                }
            }
        }

        /*
        * alignStatementBlockComments re-indents every standalone block comment
        * (a line whose only non-whitespace token is a "/ * ... * /" comment) to
        * match the indentation of the next real code line below it.
        *
        * Motivation: the formatter emits a block comment at the indentation level
        * that is active when the comment token is processed.  For comments placed
        * just before a clause keyword that pops the indent stack (e.g. a WHEN that
        * follows a VALUES block), the comment ends up one level too deep.
        * Post-processing is the correct fix because only at that point is the
        * full formatted output — and therefore the next line's indentation — known.
        *
        * "Next real code line" means the first subsequent line that contains at
        * least one non-whitespace, non-block-comment token.  Blank lines and lines
        * that are themselves standalone block comments are skipped so that a run of
        * consecutive block comments all align to the same target.
        *
        * Algorithm (single forward pass, deferred apply):
        *   1. Collect (wsTokenIndex, targetIndent) fix descriptors.
        *   2. Apply all whitespace-replacement fixes (no index shift).
        *   3. Apply whitespace-insert fixes in reverse index order (rare; only when
        *      a comment sits at column 0 with no leading whitespace token).
        */
        void alignStatementBlockComments(int from)
        {
            if (from < 0 || from >= (int)tokens.size()) return;

            struct Fix
            {
                int     wsIdx;           // index of existing leading-whitespace token (-1 = none)
                int     insertBeforeIdx; // index to insert new ws token before (-1 = not needed)
                int     commentIdx;      // index of the block comment token (for realign_comment)
                wstring oldIndent;       // leading whitespace before realignment
                wstring newIndent;
            };
            std::vector<Fix> fixes;

            int n = (int)tokens.size();
            int i = from;

            while (i < n)
            {
                int lineStart  = i;
                int wsIdx      = -1;  // leading-whitespace token index on this line
                int commentIdx = -1;  // block-comment token index on this line
                int nonWsCount = 0;

                // Scan to EOL, classifying the line.
                while (i < n && tokens[i].token != etEOL)
                {
                    const auto& t = tokens[i];
                    if (t.token == etWHITESPACE)
                    {
                        if (i == lineStart) wsIdx = i;
                        ++i; continue;
                    }
                    ++nonWsCount;
                    if (nonWsCount == 1
                        && t.token == etCOMMENT
                        && !is_end_line_comment(t))
                        commentIdx = i;
                    ++i;
                }

                bool isStandalone = (nonWsCount == 1 && commentIdx >= 0);

                if (isStandalone)
                {
                    // Look ahead for the first real code line (skip blank lines and
                    // lines that are themselves standalone block comments).
                    int scanI = (i < n) ? i + 1 : n; // position after EOL
                    wstring targetIndent;
                    bool found = false;

                    while (scanI < n && !found)
                    {
                        int     lineS       = scanI;
                        wstring lineIndent;
                        int     lineNonWs   = 0;
                        int     lineComment = -1;

                        while (scanI < n && tokens[scanI].token != etEOL)
                        {
                            const auto& t = tokens[scanI];
                            if (t.token == etWHITESPACE)
                            {
                                if (scanI == lineS) lineIndent = t.value;
                                ++scanI; continue;
                            }
                            ++lineNonWs;
                            if (lineNonWs == 1
                                && t.token == etCOMMENT
                                && !is_end_line_comment(t))
                                lineComment = scanI;
                            ++scanI;
                        }

                        // Accept this line as the target only if it is not blank
                        // and not itself a standalone block comment.
                        bool isBlank          = (lineNonWs == 0);
                        bool isAnotherComment = (lineNonWs == 1 && lineComment >= 0);
                        if (!isBlank && !isAnotherComment)
                        {
                            targetIndent = lineIndent;
                            found = true;
                        }

                        if (scanI < n) ++scanI; // skip EOL
                    }

                    if (found)
                    {
                        Fix f;
                        f.wsIdx           = wsIdx;
                        f.insertBeforeIdx = (wsIdx < 0) ? commentIdx : -1;
                        f.commentIdx      = commentIdx;
                        f.oldIndent       = (wsIdx >= 0) ? tokens[wsIdx].value : L"";
                        f.newIndent       = targetIndent;
                        fixes.push_back(f);
                    }
                }

                if (i < n) ++i; // skip EOL
            }

            // Apply whitespace-replacement fixes (no index shift, any order).
            for (const auto& f : fixes)
                if (f.wsIdx >= 0)
                {
                    int delta = (int)f.newIndent.size() - (int)f.oldIndent.size();
                    tokens[f.wsIdx].value = f.newIndent;
                    realign_comment(tokens[f.commentIdx], delta);
                }

            // Apply insertion fixes in reverse index order so earlier insertions
            // do not shift the indices of later ones.
            std::sort(fixes.begin(), fixes.end(),
                [](const Fix& a, const Fix& b) { return a.insertBeforeIdx > b.insertBeforeIdx; });
            for (const auto& f : fixes)
            {
                if (f.insertBeforeIdx >= 0 && !f.newIndent.empty())
                {
                    int delta = (int)f.newIndent.size() - (int)f.oldIndent.size();
                    OpenEditor::Token wsTk;
                    wsTk.token  = etWHITESPACE;
                    wsTk.value  = f.newIndent;
                    wsTk.length = static_cast<OpenEditor::Token::size_pos>(f.newIndent.size());
                    tokens.insert(tokens.begin() + f.insertBeforeIdx, wsTk);
                    // After inserting the whitespace, commentIdx shifted by +1.
                    realign_comment(tokens[f.commentIdx + 1], delta);
                }
            }
        }
    };

    static
    bool is_join_qualifier(Token tk)
    {
        switch (tk.token)
        {
        case etINNER:
        case etLEFT:
        case etRIGHT:
        case etFULL:
        case etNATURAL:
        case etOUTER:
            return true;
        }
        return false;
    }

    static
    bool is_comparison_opt(Token tk)
    {
        switch (tk.token)
        {
        case etEQUAL:
        case etLESS:
        case etGREATER:
            return true;
        case etUNKNOWN:
            if (tk.value == L">=" || tk.value == L"<=" || tk.value == L"<>")
                return true;
        }

        return false;
    }

    static
    bool new_line_after(Token tk)
    {
        //return true;

        if (tk.token == etSELECT)
            return false;

        if (tk.token == etFROM)
            return false;

        if (tk.token == etWHERE)
            return false;

        if (tk.token == etHAVING)
            return false;

        if (tk.token == etJOIN)
            return false;

        if (is_join_qualifier(tk)) // INNER, LEFT, RIGHT, FULL, NATURAL — always followed by JOIN on same line
            return false;

        if (tk.token == etBY)
            return false;

        if (tk.token == etUNION
            || tk.token == etINTERSECT
            || tk.token == etMINUS_SQL)
            return false; // ALL (after UNION) or SELECT follow on the same line

        return true;
    }

    static
    int find_eol_backward(const OutputTokenStream& str)
    {
        auto it = str.tokens.rbegin();
        for (; it != str.tokens.rend(); ++it)
            if (it->token == etEOL)
                break;

        return it - str.tokens.rbegin();
    }

    static
    void align_comment(Token& tk, const wstring& spacer)
    {
        if (tk.token == etCOMMENT
            && tk.value.find(L'\n') != wstring::npos)
        {
            wstring line, buffer;
            wistringstream in(tk.value);

            while (getline(in, line))
            {
                auto it = line.begin();
                if (!buffer.empty()) // take the first line as is
                {
                    buffer += L'\n';
                    buffer += spacer;
                    for (int i = 0; it != line.end() && i < tk.offset && iswspace(*it); ++it, ++i)
                        ;
                }
                buffer.append(it, line.end());
            }
            tk.value = buffer;
        }
    }

    /*
    * realign_comment adjusts the indentation of every continuation line of a
    * multi-line block comment by a signed character delta.
    *   delta > 0 : add that many spaces at the start of each continuation line.
    *   delta < 0 : remove up to (-delta) leading spaces from each continuation line.
    *   delta = 0 : no-op.
    * Single-line comments (no '\n' in value) are left untouched.
    */
    static
    void realign_comment(Token& tk, int delta)
    {
        if (delta == 0) return;
        if (tk.token != etCOMMENT) return;
        if (tk.value.find(L'\n') == wstring::npos) return;

        wstring line, buffer;
        wistringstream in(tk.value);
        bool first = true;

        while (getline(in, line))
        {
            if (!first)
            {
                buffer += L'\n';
                if (delta > 0)
                {
                    buffer.append((size_t)delta, L' ');
                    buffer += line;
                }
                else
                {
                    // Remove up to (-delta) leading spaces.
                    int toRemove = -delta;
                    size_t i = 0;
                    while (i < line.size() && line[i] == L' ' && (int)i < toRemove)
                        ++i;
                    buffer += line.substr(i);
                }
            }
            else
            {
                buffer += line; // first line of the comment: leave as-is
                first = false;
            }
        }
        tk.value = buffer;
    }

    /*
    * alignList is called when a clause terminator (e.g. FROM) is encountered
    * but BEFORE that terminator is emitted into the token stream. Therefore
    * out.tokens.end() is exactly the end of the column list — nothing beyond it
    * exists yet. All three passes iterate to out.tokens.end() intentionally;
    * no terminator keyword check inside the loop is needed or correct.
    * 'from' is the output position recorded when SELECT was emitted, pointing to
    * the first token of the column list.
    */
    static void alignList(OutputTokenStream& out, int from, int indent)
    {
        if (from >= 0 && from < (int)out.tokens.size())
        {
            // 1st pass - collect aliases for every line, put -1 if not found
            std::vector<int> aliasTokenPos;
            bool hasAs = false; // true if at least one column has AS before an alias
            {
                auto it = out.tokens.begin() + from;
                int p1Depth = 0; // paren nesting depth, tracked across lines

                for (; it != out.tokens.end(); ++it)
                {
                    wstring buffer;
                    std::vector<OpenEditor::Token*> lineTokens;
                    int lineDelta = 0; // net paren change on this line

                    for (; it != out.tokens.end() && it->token != etEOL; ++it)
                    {
                        buffer += it->value;
                        lineTokens.push_back(&*it);
                        if (it->token == etLEFT_ROUND_BRACKET)  ++lineDelta;
                        else if (it->token == etRIGHT_ROUND_BRACKET) --lineDelta;
                    }
                    int lineEndDepth = p1Depth + lineDelta;
                    p1Depth = lineEndDepth;

                    auto rAsIt = lineTokens.rend();

                    if (lineEndDepth == 0 && !lineTokens.empty()
                        && (
                            (
                                ((*lineTokens.begin())->token == etWHITESPACE)
                                && ((*lineTokens.begin())->value.size() == (size_t)indent)
                                && (it == out.tokens.end()
                                    || (*lineTokens.rbegin())->token == etCOMMA
                                    || (*lineTokens.rbegin())->token == etCOMMENT)
                                )
                            || !aliasTokenPos.size()
                            )
                        )
                    {
                        // looking for (indent)(anything)(space)[(as)(space)](identifier)[,][space][comment] in backward
                        auto rIt = lineTokens.rbegin();

                        if (rIt != lineTokens.rend() && **rIt == etCOMMENT) // skip inline comment
                        {
                            rIt++;
                            if (rIt != lineTokens.rend() && (**rIt == etWHITESPACE)) // and the space before it
                                rIt++;
                        }
                        if (rIt != lineTokens.rend()
                            && (**rIt == etCOMMA          // normal columns end with comma
                                || **rIt == etWHITESPACE))  // last column: no comma, space before alias
                            rIt++;

                        if (rIt != lineTokens.rend()
                            && **rIt != etWHITESPACE
                            && **rIt != etDOT
                            && (**rIt == etDOUBLE_QUOTED_STRING
                                || (!(*rIt)->value.empty()
                                    && (iswalpha((*rIt)->value[0]) || (*rIt)->value[0] == L'_')))
                            )
                        {
                            rIt++;
                            if (rIt != lineTokens.rend()
                                && **rIt == etWHITESPACE)
                            {
                                rAsIt = rIt; // possibly found
                                rIt++;

                                if (rIt == lineTokens.rend()  // end of line so no match
                                    || **rIt == etSELECT  // in case of the first line
                                    )
                                {
                                    rAsIt = lineTokens.rend();
                                }
                                else if (**rIt == etAS)
                                {
                                    rIt++;
                                    if (rIt != lineTokens.rend()
                                        && **rIt == etWHITESPACE)
                                    {
                                        rAsIt = rIt;
                                        hasAs = true;
                                    }
                                }
                                else
                                    ; // do nothing because it is already found
                            }
                        }
                    }

                    if (!lineTokens.empty() && rAsIt != lineTokens.rend())
                    {
                        int inx = (int)lineTokens.size() - 1 - (int)(rAsIt - lineTokens.rbegin());
                        aliasTokenPos.push_back(inx);
                    }
                    else
                        aliasTokenPos.push_back(-1);

                    if (it == out.tokens.end())
                        break;
                }
            }
            // 2d pass - calculating max distance to alias
            // All lines (including subquery interior) contribute to maxDistance so that
            // the outer aliases are padded consistently. Pass 3 will skip actually
            // adjusting whitespace inside (SELECT ...) blocks.
            size_t maxDistance = 0;
            {
                auto it = out.tokens.begin() + from;
                for (int line = 0; it != out.tokens.end(); ++it, ++line)
                {
                    wstring buffer;
                    bool hasAlias = false;
                    for (int pos = 0; it != out.tokens.end() && it->token != etEOL; ++it, ++pos)
                    {
                        if (it->token != etCOMMENT) // assuming it can be at EOL only!
                            buffer += it->value;

                        if (aliasTokenPos[line] == pos)
                        {
                            hasAlias = true;
                            maxDistance = std::max(maxDistance, buffer.length());
                        }
                    }
                    if (!hasAlias)
                        maxDistance = std::max(maxDistance, buffer.length());

                    if (it == out.tokens.end())
                        break;
                }
            }
            // 3d pass - adjusting whitespace to align aliases.
            // Subquery detector: push false for every '('; flip to true when the
            // immediately following meaningful token is SELECT (marking a subquery).
            // sqDepth counts how many subquery-opening parens we are currently inside.
            // Alias whitespace is only adjusted when sqDepth == 0 at the alias position,
            // so interior lines of (SELECT ...) are left untouched while DECODE / NVL /
            // CASE multi-line expressions (where no SELECT follows '(') are aligned normally.
            {
                auto it = out.tokens.begin() + from;
                std::stack<bool> p3sqStack; // true = this '(' opens a subquery
                int p3sqDepth = 0;          // subquery nesting depth, carried across lines
                OpenEditor::Token p3Prev;
                p3Prev.token = etNONE;

                for (int line = 0; it != out.tokens.end(); ++it, ++line)
                {
                    wstring buffer;
                    int curSqDepth = p3sqDepth; // subquery depth at start of line

                    for (int pos = 0; it != out.tokens.end() && it->token != etEOL; ++it, ++pos)
                    {
                        buffer += it->value;

                        // --- subquery depth tracking ---
                        if (it->token == etLEFT_ROUND_BRACKET)
                        {
                            p3sqStack.push(false); // assume expression until SELECT is seen
                        }
                        else if (it->token == etSELECT
                            && p3Prev.token == etLEFT_ROUND_BRACKET
                            && !p3sqStack.empty() && !p3sqStack.top())
                        {
                            p3sqStack.top() = true; // confirm: this ( opened a subquery
                            ++curSqDepth;
                        }
                        else if (it->token == etRIGHT_ROUND_BRACKET
                            && !p3sqStack.empty())
                        {
                            if (p3sqStack.top()) --curSqDepth;
                            p3sqStack.pop();
                        }

                        // track last meaningful token (skip whitespace/comments)
                        if (it->token != etWHITESPACE
                            && it->token != etCOMMENT)
                            p3Prev = *it;

                        // --- alias alignment (only outside subqueries) ---
                        if (aliasTokenPos[line] == pos
                            && curSqDepth == 0
                            && maxDistance >= buffer.length())
                        {
                            auto next = it;
                            if (!hasAs
                                || (++next != out.tokens.end() && next->token == etAS)
                                )
                                it->value += wstring(maxDistance - buffer.length(), L' ');
                            else
                                it->value += wstring(maxDistance - buffer.length() + 3/*"AS "*/, L' ');
                        }
                    }

                    p3sqDepth = curSqDepth; // carry depth forward to next line
                    if (it == out.tokens.end())
                        break;
                }
            }

			// End-line comment alignment is done at statement scope by
			// alignStatementComments(), called at each ; / boundary.
		}
	}

	/*
	 * alignSetAssignments scans the output region starting at [from] — the SET
	 * assignment list — and aligns the '=' operator of every Form A assignment to
	 * a common column.
	 *
	 * Self-termination: the scan stops as soon as it encounters a line whose first
	 * token is whitespace of length != assignIndent.  This naturally covers the
	 * end of the SET body whether it is followed by WHERE, RETURNING, LOG, ';', '/'
	 * or plain end-of-stream — without any explicit boundary notification.
	 *
	 * Algorithm (2-pass):
	 *   Pass 1 — for each assignment line (depth 0, starts with assignIndent),
	 *            find the first etEQUAL token and record its output-token index
	 *            and the accumulated character width of the LHS.
	 *   Pass 2 — pad the whitespace token immediately before each '=' so that all
	 *            '=' signs land at the same column (maxLhsWidth).
	 *
	 * Skips: lines inside '(…)' (depth > 0), lines with no '=', lines that start
	 * with a different indent (clause keywords or continuation of multi-line exprs).
	 * No-op when fewer than 2 qualifying lines are found.
	 */
	static void alignSetAssignments(OutputTokenStream& out, int from, size_t assignIndent)
	{
		if (from < 0 || from >= (int)out.tokens.size()) return;

		struct LineInfo
		{
			int    equalPos = -1;   // absolute index in out.tokens of the etEQUAL token
			size_t lhsWidth = 0;    // character width up to (not including) the '='
		};

		auto stmtBegin = out.tokens.begin() + from;
		auto stmtEnd   = out.tokens.end();

		// Pass 1 — collect one LineInfo per assignment line.
		std::vector<LineInfo> lines;
		{
			int outerDepth = 0; // paren depth carried across lines
			int caseDepth  = 0; // CASE depth carried across lines
			// True while processing continuation lines of a multiline value
			// (dangling '=' on previous line, or inside an open paren/CASE block).
			bool inMultilineValue = false;

			for (auto it = stmtBegin; it != stmtEnd; )
			{
				// Self-termination: if the line's first token is whitespace of a
				// length that does not match the assignment-list indent, we have
				// left the SET body — stop without processing this line.
				// Skip this check when inside a multiline value: those continuation
				// lines have a deeper indent but still belong to the SET list.
				if (!inMultilineValue)
				{
					if (it->token == etWHITESPACE && it->value.size() != assignIndent)
						break;
				}

				LineInfo li;
				int depth = outerDepth; // start inner scan from carried paren depth
				size_t width = 0;
				int pos = (int)(it - out.tokens.begin()); // absolute index
				// Set to true when any non-whitespace token is seen after the '=',
				// used to detect a dangling '=' (value on the next line).
				bool valueStarted = false;

				for (; it != stmtEnd && it->token != etEOL; ++it, ++pos)
				{
					if (it->token == etLEFT_ROUND_BRACKET)  { ++depth; width += it->value.size(); continue; }
					if (it->token == etRIGHT_ROUND_BRACKET) { if (depth > 0) --depth; width += it->value.size(); continue; }
					if (it->token == etCASE) { ++caseDepth; width += it->value.size(); continue; }
					if (it->token == etEND)  { if (caseDepth > 0) --caseDepth; width += it->value.size(); continue; }

					if (depth == 0 && caseDepth == 0 && it->token == etEQUAL && li.equalPos < 0)
					{
						li.equalPos = pos;
						li.lhsWidth = width;
					}
					else if (li.equalPos >= 0 && it->token != etWHITESPACE)
					{
						valueStarted = true; // something follows '=' on the same line
					}
					width += it->value.size();
				}
				outerDepth = depth; // carry paren depth to next line

				// Decide whether the next line is a continuation of this value.
				if (outerDepth > 0 || caseDepth > 0)
					inMultilineValue = true;       // still inside an open paren/CASE block
				else if (li.equalPos >= 0 && !valueStarted)
					inMultilineValue = true;       // dangling '=': value starts on the next line
				else
					inMultilineValue = false;      // complete line at base depth

				lines.push_back(li);
				if (it != stmtEnd) ++it; // skip etEOL
			}
		}

		// Only align when at least 2 lines have a depth-0 '='.
		int qualifying = 0;
		size_t maxLhs = 0;
		for (auto& li : lines)
			if (li.equalPos >= 0) { ++qualifying; maxLhs = std::max(maxLhs, li.lhsWidth); }
		if (qualifying < 2) return;

		// Pass 2 — pad the whitespace token immediately before each '='.
		for (auto& li : lines)
		{
			if (li.equalPos < 0) continue;
			if (li.equalPos > 0)
			{
				auto& wsTk = out.tokens[li.equalPos - 1];
				if (wsTk.token == etWHITESPACE && maxLhs >= li.lhsWidth)
					wsTk.value = wstring(1 + (maxLhs - li.lhsWidth), L' ');
			}
		}
	}

	/*
	 * alignCaseInSetList — post-pass run after alignSetAssignments.
	 *
	 * For each dangling-'=' assignment (value on the next line) whose value
	 * block starts with CASE, re-indent every line of that CASE…END block so
	 * CASE aligns with the first indent boundary that is a multiple of
	 * indentSize and >= (equalColumn + 1).
	 *
	 * "equal column" is the 0-based character column of '=' on the assignment
	 * line, computed from the (already-padded) output tokens of that line.
	 *
	 * All continuation lines of the block (indent > assignIndent) are shifted
	 * by the same delta so relative internal indentation is preserved.
	 *
	 * Self-termination: the block ends at the first line with
	 * indent <= assignIndent (next assignment, WHERE, etc.).
	 */
	static void alignCaseInSetList(OutputTokenStream& out, int from,
								   size_t assignIndent, int indentSize)
	{
		if (from < 0 || from >= (int)out.tokens.size()) return;
		if (indentSize < 1) indentSize = 2;

		auto stmtBegin = out.tokens.begin() + from;
		auto stmtEnd   = out.tokens.end();

		for (auto lineIt = stmtBegin; lineIt != stmtEnd; )
		{
			// --- locate a line at exactly assignIndent ---
			if (!(lineIt->token == etWHITESPACE && lineIt->value.size() == assignIndent))
			{
				// advance to next line
				while (lineIt != stmtEnd && lineIt->token != etEOL) ++lineIt;
				if (lineIt != stmtEnd) ++lineIt;
				continue;
			}

			// Measure the '=' column on this line and check it is the last
			// non-EOL non-comment token (dangling assignment).
			size_t equalCol   = 0;
			int    equalTokIdx = -1;
			bool   hasValueToken = false;
			int    pos = (int)(lineIt - out.tokens.begin());
			auto   it  = lineIt;
			for (; it != stmtEnd && it->token != etEOL; ++it, ++pos)
			{
				if (it->token == etEQUAL && equalTokIdx < 0)
				{
					equalCol    = (size_t)(it - lineIt); // we accumulate below
					equalTokIdx = pos;
					// recompute equalCol as accumulated char width up to '='
					equalCol = 0;
					for (auto w = lineIt; w != it; ++w)
						equalCol += w->value.size();
				}
				else if (equalTokIdx >= 0
						 && it->token != etWHITESPACE
						 && it->token != etCOMMENT)
				{
					hasValueToken = true;
				}
			}

			// Not a dangling '=': skip line
			if (equalTokIdx < 0 || hasValueToken)
			{
				if (it != stmtEnd) ++it; // skip EOL
				lineIt = it;
				continue;
			}

			// Advance past the EOL to the next line
			if (it != stmtEnd) ++it;
			auto blockStart = it;

			// Check that the next non-empty line starts with CASE
			{
				bool foundCase = false;
				for (auto peek = blockStart; peek != stmtEnd && peek->token != etEOL; ++peek)
				{
					if (peek->token == etWHITESPACE) continue;
					if (peek->token == etCASE) { foundCase = true; }
					break;
				}
				if (!foundCase)
				{
					lineIt = blockStart;
					continue;
				}
			}

			// Compute the target indent: smallest multiple of indentSize
			// that is > equalColumn.
			size_t targetIndent = ((equalCol / (size_t)indentSize) + 1) * (size_t)indentSize;

			// Current indent of the CASE line (the block's base).
			size_t caseIndent = 0;
			if (blockStart != stmtEnd && blockStart->token == etWHITESPACE)
				caseIndent = blockStart->value.size();

			int delta = (int)targetIndent - (int)caseIndent;
			if (delta == 0)
			{
				lineIt = blockStart;
				continue;
			}

			// Re-indent every line in the block: from blockStart until a line
			// whose indent <= assignIndent (block has ended).
			for (auto bIt = blockStart; bIt != stmtEnd; )
			{
				// Check first token of line
				if (bIt->token == etWHITESPACE)
				{
					size_t curIndent = bIt->value.size();
					if (curIndent <= assignIndent)
						break; // left the CASE block

					int newIndent = (int)curIndent + delta;
					if (newIndent < 1) newIndent = 1;
					bIt->value = wstring((size_t)newIndent, L' ');
				}
				else if (bIt->token == etEOL || bIt == blockStart)
				{
					// empty line or first token isn't whitespace — leave alone
				}
				else
				{
					break; // non-indented line at base level — stop
				}

				// advance to next line
				while (bIt != stmtEnd && bIt->token != etEOL) ++bIt;
				if (bIt != stmtEnd) ++bIt;
			}

			lineIt = blockStart;
		}
	}

	// Emits tokens [begin, end) into out with only spacing rules applied — no newlines.
	// etEOL and etWHITESPACE tokens are skipped.  Uses the same non-breakable space rules as the main loop.
	// prevToken is updated in/out so the caller's spacing state stays consistent.
	static void putSingleLine (
		OutputTokenStream& out,
		std::vector<OpenEditor::Token>::const_iterator begin,
		std::vector<OpenEditor::Token>::const_iterator end,
		OpenEditor::Token& prevToken)
	{
		for (auto it = begin; it != end; ++it)
		{
			if (it->token == etEOL) continue;
			if (it->token == etWHITESPACE) continue;
				if (it->token == etCOMMENT
				&&  it->value.size() >= 2
				&&  it->value[0] == L'-' && it->value[1] == L'-') continue; // end-line comments break single-line

			const OpenEditor::Token& tk = *it;

			if (prevToken.token != etNONE
			&& prevToken.token != etDOT
			&& tk.token       != etDOT
			&& prevToken.token != etLEFT_ROUND_BRACKET
			&& tk.token       != etLEFT_ROUND_BRACKET
			&& tk.token       != etRIGHT_ROUND_BRACKET
			&& prevToken.token != etCOLON
			&& tk.token       != etCOMMA
			&& tk.token       != etSEMICOLON)
				out.putSpace();
			else if (prevToken.reserved != 0 && tk.token == etLEFT_ROUND_BRACKET)
				out.putSpace();

			out.putToken(tk);
			prevToken = tk;
		}
	}
    
	struct SQLFormatter 
	{
		// ---- settings (read once from persistent settings) ----
		int  FMT_INDENT_SIZE               = 0;
		bool FMT_SIMPLE_STMT_ENABLED       = false;
		int  FMT_SIMPLE_STMT_MAX_LEN       = 0;
		bool FMT_SELECT_SHORT_LIST_ENABLED = false;
		int  FMT_SELECT_SHORT_LIST_MAX_LEN = 0;
		bool FMT_FROM_SHORT_LIST_ENABLED   = false;
		int  FMT_FROM_SHORT_LIST_MAX_LEN   = 0;
		bool FMT_ALIGN_COLUMN_ALIASES      = false;
		int  FMT_BLOCK_SHORT_MAX_LEN       = 0;
		bool FMT_SELECT_STMT_ENABLED       = false;
		int  FMT_SELECT_STMT_MAX_LEN       = 0;
		bool FMT_GROUPBY_SHORT_LIST_ENABLED = false;
		int  FMT_GROUPBY_SHORT_LIST_MAX_LEN = 0;
		bool FMT_ORDERBY_SHORT_LIST_ENABLED = false;
		int  FMT_ORDERBY_SHORT_LIST_MAX_LEN = 0;
		bool FMT_SET_OP_BLANK_LINE          = false;
		int  FMT_CASE_WHEN_SHORT_MAX_LEN    = 0;
		int  FMT_RETURNING_MAX_LEN          = 0;
		int  FMT_RETURNING_MAX_ITEMS         = 0;
		int  FMT_LOG_ERRORS_MAX_LEN         = 0;
		bool FMT_ALIGN_SET_ASSIGNMENTS      = false;
		bool FMT_ALIGN_CASE_IN_SET          = false;
		bool FMT_BRICK_LIST_ENABLED         = false;
		int  FMT_BRICK_LIST_MAX_LINE_LEN    = 0;
		int  FMT_BRICK_LIST_MAX_LINE_ITEMS  = 0;
		int  FMT_BRICK_LIST_MIN_ITEMS       = 0;
		bool FMT_BRICK_LIST_LINKED_INSERT   = true;  // link INSERT col-list and VALUES wrapping
		bool FMT_BRICK_LIST_LINKED_SET_OP   = true;  // link SELECT col-lists across UNION/INTERSECT/MINUS

		// ---- set-operation brick link state ----
		// Persists SELECT column-list brick wrap positions across UNION/INTERSECT/MINUS
		// boundaries so every branch of a compound query at the same nesting level
		// mirrors the first branch's wrap positions when item counts match.
		// Keyed by parenDepth at the time the SELECT was pushed.
		struct SetOpBrickState
		{
			std::vector<int> wrapPositions;
			int              columnCount = 0; // 0 = not yet established
		};
		std::map<int, SetOpBrickState> setOpBrickByDepth;

		// ---- syntax tree (built once before the main loop) ----
		FormatContext tree;

		// ---- output / indent state ----
		OutputTokenStream  out;
		IndentCtxStack     stack { 4 };   // size overwritten in constructor
		std::wostringstream ostr;

		// ---- per-statement state ----
		int  parenDepth = 0;

		// Unified scope frame for SELECT, CASE, DELETE, and UPDATE contexts.
		struct ScopeFrame
		{
			enum Kind { Select, Case, Delete, Update, Insert, Merge } kind  = Select;
			FormatContext* node              = nullptr;
			int  pushedAtParenDepth          = 0;
			// Select-specific
			int  selectOutPos                = -1;
			bool selectHasInto              = false;   // PL/SQL SELECT INTO clause present
			int  selectIntoSrcIdx           = -1;      // token index of INTO keyword
			std::vector<int> selectBrickWrapCommas;    // source indices of commas that caused line-wraps
			int  selectBrickColumnCount     = 0;       // item count matching selectBrickWrapCommas
			// Case-specific
			bool caseFirstWhenSeen           = false;
			bool caseThenPushed              = false;
			bool caseExtraIndentPushed       = false; // extra indent before CASE in UPDATE SET value
			// Delete-specific
			bool deleteWhereIndentPushed     = false; // +1 pushed by WHERE post-emit
			// Update-specific: tracks whether SET body indent has been pushed
			bool updateSetBodyPushed         = false;
			// Update-specific: last clause keyword seen (etSET/etWHERE/etRETURNING/etINTO/etLOG)
			EToken updateLastClause          = etNONE;
			// Update-specific: break before REJECT LIMIT when LOG ERRORS line is long
			bool updateLogRejectBreak        = false;
			// Update-specific: output stream position right after SET was emitted (for alignment)
			int    updateSetOutPos  = -1;
			// Update-specific: character width of the assignment-list indent (for self-termination)
			size_t updateSetIndent  = 0;
			// Update-specific: a multi-line CASE is about to be pushed; stamp the Case frame
			bool updateCasePendingExtraIndent = false;
			// Insert-specific
			bool   insertAllFirst        = false;  // ALL or FIRST form
			bool   insertConditional     = false;  // WHEN clauses seen
			int    insertExtraIndents    = 0;       // extra special-indents pushed beyond base +1
			bool   insertHintNewline     = false;  // end-line hint → INTO on next line
			bool   insertLogRejectBreak  = false;  // break before REJECT LIMIT
			EToken insertLastClause      = etNONE; // last INSERT clause keyword seen
			// Brick wrap positions recorded when the INSERT column list is formatted,
			// so the VALUES list can mirror them (linked brick formatting).
			std::vector<int> insertBrickWrapPositions;
			int              insertBrickColumnCount = 0;
			// Legacy flags kept for tryReturning / LOG — use insertExtraIndents instead.
			bool   insertValPushed       = false;
			bool   insertIntoPushed      = false;
			// Merge-specific
			EToken mergeLastClause        = etNONE; // last top-level clause keyword seen
			bool   mergeInMatchedBlock    = false;  // inside WHEN MATCHED block
			bool   mergeInNotMatchedBlock = false;  // inside WHEN NOT MATCHED block
			int    mergeBodyIndents       = 0;      // pushes inside current WHEN block (THEN+SET+INSERT+WHERE)
			bool   mergeLogRejectBreak    = false;  // break before REJECT LIMIT
			int    mergeSetOutPos         = -1;     // output position after SET EOL (for = alignment)
			size_t mergeSetIndent         = 0;      // assignment-list indent width (for = alignment)
		};
		std::vector<ScopeFrame> scopeStack;

		std::vector<int>    betweenStack;
		int                 stmtBookmark = -1;
		OpenEditor::Token   prevTk;
		OpenEditor::Token   prevNonCommentTk;

		// ---- scope helpers ----
		ScopeFrame* lastScope(ScopeFrame::Kind k)
		{
			for (auto it = scopeStack.rbegin(); it != scopeStack.rend(); ++it)
				if (it->kind == k) return &*it;
			return nullptr;
		}

		void popScope(ScopeFrame::Kind k)
		{
			for (auto it = scopeStack.rbegin(); it != scopeStack.rend(); ++it)
				if (it->kind == k) { scopeStack.erase((it + 1).base()); return; }
		}

		void resetStatementState()
		{
			while (stack.size() > 1) stack.pop();
			stack.resetCurrentLineTokenCounter();
			parenDepth = 0;
			scopeStack.clear();
			betweenStack.clear();
			stmtBookmark = -1;
			prevTk = OpenEditor::Token();
			prevNonCommentTk = OpenEditor::Token();
			setOpBrickByDepth.clear();
		}

		// ---- short-path helpers (each returns true + advances it when the block is consumed) ----

		// Whole simple statement on one line.
		bool trySimpleStmt(std::vector<OpenEditor::Token>& tokens,
						   std::vector<OpenEditor::Token>::iterator& it)
		{
			if (!FMT_SIMPLE_STMT_ENABLED) return false;
			int curIdx = (int)(it - tokens.begin());
			auto* stmtNode = findInTree(tree, curIdx);
			if (!stmtNode || stmtNode->formattedLen > FMT_SIMPLE_STMT_MAX_LEN
				|| stmtNode->hasComment || stmtNode->closeSrcIdx < 0)
				return false;
			out.putSpace(stack.getSpacer());
			OpenEditor::Token prev; prev.token = etNONE;
			auto endIt = tokens.begin() + stmtNode->closeSrcIdx;
			putSingleLine(out, it, endIt, prev);
			prevTk = prev;
			prevNonCommentTk = prev;
			it = endIt - 1;
			return true;
		}

		// Short parenthesised block: emit content + closing ')' on one line.
		bool tryShortParen(std::vector<OpenEditor::Token>& tokens,
						   std::vector<OpenEditor::Token>::iterator& it,
						   const OpenEditor::Token& tk)
		{
			auto firstInBlock = it + 1;
			while (firstInBlock != tokens.end() && firstInBlock->token == etEOL)
				++firstInBlock;
			bool firstIsSelect = (firstInBlock != tokens.end() && firstInBlock->token == etSELECT);
			int  maxLen  = firstIsSelect ? FMT_SELECT_STMT_MAX_LEN  : FMT_BLOCK_SHORT_MAX_LEN;
			bool enabled = firstIsSelect ? FMT_SELECT_STMT_ENABLED  : true;

			int curIdx = (int)(it - tokens.begin());
			auto* parenNode = findInTree(tree, curIdx);
			if (!parenNode || !enabled || parenNode->formattedLen > maxLen
				|| parenNode->hasComment || parenNode->closeSrcIdx < 0)
				return false;

			auto endIt = tokens.begin() + parenNode->closeSrcIdx + 1; // include ')'
			OpenEditor::Token prev = tk;
			putSingleLine(out, it + 1, endIt, prev);
			prevTk = prev;
			prevNonCommentTk = prev;
			it = endIt - 1;
			return true;
		}

		// Whole SELECT statement on one line.
		bool trySelectOneLine(std::vector<OpenEditor::Token>& tokens,
							  std::vector<OpenEditor::Token>::iterator& it,
							  ScopeFrame& sel,
							  const OpenEditor::Token& tk)
		{
			if (!sel.node || !FMT_SELECT_STMT_ENABLED
				|| sel.node->formattedLen > FMT_SELECT_STMT_MAX_LEN
				|| sel.node->hasComment || sel.node->closeSrcIdx < 0)
				return false;
			stack.pushSpecialIndent();
			stack.incrementCurrentLineTokenCounter();
			auto step_back = find_eol_backward(out);
			sel.selectOutPos = out.position() - step_back;
			auto endIt = tokens.begin() + sel.node->closeSrcIdx;
			OpenEditor::Token prev = tk;
			putSingleLine(out, it + 1, endIt, prev);
			prevTk = prev;
			prevNonCommentTk = prev;
			it = endIt - 1;
			return true;
		}

		// Short SELECT column list (up to FROM / INTO).
		bool trySelectShortList(std::vector<OpenEditor::Token>& tokens,
								std::vector<OpenEditor::Token>::iterator& it,
								ScopeFrame& sel,
								const OpenEditor::Token& tk)
		{
			if (!FMT_SELECT_SHORT_LIST_ENABLED) return false;
			const auto* selKw = sel.node ? getKeyword(sel.node->keywords, etSELECT) : nullptr;
			if (!selKw || selKw->hasComment) return false;
			const auto* fromKw = sel.node ? getKeyword(sel.node->keywords, etFROM) : nullptr;
			if (!fromKw || fromKw->srcIdx < 0) return false;
			const auto* intoKw = sel.node ? getKeyword(sel.node->keywords, etINTO) : nullptr;

			// The SELECT col-list ends at INTO (if present) or FROM.
			// selKw->formattedLen covers SELECT→INTO (or SELECT→FROM if no INTO);
			// check its width against the short-list threshold.
			if (selKw->formattedLen > FMT_SELECT_SHORT_LIST_MAX_LEN) return false;

			stack.incrementCurrentLineTokenCounter();

			// Emit SELECT col-list (up to INTO or FROM).
			int colEndIdx = intoKw ? intoKw->srcIdx : fromKw->srcIdx;
			auto colEndIt = tokens.begin() + colEndIdx;
			OpenEditor::Token prev = tk;
			putSingleLine(out, it + 1, colEndIt, prev);

			if (intoKw && intoKw->srcIdx >= 0 && intoKw->closeSrcIdx >= 0)
			{
				// Emit INTO on a new line at the same indent level as SELECT (after popSpecialIndent).
				// The indent stack still has the SELECT col-list indent pushed; we pop it to reach
				// SELECT level, emit INTO + var-list, then the main loop's FROM handler will
				// pop again (it does one popSpecialIndent for FROM).  To keep the stack balanced,
				// push it back after emitting INTO.
				stack.popSpecialIndent();
				out.putEol();
				out.putSpace(stack.getSpacer());
				auto intoIt = tokens.begin() + intoKw->srcIdx;
				out.putToken(*intoIt); // INTO
				prev = *intoIt;
				auto varEndIt = tokens.begin() + intoKw->closeSrcIdx;
				putSingleLine(out, intoIt + 1, varEndIt, prev);
				prevTk = prev;
				prevNonCommentTk = prev;
				stack.pushSpecialIndent(); // rebalance for FROM's pop
				it = varEndIt - 1;
				sel.selectHasInto = true;
				sel.selectIntoSrcIdx = intoKw->srcIdx;
			}
			else
			{
				prevTk = prev;
				prevNonCommentTk = prev;
				it = colEndIt - 1;
			}
			return true;
		}

		// SELECT column list in "brick" (rectangular) format.
		// Called after pushSpecialIndent has already been done for the SELECT body.
		// Items are collected between SELECT (exclusive) and INTO/FROM (exclusive).
		// Each item is checked with isSimpleItem; if any is complex the whole list
		// falls through to normal per-column formatting.
		// Wraps when the line exceeds FMT_BRICK_LIST_MAX_LINE_LEN or FMT_BRICK_LIST_MAX_LINE_ITEMS.
		// If a top-level SELECT INTO is present, emits the INTO var list on the next line
		// mirroring the SELECT column line-wrap positions when the counts match.
		bool trySelectBrickList(std::vector<OpenEditor::Token>& tokens,
								std::vector<OpenEditor::Token>::iterator& it,
								ScopeFrame& sel,
								const OpenEditor::Token& tk)
		{
			if (!FMT_BRICK_LIST_ENABLED) return false;

			// Need both SELECT and FROM keyword metadata.
			const auto* selKw  = sel.node ? getKeyword(sel.node->keywords, etSELECT) : nullptr;
			if (!selKw) return false;
			// selKw->hasComment is true when a hint comment follows SELECT; that is
			// handled explicitly below and must NOT cause a bail-out here.
			const auto* fromKw = sel.node ? getKeyword(sel.node->keywords, etFROM) : nullptr;
			if (!fromKw || fromKw->srcIdx < 0) return false;
			const auto* intoKw = sel.node ? getKeyword(sel.node->keywords, etINTO) : nullptr;

			int fromIdx   = fromKw->srcIdx;
			int selectIdx = (int)(it - tokens.begin());

			// SELECT col list ends at INTO (if present) or FROM.
			int colEndIdx = (intoKw && intoKw->srcIdx >= 0) ? intoKw->srcIdx : fromIdx;

			// Detect an optional hint comment immediately after SELECT.
			// Both end-line (--+...) and block-style (/*+...*/) hints stay on the
			// SELECT line; item collection begins after the hint.
			int hintTokenIdx = -1; // source index of the hint comment token, or -1
			{
				int peek = selectIdx + 1;
				while (peek < (int)tokens.size()
					   && (tokens[peek].token == etEOL || tokens[peek].token == etWHITESPACE))
					++peek;
				if (peek < (int)tokens.size() && tokens[peek].token == etCOMMENT)
					hintTokenIdx = peek;
			}

			// If there are comments *inside* the column list (beyond any hint), bail out.
			if (selKw->hasComment)
			{
				bool hasNonHintComment = false;
				for (int i = selectIdx + 1; i < colEndIdx; ++i)
					if (tokens[i].token == etCOMMENT && i != hintTokenIdx)
					{ hasNonHintComment = true; break; }
				if (hasNonHintComment) return false;
			}

			// Item collection starts after the hint (if any), otherwise right after SELECT.
			int colStartIdx = (hintTokenIdx >= 0) ? hintTokenIdx + 1 : selectIdx + 1;

			// Helper: build item list between two token-index bounds (depth-0 commas).
			struct Item { int start; int end; int width; };
			auto buildItems = [&](int startIdx, int endIdx) -> std::vector<Item>
			{
				std::vector<Item> result;
				int depth     = 0;
				int itemStart = startIdx;
				for (int i = startIdx; i < endIdx; ++i)
				{
					const auto& t = tokens[i];
					if (t.token == etLEFT_ROUND_BRACKET)  { ++depth; continue; }
					if (t.token == etRIGHT_ROUND_BRACKET) { --depth; continue; }
					if (t.token == etCOMMA && depth == 0)
					{
						int w = 0;
						for (int j = itemStart; j < i; ++j)
							if (tokens[j].token != etEOL && tokens[j].token != etWHITESPACE)
								w += (int)tokens[j].value.size();
						result.push_back({itemStart, i, w});
						itemStart = i + 1;
					}
				}
				int w = 0;
				for (int j = itemStart; j < endIdx; ++j)
					if (tokens[j].token != etEOL && tokens[j].token != etWHITESPACE)
						w += (int)tokens[j].value.size();
				result.push_back({itemStart, endIdx, w});
				return result;
			};

			std::vector<Item> items = buildItems(colStartIdx, colEndIdx);
			if (items.empty()) return false;

			// Reject if any item is too complex.
			for (const auto& item : items)
				if (!isSimpleItem(tokens, item.start, item.end))
					return false;

			// Reject if the list is too short for brick layout.
			if ((int)items.size() < FMT_BRICK_LIST_MIN_ITEMS) return false;

			// pushSpecialIndent was already called by the caller; stack.getSpacer() is
			// the continuation indent for the column list body.
			const wstring contSpacer = stack.getSpacer();

			// Emit the hint comment (if any) on the same line as SELECT, then begin
			// the brick column list on the next line.
			if (hintTokenIdx >= 0)
			{
				out.putSpace();
				OpenEditor::Token hintTk = tokens[hintTokenIdx];
				align_comment(hintTk, contSpacer);
				out.putToken(hintTk);
				prevTk = hintTk;
				if (!is_end_line_comment(hintTk)) prevNonCommentTk = hintTk;
			}

			stack.incrementCurrentLineTokenCounter();

			// Helper: emit one brick block (SELECT or INTO var list).
			// Returns the set of item indices (0-based) where a line-wrap occurred.
			// If mirrorWraps is non-empty, forces wraps at those indices instead of
			// recalculating.
			auto emitBrickItems = [&](const std::vector<Item>& lst,
									  const std::vector<int>& mirrorWraps,
									  std::vector<int>& outWraps)
			{
				OpenEditor::Token fakeLParen;
				fakeLParen.token = etLEFT_ROUND_BRACKET;

				out.putEol();
				out.putSpace(contSpacer);
				int lineLen     = 0;
				int itemsOnLine = 0;
				bool firstItem  = true;

				for (int idx = 0; idx < (int)lst.size(); ++idx)
				{
					const auto& item = lst[idx];
					bool isLast = (idx + 1 == (int)lst.size());
					int addLen  = item.width + (firstItem ? 0 : 2) + (isLast ? 0 : 1);

					bool wrapHere;
					if (!mirrorWraps.empty())
					{
						wrapHere = !firstItem &&
							std::find(mirrorWraps.begin(), mirrorWraps.end(), idx) != mirrorWraps.end();
					}
					else
					{
						wrapHere = !firstItem &&
							(lineLen + addLen > FMT_BRICK_LIST_MAX_LINE_LEN
							|| itemsOnLine >= FMT_BRICK_LIST_MAX_LINE_ITEMS);
					}

					OpenEditor::Token prevEmit;
					if (wrapHere)
					{
						out.putEol();
						out.putSpace(contSpacer);
						lineLen     = 0;
						itemsOnLine = 0;
						prevEmit    = fakeLParen;
						outWraps.push_back(idx);
					}
					else
					{
						prevEmit = firstItem ? fakeLParen : prevTk;
					}

					for (int j = item.start; j < item.end; ++j)
					{
						const auto& t = tokens[j];
						if (t.token == etEOL || t.token == etWHITESPACE) continue;
						if (prevEmit.token != etNONE
							&& prevEmit.token != etDOT
							&& t.token       != etDOT
							&& prevEmit.token != etLEFT_ROUND_BRACKET
							&& t.token       != etRIGHT_ROUND_BRACKET
							&& t.token       != etCOMMA
							&& t.token       != etSEMICOLON
							&& prevEmit.token != etCOLON)
						{
							out.putSpace(L" ");
							lineLen += 1;
						}
						else if (prevEmit.reserved != 0 && t.token == etLEFT_ROUND_BRACKET)
						{
							out.putSpace(L" ");
							lineLen += 1;
						}
						out.putToken(t);
						lineLen += (int)t.value.size();
						prevEmit = t;
					}
					prevTk = prevEmit;
					prevNonCommentTk = prevEmit;

					if (!isLast)
					{
						OpenEditor::Token comma;
						comma.token  = etCOMMA;
						comma.value  = L",";
						comma.length = 1;
						out.putToken(comma);
						lineLen += 1;
						prevTk = comma;
						prevNonCommentTk = comma;
					}

					itemsOnLine++;
					firstItem = false;
				}
			};

			// --- Emit SELECT col list ---
			// Consult the set-operation brick side table: if a previous SELECT branch
			// at the same query level already established wrap positions and the item
			// count matches, mirror those positions onto this branch.
			std::vector<int> selWrapIndices;
			{
				auto& state = setOpBrickByDepth[parenDepth];
				if (FMT_BRICK_LIST_LINKED_SET_OP
					 && state.columnCount > 0 && (int)items.size() == state.columnCount)
				{
					// Mirror branch: use stored wrap positions.
					emitBrickItems(items, state.wrapPositions, selWrapIndices);
				}
				else
				{
					// First (or mismatched) branch: emit independently.
					std::vector<int> noMirror;
					emitBrickItems(items, noMirror, selWrapIndices);
					// Establish this branch as the reference for subsequent branches.
					if (FMT_BRICK_LIST_LINKED_SET_OP && state.columnCount == 0)
					{
						state.wrapPositions = selWrapIndices;
						state.columnCount   = (int)items.size();
					}
				}
			}

			// Store wrap positions in ScopeFrame for potential INTO mirroring.
			sel.selectBrickWrapCommas  = selWrapIndices;
			sel.selectBrickColumnCount = (int)items.size();

			// --- Emit INTO var list (if present) ---
			if (intoKw && intoKw->srcIdx >= 0 && intoKw->closeSrcIdx >= 0)
			{
				int intoIdx    = intoKw->srcIdx;
				int varEndIdx  = intoKw->closeSrcIdx;

				std::vector<Item> intoItems = buildItems(intoIdx + 1, varEndIdx);

				// Mirror wraps only when the cardinality matches.
				std::vector<int> mirror;
				if ((int)intoItems.size() == (int)items.size())
					mirror = selWrapIndices;

				// Emit INTO keyword at base indent (one level up from col-list indent).
				stack.popSpecialIndent();
				out.putEol();
				out.putSpace(stack.getSpacer());
				auto& intoTk = tokens[intoIdx];
				out.putToken(intoTk);
				prevTk = intoTk;
				prevNonCommentTk = intoTk;
				stack.pushSpecialIndent(); // rebalance: FROM handler will pop once

				// Emit var list using contSpacer (same as SELECT col-list indent).
				std::vector<int> intoWraps;
				emitBrickItems(intoItems, mirror, intoWraps);

				sel.selectHasInto   = true;
				sel.selectIntoSrcIdx = intoIdx;

				// Advance iterator to just before FROM (skip INTO body).
				it = tokens.begin() + varEndIdx - 1;
			}
			else
			{
				// Advance iterator to just before FROM.
				it = tokens.begin() + fromIdx - 1;
			}
			return true;
		}

		// Short FROM table list.
		bool tryFromShortList(std::vector<OpenEditor::Token>& tokens,
							  std::vector<OpenEditor::Token>::iterator& it,
							  const OpenEditor::Token& tk)
		{
			if (!FMT_FROM_SHORT_LIST_ENABLED) return false;
			int curIdx = (int)(it - tokens.begin());
			auto* _sel = lastScope(ScopeFrame::Select);
			const auto* ce = (_sel && _sel->node) ? getKeyword(_sel->node->keywords, etFROM, curIdx) : nullptr;
			if (!ce || ce->formattedLen > FMT_FROM_SHORT_LIST_MAX_LEN
				|| ce->hasComment || ce->closeSrcIdx < 0)
				return false;
			stack.incrementCurrentLineTokenCounter();
			auto endIt = tokens.begin() + ce->closeSrcIdx;
			OpenEditor::Token prev = tk;
			putSingleLine(out, it + 1, endIt, prev);
			prevTk = prev;
			prevNonCommentTk = prev;
			it = endIt - 1;
			return true;
		}

		// Short GROUP BY list.
		bool tryGroupByShortList(std::vector<OpenEditor::Token>& tokens,
								 std::vector<OpenEditor::Token>::iterator& it,
								 const OpenEditor::Token& tk)
		{
			if (!FMT_GROUPBY_SHORT_LIST_ENABLED) return false;
			int curIdx = (int)(it - tokens.begin());
			auto* _sel = lastScope(ScopeFrame::Select);
			const auto* ce = (_sel && _sel->node) ? getKeyword(_sel->node->keywords, etBY, curIdx) : nullptr;
			if (!ce || ce->formattedLen > FMT_GROUPBY_SHORT_LIST_MAX_LEN
				|| ce->hasComment || ce->closeSrcIdx < 0)
				return false;
			stack.incrementCurrentLineTokenCounter();
			auto endIt = tokens.begin() + ce->closeSrcIdx;
			OpenEditor::Token prev = tk;
			putSingleLine(out, it + 1, endIt, prev);
			prevTk = prev;
			prevNonCommentTk = prev;
			it = endIt - 1;
			return true;
		}

		// Short ORDER BY list.
		bool tryOrderByShortList(std::vector<OpenEditor::Token>& tokens,
								 std::vector<OpenEditor::Token>::iterator& it,
								 const OpenEditor::Token& tk)
		{
			if (!FMT_ORDERBY_SHORT_LIST_ENABLED) return false;
			int curIdx = (int)(it - tokens.begin());
			auto* _sel = lastScope(ScopeFrame::Select);
			const auto* ce = (_sel && _sel->node) ? getKeyword(_sel->node->keywords, etBY, curIdx) : nullptr;
			if (!ce || ce->formattedLen > FMT_ORDERBY_SHORT_LIST_MAX_LEN
				|| ce->hasComment || ce->closeSrcIdx < 0)
				return false;
			stack.incrementCurrentLineTokenCounter();
			auto endIt = tokens.begin() + ce->closeSrcIdx;
			OpenEditor::Token prev = tk;
			putSingleLine(out, it + 1, endIt, prev);
			prevTk = prev;
			prevNonCommentTk = prev;
			it = endIt - 1;
			return true;
		}

		// Returns true when the token range [start, end) is simple enough for
		// brick layout: no CASE, no scalar subquery (SELECT inside parens), no block
		// comment, paren nesting depth <= 1, and the argument text inside a call
		// paren does not exceed BRICK_CALL_ARG_MAX_LEN characters.
		static bool isSimpleItem(const std::vector<OpenEditor::Token>& tokens,
								 int start, int end)
		{
			constexpr int BRICK_CALL_ARG_MAX_LEN = 30;
			int  depth        = 0;
			int  callArgStart = -1; // index of '(' that opened the current call

			for (int i = start; i < end; ++i)
			{
				const auto& t = tokens[i];
				if (t.token == etEOL || t.token == etWHITESPACE) continue;

				// Hard disqualifiers
					if (t.token == etCASE)                              return false;
					if (t.token == etSELECT)                            return false;
					// Block comment (/* ... */) — value does not start with "--"
					if (t.token == etCOMMENT
						&& (t.value.size() < 2 || t.value[0] != L'-'))  return false;
					// Multiline string literal
					if ((t.token == etQUOTED_STRING || t.token == etDOUBLE_QUOTED_STRING)
						&& t.value.find(L'\n') != std::wstring::npos)    return false;

				if (t.token == etLEFT_ROUND_BRACKET)
				{
					++depth;
					if (depth == 1)
						callArgStart = i; // remember where call args begin
					else
						return false;     // depth > 1 — nested parens
				}
				else if (t.token == etRIGHT_ROUND_BRACKET)
				{
					if (depth == 1 && callArgStart >= 0)
					{
						// Measure raw text width of tokens inside the call parens.
						int argWidth = 0;
						for (int j = callArgStart + 1; j < i; ++j)
						{
							if (tokens[j].token != etEOL && tokens[j].token != etWHITESPACE)
								argWidth += (int)tokens[j].value.size();
						}
						if (argWidth > BRICK_CALL_ARG_MAX_LEN)     return false;
						callArgStart = -1;
					}
					--depth;
				}
			}
			return true;
		}

		// INSERT column list in "brick" (rectangular) format.
		// Emits items across multiple lines, wrapping when the current line exceeds
		// FMT_BRICK_LIST_MAX_LINE_LEN characters or FMT_BRICK_LIST_MAX_LINE_ITEMS items.
		// Continuation lines use base-indent + 1 level.
		// Called at the opening '(' of an INSERT column list.
		// Returns false to fall through if the feature is off or the list contains
		// a block comment or a complex expression item.
		bool tryInsertBrickList(std::vector<OpenEditor::Token>& tokens,
								std::vector<OpenEditor::Token>::iterator& it,
								const OpenEditor::Token& tk)
		{
			if (!FMT_BRICK_LIST_ENABLED) return false;

			// Only applies to the INSERT column-list '(' — must be inside an Insert
			// scope and this must be the immediately outer paren (parenDepth was already
			// incremented before this call, so compare against pushedAtParenDepth + 1).
			auto* ins = lastScope(ScopeFrame::Insert);
			auto* mrg = lastScope(ScopeFrame::Merge);

			// Merge INSERT/VALUES: accept when inside a Merge scope whose INSERT or VALUES
			// clause is active and this is the first paren level inside that clause.
			if (!ins && mrg)
			{
				if (mrg->mergeLastClause != etINSERT && mrg->mergeLastClause != etVALUES)
					return false;
				if (parenDepth != mrg->pushedAtParenDepth + 1) return false;
				// fall through to the shared item-collection logic below
			}
			else
			{
				if (!ins) return false;
				if (parenDepth != ins->pushedAtParenDepth + 1) return false;
				// Only the INSERT column list or VALUES list — not a subexpr.
				if (ins->insertLastClause != etINTO && ins->insertLastClause != etVALUES) return false;
			}

			int curIdx = (int)(it - tokens.begin());
			auto* parenNode = findInTree(tree, curIdx);
			if (!parenNode || parenNode->closeSrcIdx < 0) return false;
			if (parenNode->hasComment) return false;

			// If the first real token inside the paren is SELECT or WITH this is a
			// subquery-form INSERT or CTE — not a column/values list.
			{
				int closeIdx = parenNode->closeSrcIdx;
				for (int i = curIdx + 1; i < closeIdx; ++i)
				{
					if (tokens[i].token == etEOL || tokens[i].token == etWHITESPACE) continue;
					if (tokens[i].token == etSELECT || tokens[i].token == etWITH) return false;
					break; // first real token is something else — proceed
				}
			}

			// Build the item list: collect token ranges between commas at depth 0.
			// Depth 0 is inside the column-list parens (we start after the opening '(').
			struct Item { int start; int end; int width; }; // half-open token ranges
			std::vector<Item> items;
			{
				int closeIdx = parenNode->closeSrcIdx; // index of ')'
				int depth = 0;
				int itemStart = curIdx + 1;
				for (int i = curIdx + 1; i < closeIdx; ++i)
				{
					const auto& t = tokens[i];
					if (t.token == etLEFT_ROUND_BRACKET)  { ++depth; continue; }
					if (t.token == etRIGHT_ROUND_BRACKET) { --depth; continue; }
					if (t.token == etCOMMA && depth == 0)
					{
						// measure visible width of [itemStart, i) skipping whitespace/EOL
						int w = 0;
						for (int j = itemStart; j < i; ++j)
						{
							if (tokens[j].token != etEOL && tokens[j].token != etWHITESPACE)
								w += (int)tokens[j].value.size();
						}
						items.push_back({itemStart, i, w});
						itemStart = i + 1; // after the comma
					}
				}
				// last item (no trailing comma)
				{
					int w = 0;
					for (int j = itemStart; j < closeIdx; ++j)
					{
						if (tokens[j].token != etEOL && tokens[j].token != etWHITESPACE)
							w += (int)tokens[j].value.size();
					}
					items.push_back({itemStart, closeIdx, w});
				}
			}

			// Reject brick layout if any item is too complex.
			for (const auto& item : items)
				if (!isSimpleItem(tokens, item.start, item.end))
					return false;

			// Reject if the list is too short for brick layout.
			if ((int)items.size() < FMT_BRICK_LIST_MIN_ITEMS) return false;

			// Determine the indented line prefix for continuation lines (base + 1 level).
			stack.pushSpecialIndent();
			const wstring contSpacer = stack.getSpacer();
			stack.popSpecialIndent();

			// Emit one brick block.
			// mirrorWraps: if non-null, forces line-breaks at those item indices instead of
			//              recalculating from width/count thresholds.
			// outWraps:    receives item indices where a line-break was emitted
			//              (only populated when mirrorWraps is null).
			auto emitInsertBrickBlock = [&](
				const std::vector<Item>& blkItems,
				const std::vector<int>*  mirrorWraps,
				std::vector<int>&        outWraps)
			{
				out.putEol();
				out.putSpace(contSpacer);
				int lineLen     = 0;
				int itemsOnLine = 0;
				bool firstItem  = true;
				OpenEditor::Token fakeLParen;
				fakeLParen.token = etLEFT_ROUND_BRACKET;

				for (int idx = 0; idx < (int)blkItems.size(); ++idx)
				{
					const auto& item = blkItems[idx];
					bool isLast = (idx + 1 == (int)blkItems.size());
					// +2 for ", " separator before non-first items; +1 for "," after non-last
					int addLen = item.width + (firstItem ? 0 : 2) + (isLast ? 0 : 1);

					bool wrapHere;
					if (mirrorWraps)
						wrapHere = !firstItem &&
							std::find(mirrorWraps->begin(), mirrorWraps->end(), idx) != mirrorWraps->end();
					else
						wrapHere = !firstItem &&
							(lineLen + addLen > FMT_BRICK_LIST_MAX_LINE_LEN
							 || itemsOnLine >= FMT_BRICK_LIST_MAX_LINE_ITEMS);

					OpenEditor::Token prevEmit;
					if (wrapHere)
					{
						out.putEol();
						out.putSpace(contSpacer);
						lineLen     = 0;
						itemsOnLine = 0;
						prevEmit    = fakeLParen;
						if (!mirrorWraps) outWraps.push_back(idx);
					}
					else
					{
						prevEmit = firstItem ? fakeLParen : prevTk;
					}

					for (int j = item.start; j < item.end; ++j)
					{
						const auto& t = tokens[j];
						if (t.token == etEOL || t.token == etWHITESPACE) continue;
						if (prevEmit.token != etNONE
							&& prevEmit.token != etDOT
							&& t.token       != etDOT
							&& prevEmit.token != etLEFT_ROUND_BRACKET
							&& t.token       != etRIGHT_ROUND_BRACKET
							&& t.token       != etCOMMA
							&& t.token       != etSEMICOLON)
						{
							out.putSpace(L" ");
							lineLen += 1;
						}
						else if (prevEmit.reserved != 0 && t.token == etLEFT_ROUND_BRACKET)
						{
							out.putSpace(L" ");
							lineLen += 1;
						}
						out.putToken(t);
						lineLen += (int)t.value.size();
						prevEmit = t;
					}
					prevTk = prevEmit;
					prevNonCommentTk = prevEmit;

					// Emit comma after item (except the last).
					if (!isLast)
					{
						OpenEditor::Token comma;
						comma.token  = etCOMMA;
						comma.value  = L",";
						comma.length = 1;
						out.putToken(comma);
						lineLen += 1;
						prevTk = comma;
						prevNonCommentTk = comma;
					}

					itemsOnLine++;
					firstItem = false;
				}
			};

			// Column list (etINTO): emit and record wrap positions for mirroring.
			// VALUES list (etVALUES): mirror column-list wraps when item counts match.
			if (ins && ins->insertLastClause == etINTO)
			{
				std::vector<int> wrapPositions;
				emitInsertBrickBlock(items, nullptr, wrapPositions);
				if (FMT_BRICK_LIST_LINKED_INSERT)
				{
					ins->insertBrickWrapPositions = wrapPositions;
					ins->insertBrickColumnCount   = (int)items.size();
				}
			}
			else if (ins && ins->insertLastClause == etVALUES
					 && FMT_BRICK_LIST_LINKED_INSERT
					 && ins->insertBrickColumnCount > 0
					 && (int)items.size() == ins->insertBrickColumnCount)
			{
				// Mirror column-list wrap positions onto the VALUES list.
				std::vector<int> unused;
				emitInsertBrickBlock(items, &ins->insertBrickWrapPositions, unused);
			}
			else
			{
				// Merge INSERT/VALUES path, or mismatched item counts: independent layout.
				std::vector<int> unused;
				emitInsertBrickBlock(items, nullptr, unused);
			}

			// Closing ')' on its own line, dedented back to the INSERT body level.
			out.putEol();
			out.putSpace(stack.getSpacer());
			it = tokens.begin() + parenNode->closeSrcIdx;
			out.putToken(*it); // ')'
			stack.incrementCurrentLineTokenCounter();
			prevTk = *it;
			prevNonCommentTk = *it;
			return true;
		}

		// Short WHEN clause.
		bool tryWhenShortLine(std::vector<OpenEditor::Token>& tokens,
							  std::vector<OpenEditor::Token>::iterator& it,
							  const OpenEditor::Token& tk)
		{
			ScopeFrame* caseCtx = lastScope(ScopeFrame::Case);
			if (!caseCtx || !caseCtx->node) return false;
			int curIdx = (int)(it - tokens.begin());
			const auto* wce = getKeyword(caseCtx->node->keywords, etWHEN, curIdx);
			if (!wce || wce->formattedLen > FMT_CASE_WHEN_SHORT_MAX_LEN
				|| wce->hasComment || wce->closeSrcIdx < 0)
				return false;
			auto endIt = tokens.begin() + wce->closeSrcIdx;
			out.putSpace(stack.getSpacer());
			out.putToken(tk); // WHEN
			OpenEditor::Token prev = tk;
			putSingleLine(out, it + 1, endIt, prev);
			prevTk = prev;
			prevNonCommentTk = prev;
			it = endIt - 1;
			return true;
		}

		// Whole CASE…END expression on one line (used from the pre-emit path).
		// `it` points at etCASE, which has NOT been emitted yet.
		bool tryCaseOneLine(std::vector<OpenEditor::Token>& tokens,
							std::vector<OpenEditor::Token>::iterator& it)
		{
			int curIdx = (int)(it - tokens.begin());
			auto* caseNode = findInTree(tree, curIdx);
			if (!caseNode || caseNode->formattedLen > FMT_CASE_WHEN_SHORT_MAX_LEN
				|| caseNode->hasComment || caseNode->closeSrcIdx < 0)
				return false;
			// closeSrcIdx points at END; include it.
			auto endIt = tokens.begin() + caseNode->closeSrcIdx + 1;
			// When CASE is mid-line (after = or another token), emit a single space,
			// not the full indentation prefix.
			if (stack.getCurrentLineTokenCounter() == 0)
				out.putSpace(stack.getSpacer());
			else
				out.putSpace();
			OpenEditor::Token prev; prev.token = etNONE;
			putSingleLine(out, it, endIt, prev);
			prevTk = prev;
			prevNonCommentTk = prev;
			it = endIt - 1;
			return true;
		}

		// Called at statement boundaries (;, /, end-of-stream) alongside
		// alignStatementComments.  alignSetAssignments self-terminates at the
		// first line that doesn't start with the assignment-list indent, so no
		// per-clause-boundary calls are needed.
		void flushSetAlignment()
		{
			if (!FMT_ALIGN_SET_ASSIGNMENTS) return;
			auto* upd = lastScope(ScopeFrame::Update);
			if (upd && upd->updateSetOutPos >= 0)
			{
				alignSetAssignments(out, upd->updateSetOutPos, upd->updateSetIndent);
				if (FMT_ALIGN_CASE_IN_SET)
					alignCaseInSetList(out, upd->updateSetOutPos,
									   upd->updateSetIndent, FMT_INDENT_SIZE);
			}
			auto* mrg = lastScope(ScopeFrame::Merge);
			if (mrg && mrg->mergeSetOutPos >= 0)
			{
				alignSetAssignments(out, mrg->mergeSetOutPos, mrg->mergeSetIndent);
				if (FMT_ALIGN_CASE_IN_SET)
					alignCaseInSetList(out, mrg->mergeSetOutPos,
									   mrg->mergeSetIndent, FMT_INDENT_SIZE);
				mrg->mergeSetOutPos = -1; // reset so next WHEN block starts fresh
			}
			// No need to reset updateSetOutPos — resetStatementState() clears the frame.
		}

		// RETURNING / INTO — three layout paths:
		//   1. Single-line  : total width <= FMT_RETURNING_MAX_LEN
		//   2. Brick        : FMT_BRICK_LIST_ENABLED — emit two brick blocks,
		//                     one after RETURNING and one after INTO, each
		//                     line-wrapped at FMT_RETURNING_MAX_LEN with at most
		//                     FMT_RETURNING_MAX_ITEMS items per line.
		//   3. Two-line     : fallback — RETURNING list on one line, INTO on next.
		// skipPop: caller already popped the active clause indent.
		bool tryReturning(std::vector<OpenEditor::Token>& tokens,
						  std::vector<OpenEditor::Token>::iterator& it,
						  const OpenEditor::Token& tk,
						  bool skipPop = false)
		{
			auto* del = lastScope(ScopeFrame::Delete);
			auto* upd = lastScope(ScopeFrame::Update);
			auto* ins = lastScope(ScopeFrame::Insert);
			if (tk.token != etRETURNING || (!del && !upd && !ins)) return false;

			FormatContext* scopeNode = del ? del->node : upd ? upd->node : ins ? ins->node : nullptr;

			if (!skipPop) stack.popSpecialIndent();
			out.putEol();

			const auto* retKw  = scopeNode ? getKeyword(scopeNode->keywords, etRETURNING) : nullptr;
			const auto* intoKw = scopeNode ? getKeyword(scopeNode->keywords, etINTO)      : nullptr;

			// canShorten: both lists are comment-free and bounded — enables paths 1/2/3.
			bool canShorten = retKw  && retKw->closeSrcIdx  >= 0 && !retKw->hasComment
						   && intoKw && intoKw->closeSrcIdx >= 0 && !intoKw->hasComment;

			int retLen  = (retKw  && retKw->formattedLen  < INT_MAX/2) ? retKw->formattedLen  : INT_MAX;
			int intoLen = (intoKw && intoKw->formattedLen < INT_MAX/2) ? intoKw->formattedLen : INT_MAX;
			int totalLen = (retLen < INT_MAX/2 && intoLen < INT_MAX/2)
						 ? retLen + 6 /* " INTO " */ + intoLen : INT_MAX;

			auto retListBegin  = it + 1;
			auto intoIt        = canShorten ? tokens.begin() + retKw->closeSrcIdx  : tokens.end();
			auto intoListBegin = canShorten ? intoIt + 1                           : tokens.end();
			auto intoListEnd   = canShorten ? tokens.begin() + intoKw->closeSrcIdx : tokens.end();

			// ---- path 1: single-line ----
			// Both lists + " INTO " fit within FMT_RETURNING_MAX_LEN.
			if (canShorten && totalLen <= FMT_RETURNING_MAX_LEN)
			{
				out.putSpace(stack.getSpacer());
				out.putToken(tk);
				stack.resetCurrentLineTokenCounter();
				stack.incrementCurrentLineTokenCounter();
				OpenEditor::Token prev = tk;
				putSingleLine(out, retListBegin, intoIt, prev);
				out.putSpace();
				out.putToken(*intoIt);
				prev = *intoIt;
				putSingleLine(out, intoListBegin, intoListEnd, prev);
				prevTk = prev;
				it = intoListEnd - 1;
				if (tk.token != etCOMMENT) prevNonCommentTk = tk;
				return true;
			}

			// ---- path 2: two-line ----
			// Each list individually fits on one line; emit RETURNING list then INTO list.
			if (canShorten && retLen <= FMT_RETURNING_MAX_LEN && intoLen <= FMT_RETURNING_MAX_LEN)
			{
				out.putSpace(stack.getSpacer());
				out.putToken(tk);
				stack.resetCurrentLineTokenCounter();
				stack.incrementCurrentLineTokenCounter();
				OpenEditor::Token prev = tk;
				putSingleLine(out, retListBegin, intoIt, prev);
				out.putEol();
				out.putSpace(stack.getSpacer());
				out.putToken(*intoIt);
				stack.resetCurrentLineTokenCounter();
				stack.incrementCurrentLineTokenCounter();
				prev = *intoIt;
				putSingleLine(out, intoListBegin, intoListEnd, prev);
				prevTk = prev;
				it = intoListEnd - 1;
				if (tk.token != etCOMMENT) prevNonCommentTk = tk;
				return true;
			}

			// ---- path 3: brick ----
			// Lists are too wide for a single line each; use brick layout.
			// Controlled by FMT_RETURNING_MAX_ITEMS (independent of FMT_BRICK_LIST_ENABLED).
			// Wrap positions are mirrored onto the INTO list when item counts match.

			// Helper: collect comma-separated items from a token range [begin, end).
			struct BrickItem { int start; int end; int width; };
			auto collectItems = [&](
				std::vector<OpenEditor::Token>::const_iterator begin,
				std::vector<OpenEditor::Token>::const_iterator end)
				-> std::vector<BrickItem>
			{
				std::vector<BrickItem> items;
				int depth = 0;
				int iStart = (int)(begin - tokens.begin());
				int iEnd   = (int)(end   - tokens.begin());
				for (int i = iStart; i < iEnd; ++i)
				{
					const auto& t = tokens[i];
					if (t.token == etEOL || t.token == etWHITESPACE) continue;
					if (t.token == etLEFT_ROUND_BRACKET)  { ++depth; continue; }
					if (t.token == etRIGHT_ROUND_BRACKET) { --depth; continue; }
					if (t.token == etCOMMA && depth == 0)
					{
						int w = 0;
						for (int j = iStart; j < i; ++j)
							if (tokens[j].token != etEOL && tokens[j].token != etWHITESPACE)
								w += (int)tokens[j].value.size();
						items.push_back({iStart, i, w});
						iStart = i + 1;
					}
				}
				{
					int w = 0;
					for (int j = iStart; j < iEnd; ++j)
						if (tokens[j].token != etEOL && tokens[j].token != etWHITESPACE)
							w += (int)tokens[j].value.size();
					items.push_back({iStart, iEnd, w});
				}
				return items;
			};

			// Helper: emit one brick block.
			// mirrorWraps: if non-null, forces line-breaks at those item indices instead of
			//              recalculating from width/count thresholds.
			// outWraps:    receives item indices where a line-break was emitted
			//              (only populated when mirrorWraps is null).
			auto emitBrickBlock = [&](
				const std::vector<BrickItem>& items,
				const wstring& contSpacer,
				const std::vector<int>* mirrorWraps,
				std::vector<int>& outWraps)
			{
				out.putEol();
				out.putSpace(contSpacer);
				int lineLen = 0, itemsOnLine = 0;
				bool firstItem = true;
				OpenEditor::Token fakeLParen; fakeLParen.token = etLEFT_ROUND_BRACKET;
				for (int idx = 0; idx < (int)items.size(); ++idx)
				{
					const auto& item = items[idx];
					bool isLast = (idx + 1 == (int)items.size());
					int addLen = item.width + (firstItem ? 0 : 2) + (isLast ? 0 : 1);

					bool wrapHere;
					if (mirrorWraps)
						wrapHere = !firstItem &&
							std::find(mirrorWraps->begin(), mirrorWraps->end(), idx) != mirrorWraps->end();
					else
						wrapHere = !firstItem &&
							(lineLen + addLen > FMT_RETURNING_MAX_LEN
							|| itemsOnLine >= FMT_RETURNING_MAX_ITEMS);

					OpenEditor::Token prevEmit = wrapHere ? fakeLParen
											  : firstItem ? fakeLParen : prevTk;
					if (wrapHere)
					{
						out.putEol(); out.putSpace(contSpacer);
						lineLen = 0; itemsOnLine = 0;
						if (!mirrorWraps) outWraps.push_back(idx);
					}

					for (int j = item.start; j < item.end; ++j)
					{
						const auto& t = tokens[j];
						if (t.token == etEOL || t.token == etWHITESPACE) continue;
						if (prevEmit.token != etNONE
							&& prevEmit.token != etDOT
							&& t.token != etDOT
							&& prevEmit.token != etLEFT_ROUND_BRACKET
							&& t.token != etLEFT_ROUND_BRACKET
							&& t.token != etRIGHT_ROUND_BRACKET
							&& prevEmit.token != etCOLON
							&& t.token != etCOMMA)
							out.putSpace();
						else if (prevEmit.reserved != 0 && t.token == etLEFT_ROUND_BRACKET)
							out.putSpace();
						out.putToken(t);
						prevEmit = t;
					}
					if (!isLast)
					{
						OpenEditor::Token comma; comma.token = etCOMMA; comma.value = L",";
						out.putToken(comma);
						prevEmit = comma;
					}
					lineLen += addLen;
					++itemsOnLine;
					firstItem = false;
					prevTk = prevEmit;
				}
			};

			if (canShorten && FMT_RETURNING_MAX_ITEMS > 0)
			{
				auto retItems  = collectItems(retListBegin,  intoIt);
				auto intoItems = collectItems(intoListBegin, intoListEnd);

				out.putSpace(stack.getSpacer());
				out.putToken(tk); // RETURNING
				stack.resetCurrentLineTokenCounter();
				stack.incrementCurrentLineTokenCounter();

				stack.pushSpecialIndent();
				wstring contSpacer = stack.getSpacer();
				stack.popSpecialIndent();

				// Emit RETURNING list; collect wrap positions.
				std::vector<int> retWrapIndices;
				emitBrickBlock(retItems, contSpacer, nullptr, retWrapIndices);

				// Emit INTO keyword at RETURNING level.
				out.putEol();
				out.putSpace(stack.getSpacer());
				out.putToken(*intoIt);
				stack.resetCurrentLineTokenCounter();
				stack.incrementCurrentLineTokenCounter();

				// Mirror wrap positions onto INTO list when item counts match.
				std::vector<int> unused;
				const std::vector<int>* mirror =
					(intoItems.size() == retItems.size()) ? &retWrapIndices : nullptr;
				emitBrickBlock(intoItems, contSpacer, mirror, unused);

				prevTk = (tokens.begin() + intoItems.back().end - 1) != tokens.begin()
						 ? tokens[intoItems.back().end - 1] : *intoIt;
				it = intoListEnd - 1;
				if (tk.token != etCOMMENT) prevNonCommentTk = tk;
				return true;
			}

			// ---- path 4: full-format (main loop) ----
			// Reached only when canShorten is false (comments in either list).
			// Token-by-token; the etINTO handler in the main loop deals with INTO.
			out.putSpace(stack.getSpacer());
			out.putToken(tk);
			out.putEol();
			stack.pushSpecialIndent(); // col-list indent (popped when INTO arrives)
			prevTk = tk;
			if (tk.token != etCOMMENT) prevNonCommentTk = tk;
			return true;
		}

		// ---- constructor: reads settings, builds tree, runs the main loop ----
		SQLFormatter(std::vector<OpenEditor::Token>& tokens, const Settings& s)
			: stack(s.GetIndentSpacing())
		{
			// Read formatter settings from persistent settings
			FMT_INDENT_SIZE               = s.GetIndentSpacing();
			FMT_SIMPLE_STMT_ENABLED       = s.GetFmtSimpleStmtEnabled();
			FMT_SIMPLE_STMT_MAX_LEN       = s.GetFmtSimpleStmtMaxLen();
			FMT_SELECT_SHORT_LIST_ENABLED = s.GetFmtSelectShortListEnabled();
			FMT_SELECT_SHORT_LIST_MAX_LEN = s.GetFmtSelectShortListMaxLen();
			FMT_FROM_SHORT_LIST_ENABLED   = s.GetFmtFromShortListEnabled();
			FMT_FROM_SHORT_LIST_MAX_LEN   = s.GetFmtFromShortListMaxLen();
			FMT_ALIGN_COLUMN_ALIASES      = s.GetFmtAlignColumnAliases();
			FMT_BLOCK_SHORT_MAX_LEN       = s.GetFmtBlockShortMaxLen();
			FMT_SELECT_STMT_ENABLED       = s.GetFmtSelectStmtEnabled();
			FMT_SELECT_STMT_MAX_LEN       = s.GetFmtSelectStmtMaxLen();
			FMT_GROUPBY_SHORT_LIST_ENABLED = s.GetFmtGroupByShortListEnabled();
			FMT_GROUPBY_SHORT_LIST_MAX_LEN = s.GetFmtGroupByShortListMaxLen();
			FMT_ORDERBY_SHORT_LIST_ENABLED = s.GetFmtOrderByShortListEnabled();
			FMT_ORDERBY_SHORT_LIST_MAX_LEN = s.GetFmtOrderByShortListMaxLen();
			FMT_SET_OP_BLANK_LINE          = s.GetFmtSetOpBlankLine();
			FMT_CASE_WHEN_SHORT_MAX_LEN    = s.GetFmtCaseWhenShortMaxLen();
			FMT_RETURNING_MAX_LEN          = s.GetFmtReturningMaxLen();
			FMT_RETURNING_MAX_ITEMS        = s.GetFmtReturningMaxItems();
			FMT_LOG_ERRORS_MAX_LEN         = s.GetFmtLogErrorsMaxLen();
			FMT_ALIGN_SET_ASSIGNMENTS      = s.GetFmtAlignSetAssignments();
			FMT_ALIGN_CASE_IN_SET          = s.GetFmtAlignCaseInSet();
			FMT_BRICK_LIST_ENABLED         = s.GetFmtBrickListEnabled();
			FMT_BRICK_LIST_MAX_LINE_LEN    = s.GetFmtBrickListMaxLineLen();
			FMT_BRICK_LIST_MAX_LINE_ITEMS  = s.GetFmtBrickListMaxLineItems();
			FMT_BRICK_LIST_MIN_ITEMS       = s.GetFmtBrickListMinItems();
			FMT_BRICK_LIST_LINKED_INSERT   = s.GetFmtBrickListLinkedInsert();
			FMT_BRICK_LIST_LINKED_SET_OP   = s.GetFmtBrickListLinkedSetOp();

			// Pre-built scope metadata
			tree = buildTree(tokens);
			calcWidths(tree, tokens);

            for (auto it = tokens.begin(); it != tokens.end(); ++it)
            {
                OpenEditor::Token tk = *it;

                if (tk.token != etEOL)
                {
                    // SQL*Plus / separator: always at column 0 on its own line — never indented.
                    // After it, emit a blank line and fully reset formatter state so the next
                    // statement starts fresh at column 0 with clean indentation.
                    if (tk.token == etEOS)
                    {
                        if (!out.endsWithEol())
                            out.putEol();
                        // BugFix: corrupted output after multi - line string token
                        //out.putToken(tk, str + tk.offset, tk.length);
                        out.putToken(tk);

                        // blank separator after / only when another statement follows
                        auto nextIt = it + 1;
                        while (nextIt != tokens.end() && nextIt->token == etEOL)
                            ++nextIt;
                        bool moreFollow = (nextIt != tokens.end());
                        if (moreFollow)
                        {
                            out.putEol(); // end the / line
                            out.putEol(); // blank separator before next statement
                        }

                        // flush SET assignment alignment before comment alignment
                        flushSetAlignment();

                        // align end-line comments across the whole statement
                        out.alignStatementComments(stmtBookmark);
                        out.alignStatementBlockComments(stmtBookmark);

                        // reset indent stack to root level and per-statement state
                        resetStatementState();
                        continue;
                    }

                    // Record the statement-start bookmark for comment alignment.
                    if (prevTk.token == etNONE
                        && tk.token != etEOS)
                    {
                        // COMMENT RULE EXCEPTION: before the beginning of statement 
                        while (it != tokens.end()
                            && (*it == etWHITESPACE || *it == etEOL || *it == etCOMMENT))
                        {
                            if (it->token == etCOMMENT)
                            {
                                align_comment(*it, stack.getSpacer());
                                out.putToken(*it);
                                out.putEol();
                            }
                            ++it;
                            tk = *it;
                            prevTk.token = etNONE; // pretend comment did mot happen
                        }
                        stmtBookmark = out.position();
                    }

                    // At the start of each statement, check whether the whole thing is short
                    // enough to be formatted on a single line.
                    if (FMT_SIMPLE_STMT_ENABLED
                        && prevTk.token == etNONE
                        && tk.token != etEOS)
                    {
                        if (trySimpleStmt(tokens, it)) continue;
                    }

                    // ---- MERGE clause handlers ----
                    // Must come before INSERT handlers and the shared pre-emit block so that
                    // etUSING / etON / etWHEN / etTHEN are intercepted here first.
                    {
                        auto* mrg = lastScope(ScopeFrame::Merge);
                        // Only fire when Merge is the innermost non-Case scope.
                        bool mergeIsInnermost = mrg
                            && (scopeStack.empty() || scopeStack.back().kind != ScopeFrame::Case)
                            && parenDepth == mrg->pushedAtParenDepth;

                        if (mrg && mergeIsInnermost)
                        {
                            // Helper macro-style inline: pop all indents inside current WHEN block.
                            // Used before WHEN / RETURNING / LOG where the full block must be unwound.
                            #define POP_MERGE_BODY_INDENTS() \
                                while (mrg->mergeBodyIndents > 0) \
                                { stack.popSpecialIndent(); --mrg->mergeBodyIndents; }

                            // USING: emit at MERGE+1 (already the current level), then reset counter.
                            if (tk.token == etUSING)
                            {
                                stack.resetCurrentLineTokenCounter();
                                out.putEol();
                                out.putSpace(stack.getSpacer());
                                out.putToken(tk);
                                stack.incrementCurrentLineTokenCounter();
                                mrg->mergeLastClause = etUSING;
                                prevTk = tk; prevNonCommentTk = tk;
                                continue;
                            }

                            // ON: same — emit at MERGE+1 without changing stack depth.
                            if (tk.token == etON)
                            {
                                stack.resetCurrentLineTokenCounter();
                                out.putEol();
                                out.putSpace(stack.getSpacer());
                                out.putToken(tk);
                                stack.incrementCurrentLineTokenCounter();
                                mrg->mergeLastClause = etON;
                                prevTk = tk; prevNonCommentTk = tk;
                                continue;
                            }

                            // WHEN: pop all body indents from the previous WHEN block, then emit
                            // at MERGE+1 without any additional push (WHEN lives at +1).
                            if (tk.token == etWHEN)
                            {
                                flushSetAlignment(); // flush previous WHEN block's SET alignment
                                POP_MERGE_BODY_INDENTS();
                                stack.resetCurrentLineTokenCounter();
                                out.putEol();
                                out.putSpace(stack.getSpacer());
                                out.putToken(tk); // WHEN
                                stack.incrementCurrentLineTokenCounter();
                                // Detect MATCHED vs NOT MATCHED by peeking ahead.
                                {
                                    auto peek = it + 1;
                                    while (peek != tokens.end()
                                        && (peek->token == etEOL || peek->token == etWHITESPACE))
                                        ++peek;
                                    bool isNot = (peek != tokens.end()
                                        && !_wcsicmp(peek->value.c_str(), L"NOT"));
                                    mrg->mergeInNotMatchedBlock = isNot;
                                    mrg->mergeInMatchedBlock    = !isNot;
                                }
                                mrg->mergeLastClause = etWHEN;
                                prevTk = tk; prevNonCommentTk = tk;
                                continue;
                            }

                            // THEN: stays on the WHEN line; pushes +1 for the body (UPDATE/INSERT level).
                            if (tk.token == etTHEN)
                            {
                                out.putSpace();
                                out.putToken(tk); // THEN
                                stack.incrementCurrentLineTokenCounter();
                                stack.pushSpecialIndent(); // body level (+2 from MERGE base)
                                ++mrg->mergeBodyIndents;
                                mrg->mergeLastClause = etTHEN;
                                prevTk = tk; prevNonCommentTk = tk;
                                continue;
                            }

                            // UPDATE (WHEN MATCHED): own line at current body level (+2).
                            if (tk.token == etUPDATE && mrg->mergeInMatchedBlock)
                            {
                                stack.resetCurrentLineTokenCounter();
                                out.putEol();
                                out.putSpace(stack.getSpacer());
                                out.putToken(tk); // UPDATE
                                stack.incrementCurrentLineTokenCounter();
                                mrg->mergeLastClause = etUPDATE;
                                prevTk = tk; prevNonCommentTk = tk;
                                continue;
                            }

                            // SET (after UPDATE): emit SET at current body level, then push
                            // one more indent for the assignment list — same order as standalone UPDATE.
                            if (tk.token == etSET && mrg->mergeLastClause == etUPDATE)
                            {
                                // Flush any previous WHEN block's SET alignment first.
                                flushSetAlignment();
                                // Emit SET at the current body level (MERGE+2).
                                stack.resetCurrentLineTokenCounter();
                                out.putEol();
                                out.putSpace(stack.getSpacer());
                                out.putToken(tk); // SET
                                // Push assignment-list indent (MERGE+3); resets counter to 0.
                                stack.pushSpecialIndent();
                                ++mrg->mergeBodyIndents;
                                // EOL after SET so first assignment starts at the new indent level.
                                out.putEol();
                                // Record position and indent for = alignment, matching standalone UPDATE.
                                mrg->mergeSetOutPos = out.position();
                                mrg->mergeSetIndent = stack.getSpacer().size();
                                // counter stays 0 — first assignment token gets the full spacer.
                                mrg->mergeLastClause = etSET;
                                prevTk = tk; prevNonCommentTk = tk;
                                continue;
                            }

                            // DELETE (WHEN MATCHED, after UPDATE SET or WHERE): own line at body level.
                            if (tk.token == etDELETE && mrg->mergeInMatchedBlock)
                            {
                                // Pop SET indent or WHERE indent if present.
                                if (mrg->mergeLastClause == etSET
                                    || mrg->mergeLastClause == etWHERE)
                                {
                                    stack.popSpecialIndent();
                                    --mrg->mergeBodyIndents;
                                }
                                stack.resetCurrentLineTokenCounter();
                                out.putEol();
                                out.putSpace(stack.getSpacer());
                                out.putToken(tk); // DELETE
                                stack.incrementCurrentLineTokenCounter();
                                mrg->mergeLastClause = etDELETE;
                                prevTk = tk; prevNonCommentTk = tk;
                                continue;
                            }

                            // INSERT (WHEN NOT MATCHED): own line at body level (+2).
                            if (tk.token == etINSERT && mrg->mergeInNotMatchedBlock)
                            {
                                stack.resetCurrentLineTokenCounter();
                                out.putEol();
                                out.putSpace(stack.getSpacer());
                                out.putToken(tk); // INSERT
                                stack.pushSpecialIndent(); // column-list / VALUES level (+3)
                                ++mrg->mergeBodyIndents;
                                stack.incrementCurrentLineTokenCounter();
                                mrg->mergeLastClause = etINSERT;
                                prevTk = tk; prevNonCommentTk = tk;
                                continue;
                            }

                            // VALUES (after INSERT in WHEN NOT MATCHED).
                            if (tk.token == etVALUES && mrg->mergeLastClause == etINSERT)
                            {
                                stack.resetCurrentLineTokenCounter();
                                out.putEol();
                                out.putSpace(stack.getSpacer());
                                out.putToken(tk); // VALUES
                                stack.incrementCurrentLineTokenCounter();
                                mrg->mergeLastClause = etVALUES;
                                prevTk = tk; prevNonCommentTk = tk;
                                continue;
                            }

                            // WHERE inside a WHEN block (update-where / delete-where / insert-where).
                            if (tk.token == etWHERE
                                && (mrg->mergeLastClause == etSET
                                    || mrg->mergeLastClause == etDELETE
                                    || mrg->mergeLastClause == etVALUES
                                    || mrg->mergeLastClause == etINSERT))
                            {
                                // Pop SET assignment-list indent if coming from SET.
                                if (mrg->mergeLastClause == etSET)
                                {
                                    stack.popSpecialIndent();
                                    --mrg->mergeBodyIndents;
                                }
                                // Pop INSERT column-list indent if coming from INSERT or VALUES.
                                if (mrg->mergeLastClause == etINSERT
                                    || mrg->mergeLastClause == etVALUES)
                                {
                                    stack.popSpecialIndent();
                                    --mrg->mergeBodyIndents;
                                }
                                stack.resetCurrentLineTokenCounter();
                                out.putEol();
                                out.putSpace(stack.getSpacer());
                                out.putToken(tk); // WHERE
                                stack.pushSpecialIndent();
                                ++mrg->mergeBodyIndents;
                                stack.incrementCurrentLineTokenCounter();
                                mrg->mergeLastClause = etWHERE;
                                prevTk = tk; prevNonCommentTk = tk;
                                continue;
                            }

                            // RETURNING: pop all WHEN-block indents, delegate to tryReturning.
                            // tryReturning handles all three paths:
                            //   single-line  (total width <= FmtReturningMaxLen)
                            //   two-line     (each part <= FmtReturningMaxLen)
                            //   full-format  (RETURNING + col-list indented, INTO on its own line)
                            // Single-line and two-line paths consume all tokens up to end of INTO
                            // list inside tryReturning, so no further handling is needed.
                            // Full-format path returns after emitting RETURNING and pushing the
                            // col-list indent; the INTO token then arrives in the next iteration
                            // and is handled by the etINTO block below (mergeLastClause==etRETURNING).
                            if (tk.token == etRETURNING)
                            {
                                POP_MERGE_BODY_INDENTS();
                                // Stack is now at MERGE+1.  tryReturning() calls popSpecialIndent()
                                // once internally, so push one extra level to compensate.
                                stack.pushSpecialIndent();
                                mrg->mergeLastClause = etRETURNING;
                                // Delegate via a temporary fake Delete scope so that
                                // getKeyword() inside tryReturning finds RETURNING/INTO entries
                                // on the Merge tree node.
                                {
                                    ScopeFrame fakeScope;
                                    fakeScope.kind = ScopeFrame::Delete;
                                    fakeScope.node = mrg->node;
                                    fakeScope.pushedAtParenDepth = -1; // sentinel
                                    scopeStack.push_back(fakeScope);
                                    if (tryReturning(tokens, it, tk)) { scopeStack.pop_back(); continue; }
                                    scopeStack.pop_back();
                                }
                                continue;
                            }

                            // INTO after RETURNING (full-format path only).
                            // Single-line and two-line paths are consumed entirely inside
                            // tryReturning; only the full-format path leaves INTO for this handler.
                            if (tk.token == etINTO && mrg->mergeLastClause == etRETURNING)
                            {
                                stack.popSpecialIndent(); // col-list indent
                                out.putEol();
                                out.putSpace(stack.getSpacer());
                                out.putToken(tk); // INTO
                                out.putEol();
                                stack.pushSpecialIndent(); // var-list indent
                                prevTk = tk; prevNonCommentTk = tk;
                                continue;
                            }

                            // LOG ERRORS: pop all WHEN-block indents, apply standard LOG logic.
                            if (tk.token == etLOG)
                            {
                                POP_MERGE_BODY_INDENTS();
                                // Stack is now at MERGE+1 — emit LOG at that level.
                                stack.resetCurrentLineTokenCounter();
                                out.putEol();
                                out.putSpace(stack.getSpacer());
                                out.putToken(tk); // LOG

                                // Measure LOG ERRORS ... width for REJECT LIMIT break decision.
                                if (FMT_LOG_ERRORS_MAX_LEN > 0)
                                {
                                    int rejectIdx = -1;
                                    for (auto si = it + 1; si != tokens.end(); ++si)
                                    {
                                        if (si->token == etSEMICOLON || si->token == etEOS) break;
                                        if (si->token == etUNKNOWN
                                            && !_wcsicmp(si->value.c_str(), L"REJECT"))
                                        { rejectIdx = (int)(si - tokens.begin()); break; }
                                    }
                                    if (rejectIdx > 0)
                                    {
                                        int width = (int)tk.value.size();
                                        OpenEditor::Token prev = tk;
                                        bool tainted = false;
                                        auto endScan = tokens.begin() + rejectIdx;
                                        for (auto si = it + 1; si != endScan; ++si)
                                        {
                                            if (si->token == etEOL) continue;
                                            if (si->token == etCOMMENT) { tainted = true; break; }
                                            if (prev.token != etNONE
                                                && prev.token != etLEFT_ROUND_BRACKET
                                                && si->token != etRIGHT_ROUND_BRACKET
                                                && si->token != etCOMMA)
                                                width += 1;
                                            width += (int)si->value.size();
                                            prev = *si;
                                        }
                                        mrg->mergeLogRejectBreak = !tainted && (width > FMT_LOG_ERRORS_MAX_LEN);
                                    }
                                }
                                stack.pushSpecialIndent();
                                stack.incrementCurrentLineTokenCounter();
                                mrg->mergeLastClause = etLOG;
                                prevTk = tk; prevNonCommentTk = tk;
                                continue;
                            }

                            #undef POP_MERGE_BODY_INDENTS
                        } // end mergeIsInnermost

                        // REJECT LIMIT break inside a Merge LOG ERRORS clause.
                        if (mrg && tk.token == etUNKNOWN
                            && !_wcsicmp(tk.value.c_str(), L"REJECT")
                            && mrg->mergeLogRejectBreak
                            && mrg->mergeLastClause == etLOG)
                        {
                            stack.resetCurrentLineTokenCounter();
                            out.putEol();
                            out.putSpace(stack.getSpacer());
                            out.putToken(tk); // REJECT
                            stack.incrementCurrentLineTokenCounter();
                            prevTk = tk; prevNonCommentTk = tk;
                            continue;
                        }
                    } // end Merge clause handlers

                    // ---- INSERT clause handlers ----
                    //
                    // insertExtraIndents tracks pushSpecialIndent calls beyond the base +1
                    // that etINSERT handler already pushes.  Decrement on each popSpecialIndent.

                    // INTO inside an active Insert scope that is NOT a RETURNING INTO.
                    if (tk.token == etINTO)
                    {
                        auto* ins = lastScope(ScopeFrame::Insert);
                        if (ins && ins->insertLastClause != etRETURNING)
                        {
                            // ---- Simple INSERT (no ALL/FIRST): INTO stays on the INSERT line.
                            // The +1 base indent is already pushed.
                            // Just update the clause tracker and fall through to the normal
                            // pre-emit path so prevTk-based newline rules (e.g. after a
                            // --end-line hint comment) are applied correctly.
                            if (!ins->insertAllFirst)
                            {
                                ins->insertLastClause = etINTO;
                                // Fall through — do NOT continue or goto.
                            }
                            else
                            {
                                // ---- ALL/FIRST form: INTO goes on a new line.
                                // Pop any VALUES-level indent left over from the previous block.
                                if (ins->insertValPushed)
                                {
                                    stack.popSpecialIndent();
                                    --ins->insertExtraIndents;
                                    ins->insertValPushed = false;
                                }
                                // Each INTO/VALUES pair links independently — reset column brick state.
                                ins->insertBrickWrapPositions.clear();
                                ins->insertBrickColumnCount = 0;
                                // For unconditional ALL/FIRST: INTO is at the base +1 level.
                                // For conditional (WHEN/ELSE pushed +1 extra): INTO is at base+2.
                                // Either way, the current stack level is already correct — just newline.
                                stack.resetCurrentLineTokenCounter();
                                out.putEol();
                                out.putSpace(stack.getSpacer());
                                out.putToken(tk); // INTO
                                ins->insertLastClause = etINTO;
                                stack.incrementCurrentLineTokenCounter();
                                prevTk = tk;
                                prevNonCommentTk = tk;
                                continue;
                            }
                        }
                    }

                    // VALUES inside an Insert scope.
                    if (tk.token == etVALUES)
                    {
                        auto* ins = lastScope(ScopeFrame::Insert);
                        if (ins)
                        {
                            // Pop any prior VALUES indent before starting a new VALUES line.
                            if (ins->insertValPushed)
                            {
                                stack.popSpecialIndent();
                                --ins->insertExtraIndents;
                                ins->insertValPushed = false;
                            }
                            stack.resetCurrentLineTokenCounter();
                            out.putEol();
                            // ALL/FIRST form (conditional and unconditional): VALUES is at +1
                            // from the INTO level — push an extra indent.
                            if (ins->insertAllFirst)
                            {
                                stack.pushSpecialIndent();
                                ++ins->insertExtraIndents;
                                ins->insertValPushed = true;
                            }
                            out.putSpace(stack.getSpacer());
                            out.putToken(tk); // VALUES
                            stack.incrementCurrentLineTokenCounter();
                            ins->insertLastClause = etVALUES;
                            prevTk = tk;
                            prevNonCommentTk = tk;
                            continue;
                        }
                    }

                    // WHEN inside an Insert scope (conditional form — guards against CASE WHEN).
                    if (tk.token == etWHEN)
                    {
                        auto* ins = lastScope(ScopeFrame::Insert);
                        // Only fire when Insert is the innermost non-Case scope.
                        if (ins && (scopeStack.empty()
                            || scopeStack.back().kind != ScopeFrame::Case))
                        {
                            // Pop all extra indents to return to INSERT base+1 level.
                            while (ins->insertExtraIndents > 0)
                            {
                                stack.popSpecialIndent();
                                --ins->insertExtraIndents;
                            }
                            ins->insertValPushed  = false;
                            ins->insertIntoPushed = false;
                            ins->insertConditional = true;
                            ins->insertLastClause  = etWHEN;
                            stack.resetCurrentLineTokenCounter();
                            out.putEol();
                            out.putSpace(stack.getSpacer());
                            out.putToken(tk); // WHEN
                            // Push +1 so INTO under WHEN sits one level deeper.
                            stack.pushSpecialIndent();
                            ++ins->insertExtraIndents;
                            stack.incrementCurrentLineTokenCounter();
                            prevTk = tk;
                            prevNonCommentTk = tk;
                            continue;
                        }
                    }

                    // THEN inside an Insert conditional scope — stays on the WHEN line.
                    if (tk.token == etTHEN)
                    {
                        auto* ins = lastScope(ScopeFrame::Insert);
                        if (ins && ins->insertConditional
                            && (scopeStack.empty()
                                || scopeStack.back().kind != ScopeFrame::Case))
                        {
                            ins->insertLastClause = etTHEN;
                            // THEN is on the same line as the WHEN condition — emit with
                            // a single space and continue so the CASE THEN newline path
                            // in the shared pre-emit block is never reached.
                            out.putSpace();
                            out.putToken(tk); // THEN
                            stack.incrementCurrentLineTokenCounter();
                            prevTk = tk;
                            prevNonCommentTk = tk;
                            continue;
                        }
                    }

                    // ELSE inside an Insert conditional scope.
                    if (tk.token == etELSE)
                    {
                        auto* ins = lastScope(ScopeFrame::Insert);
                        if (ins && ins->insertConditional
                            && (scopeStack.empty()
                                || scopeStack.back().kind != ScopeFrame::Case))
                        {
                            // Pop all extra indents back to INSERT base+1 level.
                            while (ins->insertExtraIndents > 0)
                            {
                                stack.popSpecialIndent();
                                --ins->insertExtraIndents;
                            }
                            ins->insertValPushed  = false;
                            ins->insertIntoPushed = false;
                            ins->insertLastClause = etELSE;
                            stack.resetCurrentLineTokenCounter();
                            out.putEol();
                            out.putSpace(stack.getSpacer());
                            out.putToken(tk); // ELSE
                            // Push +1 so INTO under ELSE is at base+2.
                            stack.pushSpecialIndent();
                            ++ins->insertExtraIndents;
                            ins->insertIntoPushed = true;
                            stack.incrementCurrentLineTokenCounter();
                            prevTk = tk;
                            prevNonCommentTk = tk;
                            continue;
                        }
                    }

                    // ---- RETURNING (DELETE / UPDATE / INSERT) ----
                    // Handles all three layout paths (single-line, two-line, full-format)
                    // without touching the shared spacing/emit block below.
                    if (tk.token == etRETURNING
                        && (lastScope(ScopeFrame::Delete)
                            || lastScope(ScopeFrame::Update)
                            || lastScope(ScopeFrame::Insert)))
                    {
                        auto* del = lastScope(ScopeFrame::Delete);
                        auto* upd = lastScope(ScopeFrame::Update);
                        auto* ins = lastScope(ScopeFrame::Insert);
                        if (del)
                        {
                            // DELETE RETURNING: pop the WHERE body indent if one was pushed
                            // so RETURNING aligns with WHERE (+1 under DELETE).
                            // If no WHERE was seen (RETURNING right after table), don't pop.
                            if (del->deleteWhereIndentPushed)
                            {
                                stack.popSpecialIndent();
                                del->deleteWhereIndentPushed = false;
                            }
                            if (tryReturning(tokens, it, tk, /*skipPop=*/true)) continue;
                        }
                        if (upd)
                        {
                            // If SET body indent is still on the stack (no WHERE clause seen),
                            // pop the extra assignment-list indent before RETURNING.
                            if (upd->updateSetBodyPushed)
                            {
                                stack.popSpecialIndent(); // assignment-list indent
                                upd->updateSetBodyPushed = false;
                            }
                            upd->updateLastClause = etRETURNING;
                        }
                        if (ins)
                        {
                            // Pop any VALUES/INTO body indents still on the stack.
                            while (ins->insertExtraIndents > 0)
                            {
                                stack.popSpecialIndent();
                                --ins->insertExtraIndents;
                            }
                            ins->insertValPushed  = false;
                            ins->insertIntoPushed = false;
                            ins->insertLastClause = etRETURNING;
                            // tryReturning() calls popSpecialIndent() once to step back from
                            // the last clause level.  For INSERT the base +1 indent is still on
                            // the stack; we push one extra level so tryReturning's pop lands at
                            // +1 (same as VALUES), keeping RETURNING indented under INSERT.
                            stack.pushSpecialIndent();
                        }
                        if (tryReturning(tokens, it, tk)) continue;
                    }

                    // ---- INTO (full-format path only): self-contained handler ----
                    // Single/two-line paths consume INTO inside the RETURNING block above.
                    // This only fires when RETURNING took the full-format path.
                    // For UPDATE: only fire when the last clause keyword was RETURNING.
                    // For INSERT: only fire when the last clause keyword was RETURNING.
                    {
                        bool isDeleteInto = (tk.token == etINTO && lastScope(ScopeFrame::Delete));
                        auto* upd = lastScope(ScopeFrame::Update);
                        bool isUpdateReturningInto = (tk.token == etINTO && upd
                                                   && upd->updateLastClause == etRETURNING);
                        auto* ins = lastScope(ScopeFrame::Insert);
                        bool isInsertReturningInto = (tk.token == etINTO && ins
                                                   && ins->insertLastClause == etRETURNING);
                        if (isDeleteInto || isUpdateReturningInto || isInsertReturningInto)
                        {
                            stack.popSpecialIndent(); // col-list indent
                            out.putEol();
                            out.putSpace(stack.getSpacer());
                            out.putToken(tk); // INTO
                            out.putEol();
                            stack.pushSpecialIndent(); // var-list indent
                            prevTk = tk;
                            prevNonCommentTk = tk;
                            continue;
                        }
                    }

                    // ---- LOG ERRORS (UPDATE and INSERT scope) ----
                    if (tk.token == etLOG
                        && (lastScope(ScopeFrame::Update) || lastScope(ScopeFrame::Insert)))
                    {
                        auto* upd = lastScope(ScopeFrame::Update);
                        auto* ins = lastScope(ScopeFrame::Insert);
                        // If SET body indent is still on the stack (no WHERE before LOG),
                        // pop the extra assignment-list indent first.
                        if (upd && upd->updateSetBodyPushed)
                        {
                            stack.popSpecialIndent(); // assignment-list indent
                            upd->updateSetBodyPushed = false;
                        }
                        // For INSERT, pop all extra body indents if present.
                        if (ins)
                        {
                            while (ins->insertExtraIndents > 0)
                            {
                                stack.popSpecialIndent();
                                --ins->insertExtraIndents;
                            }
                            ins->insertValPushed  = false;
                            ins->insertIntoPushed = false;
                        }
                        stack.popSpecialIndent(); // pop active clause indent
                        out.putEol();
                        out.putSpace(stack.getSpacer());
                        out.putToken(tk); // LOG

                        // Measure the inline width from after LOG to the REJECT keyword (if any).
                        // If the LOG ERRORS … portion exceeds FMT_LOG_ERRORS_MAX_LEN, we will
                        // break before REJECT LIMIT on a new indented line.
                        // Reuse updateLogRejectBreak on Update, or insertLogRejectBreak via ins.
                        if (FMT_LOG_ERRORS_MAX_LEN > 0)
                        {
                            // Find the REJECT token index by scanning ahead.
                            int rejectIdx = -1;
                            for (auto si = it + 1; si != tokens.end(); ++si)
                            {
                                if (si->token == etSEMICOLON || si->token == etEOS) break;
                                if (si->token == etUNKNOWN
                                    && !_wcsicmp(si->value.c_str(), L"REJECT"))
                                {
                                    rejectIdx = (int)(si - tokens.begin());
                                    break;
                                }
                            }
                            if (rejectIdx > 0)
                            {
                                int width = (int)tk.value.size(); // "LOG"
                                OpenEditor::Token prev = tk;
                                bool tainted = false;
                                auto endScan = tokens.begin() + rejectIdx;
                                for (auto si = it + 1; si != endScan; ++si)
                                {
                                    if (si->token == etEOL) continue;
                                    if (si->token == etCOMMENT) { tainted = true; break; }
                                    if (prev.token != etNONE
                                        && prev.token != etLEFT_ROUND_BRACKET
                                        && si->token != etRIGHT_ROUND_BRACKET
                                        && si->token != etCOMMA)
                                        width += 1;
                                    width += (int)si->value.size();
                                    prev = *si;
                                }
                                bool doBreak = !tainted && (width > FMT_LOG_ERRORS_MAX_LEN);
                                if (upd) upd->updateLogRejectBreak = doBreak;
                                if (ins) ins->insertLogRejectBreak  = doBreak;
                            }
                        }

                        // LOG ERRORS INTO ... stays on the same line; push indent for the body.
                        stack.pushSpecialIndent();
                        stack.incrementCurrentLineTokenCounter();
                        if (upd) upd->updateLastClause  = etLOG;
                        if (ins) ins->insertLastClause   = etLOG;
                        prevTk = tk;
                        prevNonCommentTk = tk;
                        continue;
                    }

                    // ---- REJECT LIMIT break (UPDATE or INSERT LOG ERRORS scope) ----
                    if (tk.token == etUNKNOWN
                        && !_wcsicmp(tk.value.c_str(), L"REJECT"))
                    {
                        auto* upd = lastScope(ScopeFrame::Update);
                        auto* ins = lastScope(ScopeFrame::Insert);
                        bool doBreak = (upd && upd->updateLogRejectBreak && upd->updateLastClause == etLOG)
                                    || (ins && ins->insertLogRejectBreak  && ins->insertLastClause  == etLOG);
                        if (doBreak)
                        {
                            out.putEol();
                            out.putSpace(stack.getSpacer());
                            out.putToken(tk); // REJECT
                            stack.incrementCurrentLineTokenCounter();
                            prevTk = tk;
                            prevNonCommentTk = tk;
                            continue;
                        }
                    }

                    // ---- whole CASE…END on one line ----
                    if (tk.token == etCASE)
                    {
                        if (tryCaseOneLine(tokens, it)) continue;

                        // Multi-line CASE after '=' inside an UPDATE SET body:
                        // break onto a new indented line (assignment-body + 1).
                        if (prevNonCommentTk.token == etEQUAL)
                        {
                            auto* upd = lastScope(ScopeFrame::Update);
                            if (upd && upd->updateSetBodyPushed)
                            {
                                out.putEol();               // break after '='
                                stack.pushSpecialIndent();  // extra indent for CASE block
                                upd->updateCasePendingExtraIndent = true;
                            }
                        }
                    }

                    {
                        /*
                        adding EOL and decreasing INDENT here
                        */
                        if (tk.token == etRIGHT_ROUND_BRACKET)
                        {
                            stack.popBlockIndent();
                            out.putEol();
                        }
                        else if (tk.token == etEND)
                        {
                            // pop THEN indent only if the last WHEN's THEN actually pushed it
                            ScopeFrame* caseCtx = lastScope(ScopeFrame::Case);
                            if (caseCtx && caseCtx->caseThenPushed)
                                stack.popSpecialIndent(); // for THEN
                            stack.popSpecialIndent(); // for CASE itself
                            if (caseCtx && caseCtx->caseExtraIndentPushed)
                            {
                                // Stack is now at the extra-indent level — the same column
                                // as the CASE keyword.  Emit END there, then pop the extra
                                // indent so the next token (comma, WHERE, …) returns to
                                // the assignment-body level.
                                popScope(ScopeFrame::Case);
                                out.putEol();
                                out.putSpace(stack.getSpacer());
                                out.putToken(tk);               // END
                                stack.popSpecialIndent();       // back to assignment-body
                                stack.incrementCurrentLineTokenCounter();
                                prevTk = tk;
                                prevNonCommentTk = tk;
                                continue;
                            }
                            popScope(ScopeFrame::Case);
                            out.putEol();
                        }
                        else if (tk.token == etFROM
                            || tk.token == etWHERE
                            || tk.token == etHAVING
                            || (tk.token == etGROUP)
                            || (tk.token == etORDER)
                            || (tk.token == etEND)
                            || (tk.token == etINTO && !scopeStack.empty()
                                && scopeStack.back().kind == ScopeFrame::Select
                                && !scopeStack.back().selectHasInto)
                            )
                        {
                            // etFROM inside a DELETE scope is the optional "DELETE FROM table"
                            // keyword — it stays on the same line, no indent change.
                            // Only applies when DELETE is the innermost scope; if a SELECT has been
                            // pushed on top (subquery), treat FROM normally.
                            // Exception: if the preceding token was an end-line comment (hint),
                            // FROM starts on the next line at +1, and all subsequent clauses
                            // (WHERE, RETURNING, …) move to +2 via an extra pushSpecialIndent.
                            if (tk.token == etFROM
                                && !scopeStack.empty()
                                && scopeStack.back().kind == ScopeFrame::Delete)
                            {
                                if (is_end_line_comment(prevTk))
                                {
                                    stack.resetCurrentLineTokenCounter();
                                    out.putEol();
                                }
                                // further handling (incrementCurrentLineTokenCounter) is in the post-emit block
                            }
                            // etWHERE / etRETURNING / etLOG inside a DELETE scope:
                            // do NOT pop the base +1 indent — just emit a newline so the
                            // keyword sits at +1 (under DELETE).  The post-emit block then
                            // pushes +1 for the clause body, giving body content at +2.
                            else if ((tk.token == etWHERE || tk.token == etRETURNING || tk.token == etLOG)
                                && !scopeStack.empty()
                                && scopeStack.back().kind == ScopeFrame::Delete)
                            {
                                stack.resetCurrentLineTokenCounter();
                                out.putEol();
                            }
                            // etWHERE inside an UPDATE scope: pop assignment-list indent
                            else if (tk.token == etWHERE
                                && !scopeStack.empty()
                                && scopeStack.back().kind == ScopeFrame::Update)
                            {
                                auto* upd = lastScope(ScopeFrame::Update);
                                if (upd && upd->updateSetBodyPushed)
                                {
                                    stack.popSpecialIndent(); // pop assignment-list indent
                                    upd->updateSetBodyPushed = false;
                                }
                                if (upd) upd->updateLastClause = etWHERE;
                                out.putEol();
                            }
                            else
                            {
                                if (tk.token == etFROM && FMT_ALIGN_COLUMN_ALIASES)
                                {
                                    auto* _sel = lastScope(ScopeFrame::Select);
                                    if (_sel && _sel->selectOutPos > -1)
                                        alignList(out, _sel->selectOutPos, (int)stack.top().spacer.size());
                                }

                                stack.popSpecialIndent();
                                out.putEol();
                            }
                        }
                        else if (tk.token == etUNION
                            || tk.token == etINTERSECT
                            || tk.token == etMINUS_SQL
                            )
                        {
                            // Set operator between two SELECT branches.
                            // Pop the last clause indent of the preceding SELECT (ORDER BY,
                            // WHERE, FROM, etc.) so the operator lands at the same level
                            // as the SELECT above it, then emit a blank separator line.
                            //
                            // Before destroying the ScopeFrame, persist its brick wrap
                            // positions into the side table so the next branch can mirror them.
                            {
                                auto* _sel = lastScope(ScopeFrame::Select);
                                if (_sel && _sel->selectBrickColumnCount > 0)
                                {
                                    auto& state = setOpBrickByDepth[_sel->pushedAtParenDepth];
                                    if (state.columnCount == 0) // first branch sets the reference
                                    {
                                        state.wrapPositions = _sel->selectBrickWrapCommas;
                                        state.columnCount   = _sel->selectBrickColumnCount;
                                    }
                                }
                            }
                            popScope(ScopeFrame::Select);
                            stack.popSpecialIndent();
                            stack.resetCurrentLineTokenCounter();
                            out.putEol(); // end the last line of the preceding SELECT
                            if (FMT_SET_OP_BLANK_LINE)
                                out.putEol(); // blank line before the set operator
                        }
                        else if (tk.token == etJOIN
                            || is_join_qualifier(tk)
                            )
                        {
                            if (is_join_qualifier(tk) && !is_join_qualifier(prevTk))
                            {
                                // first qualifier token (e.g. LEFT, INNER): start new line here
                                stack.popSpecialIndent();
                                stack.resetCurrentLineTokenCounter();
                                out.putEol();
                            }
                            else if (tk.token == etJOIN && !is_join_qualifier(prevTk))
                            {
                                // bare JOIN (no qualifier): start new line here
                                stack.popSpecialIndent();
                                stack.resetCurrentLineTokenCounter();
                                out.putEol();
                            }
                            // JOIN after a qualifier (e.g. INNER JOIN): stays on the same line,
                            // space before JOIN is handled by the general spacing rules below
                        }
                        else if ((tk.token == etSELECT || tk.token == etWITH)
                            && stack.getCurrentLineTokenCounter() > 0)
                        {
                            // SELECT / WITH-CTE that is not the first token on the current line
                            // (e.g. after the closing ) of a CTE, or after UNION/INTERSECT/MINUS,
                            // or the trailing subquery of an INSERT ALL/FIRST, or a CTE in INSERT)
                            // must start on its own line.
                            // For INSERT ALL/FIRST: pop any VALUES/INTO body indent first.
                            {
                                auto* ins = lastScope(ScopeFrame::Insert);
                                if (ins && ins->insertExtraIndents > 0)
                                {
                                    while (ins->insertExtraIndents > 0)
                                    {
                                        stack.popSpecialIndent();
                                        --ins->insertExtraIndents;
                                    }
                                    ins->insertValPushed  = false;
                                    ins->insertIntoPushed = false;
                                }
                            }
                            stack.resetCurrentLineTokenCounter();
                            out.putEol();
                        }
                        else if (tk.token == etSET
                            && !scopeStack.empty()
                            && scopeStack.back().kind == ScopeFrame::Update)
                        {
                            // SET clause keyword inside UPDATE: emit EOL.
                            // Reset the counter first so the spacing block uses
                            // the full indentation spacer, not a single space.
                            stack.resetCurrentLineTokenCounter();
                            out.putEol();
                        }
                        else if (tk.token == etWHEN
                            || tk.token == etELSE
                            )
                        {
                            ScopeFrame* caseCtx = lastScope(ScopeFrame::Case);
                            bool isFirst = caseCtx && !caseCtx->caseFirstWhenSeen;
                            if (!isFirst && caseCtx && caseCtx->caseThenPushed)
                                stack.popSpecialIndent(false);
                            if (caseCtx)
                            {
                                caseCtx->caseFirstWhenSeen = true;
                                caseCtx->caseThenPushed = false;
                            }

                            stack.resetCurrentLineTokenCounter();
                            out.putEol();

                            // Short WHEN: keep "WHEN expr THEN value" on one line when it fits.
                            if (tk.token == etWHEN)
                            {
                                if (tryWhenShortLine(tokens, it, tk)) continue;
                            }
                        }
                        else if (tk.token == etTHEN)
                        {
                            stack.pushSpecialIndent();
                            out.putEol();
                            if (auto* c = lastScope(ScopeFrame::Case)) c->caseThenPushed = true;
                        }
                        else if ((tk.token == etAND
                            && betweenStack.empty())
                            || tk.token == etOR
                            || tk.token == etON
                            || tk.token == etUSING
                            || prevTk.token == etCOMMA
                            || (prevTk.token == etCOMMENT
                                // Suppress the newline after a block hint comment (/*+...*/) in
                                // an UPDATE or DELETE header so the table name stays on the same line.
                                // End-line comments still force a newline as usual.
                                && !(   !is_end_line_comment(prevTk)
                                     && !scopeStack.empty()
                                     && (scopeStack.back().kind == ScopeFrame::Update
                                         || scopeStack.back().kind == ScopeFrame::Delete
                                         || scopeStack.back().kind == ScopeFrame::Insert)))
                            // A block comment that was on its own source line (preceded by EOL
                            // in the raw token stream) must start on its own output line.
                            // This covers /* ... */ comments between INTO/VALUES blocks in
                            // INSERT ALL, between SET assignments in UPDATE, etc.
                            || (tk.token == etCOMMENT
                                && !is_end_line_comment(tk)
                                && [&]() -> bool {
                                    // scan backward past whitespace to find EOL before this comment
                                    for (auto r = it; r != tokens.begin(); )
                                    {
                                        --r;
                                        if (r->token == etEOL)      return true;
                                        if (r->token == etWHITESPACE) continue;
                                        return false;
                                    }
                                    return false;
                                }())
                            || (prevTk.token == etTHEN && tk.token == etCASE)
                            || (prevTk.token == etELSE && tk.token == etCASE)
                            || (prevTk.token == etCASE && tk.token == etCASE)
                            || (tk.token == etCOMMENT && tk.value.find(L'\n') != wstring::npos) // multiline comments
                            //comma in front || tk.token == etCOMMA
                            )
                        {
                            // Suppress the newline before an end-line comment so it stays
                            // on the same line as the preceding token (e.g. after a comma).
                            // Exception: prevTk is itself a comment — a second consecutive
                            // end-line comment must start on its own new line.
                            if (is_end_line_comment(tk) && !is_end_line_comment(prevTk))
                            {
                                // end-line comment: keep on the current line, spacing logic adds a space
                            }
                            else
                            {
                                stack.resetCurrentLineTokenCounter();
                                out.putEol();
                            }
                        }
                    }

                    if (stack.getCurrentLineTokenCounter() == 0)
                    {
                        out.putSpace(stack.getSpacer());
                    }
                    else
                    {
                        // non-breackable
                        if (prevTk.token != etNONE
                            && prevTk.token != etDOT
                            && tk.token != etDOT
                            && prevTk.token != etLEFT_ROUND_BRACKET
                            && tk.token != etLEFT_ROUND_BRACKET
                            && tk.token != etRIGHT_ROUND_BRACKET
                            && prevTk.token != etCOLON // for SQL only
                            && tk.token != etCOMMA
                            && tk.token != etSEMICOLON
                            )
                            out.putSpace();
                        else if (prevTk.reserved != 0 && tk.token == etLEFT_ROUND_BRACKET)
                            out.putSpace();
                        else if (tk.token == etLEFT_ROUND_BRACKET
                            && (prevTk.token == etJOIN || is_join_qualifier(prevTk)))
                            out.putSpace(); // space between JOIN/qualifier and opening subquery bracket
                        else if (tk.token == etLEFT_ROUND_BRACKET && is_comparison_opt(prevTk)) // my
                            out.putSpace();
                    }

                    if (tk.token == etCOMMENT)
                    {
                        align_comment(tk, stack.getSpacer());
                        out.putToken(tk);
                    }
                    else
                        out.putToken(tk);

                    stack.incrementCurrentLineTokenCounter();

                    {
                        /*
                        increasing INDENT here
                        */
                        if (tk.token == etLEFT_ROUND_BRACKET)
                        {
                            ++parenDepth; // count the '(' now; continue below skips the normal tracking
                            // Each new subparen resets the set-op brick state at this depth so
                            // independent compound subqueries at the same nesting level are isolated.
                            setOpBrickByDepth.erase(parenDepth);
                            if (tryShortParen(tokens, it, tk)) { --parenDepth; continue; }
                            if (tryInsertBrickList(tokens, it, tk)) { --parenDepth; continue; }
                            stack.pushBlockIndent();
                            out.putEol();
                            prevTk = tk;
                            prevNonCommentTk = tk;
                            continue; // parenDepth already incremented above; skip bottom-of-loop tracking
                        }
                        else if (tk.token == etDELETE)
                        {
                            // Push DELETE scope frame — same pattern as SELECT.
                            // Guard: not inside a Merge scope (there DELETE is a sub-clause).
                            if (!lastScope(ScopeFrame::Merge))
                            {
                                int curIdx = (int)(it - tokens.begin());
                                ScopeFrame df;
                                df.kind               = ScopeFrame::Delete;
                                df.pushedAtParenDepth = parenDepth;
                                df.node               = findInTree(tree, curIdx);
                                scopeStack.push_back(df);
                                stack.pushSpecialIndent();
                                // DELETE, optional hint/FROM, table name all stay on same line.
                                stack.incrementCurrentLineTokenCounter();
                            }
                        }
                        else if (tk.token == etUPDATE)
                        {
                            // Push UPDATE scope frame — same pattern as DELETE/SELECT.
                            // Guard: not inside a Merge scope (there UPDATE is a sub-clause).
                            if (!lastScope(ScopeFrame::Merge))
                            {
                                int curIdx = (int)(it - tokens.begin());
                                ScopeFrame uf;
                                uf.kind               = ScopeFrame::Update;
                                uf.pushedAtParenDepth = parenDepth;
                                uf.node               = findInTree(tree, curIdx);
                                scopeStack.push_back(uf);
                                stack.pushSpecialIndent();
                                // UPDATE, hint, table name, alias all stay on same line.
                                stack.incrementCurrentLineTokenCounter();
                            }
                        }
                        else if (tk.token == etINSERT)
                        {
                            // Guard: not inside a Merge scope (there INSERT is a sub-clause).
                            if (!lastScope(ScopeFrame::Merge))
                            {
                            int curIdx = (int)(it - tokens.begin());
                            ScopeFrame inf;
                            inf.kind               = ScopeFrame::Insert;
                            inf.pushedAtParenDepth = parenDepth;
                            inf.node               = findInTree(tree, curIdx);
                            // Peek ahead (skipping whitespace/EOL) to detect:
                            //   - end-line hint   → INTO must go on next line
                            //   - ALL / FIRST     → multi-table form
                            {
                                auto peek = it + 1;
                                while (peek != tokens.end()
                                    && (peek->token == etEOL || peek->token == etWHITESPACE))
                                    ++peek;
                                if (peek != tokens.end())
                                {
                                    if (peek->token == etCOMMENT
                                        && is_end_line_comment(*peek))
                                        inf.insertHintNewline = true;
                                    else if (peek->token == etALL
                                        || (peek->token == etUNKNOWN
                                            && !_wcsicmp(peek->value.c_str(), L"FIRST")))
                                        inf.insertAllFirst = true;
                                }
                            }
                            scopeStack.push_back(inf);
                            stack.pushSpecialIndent();
                            // INSERT + block hint stay on same line; end-line hint forces
                            // INTO to the next line (insertHintNewline flag).
                            stack.incrementCurrentLineTokenCounter();
                            } // end guard: not inside Merge
                        }
                        else if (tk.token == etMERGE)
                        {
                            // Push MERGE scope frame.
                            int curIdx = (int)(it - tokens.begin());
                            ScopeFrame mf;
                            mf.kind               = ScopeFrame::Merge;
                            mf.pushedAtParenDepth = parenDepth;
                            mf.node               = findInTree(tree, curIdx);
                            scopeStack.push_back(mf);
                            stack.pushSpecialIndent();
                            // MERGE, hint, INTO, table, alias all stay on the same line.
                            stack.incrementCurrentLineTokenCounter();
                        }
                        else if (tk.token == etSET
                            && !scopeStack.empty()
                            && scopeStack.back().kind == ScopeFrame::Update)
                        {
                            // Push one indent for the assignment list body.
                            // SET sits at UPDATE+1; assignments are at UPDATE+2.
                            stack.pushSpecialIndent();
                            out.putEol();
                            auto* upd = lastScope(ScopeFrame::Update);
                            if (upd)
                            {
                                upd->updateSetBodyPushed = true;
                                upd->updateLastClause    = etSET;
                                // Record the output position and indent size of the first
                                // assignment line so alignSetAssignments can self-terminate.
                                upd->updateSetOutPos  = out.position();
                                upd->updateSetIndent  = stack.getSpacer().size();
                            }
                        }
                        else if (tk.token == etSELECT)
                        {
                            // Push SELECT runtime entry — pointer into tree + output-side state.
                            int curIdx = (int)(it - tokens.begin());
                            ScopeFrame sr;
                            sr.kind = ScopeFrame::Select;
                            sr.pushedAtParenDepth = parenDepth;
                            sr.node = findInTree(tree, curIdx);
                            scopeStack.push_back(sr);
                            auto& sel = scopeStack.back();

                            if (trySelectOneLine(tokens, it, sel, tk)) continue;

                            stack.pushSpecialIndent();
                            if (trySelectShortList(tokens, it, sel, tk)) continue;
                            if (trySelectBrickList(tokens, it, sel, tk)) continue;

                            // Long column list: check for hint comment after SELECT
                            auto nextIt = it + 1;
                            while (nextIt != tokens.end() && nextIt->token == etEOL)
                                ++nextIt;
                            bool nextIsComment = (nextIt != tokens.end()
                                && nextIt->token == etCOMMENT);
                            if (nextIsComment)
                            {
                                stack.incrementCurrentLineTokenCounter();
                                auto step_back = find_eol_backward(out);
                                sel.selectOutPos = out.position() - step_back;
                            }
                            else
                            {
                                out.putEol();
                                sel.selectOutPos = out.position();
                            }
                        }
                        else if (tk.token == etFROM
                            || tk.token == etJOIN
                            || is_join_qualifier(tk)
                            || tk.token == etWHERE
                            || tk.token == etHAVING
                            || (tk.token == etBY && prevTk.token == etGROUP)
                            || (tk.token == etBY && prevTk.token == etORDER)
                            || (tk.token == etINTO && !scopeStack.empty()
                                && scopeStack.back().kind == ScopeFrame::Select
                                && !scopeStack.back().selectHasInto)
                            )
                        {
                            // etFROM inside a DELETE scope is the optional "DELETE FROM table"
                            // keyword — keep on same line, no indent change.
                            // Only applies when DELETE is the innermost scope (not a subquery SELECT on top).
                            if (tk.token == etFROM
                                && !scopeStack.empty()
                                && scopeStack.back().kind == ScopeFrame::Delete)
                            {
                                stack.incrementCurrentLineTokenCounter();
                            }
                            // etWHERE / etRETURNING / etLOG inside a DELETE scope:
                            // push the clause-body indent without popping first (pre-emit already
                            // emitted the newline at the current +1 level).
                            else if ((tk.token == etWHERE || tk.token == etRETURNING || tk.token == etLOG)
                                && !scopeStack.empty()
                                && scopeStack.back().kind == ScopeFrame::Delete)
                            {
                                stack.pushSpecialIndent();
                                stack.incrementCurrentLineTokenCounter();
                                if (tk.token == etWHERE)
                                {
                                    auto* del = lastScope(ScopeFrame::Delete);
                                    if (del) del->deleteWhereIndentPushed = true;
                                }
                            }
                            else
                            {
                            // Only push indent for the real JOIN keyword or other clause starters,
                            // not for qualifier tokens that precede JOIN (INNER, LEFT, RIGHT, FULL, NATURAL)
                            if (!is_join_qualifier(tk))
                                stack.pushSpecialIndent();

                            // Mark SELECT INTO as handled so subsequent tokens don't re-trigger.
                            if (tk.token == etINTO && !scopeStack.empty()
                                && scopeStack.back().kind == ScopeFrame::Select)
                            {
                                scopeStack.back().selectHasInto   = true;
                                scopeStack.back().selectIntoSrcIdx = (int)(it - tokens.begin());
                            }

                            // record clause start position in the active SELECT context (output-side is unused now)
                            // source-side measurements already on keywords ClauseEntry

                            if (tk.token == etFROM
                                && FMT_FROM_SHORT_LIST_ENABLED)
                            {
                                if (tryFromShortList(tokens, it, tk)) continue;
                            }
                            else if (FMT_GROUPBY_SHORT_LIST_ENABLED
                                && (tk.token == etBY && prevTk.token == etGROUP))
                            {
                                if (tryGroupByShortList(tokens, it, tk)) continue;
                            }
                            else if (FMT_ORDERBY_SHORT_LIST_ENABLED
                                && (tk.token == etBY && prevTk.token == etORDER))
                            {
                                if (tryOrderByShortList(tokens, it, tk)) continue;
                            }
                            if (new_line_after(tk))
                                out.putEol();
                            else
                                stack.incrementCurrentLineTokenCounter(); // to suppress indent whitespaces
                            } // end else (not DELETE FROM / DELETE WHERE)
                        }
                        else if (tk.token == etCASE
                            || tk.token == etELSE
                            )
                        {
                            stack.pushSpecialIndent(tk.token == etCASE);
                            if (tk.token == etCASE)
                            {
                                int curIdx = (int)(it - tokens.begin());
                                ScopeFrame cr;
                                cr.kind = ScopeFrame::Case;
                                cr.node = findInTree(tree, curIdx);
                                cr.pushedAtParenDepth = parenDepth;
                                // Transfer pending extra-indent flag from the Update frame.
                                auto* upd = lastScope(ScopeFrame::Update);
                                if (upd && upd->updateCasePendingExtraIndent)
                                {
                                    cr.caseExtraIndentPushed = true;
                                    upd->updateCasePendingExtraIndent = false;
                                }
                                scopeStack.push_back(cr);
                                // Keep the expression on the same line as CASE (single space).
                                // The first WHEN/ELSE will reset the counter and emit a newline.
                                stack.incrementCurrentLineTokenCounter();
                            }
                            else // etELSE: mark active CASE so END knows THEN indent was pushed
                            {
                                if (auto* c = lastScope(ScopeFrame::Case)) c->caseThenPushed = true;
                            }
                        }
                    }

                    // track paren depth for all brackets in the stream
                    if (tk.token == etLEFT_ROUND_BRACKET)
                        ++parenDepth;
                    else if (tk.token == etRIGHT_ROUND_BRACKET && parenDepth > 0)
                    {
                        --parenDepth;
                        // pop all context entries pushed inside the now-closed paren
                        while (!scopeStack.empty() && scopeStack.back().pushedAtParenDepth > parenDepth)
                            scopeStack.pop_back();
                    }

                    // track BETWEEN...AND so AND does not get a spurious newline
                    if (tk.token == etUNKNOWN
                        && !_wcsicmp(tk.value.c_str(), L"BETWEEN"))
                        betweenStack.push_back(parenDepth);
                    else if (tk.token == etAND && !betweenStack.empty()
                        && betweenStack.back() == parenDepth)
                        betweenStack.pop_back();

                    prevTk = tk;
                    if (tk.token != etCOMMENT)
                        prevNonCommentTk = tk;

                    // After ; end of statement:
                    // reset indent and per-statement state
                    if (tk.token == etSEMICOLON)
                    {
                        // flush SET assignment alignment before comment alignment
                        flushSetAlignment();

                        // align end-line comments across the whole statement
                        out.alignStatementComments(stmtBookmark);
                        out.alignStatementBlockComments(stmtBookmark);

                        out.putEol(); // end the semicolon's line

                        // peek ahead: skip EOL tokens to find the next real token
                        auto nextIt = it + 1;
                        while (nextIt != tokens.end() && nextIt->token == etEOL)
                            ++nextIt;
                        bool nextIsSlash = (nextIt != tokens.end()
                            && nextIt->token == etEOS);
                        if (!nextIsSlash)
                            out.putEol(); // blank separator before next statement

                        resetStatementState();
                    }
                }
            }

            // End of token stream: flush any pending SET alignment and comment
            // alignment for a statement that had no ; or / terminator.
            flushSetAlignment();
            out.alignStatementComments(stmtBookmark);
            out.alignStatementBlockComments(stmtBookmark);

            out.flush(ostr);
        }

        wstring getResult()
        {
            return ostr.str();
        }
    };

    wstring formatSQL (std::vector<OpenEditor::Token>& tokens, const Settings& s)
    {
        SQLFormatter formatter(tokens, s);
        return formatter.getResult();
    }


} // namespace OpenEditor
