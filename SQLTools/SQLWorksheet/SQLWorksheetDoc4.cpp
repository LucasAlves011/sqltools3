/*
    SQLTools is a tool for Oracle database developers and DBAs.
    Copyright (C) 1997-2025 Aleksey Kochetov

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
#include "SQLWorksheetDoc.h"
#include "SQLWorksheet/PlsSqlParser.h"
#include <utility>

// Forward declaration for GetSimpleFromTable
struct AliasScope;
struct ParseContext;

static std::wstring GetSingleTableFromSimpleSubquery(const std::vector<Token>& tokens, const std::vector<int>& tokenContext, const std::vector<ParseContext>& contextTree, const ParseContext& ctx);
static void GetTableName(const std::vector<Token>& tokens, size_t& i, int currentContext, std::vector<int>& tokenContext, std::vector<ParseContext>& contextTree);

// Helper function for recursive CTE resolution
static std::wstring ResolveCteTable(const std::wstring& cteName, int beforeTokenIdx, int ctx, const std::vector<ParseContext>& contextTree);

// Forward declarations for helpers used in resolution and column extraction
static std::vector<std::wstring> CollectSelectColumns(const std::vector<Token>& tokens, const std::vector<int>& tokenContext, int ctxIdx);
static std::vector<std::wstring> TryGetColumnsForAlias(const AliasScope& aliasScope, int ctxIdxToSearchFrom, const std::vector<ParseContext>& contextTree, const std::vector<Token>& tokens, const std::vector<int>& tokenContext);

// StatementType and ClauseType enums
enum StatementType { STMT_UNKNOWN, STMT_SELECT, STMT_INSERT, STMT_UPDATE, STMT_DELETE, STMT_MERGE };
// unified ClauseType enum (removed earlier duplicate) adding INTO/USING
enum ClauseType { CLAUSE_NONE, CLAUSE_COLUMN_LIST, CLAUSE_FROM, CLAUSE_WHERE, CLAUSE_GROUP_BY, CLAUSE_ORDER_BY, CLAUSE_WITH, CLAUSE_SET, CLAUSE_INTO, CLAUSE_USING };
// AliasScope and ParseContext structs
struct AliasScope { std::wstring alias; std::wstring table; int tokenIdx; int ctxIdx; std::vector<std::wstring> columns; };
struct ParseContext {
    ClauseType clause = CLAUSE_NONE;
    StatementType stmtType = STMT_UNKNOWN;
    std::vector<AliasScope> aliases; // regular table/subquery aliases
    std::vector<AliasScope> ctes;    // CTE definitions (scoped to this context)
    int parent = -1; // index of parent context, -1 for root
    int bracketCount = 0; // for subquery context
    bool isSubquery = false; // true only for (SELECT ...) subqueries
    std::wstring pendingCteName; // holds CTE name until subquery closes
    std::vector<std::wstring> pendingCteColumns; // holds CTE explicit column list, if specified
};

/*
* todo: 
    0) * check if pos should be converted from screen pos to index
    1) * move the cursor to the left if if it is on ",;()" after the alias
    2) * handle joins
    3) * handle quoted identifiers and schema names
    4) * handle subqueries in the FROM clause
	5) * add stack for nested queries
    6) * handle CTEs (WITH ... AS ...)
	7) * handle explicit list of columns after SELECT
    8) * handle DELETE, MERGE, UPDATE
	9)  handle INSERT and INSERT ALL
	10) make sure that SELECT ... INTO works
*/

