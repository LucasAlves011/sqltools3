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
#include <algorithm>
#include <climits>

namespace OpenEditor {

// ============================================================================
// getKeyword
// ============================================================================

ClauseEntry* getKeyword(std::vector<ClauseEntry>& v, EToken key, int srcIdx)
{
	for (auto& e : v)
		if (e.token == key && (srcIdx < 0 || e.srcIdx == srcIdx)) return &e;
	return nullptr;
}

const ClauseEntry* getKeyword(const std::vector<ClauseEntry>& v, EToken key, int srcIdx)
{
	for (const auto& e : v)
		if (e.token == key && (srcIdx < 0 || e.srcIdx == srcIdx)) return &e;
	return nullptr;
}

// ============================================================================
// findInTree
// ============================================================================

FormatContext* findInTree(FormatContext& node, int targetIdx)
{
	if (node.openSrcIdx == targetIdx) return &node;
	for (auto& child : node.children)
	{
		auto* p = findInTree(child, targetIdx);
		if (p) return p;
	}
	return nullptr;
}

// ============================================================================
// buildTree — structural pre-pass (no width estimation).
// ============================================================================

FormatContext buildTree(const std::vector<OpenEditor::Token>& tokens)
{
	FormatContext root;
	root.kind       = FormatContext::Root;
	root.openSrcIdx = -1;

	struct Frame { FormatContext* node; };
	std::vector<Frame> path;
	path.push_back({ &root });

	auto cur = [&]() -> FormatContext& { return *path.back().node; };

	// Open a child scope: append to cur().children, push a new frame.
	auto openScope = [&](FormatContext::Kind k, int idx)
	{
		FormatContext child;
		child.kind       = k;
		child.openSrcIdx = idx;
		cur().children.push_back(std::move(child));
		path.push_back({ &cur().children.back() });
	};

	// Close top-of-stack scope: seal last keyword entry, set closeSrcIdx, pop frame.
	auto closeScope = [&](int idx)
	{
		auto& kws = cur().keywords;
		if (!kws.empty() && kws.back().closeSrcIdx < 0)
			kws.back().closeSrcIdx = idx;
		cur().closeSrcIdx = idx;
		if (path.size() > 1) path.pop_back();
	};

	// Add a keyword entry; simultaneously closes the previous clause range.
	auto addKeyword = [&](EToken t, int idx)
	{
		auto& kws = cur().keywords;
		if (!kws.empty() && kws.back().closeSrcIdx < 0)
			kws.back().closeSrcIdx = idx;
		ClauseEntry ce;
		ce.token  = t;
		ce.srcIdx = idx;
		kws.push_back(ce);
	};

	int parenDepth = 0;
	std::vector<int> casePathDepth;

	const int N = (int)tokens.size();
	for (int i = 0; i < N; ++i)
	{
		const auto& tk = tokens[i];
		if (tk.token == etEOL) continue;

		// ---- propagate hasComment to current scope and its innermost open keyword ----
		if (tk.token == etCOMMENT
		&&  tk.value.size() >= 2
		&&  tk.value[0] == L'-' && tk.value[1] == L'-')
		{
			cur().hasComment = true;
			auto& kws = cur().keywords;
			if (!kws.empty() && kws.back().closeSrcIdx < 0)
				kws.back().hasComment = true;
			continue;
		}

		// ---- statement boundary ----
		if (tk.token == etSEMICOLON || tk.token == etEOS)
		{
			while (path.size() > 1)
				closeScope(i);
			casePathDepth.clear();
			parenDepth = 0;
			continue;
		}

		// ---- SELECT ----
		if (tk.token == etSELECT)
		{
			openScope(FormatContext::Select, i);
			addKeyword(etSELECT, i);
			continue;
		}

		// ---- DELETE ----
		if (tk.token == etDELETE)
		{
			// Inside a Merge scope: DELETE is a sub-clause keyword (handled above).
			if (cur().kind != FormatContext::Merge)
			{
				openScope(FormatContext::Delete, i);
				addKeyword(etDELETE, i);
			}
			continue;
		}

		// ---- UPDATE ----
		if (tk.token == etUPDATE)
		{
			// Inside a Merge scope: UPDATE is a sub-clause keyword (handled above).
			if (cur().kind != FormatContext::Merge)
			{
				openScope(FormatContext::Update, i);
				addKeyword(etUPDATE, i);
			}
			continue;
		}

		// ---- INSERT ----
		if (tk.token == etINSERT)
		{
			// Inside a Merge scope: INSERT is a sub-clause keyword, not a new scope.
			if (cur().kind == FormatContext::Merge)
			{
				addKeyword(etINSERT, i);
				continue;
			}
			// Choose scope kind after peeking: if ALL or FIRST follows → conditional.
			// We open InsertSimple first; if we later see WHEN we can't easily change
			// the kind, so we use InsertConditional for all ALL/FIRST forms and
			// InsertSimple for the plain single-INTO form.  We'll refine on seeing
			// ALL/FIRST.
			openScope(FormatContext::InsertSimple, i);
			addKeyword(etINSERT, i);
			continue;
		}

		// ---- MERGE ----
		if (tk.token == etMERGE)
		{
			openScope(FormatContext::Merge, i);
			addKeyword(etMERGE, i);
			continue;
		}

		// ---- MERGE clause keywords ----
		if (cur().kind == FormatContext::Merge)
		{
			if (tk.token == etUSING
			||  tk.token == etON
			||  tk.token == etWHEN
			||  tk.token == etUPDATE
			||  tk.token == etSET
			||  tk.token == etDELETE
			||  tk.token == etVALUES
			||  tk.token == etWHERE
			||  tk.token == etRETURNING
			||  tk.token == etLOG)
			{
				addKeyword(tk.token, i);
				continue;
			}
			// THEN: only record as keyword so width spans cover MATCHED..THEN..body
			if (tk.token == etTHEN)
			{
				addKeyword(etTHEN, i);
				continue;
			}
			// INTO: record only after RETURNING (disambiguate from MERGE INTO / USING)
			if (tk.token == etINTO)
			{
				auto& kws = cur().keywords;
				if (!kws.empty() && kws.back().token == etRETURNING)
					addKeyword(etINTO, i);
				continue;
			}
		}

		// ---- INSERT clause keywords ----
		if (cur().kind == FormatContext::InsertSimple
		||  cur().kind == FormatContext::InsertConditional)
		{
			// ALL/FIRST: upgrade scope kind to InsertConditional
			if (tk.token == etALL
			|| (tk.token == etUNKNOWN && !_wcsicmp(tk.value.c_str(), L"FIRST")))
			{
				cur().kind = FormatContext::InsertConditional;
				continue;
			}
			// RETURNING and LOG are always clause boundaries.
			if (tk.token == etRETURNING
			||  tk.token == etLOG)
			{
				addKeyword(tk.token, i);
				continue;
			}
			// INTO: only add as a keyword entry when it follows RETURNING
			// (i.e. it is the RETURNING INTO, not the INSERT INTO table clause).
			// Non-RETURNING INTOs are handled directly by the formatter loop.
			if (tk.token == etINTO)
			{
				auto& kws = cur().keywords;
				if (!kws.empty() && kws.back().token == etRETURNING)
					addKeyword(etINTO, i);
				continue; // always consume — never falls through to child-scope handling
			}
			// VALUES: clause boundary for width measurement.
			if (tk.token == etVALUES)
			{
				addKeyword(tk.token, i);
				continue;
			}
			// Conditional-form only: WHEN, THEN, ELSE
			if (cur().kind == FormatContext::InsertConditional)
			{
				if (tk.token == etWHEN
				||  tk.token == etTHEN
				||  tk.token == etELSE)
				{
					addKeyword(tk.token, i);
					continue;
				}
			}
		}

		// ---- UPDATE clause keywords ----
		if (cur().kind == FormatContext::Update)
		{
			if (tk.token == etSET
			||  tk.token == etWHERE
			||  tk.token == etRETURNING
			||  tk.token == etINTO
			||  tk.token == etLOG)
			{
				addKeyword(tk.token, i);
				continue;
			}
		}

		// ---- DELETE clause keywords ----
		if (cur().kind == FormatContext::Delete)
		{
			// etFROM is optional and stays on the same line — not a clause boundary.
			if (tk.token == etFROM)
				continue;
			if (tk.token == etWHERE
			||  tk.token == etRETURNING
			||  tk.token == etINTO)
			{
				addKeyword(tk.token, i);
				continue;
			}
		}

		// ---- SELECT clause keywords ----
		if (cur().kind == FormatContext::Select)
		{
			// Set operators terminate the current SELECT scope but are NOT
			// clause entries inside it — they are statement-level connectors.
			// Close the last open clause range, then close the SELECT scope.
			if (tk.token == etUNION
			||  tk.token == etINTERSECT
			||  tk.token == etMINUS_SQL)
			{
				auto& kws = cur().keywords;
				if (!kws.empty() && kws.back().closeSrcIdx < 0)
					kws.back().closeSrcIdx = i;
				closeScope(i);
				continue;
			}

			if (tk.token == etFROM
			|| tk.token == etWHERE
			|| tk.token == etHAVING
			|| tk.token == etGROUP
			|| tk.token == etORDER)
			{
				addKeyword(tk.token, i);
				continue;
			}
			// INTO between SELECT col-list and FROM (PL/SQL SELECT INTO).
			// Only at paren depth 0; ignore INTO inside subexpressions.
			if (tk.token == etINTO && parenDepth == 0)
			{
				addKeyword(etINTO, i);
				continue;
			}
			if (tk.token == etBY)
			{
				bool afterGroupOrder = false;
				for (int k = i - 1; k >= 0; --k)
				{
					if (tokens[k].token == etEOL) continue;
					afterGroupOrder = (tokens[k].token == etGROUP
								   || tokens[k].token == etORDER);
					break;
				}
				if (afterGroupOrder)
				{
					addKeyword(etBY, i);
					continue;
				}
			}
		}

		// ---- CASE ----
		if (tk.token == etCASE)
		{
			openScope(FormatContext::CaseExpr, i);
			casePathDepth.push_back((int)path.size());
			continue;
		}

		// ---- CASE clause keywords and END ----
		if (!casePathDepth.empty()
		&&  (int)path.size() >= casePathDepth.back())
		{
			// THEN is subordinate to WHEN (part of its span), not a peer clause.
			// It must not close the open WHEN entry — the WHEN range must extend
			// all the way to the next WHEN/ELSE/END so that formattedLen covers
			// "cond THEN value" and the short-WHEN heuristic works correctly.
			if (tk.token == etTHEN)
				continue;

			if (tk.token == etWHEN || tk.token == etELSE)
				{
					for (int p = (int)path.size() - 1; p >= 0; --p)
					{
						if (path[p].node->kind == FormatContext::CaseExpr)
						{
							auto& kws = path[p].node->keywords;
							if (!kws.empty() && kws.back().closeSrcIdx < 0)
								kws.back().closeSrcIdx = i;
							ClauseEntry ce;
							ce.token  = tk.token;
							ce.srcIdx = i;
							kws.push_back(ce);
							break;
						}
					}
					continue;
				}
			if (tk.token == etEND)
			{
				while (path.size() > 1 && cur().kind != FormatContext::CaseExpr)
					closeScope(i);
				if (cur().kind == FormatContext::CaseExpr)
				{
					closeScope(i);
					if (!casePathDepth.empty()) casePathDepth.pop_back();
				}
				continue;
			}
		}

		// ---- parentheses ----
		if (tk.token == etLEFT_ROUND_BRACKET)
		{
			++parenDepth;
			openScope(FormatContext::Paren, i);
			continue;
		}
		if (tk.token == etRIGHT_ROUND_BRACKET && parenDepth > 0)
		{
			--parenDepth;
			while (path.size() > 1 && cur().kind != FormatContext::Paren)
				closeScope(i);
			if (cur().kind == FormatContext::Paren)
				closeScope(i);
			while (!casePathDepth.empty()
			&&      casePathDepth.back() > (int)path.size())
				casePathDepth.pop_back();
			continue;
		}

		// ---- BETWEEN…AND ----
		if (tk.token == etUNKNOWN
		&& !_wcsicmp(tk.value.c_str(), L"BETWEEN"))
		{
			openScope(FormatContext::BetweenExpr, i);
			continue;
		}
		if (tk.token == etAND && cur().kind == FormatContext::BetweenExpr)
		{
			closeScope(i);
			continue;
		}
	}

	while (path.size() > 1)
		closeScope(N > 0 ? N - 1 : 0);

	return root;
}

// ============================================================================
// calcWidths — formatter-policy width pass.
// ============================================================================

static bool widthNeedsSpace(EToken prev, EToken cur)
{
	if (prev == etNONE)                        return false;
	if (prev == etDOT  || cur == etDOT)        return false;
	if (prev == etLEFT_ROUND_BRACKET)          return false;
	if (cur  == etLEFT_ROUND_BRACKET)          return false;
	if (cur  == etRIGHT_ROUND_BRACKET)         return false;
	if (cur  == etCOMMA || cur == etSEMICOLON) return false;
	return true;
}

struct ChildRange { int open; int close; int len; };

static int accumWidth(
	const std::vector<OpenEditor::Token>& tokens,
	int from, int to,
	const std::vector<ChildRange>& childRanges,
	EToken& prevToken,
	bool&   tainted)
{
	int width = 0;
	int i = from;
	while (i < to)
	{
		bool skipped = false;
		for (const auto& cr : childRanges)
		{
			if (i == cr.open)
			{
				if (cr.len == INT_MAX) { tainted = true; }
				else
				{
					if (widthNeedsSpace(prevToken, etLEFT_ROUND_BRACKET))
						width += 1;
					width += cr.len;
					prevToken = etRIGHT_ROUND_BRACKET;
				}
				i = cr.close + 1;
				skipped = true;
				break;
			}
		}
		if (skipped) continue;

		const auto& tk = tokens[i];
		++i;
		if (tk.token == etEOL) continue;
		if (tk.token == etCOMMENT)
		{
			if (tk.value.size() >= 2 && tk.value[0] == L'-' && tk.value[1] == L'-')
				tainted = true;
			continue;
		}
		if (widthNeedsSpace(prevToken, tk.token))
			width += 1;
		width += (int)tk.value.size();
		prevToken = tk.token;
	}
	return width;
}

static int calcWidthsNode(
	FormatContext& node,
	const std::vector<OpenEditor::Token>& tokens)
{
	std::vector<ChildRange> childRanges;
	for (auto& child : node.children)
	{
		int cw = calcWidthsNode(child, tokens);
		if (child.openSrcIdx >= 0 && child.closeSrcIdx >= 0)
			childRanges.push_back({ child.openSrcIdx, child.closeSrcIdx, cw });
		if (child.hasComment)
			node.hasComment = true;
	}
	std::sort(childRanges.begin(), childRanges.end(),
		[](const ChildRange& a, const ChildRange& b){ return a.open < b.open; });

	if (node.openSrcIdx < 0 || node.closeSrcIdx < 0)
	{
		node.formattedLen = INT_MAX;
		return INT_MAX;
	}

	// measure each ClauseEntry range
	for (auto& ce : node.keywords)
	{
		if (ce.srcIdx < 0 || ce.closeSrcIdx < 0) continue;
		if (ce.hasComment) { ce.formattedLen = INT_MAX; continue; }
		EToken prev  = etNONE;
		bool tainted = false;
		int w = accumWidth(tokens, ce.srcIdx + 1, ce.closeSrcIdx,
						   childRanges, prev, tainted);
		if (tainted) { ce.hasComment = true; ce.formattedLen = INT_MAX; }
		else         { ce.formattedLen = w; }
	}

	// measure the whole scope
	if (node.hasComment)
	{
		node.formattedLen = INT_MAX;
		return INT_MAX;
	}

	EToken prev  = etNONE;
	bool tainted = false;
	int w = accumWidth(tokens, node.openSrcIdx, node.closeSrcIdx,
					   childRanges, prev, tainted);
	if (node.kind == FormatContext::Paren)
		w += 2;

	if (tainted) { node.hasComment = true; node.formattedLen = INT_MAX; return INT_MAX; }
	node.formattedLen = w;
	return w;
}

void calcWidths(FormatContext& root,
				const std::vector<OpenEditor::Token>& tokens)
{
	for (auto& child : root.children)
		calcWidthsNode(child, tokens);
}

} // namespace OpenEditor
