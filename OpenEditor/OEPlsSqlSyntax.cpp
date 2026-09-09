/* 
    Copyright (C) 2002-2015 Aleksey Kochetov

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

/*
    06.03.2003 bug fix, stack overflow on closing a big pl/sql file with syntax errors
    03.06.2003 bug fix, sql find match fails on select/insert/... if there is no ending ';'
    02.05.2008 bug fix, some syntax constructins are not recognized as valid because of comments 
                        solution is to skip COMMENT token
    2011.09.21 bug fix, PL/SQLAnalyzer fails on packages with startup/shuldtown/run procedures 
                        (those are script keywords)
*/

#include "stdafx.h"
#include "OpenEditor/OEStorage.h"
#include "OpenEditor/OEPlsSqlSyntax.h"
#include "OpenEditor/OEPlsSqlSyntaxImpl.h"
#include "OpenEditor/OEPlsSqlSyntaxImpl.inl"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

namespace OpenEditor
{

bool SyntaxNode::StopSign (EToken token) const
{
    switch (token)
    {
    case etDECLARE:				return true;
    case etFUNCTION:			return true;
    case etRETURN:			    return false;
    case etPROCEDURE:			return true;
    case etPACKAGE:				return true;
    case etBODY:				return false;
    case etBEGIN:				return true;
    case etEXCEPTION:			return true;
    case etEND:					return false;
    case etIF:					return false;
    case etTHEN:				return false;
    case etELSE:				return true;
    case etELSIF:				return true;
    case etWHEN:				return true;
    case etFOR:					return true;
    case etWHILE:				return true;
    case etLOOP:				return false;
    case etEXIT:				return true;
    case etIS:					return false;
    case etAS:					return false;
    case etSEMICOLON:			return true;
    case etQUOTE:				return true;
    case etDOUBLE_QUOTE:		return false;
    case etLEFT_ROUND_BRACKET:	return true;
    case etRIGHT_ROUND_BRACKET:	return true;
    case etMINUS:   			return false;
    case etSLASH:				return true;
    case etSTAR:				return false;
    case etSELECT:				return true;
    case etINSERT:				return true;
    case etUPDATE:				return true;
    case etDELETE:				return true;
    case etALTER:				return true;
    case etANALYZE:				return true;
    case etCREATE:				return true;
    case etDROP:				return true;
    case etFROM:				return true;
    case etWHERE:				return true;
    case etSET:					return true;
    case etOPEN:                return true;
    case etUNION:               return true;
    case etINTERSECT:           return true;
    case etMINUS_SQL:           return true;   
    case etTRIGGER:             return true;   
    }
    return false;
}

SyntaxNode::SyntaxNode ()                               
{ 
    m_failure = false;
    m_failureLine = -1;
    m_sibling = m_parent = m_child = 0; 
    m_lastChild = 0;
}

void SyntaxNode::clear ()
{
    m_failure = false;
    m_failureLine = -1;
    m_sibling = m_parent = m_child = 0; 
    m_lastChild = 0;
}

void SyntaxNode::AttachSibling (SyntaxNode* sibling)    
{ 
    if (!m_sibling)
    {
        m_sibling = sibling; 
        sibling->m_parent = m_parent; 
    }
    else
        m_sibling->AttachSibling(sibling);
}

void SyntaxNode::AttachChild (SyntaxNode* child)       
{ 
    if (!m_child)
    {
        m_lastChild =
        m_child = child; 
        child->m_parent = this; 
    }
    else
    {
        m_lastChild->AttachSibling(child);
        m_lastChild = child;
    }
}

int SyntaxNode::GetLevel () const
{
    const SyntaxNode* node = this;
    int level = 0;
    for (; node && node->m_parent; node = node->m_parent, level++);
    return level;
}

///////////////////////////////////////////////////////////////////////////////

PlSqlAnalyzer::PlSqlAnalyzer ()
: m_root(new ScriptNode)
{
    m_top   = m_root.get();
    m_error = false;
}

PlSqlAnalyzer::~PlSqlAnalyzer ()
{
    try { EXCEPTION_FRAME;

        Clear();
    }
    _DESTRUCTOR_HANDLER_;
}

void PlSqlAnalyzer::Clear ()
{ 
    m_tokenCounter = 0;
    m_error = false; 

    m_top = m_root.get(); 

    std::vector<SyntaxNode*>::const_iterator it = m_pool.begin();
    for (; it != m_pool.end(); ++it)
        delete *it;

    m_pool.clear();

    m_root->Clear(); 
}

void PlSqlAnalyzer::Attach (SyntaxNode* node)
{
    if (m_top)
    {
        //TRACE("Open %s, level = %d, line = %d, col = %d\n", 
        //    node->GetName(), m_top->GetLevel()+1,
        //    node->m_tokens.front().line+1, node->m_tokens.front().offset+1);
        m_top->AttachChild(node);
        m_top = node;
        m_pool.push_back(node);
    }
}

void PlSqlAnalyzer::CloseTop (const Token& token)
{
    ASSERT(m_top);
    
    if (m_top)
    {
        //TRACE("Close %s, level = %d, line = %d, col = %d\n",
        //    m_top->GetName(), m_top->GetLevel(),
        //    m_top->GetTokens().front().line+1, m_top->GetTokens().front().offset+1);
        
        ASSERT(m_top->IsCompleted() || m_top->IsFailed());

        SyntaxNode* parent = m_top->GetParent();
        
        if (parent) 
            parent->OnChildCompletion(m_top, token);

        m_top = parent;
    }
}

void PlSqlAnalyzer::CloseAll (const Token& token) 
{
    ASSERT(m_top);
    ASSERT(token == etEOS || token == etFAILURE);

    if (m_top)
    {
        //TRACE("CloseAll, level = %d, line = %d, col = %d\n", 
        //    m_top->GetLevel(),token.line+1, token.offset+1);

        if (!m_error
        && token == etEOS
        && m_top->GetLevel() == 1
        && m_top->IsCompleted())
        {
            CloseTop(token);
        }
        else
        {
            while (m_top->GetParent())
            {
                if (token == etFAILURE
                && m_top->GetLevel() == 1)
                {
                    m_top->Failure(token.line); // set failed but don't colapse it
                    break;
                }
                m_top->Colapse(token);
                CloseTop(token);
            }
        }
    }
}

void PlSqlAnalyzer::PutToken (const Token& token)
{
    m_tokenCounter++;

    if (token == etEOF 
    || token == etEOS
    || (token == etSLASH && token.offset == 0))
    {
        Token eos;
        eos.token = etEOS;
        //eos.line = max(0, token.line-1);
        eos.line = token.line;
        //eos.length = 1; // TODO: decide what's better - either 0 or 1
        std::unique_ptr<SyntaxNode> dummy;
        m_top->PutToken(eos, dummy);
//        ASSERT(!dummy.get());
        CloseAll(eos);
        m_error = false;
        return;
    }

    if (!m_error)
    {
        if (token == etCOMMENT
        || (token == etEOL && !m_top->WantsEOL()))
            return;

        std::unique_ptr<SyntaxNode> child;

        // 2011.09.21 bug fix, PL/SQLAnalyzer fails on packages with startup/shuldtown/run procedures (those are script keywords)
        if (token.token != etUNKNOWN
        && !m_top->IsScriptNode() // so it is NOT ScriptNode
        && !PlSqlParser::IsPlSqlToken(token.token) // and it is NOT a PL/SQL token
        && PlSqlParser::IsScriptToken(token.token) // and it is a script token 
        ) 
        {
            // then convert it to etUNKNOWN
            Token _token = token;
            _token.token = etUNKNOWN;
            m_top->PutToken(_token, child);
        }
        else
            m_top->PutToken(token, child);

        if (child.get())
        {
            Attach(child.release());
        }
        else if (m_top->IsFailed())  // DOTO: throw an exception on Failed!
        {
            //TODO: make it more tolerant

            TRACE("FALURE: token = %s, line = %d, col = %d\n", PlSqlParser::GetStringToken(token), token.line+1, token.offset+1);

            m_error = true;
            Token failure  = token;
            failure.token  = etFAILURE;
            failure.length = 0;
            CloseAll(failure);
            return;
        }
        else
        {
            while (m_top->IsCompleted())
                CloseTop(token);
        }
    }
}

bool PlSqlAnalyzer::FindToken (int line, int offset, const SyntaxNode*& node, int& index) const
{ 
    return m_root->FindToken(line, offset, node, index); 
}

void PlSqlAnalyzer::GetLineStatus (LineStatusMap& statusMap) const
{ 
    int recursion = 0;
    m_root->GetLineStatus(statusMap, recursion); 
}

// ---------------------------------------------------------------------------
// Outline extraction
// ---------------------------------------------------------------------------

// Returns the wstring text of a token by reading it from Storage
static std::wstring TokenText (const Storage& storage, const Token& tok)
{
    OEStringW line;
    storage.GetLineW(tok.line, line);
    int len = tok.length;
    int off = tok.offset;
    if (off < static_cast<int>(line.length()))
    {
        len = min(len, static_cast<int>(line.length()) - off);
        return std::wstring(line.data() + off, len);
    }
    return std::wstring();
}

// Extract a whitespace-collapsed text snippet from storage in the range
// [startLine:startCol, endLine:endCol).  Appends L"\u2026" if the result
// exceeds maxLen characters.  Leading/trailing whitespace is trimmed.
static std::wstring ExtractSnippet (const Storage& storage,
                                    int startLine, int startCol,
                                    int endLine,   int endCol,
                                    int maxLen = 40)
{
    std::wstring result;
    result.reserve(maxLen + 4);
    OEStringW buf;

    for (int ln = startLine; ln <= endLine; ++ln)
    {
        storage.GetLineW(ln, buf);
        int lineLen = static_cast<int>(buf.length());
        int col0 = (ln == startLine) ? startCol : 0;
        int col1 = (ln == endLine)   ? min(endCol, lineLen) : lineLen;
        if (col0 >= col1) continue;

        const wchar_t* lineData = buf.data();
        for (int c = col0; c < col1; ++c)
        {
            wchar_t ch = lineData[c];
            // Collapse runs of whitespace to a single space
            if (ch == L'\t' || ch == L' ')
            {
                if (!result.empty() && result.back() != L' ')
                    result += L' ';
            }
            else
            {
                result += ch;
            }
            if (static_cast<int>(result.size()) >= maxLen)
            {
                // trim trailing space before ellipsis
                while (!result.empty() && result.back() == L' ')
                    result.pop_back();
                result += L'\u2026'; // …
                return result;
            }
        }
        // Separate lines with a space
        if (ln < endLine && !result.empty() && result.back() != L' ')
            result += L' ';
    }
    // Trim trailing space
    while (!result.empty() && result.back() == L' ')
        result.pop_back();
    return result;
}

// Walk a node's token array and build label "TYPE name1[.name2]"
// The name tokens are the last non-keyword identifier(s) stored before IS/AS.
// For PackageNode / ProcedureNode / FunctionNode / TriggerNode / TypeBodyNode
// the layout is: keyword(s) … eName1-token [DOT eName2-token] IS …
// We use the following heuristic:
//   - last token's line == endLine of the node (the closing ; / EOS)
//   - eName1 and optional eName2 are non-keyword, non-punctuation UNKNOWN/IDENTIFIER tokens
//     found before the IS/AS/BEGIN position and after the type keyword.

// node class name and then grab the last 1-or-2 identifiers seen before IS/AS.

// Generic label builder: scan the token array
// tokens that appear before the first IS/AS/BEGIN/BODY keyword.
template<class NodeT>
static std::wstring BuildNodeLabel(const Storage& storage,
                                   const std::wstring& typePrefix,
                                   const NodeT* n)
{
    // Collect text for tokens that look like identifiers (not reserved keywords,
    // or are DOT separators).  Stop when we see IS/AS/BEGIN.
    std::wstring name1, name2;
    bool hasDot = false;
    auto& tokens = n->GetTokens();
    for (int i = 0; i < static_cast<int>(tokens.size()); ++i)
    {
        const Token& tok = tokens[i];
        switch (tok.token)
        {
        case etIS: case etAS: case etBEGIN:
            goto done;
        case etDOT:
            hasDot = true;
            break;
        case etUNKNOWN: case etIDENTIFIER: case etDOUBLE_QUOTED_STRING:
            if (hasDot) { name2 = TokenText(storage, tok); hasDot = false; }
            else        { name1 = TokenText(storage, tok); name2.clear(); }
            break;
        default:
            break;
        }
    }
done:
    std::wstring label = typePrefix;
    if (!name1.empty())
    {
        label += L' ';
        label += name1;
        if (!name2.empty()) { label += L'.'; label += name2; }
    }
    return label;
}

// Context passed through outline extraction to carry the three settings.
struct OutlineCtx
{
    const Storage& storage;
    bool selectEnabled;   // if false, skip all SelectNodes
    bool selectOnly;      // if true, emit SELECT labels only (no FROM/WHERE children)
    int  maxTopItems;     // stop emitting depth-0 items once this count is reached
    int  topItemCount;    // running count of depth-0 items emitted so far
};

// Forward declarations
static void GetSelectOutlineItems  (OutlineCtx& ctx, const SyntaxNode* selectNode, int depth, OutlineItems& out);
static void GetOutlineItemsWithCtx (OutlineCtx& ctx, const SyntaxNode* node,       int depth, OutlineItems& out);

/*static*/ void PlSqlAnalyzer::GetOutlineItemsImpl (
    const Storage& storage, const SyntaxNode* node, int depth, OutlineItems& out)
{
    // This overload is kept for recursion from within package/proc/function bodies;
    // it creates a permissive ctx that does not limit top-item count for nested calls.
    OutlineCtx ctx{ storage, true, false, INT_MAX, 0 };
    GetOutlineItemsWithCtx(ctx, node, depth, out);
}

static void GetOutlineItemsWithCtx (
    OutlineCtx& ctx, const SyntaxNode* node, int depth, OutlineItems& out)
{
    if (!node) return;

    const SyntaxNode* child = node->GetChild();
    while (child)
    {
        // Stop emitting depth-0 items once the limit is reached
        if (depth == 0 && ctx.topItemCount >= ctx.maxTopItems)
            return;

        const SyntaxNode* next = child->GetSibling();

        OutlineItem item;
        item.depth   = depth;
        item.pos     = {};
        item.type    = oitUnknown;

        if (const auto* p = dynamic_cast<const PackageNode*>(child))
        {
            auto& tokens = p->GetTokens();
            if (!tokens.empty())
            {
                item.pos.start = { tokens.front().offset, tokens.front().line };
                item.pos.end   = { tokens.back().endCol(), tokens.back().line };
                bool isBody  = false;
                for (int i = 0; i < static_cast<int>(tokens.size()); ++i)
                {
                    auto tk = tokens[i];
                    if (tk.token == etBODY) { isBody = true; break; }
                }
                if (isBody) { item.type = oitPackageBody; item.label = BuildNodeLabel(ctx.storage, L"PACKAGE BODY", p); }
                else        { item.type = oitPackage;     item.label = BuildNodeLabel(ctx.storage, L"PACKAGE", p); }
                out.push_back(item);
                if (depth == 0) ++ctx.topItemCount;
                GetOutlineItemsWithCtx(ctx, child, depth + 1, out);
            }
        }
        else if (const auto* p = dynamic_cast<const ProcedureNode*>(child))
        {
            auto& tokens = p->GetTokens();
            if (!tokens.empty())
            {
                item.pos.start = { tokens.front().offset, tokens.front().line };
                item.pos.end   = { tokens.back().endCol(), tokens.back().line };
                item.type    = oitProcedure;
                item.label   = BuildNodeLabel(ctx.storage, L"PROCEDURE", p);
                out.push_back(item);
                if (depth == 0) ++ctx.topItemCount;
                GetOutlineItemsWithCtx(ctx, child, depth + 1, out);
            }
        }
        else if (const auto* p = dynamic_cast<const FunctionNode*>(child))
        {
            auto& tokens = p->GetTokens();
            if (!tokens.empty())
            {
                item.pos.start = { tokens.front().offset, tokens.front().line };
                item.pos.end   = { tokens.back().endCol(), tokens.back().line };
                item.type    = oitFunction;
                item.label   = BuildNodeLabel(ctx.storage, L"FUNCTION", p);
                out.push_back(item);
                if (depth == 0) ++ctx.topItemCount;
                GetOutlineItemsWithCtx(ctx, child, depth + 1, out);
            }
        }
        else if (const auto* p = dynamic_cast<const TypeBodyNode*>(child))
        {
            auto& tokens = p->GetTokens();
            if (!tokens.empty())
            {
                item.pos.start = { tokens.front().offset, tokens.front().line };
                item.pos.end   = { tokens.back().endCol(), tokens.back().line };
                item.type    = oitTypeBody;
                item.label   = BuildNodeLabel(ctx.storage, L"TYPE BODY", p);
                out.push_back(item);
                if (depth == 0) ++ctx.topItemCount;
                GetOutlineItemsWithCtx(ctx, child, depth + 1, out);
            }
        }
        else if (const auto* p = dynamic_cast<const SelectNode*>(child))
        {
            // Only emit top-level SELECT statements (depth == 0).
            if (depth == 0 && ctx.selectEnabled)
            {
                GetSelectOutlineItems(ctx, p, depth, out);
                ++ctx.topItemCount;
            }
        }
        else
        {
            GetOutlineItemsWithCtx(ctx, child, depth, out);
        }

        child = next;
    }
}

/*static*/ bool PlSqlAnalyzer::HasFailedNodeImpl (const SyntaxNode* node)
{
    if (!node) return false;
    if (node->IsFailed()) return true;
    for (const SyntaxNode* child = node->GetChild(); child; child = child->GetSibling())
        if (HasFailedNodeImpl(child)) return true;
    return false;
}

// ---------------------------------------------------------------------------
// SELECT outline extraction
// ---------------------------------------------------------------------------

// Token types used as clause markers inside SelectNode::m_tokens
// (same values the parser stores via SET_AND_RET)
static bool IsSetOperator (int tok)
{
    return tok == etUNION || tok == etINTERSECT || tok == etMINUS_SQL;
}

static bool IsSetOperatorOrAll (int tok)
{
    return IsSetOperator(tok) || tok == etALL;
}

static const wchar_t* SetOperatorLabel (int tok)
{
    if (tok == etUNION)     return L"UNION SELECT\t";
    if (tok == etINTERSECT) return L"INTERSECT SELECT\t";
    return                         L"MINUS SELECT\t";
}

// Forward declaration (ctx-based; old storage-based overload removed)
static void GetSelectOutlineItems (
    OutlineCtx& ctx, const SyntaxNode* selectNode, int depth, OutlineItems& out);

// Recursively emit clause items for one SELECT leg.
// clauseTokens  — the clause keyword tokens for this leg (SELECT, FROM, WHERE …)
// legChildren   — child nodes whose start line falls within this leg
// depth         — outline depth for the clause items
static void EmitSelectLeg (
    OutlineCtx&                      ctx,
    const std::wstring&              legLabel,
    EOutlineItemType                 legType,
    const Token&                     legStartToken,
    const Token&                     selectToken,    // actual SELECT keyword of this leg
    OpenEditor::Position             legEndPos,
    const std::vector<Token>&        clauseTokens,  // FROM, WHERE … (not SELECT itself)
    const std::vector<const SyntaxNode*>& legChildren,
    int                              depth,
    OutlineItems&                    out)
{
    // Emit the SELECT / UNION-SELECT leg item itself.
    // Snippet: text between end of SELECT keyword and start of first clause (FROM).
    // If no FROM exists use legEndPos as the limit.
    {
        const Token* fromTok = nullptr;
        for (const Token& ct : clauseTokens)
            if (ct.token == etFROM) { fromTok = &ct; break; }

        int snipEndLine = fromTok ? fromTok->line   : legEndPos.line;
        int snipEndCol  = fromTok ? fromTok->offset : legEndPos.column;

        std::wstring snippet = ExtractSnippet(ctx.storage,
            selectToken.line, selectToken.endCol(),
            snipEndLine, snipEndCol);

        std::wstring fullLabel = legLabel;
        if (!snippet.empty()) { fullLabel += L"  "; fullLabel += snippet; }

        OutlineItem legItem;
        legItem.label     = std::move(fullLabel);
        legItem.type      = legType;
        legItem.pos.start = { legStartToken.offset, legStartToken.line };
        legItem.pos.end   = legEndPos;
        legItem.depth     = depth;
        out.push_back(legItem);
    }

    int clauseDepth = depth + 1;

    // Build clause slots in token order, prefixed with a sentinel (SELECT list).
    struct ClauseSlot
    {
        Token                startToken;  // the clause keyword token (FROM, WHERE …)
        OpenEditor::Position endPos;      // end position of this clause
        std::wstring         label;       // base keyword label (no snippet yet)
        EOutlineItemType     type;
    };

    std::vector<ClauseSlot> slots;

    // Sentinel slot: covers the SELECT column list (before FROM)
    slots.push_back({ selectToken, legEndPos, L"", oitSelect });

    for (const Token& tok : clauseTokens)
    {
        ClauseSlot s;
        s.startToken = tok;
        s.endPos     = legEndPos; // will be trimmed below
        switch (tok.token)
        {
        case etFROM:    
            s.label = L"FROM\t";     
            s.type = oitFromClause;
            break;
        case etWHERE:   
            s.label = L"WHERE\t";    
            s.type = oitWhereClause;
            break;
        default:        s.label = L"";         break; // GROUP BY etc. if ever added
        }
        if (!s.label.empty())
            slots.push_back(s);
    }

    // Trim each slot's end to one before the next slot's start line
    for (int i = 0; i + 1 < static_cast<int>(slots.size()); ++i)
        slots[i].endPos = { 0, slots[i + 1].startToken.line - 1 };

    // slot 0 is the sentinel for the column list — no label, kept for child placement only
    slots[0].label = L"";

    // For each clause slot that has a label, emit a clause outline item,
    // then emit any child subselects whose start line falls in that slot.
    // Children in the column-list slot (slots[0]) are emitted at clauseDepth
    // directly under the leg, before the FROM item.

    // Collect children per slot index
    std::vector<std::vector<const SyntaxNode*>> slotChildren(slots.size());
    for (const SyntaxNode* child : legChildren)
    {
        const auto* sc = dynamic_cast<const SelectNode*>(child);
        if (!sc) continue;
        auto& ct = sc->GetTokens();
        if (ct.empty()) continue;
        int childLine = ct.front().line;
        // Find the last slot whose line <= childLine
        int slotIdx = 0;
        for (int i = 1; i < static_cast<int>(slots.size()); ++i)
            if (slots[i].startToken.line <= childLine) slotIdx = i;
        slotChildren[slotIdx].push_back(child);
    }

    // Emit column-list subqueries first (slot 0), then clause slots
    for (const SyntaxNode* sub : slotChildren[0])
        GetSelectOutlineItems(ctx, sub, clauseDepth, out);

    for (int i = 1; i < static_cast<int>(slots.size()); ++i)
    {
        const ClauseSlot& slot = slots[i];

        if (!ctx.selectOnly)
        {
            // Snippet: text from end of clause keyword to start of next slot (or legEndPos)
            int snipEndLine = (i + 1 < static_cast<int>(slots.size()))
                              ? slots[i + 1].startToken.line
                              : slot.endPos.line;
            int snipEndCol  = (i + 1 < static_cast<int>(slots.size()))
                              ? slots[i + 1].startToken.offset
                              : slot.endPos.column;

            std::wstring snippet = ExtractSnippet(ctx.storage,
                slot.startToken.line, slot.startToken.endCol(),
                snipEndLine, snipEndCol == 0 ? INT_MAX : snipEndCol);

            std::wstring fullLabel = slot.label;
            if (!snippet.empty()) { fullLabel += L"  "; fullLabel += snippet; }

            OutlineItem ci;
            ci.label     = std::move(fullLabel);
            ci.type      = slot.type;
            ci.pos.start = { slot.startToken.offset, slot.startToken.line };
            ci.pos.end   = slot.endPos;
            ci.depth     = clauseDepth;
            out.push_back(ci);
        }

        // Always recurse into subqueries regardless of selectOnly
        for (const SyntaxNode* sub : slotChildren[i])
            GetSelectOutlineItems(ctx, sub, ctx.selectOnly ? clauseDepth : clauseDepth + 1, out);
    }
}

static void GetSelectOutlineItems (
    OutlineCtx& ctx, const SyntaxNode* selectNode, int depth, OutlineItems& out)
{
    const auto* sn = dynamic_cast<const SelectNode*>(selectNode);
    if (!sn) return;

    const auto& tokens = sn->GetTokens();
    if (tokens.empty()) return;

    // Collect all child SelectNodes in source order (sibling chain).
    // Subqueries are wrapped in ExpressionNode by the parser, so look one
    // level deeper when the direct child is not itself a SelectNode.
    std::vector<const SyntaxNode*> allChildren;
    for (const SyntaxNode* c = sn->GetChild(); c; c = c->GetSibling())
    {
        if (dynamic_cast<const SelectNode*>(c))
        {
            allChildren.push_back(c);
        }
        else if (dynamic_cast<const ExpressionNode*>(c))
        {
            // ExpressionNode wraps a parenthesised subquery — its first child
            // is the actual SelectNode
            for (const SyntaxNode* inner = c->GetChild(); inner; inner = inner->GetSibling())
                if (dynamic_cast<const SelectNode*>(inner))
                    allChildren.push_back(inner);
        }
    }

    // Partition tokens into legs separated by UNION/INTERSECT/MINUS.
    // Each leg starts with a SELECT token and contains its clause tokens.
    // We also need the end line of each leg = start of next leg - 1 (or last token line).
    struct Leg
    {
        EOutlineItemType     type;
        std::wstring         label;
        Token                startToken;     // first token of this leg (SELECT or set-op)
        Token                selectToken;    // the actual SELECT keyword of this leg
        OpenEditor::Position endPos;
        std::vector<Token>   clauseTokens;   // FROM, WHERE (not SELECT itself)
        int                  childStartLine;
        int                  childEndLine;
    };

    std::vector<Leg> legs;
    // Determine the end position of the whole select node.
    // If the last token is etUNKNOWN it is a synthetic marker placed at the
    // exact offset of the closing ')' — use that position directly.
    // For top-level SELECTs the last token is a real keyword.
    const Token& lastTok = tokens.back();
    OpenEditor::Position selectEndPos = lastTok.token == etUNKNOWN
        ? OpenEditor::Position{ lastTok.offset, lastTok.line }
        : OpenEditor::Position{ lastTok.endCol(), lastTok.line };

    for (int i = 0; i < static_cast<int>(tokens.size()); ++i)
    {
        const Token& tok = tokens[i];
        if (tok.token == etSELECT)
        {
            Leg leg;
            if (legs.empty())
            {
                leg.label       = L"SELECT\t";
                leg.type        = oitSelect;
                leg.startToken  = tok;
            }
            else
            {
                // Set-op leg starts at the UNION/INTERSECT/MINUS token.
                // Scan back past optional ALL to find the actual operator token.
                int j = i - 1;
                while (j > 0 && tokens[j].token == etALL) --j;
                const Token& opTok = tokens[j];
                leg.label       = SetOperatorLabel(opTok.token);
                leg.type        = oitSelect;
                leg.startToken  = opTok;
            }
            leg.selectToken    = tok;   // always the SELECT keyword
            leg.endPos         = selectEndPos;
            leg.childStartLine = tok.line;
            leg.childEndLine   = selectEndPos.line;
            legs.push_back(leg);
        }
        else if (!legs.empty() && !IsSetOperatorOrAll(tok.token) && tok.token != etWITH)
        {
            // Clause token belonging to the current leg
            legs.back().clauseTokens.push_back(tok);
        }
    }

    if (legs.empty()) return;

    // Trim each leg's end to one before the next leg's start line
    for (int i = 0; i + 1 < static_cast<int>(legs.size()); ++i)
        legs[i].endPos = { 0, legs[i + 1].startToken.line - 1 };

    // Assign children to legs by start line
    for (int i = 0; i < static_cast<int>(legs.size()); ++i)
    {
        legs[i].childEndLine   = legs[i].endPos.line;
        legs[i].childStartLine = legs[i].startToken.line;
    }

    // Emit each leg
    for (int i = 0; i < static_cast<int>(legs.size()); ++i)
    {
        const Leg& leg = legs[i];

        // Collect children whose start line falls within this leg's range
        std::vector<const SyntaxNode*> legChildren;
        for (const SyntaxNode* c : allChildren)
        {
            const auto* sc = dynamic_cast<const SelectNode*>(c);
            if (!sc) continue;
            auto& ct = sc->GetTokens();
            if (ct.empty()) continue;
            int cl = ct.front().line;
            if (cl >= leg.startToken.line && cl <= leg.endPos.line)
                legChildren.push_back(c);
        }

        EmitSelectLeg(ctx, leg.label, leg.type, leg.startToken, leg.selectToken,
                      leg.endPos, leg.clauseTokens, legChildren,
                      (i == 0) ? depth : depth + 1, out);
    }
}



void PlSqlAnalyzer::GetOutlineItems (const Storage& storage, OutlineItems& out) const
{
    out.clear();
    auto& settings = storage.GetSettings();
    OutlineCtx ctx { 
        storage, 
        settings.GetPlSqlOutlineSelectEnabled(), 
        settings.GetPlSqlOutlineSelectOnly(), 
        settings.GetPlSqlOutlineMaxTopItems() ? settings.GetPlSqlOutlineMaxTopItems() : INT_MAX,
        0 
    };
    GetOutlineItemsWithCtx(ctx, m_root.get(), 0, out);
}

bool PlSqlAnalyzer::HasAnyFailedNode () const
{
    return HasFailedNodeImpl(m_root.get());
}
};