// Extracts raw editor text for a subquery given the index of its ParseContext.
// The ( immediately precedes the first token of that context; we scan for the
// matching ) by tracking bracket depth across all tokens from that point.
static std::string ExtractSubqueryText(
    COEditorView* pEditor,
    const std::vector<Token>& tokens,
    const std::vector<int>& tokenContext,
    int ctxIdx)
{
    // Find the first token that belongs to the subquery context (the SELECT)
    int selectTokIdx = -1;
    for (int i = 0; i < (int)tokens.size(); ++i)
        if (tokenContext[i] == ctxIdx) { selectTokIdx = i; break; }
    if (selectTokIdx <= 0) return {};

    // The opening ( is exactly one token before SELECT (in the parent context)
    int openIdx = selectTokIdx - 1;
    if (tokens[openIdx].token != etLEFT_ROUND_BRACKET) return {};

    // Find the matching closing ) by tracking all bracket depth
    int closeIdx = -1;
    int depth = 1;
    for (int i = selectTokIdx; i < (int)tokens.size(); ++i)
    {
        if (tokens[i].token == etLEFT_ROUND_BRACKET)  { ++depth; continue; }
        if (tokens[i].token == etRIGHT_ROUND_BRACKET) { --depth; if (depth == 0) { closeIdx = i; break; } }
    }
    if (closeIdx < 0) return {};

    // Extract raw text from the editor: from ( (inclusive) to ) (inclusive)
    const Token& openTok  = tokens[openIdx];
    const Token& closeTok = tokens[closeIdx];

    std::string result;
    for (int l = openTok.line; l <= closeTok.line; ++l)
    {
        Common::OEStringW wbuff;
        pEditor->GetLineW(l, wbuff);
        int begin = (l == openTok.line)  ? openTok.offset  : 0;
        int end   = (l == closeTok.line) ? closeTok.offset + 1 : (int)wbuff.length();
        if (begin < end)
            result += Common::str(wbuff.data() + begin, end - begin);
        if (l < closeTok.line)
            result += '\n';
    }
    return result;
}

