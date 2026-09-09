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
#pragma once

#include "OpenEditor/OEPlsSqlParser.h"
#include <vector>
#include <climits>

namespace OpenEditor {

	class Settings;
	std::wstring formatSQL (std::vector<Token>& tokens, const Settings& s);

// ============================================================================
// ClauseEntry — one keyword boundary within a scope.
//
// For a SELECT scope:
//   etSELECT (col-list SELECT→FROM), etFROM (table-list),
//   etWHERE, etGROUP, etBY, etHAVING, …
// For a CaseExpr scope:
//   etWHEN (one per clause, range covers "cond THEN value" — up to next WHEN/ELSE/END)
//   etELSE (range covers the else-value — up to END)
//
//   NOTE: etTHEN is intentionally NOT stored as a keyword entry.
//   THEN is subordinate to the preceding WHEN, so the WHEN entry's range spans
//   the full "cond THEN value" fragment.  formattedLen therefore covers that
//   whole fragment and the short-WHEN heuristic works correctly.
//
// srcIdx      — index of this keyword in input tokens[].
// closeSrcIdx — index of the next clause's first token (the terminator); -1 until set.
// formattedLen — estimated single-line visual width of the list after this keyword.
// hasComment   — any -- comment in that list.
// ============================================================================
struct ClauseEntry
{
	EToken token        = etNONE;
	int    srcIdx       = -1;
	int    closeSrcIdx  = -1;
	int    formattedLen = INT_MAX;
	bool   hasComment   = false;
};

// ============================================================================
// FormatContext — one node in the SQL syntax tree.
//
// Built by buildTree(), widths filled by calcWidths().
// ============================================================================
struct FormatContext
{
	enum Kind {
		Root,             // invisible document root (openSrcIdx == -1)
		Paren,            // ( … ) sub-expression or subquery block
		Select,           // SELECT … ; or SELECT … UNION branch
		CaseExpr,         // CASE … END
		BetweenExpr,      // BETWEEN … AND
		Delete,           // DELETE … ;
		Update,           // UPDATE … ;
		InsertSimple,     // reserved
		InsertConditional,// reserved
		Merge             // MERGE … ;
	};

	Kind kind         = Root;

	int  openSrcIdx   = -1;       // index in tokens[] of the opener
	int  closeSrcIdx  = -1;       // index in tokens[] of the closer
	bool hasComment   = false;    // any -- comment between opener and closer
	int  formattedLen = INT_MAX;  // estimated single-line visual width

	std::vector<ClauseEntry>  keywords; // clause boundaries owned by this scope
	std::vector<FormatContext> children; // nested scopes
};

// ============================================================================
// Helpers
// ============================================================================

// Returns a pointer to the ClauseEntry matching key (and optionally srcIdx).
// Pass srcIdx >= 0 to disambiguate duplicate tokens (e.g. multiple WHEN entries).
ClauseEntry*       getKeyword(std::vector<ClauseEntry>& v,
							   EToken key, int srcIdx = -1);
const ClauseEntry* getKeyword(const std::vector<ClauseEntry>& v,
							   EToken key, int srcIdx = -1);

// Returns a pointer to the FormatContext node whose openSrcIdx == targetIdx,
// searching the whole subtree rooted at node.  Returns nullptr when not found.
FormatContext* findInTree(FormatContext& node, int targetIdx);

// ============================================================================
// buildTree — structural pre-pass (no width estimation).
//
// Returns a Root node whose children[] own the complete syntax tree for all
// statements in tokens[].  Comments are noted (hasComment); lengths are not.
// ============================================================================
FormatContext buildTree(const std::vector<OpenEditor::Token>& tokens);

// ============================================================================
// calcWidths — formatter-policy width pass.
//
// Walks the tree produced by buildTree() and fills in formattedLen on every
// FormatContext node and every ClauseEntry.  hasComment is propagated upward.
// Must be called after buildTree() and before the formatting loop.
// ============================================================================
void calcWidths(FormatContext& root,
				const std::vector<OpenEditor::Token>& tokens);

} // namespace OpenEditor