// New API: returns alias, table and/or extracted column list
SqlAliasResolution ResolveSqlAlias(COEditorView* pEditor, int line, int pos, std::string* outSubquery, bool singleTableFromSimpleSubquery)
{
    SqlAliasResolution result; // alias, table, columns

    // Backward scan for statement start
    int startLine = line;
    int startPos = 0;
    for (; startLine >= 0; --startLine)
    {
        Common::OEStringW wbuff;
        pEditor->GetLineW(startLine, wbuff);
        const wchar_t* data = wbuff.data();
        int len = wbuff.length();
        for (int i = 0; i < len; ++i)
        {
            if (data[i] == L';')
            {
                if (startLine == line && i >= pos)
                    continue;

                else if (startLine == line && i < pos)
                {
                    startPos = i + 1;
                    startLine = line;
                }
                else if (startLine < line)
                {
                    startLine++;
                    startPos = 0;
                }
                goto backward_done;
            }
        }
        int j = 0;
        while (j < len && iswspace(data[j])) ++j;
        if (j < len && data[j] == L'/')
        {
            startLine++;
            startPos = 0;
            goto backward_done;
        }
    }
    startLine = 0;
    startPos = 0;
backward_done:

    // Forward scan for statement end
    int endLine = line;
    int endPos = pos;
    int nlines = pEditor->GetLineCount();
    for (; endLine < nlines; ++endLine)
    {
        Common::OEStringW wbuff;
        pEditor->GetLineW(endLine, wbuff);
        const wchar_t* data = wbuff.data();
        int len = wbuff.length();
        for (int i = 0; i < len; ++i)
        {
            if (data[i] == L';')
            {
                if (endLine == line && i > pos)
                {
                    endLine = line;
                    endPos = i;
                }
                else if (endLine > line)
                {
                    endPos = i;
                }
                goto forward_done;
            }
        }
        int j = 0;
        while (j < len && iswspace(data[j])) ++j;
        if (j < len && data[j] == L'/')
        {
            endLine--;
            if (endLine < line) {
                endLine = line;
                endPos = pos;
            } else {
                endPos = INT_MAX;
            }
            goto forward_done;
        }
    }
    endLine = nlines - 1;
    endPos = INT_MAX;
forward_done:

    // Create a minimal TokenMap for the parser
    TokenMapPtr tokenMap(new TokenMap);
    tokenMap->insert(std::make_pair("SELECT", etSELECT));
    tokenMap->insert(std::make_pair("INSERT", etINSERT));
    tokenMap->insert(std::make_pair("UPDATE", etUPDATE));
    tokenMap->insert(std::make_pair("DELETE", etDELETE));
    tokenMap->insert(std::make_pair("MERGE",  etMERGE));
    tokenMap->insert(std::make_pair("FROM", etFROM));
    tokenMap->insert(std::make_pair("(", etLEFT_ROUND_BRACKET));
    tokenMap->insert(std::make_pair(")", etRIGHT_ROUND_BRACKET));
    tokenMap->insert(std::make_pair(".", etDOT));
    tokenMap->insert(std::make_pair(",", etCOMMA));
    tokenMap->insert(std::make_pair("AS", etAS));
	tokenMap->insert(std::make_pair("JOIN", etJOIN));
    tokenMap->insert(std::make_pair("\"", etDOUBLE_QUOTE));
    tokenMap->insert(std::make_pair("*", etSTAR));
    tokenMap->insert(std::make_pair("WITH", etWITH));
    tokenMap->insert(std::make_pair("UNION", etUNION));
    tokenMap->insert(std::make_pair("INTERSECT", etINTERSECT));
    tokenMap->insert(std::make_pair("ALL", etALL));
    tokenMap->insert(std::make_pair("MINUS", etMINUS));
    tokenMap->insert(std::make_pair("SET", etSET)); // <-- Added SET token
    tokenMap->insert(std::make_pair("INTO", etINTO));
    tokenMap->insert(std::make_pair("USING", etUSING));
    tokenMap->insert(std::make_pair("/", etSLASH));  // needed for /*hint*/ comment recognition
    tokenMap->insert(std::make_pair("-", etMINUS));  // needed for --hint comment recognition
    // We purposely ignore ORDER for alias resolution here

    SimpleSyntaxAnalyser analyser;
    PlsSqlParser parser(&analyser, tokenMap);
    for (int l = startLine; l <= endLine; ++l)
    {
        Common::OEStringW wbuff;
        pEditor->GetLineW(l, wbuff);
        int begin = (l == startLine) ? startPos : 0;
        int end = (l == endLine && endPos != INT_MAX) ? endPos : wbuff.length();
        if (begin < end)
            parser.PutLine(l, Common::str(wbuff.data() + begin, end - begin).c_str(), end - begin);
    }

    // Remove comments from tokens for easier processing
    std::vector<Token> tokens;
    for (const auto& tk : analyser.GetTokens()) {
        if (tk.token != etCOMMENT && tk.token != etEOL)
            tokens.push_back(tk);
    }

    // Find the token at (line, pos) that is a dot, and the identifier before it
    int dotTokenIdx = -1;
    for (size_t i = 0; i < tokens.size(); ++i) {
        const auto& tk = tokens[i];
        if (tk.token == etDOT && tk.line == line && tk.offset == pos - 1) {
            dotTokenIdx = (int)i;
            break;
        }
    }
    if (dotTokenIdx == -1) {
        // fallback: find the last dot before (line, pos)
        for (int i = (int)tokens.size() - 1; i >= 0; --i) {
            const auto& tk = tokens[i];
            if (tk.token == etDOT) {
                if (tk.line < line || (tk.line == line && tk.offset < pos)) {
                    dotTokenIdx = i;
                    break;
                }
            }
        }
    }
    if (dotTokenIdx == -1 || dotTokenIdx == 0)
        return result;

    // Find the identifier before the dot
    int idIdx = dotTokenIdx - 1;
    while (idIdx >= 0 && tokens[idIdx].token != etUNKNOWN)
        --idIdx;
    if (idIdx < 0)
        return result;
    std::wstring alias = Common::wstr(tokens[idIdx].value);

    // Capture column name: the identifier immediately after the dot
    if (dotTokenIdx + 1 < (int)tokens.size() && tokens[dotTokenIdx + 1].token == etUNKNOWN)
        result.column = tokens[dotTokenIdx + 1].value;

    // Now walk tokens to build alias table with scope/level
    std::vector<ParseContext> contextTree;
    int currentContext = 0;
    contextTree.push_back(ParseContext{}); // root context
    std::vector<int> tokenContext(tokens.size(), 0);
    bool topLevelSelectPartitioned = false; // indicates we created a dedicated child for the first SELECT

    for (size_t i = 0; i < tokens.size(); )
    {
        tokenContext[i] = currentContext;

        // Ensure first top-level SELECT gets its own child context
        if (tokens[i].token == etSELECT
            && currentContext == 0 /*root*/
            && contextTree[currentContext].bracketCount == 0
            && !topLevelSelectPartitioned) {
            ParseContext newCtx; newCtx.parent = currentContext; // not a subquery
            contextTree.push_back(newCtx);
            currentContext = (int)contextTree.size() - 1;
            tokenContext[i] = currentContext;
            topLevelSelectPartitioned = true;
        }

        // Detect statement type
        if (contextTree[currentContext].stmtType == STMT_UNKNOWN) {
            switch (tokens[i].token) {
                case etWITH:
                    contextTree[currentContext].stmtType = STMT_SELECT;
                    break;
                case etSELECT:
                    contextTree[currentContext].stmtType = STMT_SELECT;
                    break;
                case etINSERT:
                    contextTree[currentContext].stmtType = STMT_INSERT;
                    break;
                case etUPDATE:
                    contextTree[currentContext].stmtType = STMT_UPDATE;
                    break;
                case etDELETE:
                    contextTree[currentContext].stmtType = STMT_DELETE;
                    break;
                case etMERGE:
                    contextTree[currentContext].stmtType = STMT_MERGE;
                    break;
                default:
                    break;
            }
        }

        // Handle set operators at top-level of a SELECT context (exclude inside parentheses)
        if (contextTree[currentContext].stmtType == STMT_SELECT
            && contextTree[currentContext].bracketCount == 0
            && (tokens[i].token == etUNION || tokens[i].token == etINTERSECT || tokens[i].token == etMINUS)) {
            ++i;
            if (i < tokens.size() && tokens[i].token == etALL) {
                ++i;
            }
            // inline former CreateSiblingSelectContext
            {
                int parent = contextTree[currentContext].parent;
                ParseContext newCtx; newCtx.parent = parent; // share parent so CTEs are visible but aliases are isolated
                contextTree.push_back(newCtx);
                currentContext = (int)contextTree.size() - 1;
            }
            continue;
        }

        // Clause transitions for SELECT
        if (contextTree[currentContext].stmtType == STMT_SELECT) {
            switch (tokens[i].token) {
                case etWITH:
                    contextTree[currentContext].clause = CLAUSE_WITH;
                    break;
                case etSELECT:
                    contextTree[currentContext].clause = CLAUSE_COLUMN_LIST;
                    break;
                case etFROM:
                    contextTree[currentContext].clause = CLAUSE_FROM;
                    break;
                case etWHERE:
                    contextTree[currentContext].clause = CLAUSE_WHERE;
                    break;
                case etGROUP:
                    if (i+1 < tokens.size() && tokens[i+1].token == etBY) {
                        contextTree[currentContext].clause = CLAUSE_GROUP_BY;
                        ++i;
                    }
                    break;
                case etORDER:
                    if (i+1 < tokens.size() && tokens[i+1].token == etBY) {
                        contextTree[currentContext].clause = CLAUSE_ORDER_BY;
                        ++i;
                    }
                    break;
                default:
                    break;
            }
        }
        else if (contextTree[currentContext].stmtType == STMT_UPDATE) {
            // UPDATE table_alias SET ... WHERE ...
            switch (tokens[i].token) {
                case etUPDATE:
                    contextTree[currentContext].clause = CLAUSE_FROM; // reuse FROM to capture table/alias immediately following UPDATE
                    break;
                case etSET:
                    contextTree[currentContext].clause = CLAUSE_SET;
                    break;
                case etWHERE:
                    contextTree[currentContext].clause = CLAUSE_WHERE;
                    break;
                default:
                    break;
            }
        }
        else if (contextTree[currentContext].stmtType == STMT_DELETE) {
            // Minimal DELETE support: DELETE FROM table alias WHERE ...
            switch (tokens[i].token) {
                case etDELETE: // first token
                case etFROM:   // explicit FROM
                    contextTree[currentContext].clause = CLAUSE_FROM;
                    break;
                case etWHERE:
                    contextTree[currentContext].clause = CLAUSE_WHERE;
                    break;
                default:
                    break;
            }
        }
        else if (contextTree[currentContext].stmtType == STMT_MERGE) { // minimal MERGE: MERGE INTO tgt USING src ON (...)
            switch (tokens[i].token) {
                case etINTO:  contextTree[currentContext].clause = CLAUSE_INTO;  break;
                case etUSING: contextTree[currentContext].clause = CLAUSE_USING; break;
                case etWHERE: contextTree[currentContext].clause = CLAUSE_WHERE; break;
                default: break;
            }
        }

        // Handle parentheses for subqueries
        switch (tokens[i].token) {
            case etLEFT_ROUND_BRACKET: {
                if (i+1 < tokens.size() && tokens[i+1].token == etSELECT) {
                    ParseContext newCtx;
                    newCtx.parent = currentContext;
                    newCtx.bracketCount = 1;
                    newCtx.isSubquery = true;
                    contextTree.push_back(newCtx);
                    currentContext = (int)contextTree.size() - 1;
                    ++i;
                    continue;
                }
                // only track plain parentheses depth for subqueries
                if (contextTree[currentContext].isSubquery)
                    contextTree[currentContext].bracketCount++;
                ++i;
                continue;
            }
            case etRIGHT_ROUND_BRACKET: {
                    int parentCtx = contextTree[currentContext].parent;
                    if (contextTree[currentContext].isSubquery) {
                        if (parentCtx >= 0 && !contextTree[parentCtx].pendingCteName.empty()) {
                            std::wstring realTable = singleTableFromSimpleSubquery ? GetSingleTableFromSimpleSubquery(tokens, tokenContext, contextTree, contextTree[currentContext]) : std::wstring();
                            contextTree[parentCtx].ctes.push_back({ contextTree[parentCtx].pendingCteName, !realTable.empty() ? realTable : L"<cte>", (int)i, (int)currentContext, contextTree[parentCtx].pendingCteColumns });
                            contextTree[parentCtx].pendingCteName.clear();
                            contextTree[parentCtx].pendingCteColumns.clear();
                        } 
                        else {
                            size_t j = i + 1;
                            if (j < tokens.size() && tokens[j].token == etAS) {
                                ++j;
                            }
                            if (j < tokens.size() && tokens[j].token == etUNKNOWN) {
                                int parentCtx2 = contextTree[currentContext].parent;
                                if (parentCtx2 >= 0) {
                                    std::wstring realTable = singleTableFromSimpleSubquery ? GetSingleTableFromSimpleSubquery(tokens, tokenContext, contextTree, contextTree[currentContext]) : std::wstring();
                                    contextTree[parentCtx2].aliases.push_back({ Common::wstr(tokens[j].value),
                                        !realTable.empty() ? realTable : L"<subquery>", (int)j, (int)currentContext, {} });
                                }
                                else {
                                    contextTree[currentContext].aliases.push_back({ Common::wstr(tokens[j].value), L"<subquery>", (int)j, (int)currentContext, {} });
                                }
                            }
                        }
                        if (contextTree[currentContext].bracketCount > 0) {
                            contextTree[currentContext].bracketCount--;
                            if (contextTree[currentContext].bracketCount == 0 && currentContext != 0) {
                                currentContext = contextTree[currentContext].parent;
                            }
                        }
                    }
                    // if not a subquery context then ignore ) for context popping
                    ++i;
                    continue;
                }
            default:
                break;
        }

        if (contextTree[currentContext].clause == CLAUSE_FROM ||
            contextTree[currentContext].clause == CLAUSE_INTO ||
            contextTree[currentContext].clause == CLAUSE_USING) {
            switch (tokens[i].token) {
                case etUPDATE: // reuse for UPDATE target
                case etFROM:
                case etCOMMA:
                case etJOIN:
                case etINTO: // MERGE target (may appear before table name)
                case etUSING: { // MERGE source (may appear before table name)
                    ++i;
                    if (i < tokens.size()) {
                        tokenContext[i] = currentContext;
                    }
                    if (i < tokens.size() && (tokens[i].token == etUNKNOWN || tokens[i].token == etDOUBLE_QUOTED_STRING)) {
                        GetTableName(tokens, i, currentContext, tokenContext, contextTree);
                        continue;
                    }
                    continue;
                }
                default:
                    break;
            }
        }

        // Handle CTEs (WITH ... AS ...)
        if ((contextTree[currentContext].clause == CLAUSE_WITH)
            && (tokens[i].token == etWITH || tokens[i].token == etCOMMA) ) {
            if (tokens[i].token == etWITH) ++i; // skip WITH for the first CTE, not for subsequent
            if (tokens[i].token == etCOMMA) ++i; // skip COMMA for subsequent CTEs
                // Parse CTE name
                if (i >= tokens.size() || tokens[i].token != etUNKNOWN) break;
            contextTree[currentContext].pendingCteName = Common::wstr(tokens[i].value); // set in parent context
                ++i;
                // Optional column list in parenthesis (collect if present)
                contextTree[currentContext].pendingCteColumns.clear();
                if (i < tokens.size() && tokens[i].token == etLEFT_ROUND_BRACKET) {
                    int parenDepth = 1;
                    ++i;
                    while (i < tokens.size() && parenDepth > 0) {
                        if (tokens[i].token == etLEFT_ROUND_BRACKET) {
                            parenDepth++;
                        }
                        else if (tokens[i].token == etRIGHT_ROUND_BRACKET) {
                            parenDepth--;
                            if (parenDepth == 0) { ++i; break; }
                        }
                        else if (parenDepth == 1) {
                            if (tokens[i].token == etUNKNOWN || tokens[i].token == etDOUBLE_QUOTED_STRING) {
                                contextTree[currentContext].pendingCteColumns.push_back(Common::wstr(tokens[i].value));
                            }
                        }
                        ++i;
                    }
                }
                // Expect AS
                if (i >= tokens.size() || tokens[i].token != etAS) break;
                ++i;
                // Expect subquery in parenthesis
                if (i >= tokens.size() || tokens[i].token != etLEFT_ROUND_BRACKET) break;
            contextTree[currentContext].clause = CLAUSE_WITH;
            continue;
        }

        ++i;
    }

    auto toNarrow = [](const std::vector<std::wstring>& ws) {
        std::vector<std::string> s; s.reserve(ws.size());
        for (auto& w : ws) s.push_back(Common::str(w));
        return s;
    };

    // Use the context at the dot token for alias resolution
    int ctx = (dotTokenIdx >= 0 && dotTokenIdx < (int)tokenContext.size()) ? tokenContext[dotTokenIdx] : 0;
    while (ctx >= 0) {
        // 1. Check regular aliases
        for (auto it = contextTree[ctx].aliases.rbegin(); it != contextTree[ctx].aliases.rend(); ++it) {
            if (it->alias == alias) {
                // Check if the alias's table is a CTE name in any visible context, recursively
                int cteCtxIdx = ctx;
                std::wstring resolvedTable;
                int beforeTokenIdx = it->tokenIdx;
                while (cteCtxIdx >= 0) {
                    for (const auto& cte : contextTree[cteCtxIdx].ctes) {
                        if (cte.alias == it->table && cte.tokenIdx < beforeTokenIdx) {
                            resolvedTable = ResolveCteTable(it->table, beforeTokenIdx, cteCtxIdx, contextTree);
                            break;
                        }
                    }
                    if (!resolvedTable.empty()) break;
                    cteCtxIdx = contextTree[cteCtxIdx].parent;
                }
                if (!resolvedTable.empty() && resolvedTable != L"<cte>") {
                    result.alias  = Common::str(it->alias);
                    result.table  = Common::str(resolvedTable);
                    return result;
                }
                // Try to return column list for subquery/cte
                auto colsW = TryGetColumnsForAlias(*it, ctx, contextTree, tokens, tokenContext);
                if (!colsW.empty()) {
                    result.alias   = Common::str(it->alias);
                    result.columns = toNarrow(colsW);
                    if (outSubquery && it->table == L"<subquery>")
                        *outSubquery = ExtractSubqueryText(pEditor, tokens, tokenContext, it->ctxIdx);
                    return result;
                }
                // If not a CTE, return the table name directly (skip pseudo markers)
                result.alias = Common::str(it->alias);
                if (it->table != L"<subquery>")
                    result.table = Common::str(it->table);
                else if (outSubquery)
                    *outSubquery = ExtractSubqueryText(pEditor, tokens, tokenContext, it->ctxIdx);
                return result;
            }
        }
        // 2. Check CTEs in this context (for direct CTE reference)
        for (const auto& cte : contextTree[ctx].ctes) {
            if (cte.alias == alias) {
                std::wstring resolvedTable = ResolveCteTable(cte.alias, cte.tokenIdx, ctx, contextTree);
                if (!resolvedTable.empty() && resolvedTable != L"<cte>") {
                    result.alias = Common::str(cte.alias);
                    result.table = Common::str(resolvedTable);
                    return result;
                }
                auto colsW = TryGetColumnsForAlias(cte, ctx, contextTree, tokens, tokenContext);
                if (!colsW.empty()) {
                    result.alias   = Common::str(cte.alias);
                    result.columns = toNarrow(colsW);
                    if (outSubquery && cte.table == L"<subquery>")
                        *outSubquery = ExtractSubqueryText(pEditor, tokens, tokenContext, cte.ctxIdx);
                    return result;
                }
                // Otherwise give up table marker
                result.alias = Common::str(cte.alias);
                if (outSubquery && cte.table == L"<subquery>")
                    *outSubquery = ExtractSubqueryText(pEditor, tokens, tokenContext, cte.ctxIdx);
                return result;
            }
        }
        ctx = contextTree[ctx].parent;
    }
    result.alias = Common::str(alias);
    return result;
}

// Helper function for recursive CTE resolution (definition)
static std::wstring ResolveCteTable(const std::wstring& cteName, int beforeTokenIdx, int ctx, const std::vector<ParseContext>& contextTree) {
    while (ctx >= 0) {
        for (const auto& cte : contextTree[ctx].ctes) {
            if (cte.alias == cteName && cte.tokenIdx < beforeTokenIdx) {
                // If cte.table is another CTE, resolve recursively
                if (cte.table != L"<cte>" && cte.table != cteName) {
                    std::wstring resolved = ResolveCteTable(cte.table, cte.tokenIdx, ctx, contextTree);
                    return resolved.empty() ? cte.table : resolved;
                }
                return cte.table;
            }
        }
        ctx = contextTree[ctx].parent;
    }
    return L"";
}

// Standalone function to check if a context is a simple SELECT from one table and column list is * or alias.*
static std::wstring GetSingleTableFromSimpleSubquery(const std::vector<Token>& tokens, const std::vector<int>& tokenContext, const std::vector<ParseContext>& contextTree, const ParseContext& ctx) {
    if (ctx.stmtType == STMT_SELECT && ctx.clause != CLAUSE_NONE) {
        int fromCount = 0;
        std::wstring foundTable;
        for (const auto& a : ctx.aliases) {
            fromCount++;
            foundTable = a.table;
        }
        if (fromCount == 1 && !foundTable.empty() && foundTable != L"<subquery>") {
            int ctxIdx = (int)(&ctx - &contextTree[0]);
            size_t selectIdx = 0;
            for (; selectIdx < tokens.size(); ++selectIdx) {
                if (tokenContext[selectIdx] == ctxIdx && tokens[selectIdx].token == etSELECT)
                    break;
            }
            if (selectIdx >= tokens.size()) return L"";// Look for the next non-comma token(s)
            size_t colIdx = selectIdx + 1;
            while (colIdx < tokens.size() && tokens[colIdx].token == etCOMMA) ++colIdx;
            if (colIdx >= tokens.size()) return L"";// Accept SELECT *
            if (tokens[colIdx].token == etSTAR) {
                // Make sure next is FROM or end
                size_t nextIdx = colIdx + 1;
                while (nextIdx < tokens.size() && tokens[nextIdx].token == etCOMMA) ++nextIdx;
                if (nextIdx < tokens.size() && tokens[nextIdx].token != etFROM) return L"";// Accept SELECT alias.*
                return foundTable;
            }
            if (tokens[colIdx].token == etUNKNOWN && colIdx+2 < tokens.size() &&
                tokens[colIdx+1].token == etDOT && tokens[colIdx+2].token == etSTAR) {
                // Make sure next is FROM or end
                size_t nextIdx = colIdx + 3;
                while (nextIdx < tokens.size() && tokens[nextIdx].token == etCOMMA) ++nextIdx;
                if (nextIdx < tokens.size() && tokens[nextIdx].token != etFROM) return L"";// Accept SELECT * FROM emp (no alias)
                return foundTable;
            }
        }
    }
    return L"";
}

// Extracted logic from main loop for FROM clause table/alias parsing
// Only handles [schema.]table [AS] [alias] pattern. Returns immediately if unexpected token (e.g. subquery).
static void GetTableName(const std::vector<Token>& tokens, size_t& i, int currentContext, std::vector<int>& tokenContext, std::vector<ParseContext>& contextTree) {
    std::wstring tableName;
    std::wstring schemaName;
    bool hasSchema = false;

    if (i >= tokens.size()) return;
    if (!(tokens[i].token == etUNKNOWN || tokens[i].token == etDOUBLE_QUOTED_STRING)) return;

    schemaName = Common::wstr(tokens[i].value);
    ++i;
    if (i < tokens.size() && tokens[i].token == etDOT) {
        hasSchema = true;
        ++i;
        if (i < tokens.size() && (tokens[i].token == etUNKNOWN || tokens[i].token == etDOUBLE_QUOTED_STRING)) {
            tableName = Common::wstr(tokens[i].value);
            ++i;
        } else {
            // Malformed: schema. but no table name
            return;
        }
    } else {
        tableName = schemaName;
        schemaName.clear();
    }
    if (i < tokens.size()) {
        tokenContext[i] = currentContext;
    }
    if (i < tokens.size() && tokens[i].token == etAS) {
        ++i;
    }
    if (i < tokens.size()) {
        tokenContext[i] = currentContext;
    }
    if (i < tokens.size() && tokens[i].token == etUNKNOWN && !tableName.empty()) {
        contextTree[currentContext].aliases.push_back({Common::wstr(tokens[i].value), hasSchema ? (schemaName + L"." + tableName) : tableName, (int)i, currentContext, {}});
        ++i;
    } 
	else if (!tableName.empty()) // no alias, just table name
        contextTree[currentContext].aliases.push_back({ tableName, hasSchema ? (schemaName + L"." + tableName) : tableName, (int)i, currentContext, {}});
}

// Collect column names from a SELECT list within a specific context (until top-level FROM)
static std::vector<std::wstring> CollectSelectColumns(const std::vector<Token>& tokens, const std::vector<int>& tokenContext, int ctxIdx) {
    std::vector<std::wstring> cols;
    if (ctxIdx < 0) return cols;
    size_t i = 0;
    for (; i < tokens.size(); ++i) {
        if (tokenContext[i] == ctxIdx && tokens[i].token == etSELECT) { ++i; break; }
    }
    if (i >= tokens.size()) return cols;
    int paren = 0;
    size_t exprStart = i;
    for (; i < tokens.size(); ++i) {
        if (tokenContext[i] != ctxIdx) continue;
        auto tk = tokens[i].token;
        if (tk == etLEFT_ROUND_BRACKET) { ++paren; continue; }
        if (tk == etRIGHT_ROUND_BRACKET) { if (paren>0) --paren; continue; }
        if (paren == 0 && (tk == etCOMMA || tk == etFROM)) {
            if (exprStart < i) {
                std::wstring name;
                for (size_t j = i; j > exprStart; --j) {
                    if (tokens[j-1].token == etAS && j < i && (tokens[j].token == etUNKNOWN || tokens[j].token == etDOUBLE_QUOTED_STRING)) {
                        name = Common::wstr(tokens[j].value);
                        break;
                    }
                }
                if (name.empty()) {
                    size_t j = i;
                    if (j>exprStart && (tokens[j-1].token == etUNKNOWN || tokens[j-1].token == etDOUBLE_QUOTED_STRING)) {
                        if (!(j>=2 && tokens[j-2].token == etDOT))
                            name = Common::wstr(tokens[j-1].value);
                    }
                }
                if (name.empty()) {
                    for (size_t j = i; j > exprStart; --j) {
                        if (tokens[j-1].token == etUNKNOWN || tokens[j-1].token == etDOUBLE_QUOTED_STRING) {
                            if (j>=2 && tokens[j-2].token == etDOT) { name = Common::wstr(tokens[j-1].value); break; }
                        }
                    }
                }
                if (!name.empty()) cols.push_back(name);
            }
            exprStart = i + 1;
            if (tk == etFROM) break;
        }
    }
    return cols;
}

static std::vector<std::wstring> TryGetColumnsForAlias(const AliasScope& aliasScope, int ctxIdxToSearchFrom, const std::vector<ParseContext>& contextTree, const std::vector<Token>& tokens, const std::vector<int>& tokenContext) {
    // Prefer explicit column list if it was captured (e.g., for CTEs)
    if (!aliasScope.columns.empty()) {
        return aliasScope.columns;
    }
    if (aliasScope.table == L"<subquery>") {
        return CollectSelectColumns(tokens, tokenContext, aliasScope.ctxIdx);
    }
    int ctx = ctxIdxToSearchFrom;
    while (ctx >= 0) {
        for (const auto& cte : contextTree[ctx].ctes) {
            if (cte.alias == aliasScope.table && cte.tokenIdx < aliasScope.tokenIdx) {
                // If explicit CTE column list exists, prefer it
                if (!cte.columns.empty())
                    return cte.columns;
                return CollectSelectColumns(tokens, tokenContext, cte.ctxIdx);
            }
        }
        ctx = contextTree[ctx].parent;
    }
    return {};
}
