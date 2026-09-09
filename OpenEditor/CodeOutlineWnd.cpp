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
#include "CodeOutlineWnd.h"
#include "OpenEditor/OEDocument.h"
#include "OpenEditor/OEStorage.h"
#include "OpenEditor/OEPlsSqlSupport.h"
#include "OpenEditor/OEView.h"
#include <COMMON\AppUtilities.h>
#include <resource1.h>
#include "hlp\HTMLDefines.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

static const int STATUS_HEIGHT = 18;

static const COLORREF COLOR_FRESH   = RGB(  0,   0,   0);
static const COLORREF COLOR_PARSING = RGB(100, 100, 100);
static const COLORREF COLOR_ERROR   = RGB(180,   0,   0);
static const COLORREF COLOR_NONE    = RGB(100, 100, 100);

// Maps outline item type to image index in the IDB_DB_OBJ strip.
// Values match IDII_* constants in DbBrowser/DBBrowserList.h.
static int OutlineTypeToImageIndex (OpenEditor::EOutlineItemType type)
{
    switch (type)
    {
    case OpenEditor::oitFunction:     return  2;  
    case OpenEditor::oitProcedure:    return  3;  
    case OpenEditor::oitPackage:      return  4;  
    case OpenEditor::oitPackageBody:  return  5;  
    case OpenEditor::oitTypeBody:     return  7;  
    case OpenEditor::oitSelect:       return  8;  
    case OpenEditor::oitFromClause:   return  9;
    case OpenEditor::oitWhereClause:  return 10;
    default:                          return -1;
    }
}

///////////////////////////////////////////////////////////////////////////////

CodeOutlineWnd::CodeOutlineWnd ()
: m_pActiveDoc(nullptr)
, m_lastCursorPos{}
{
}

CodeOutlineWnd::~CodeOutlineWnd ()
{
}

BOOL CodeOutlineWnd::Create (CWnd* pParent, UINT nID)
{
	static LPCTSTR pszClass = AfxRegisterWndClass(
		CS_HREDRAW | CS_VREDRAW,
		::LoadCursor(NULL, IDC_ARROW),
		(HBRUSH)(COLOR_BTNFACE + 1),
		NULL);

	return CWnd::Create(pszClass, NULL,
		WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
		CRect(0, 0, 0, 0), pParent, nID);
}

BEGIN_MESSAGE_MAP(CodeOutlineWnd, CWnd)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_DESTROY()
	ON_MESSAGE_VOID(WM_IDLEUPDATECMDUI, OnIdleUpdateCmdUI)
	ON_NOTIFY(TVN_ITEMEXPANDED, AFX_IDW_PANE_FIRST, OnItemExpanded)
	ON_NOTIFY(NM_DBLCLK,        AFX_IDW_PANE_FIRST, OnDblclkTree)
	ON_NOTIFY(NM_RCLICK,        AFX_IDW_PANE_FIRST, OnRClickTree)
	ON_WM_CONTEXTMENU()
	ON_MESSAGE(WM_COMMANDHELP, OnCommandHelp)
END_MESSAGE_MAP()

///////////////////////////////////////////////////////////////////////////////

int CodeOutlineWnd::OnCreate (LPCREATESTRUCT lpcs)
{
	if (CWnd::OnCreate(lpcs) == -1)
		return -1;

	if (!m_tree.Create(
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TVS_FULLROWSELECT
		| TVS_LINESATROOT | TVS_HASBUTTONS | TVS_SHOWSELALWAYS,
		CRect(0, 0, 0, 0), this, AFX_IDW_PANE_FIRST))
		return -1;

	// Load the same icon strip that ObjectTree uses
	if (m_images.Create(IDB_CODE_OUTLINE, 16, 1, RGB(0, 255, 0)))
		m_tree.SetImageList(&m_images, TVSIL_NORMAL);

	if (!m_status.Create(NULL, WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
		CRect(0, 0, 0, STATUS_HEIGHT), this, AFX_IDW_PANE_FIRST + 1))
		return -1;

	m_status.ModifyStyle(0, SS_OWNERDRAW);
	m_status.AlignLeft();
	m_status.SetFont(m_tree.GetFont());
	SetStatus(L"No outline available", COLOR_NONE);

	SetWindowContextHelpId(HIDD_OE_PLSQL_OUTLINE_TREE); // use same as settings for now
	m_tree.SetWindowContextHelpId(HIDD_OE_PLSQL_OUTLINE_TREE);

	return 0;
}

void CodeOutlineWnd::OnSize (UINT nType, int cx, int cy)
{
	CWnd::OnSize(nType, cx, cy);
	if (!m_tree.GetSafeHwnd() || cx <= 0 || cy <= 0)
		return;

	int th = max(0, cy - STATUS_HEIGHT);
	m_tree.SetWindowPos  (nullptr, 0, 0,   cx, th,           SWP_NOZORDER | SWP_NOACTIVATE);
	m_status.SetWindowPos(nullptr, 0, th,  cx, STATUS_HEIGHT, SWP_NOZORDER | SWP_NOACTIVATE);
}

void CodeOutlineWnd::OnDestroy ()
{
	m_pActiveDoc = nullptr;
	CWnd::OnDestroy();
}

///////////////////////////////////////////////////////////////////////////////
// Document management

void CodeOutlineWnd::SetDocument (COEDocument* pDoc)
{
	if (pDoc == m_pActiveDoc)
		return;

	SaveExpandState();

	m_pActiveDoc          = pDoc;
	m_lastParsedActionId  = OpenEditor::SequenceId();
	m_lastParserInstanceId = 0;
	m_lastCursorPos       = { -1, -1 };

	m_tree.DeleteAllItems();

	if (!m_pActiveDoc)
	{
		SetStatus(L"No outline available", COLOR_NONE);
		return;
	}

	// Restore cached outline immediately if available from a previous parse
	auto it = m_docContexts.find(m_pActiveDoc);
	if (it != m_docContexts.end() && !it->second.items.empty())
	{
		PopulateTree(it->second);
		SetStatus(L"", COLOR_FRESH);
	}
	else
	{
		SetStatus(L"Parsing\u2026", COLOR_PARSING, true);
	}
}

void CodeOutlineWnd::OnDocumentDestroyed (COEDocument* pDoc)
{
	m_docContexts.erase(pDoc);

	if (pDoc == m_pActiveDoc)
	{
		m_pActiveDoc          = nullptr;
		m_lastParsedActionId  = OpenEditor::SequenceId();
		m_lastParserInstanceId = 0;
		m_lastCursorPos       = { -1, -1 };
		m_tree.DeleteAllItems();
		SetStatus(L"No outline available", COLOR_NONE);
	}
}

///////////////////////////////////////////////////////////////////////////////
// Idle polling — the heart of the design

void CodeOutlineWnd::OnIdleUpdateCmdUI ()
{
	if (!m_pActiveDoc)
		return;

	OpenEditor::Storage& storage = m_pActiveDoc->GetStorage();

	// Check settings gates before doing any outline work
	const OpenEditor::Settings& settings = storage.GetSettings();
	if (!settings.GetPlSqlKeywordNavigation())
	{
		if (m_tree.GetRootItem())
			m_tree.DeleteAllItems();
		SetStatus(L"Outline requires PL/SQL keyword navigation (check settings)", COLOR_NONE);
		return;
	}
	if (!settings.GetPlSqlOutlineEnabled())
	{
		if (m_tree.GetRootItem())
			m_tree.DeleteAllItems();
		SetStatus(L"Outline disabled (check settings)", COLOR_NONE);
		return;
	}

	auto* pPlSql = dynamic_cast<OpenEditor::PlSqlSupport*>(
		storage.GetLanguageSupport().get());

	if (!pPlSql)
	{
		if (m_tree.GetRootItem())
			m_tree.DeleteAllItems();
		SetStatus(L"Only PL/SQL is supported", COLOR_NONE);
		return;
	}

	// Detect a newly completed parse: compare the id the parser finished with
	// against the id we last reflected in the tree. Using GetParsedActionId()
	// (the storage id snapshotted at parse start) rather than the live
	// GetActionId() avoids the one-edit-behind race where a second edit
	// arrives while the first parse is still running.
	OpenEditor::SequenceId parsedActionId  = pPlSql->GetParsedActionId();
	int                    parserInstanceId = pPlSql->GetInstanceId();
	bool parseJustFinished = pPlSql->IsDone()
						  && (parsedActionId  != m_lastParsedActionId
						  ||  parserInstanceId != m_lastParserInstanceId);

	if (parseJustFinished)
	{
		m_lastParsedActionId   = parsedActionId;
		m_lastParserInstanceId = parserInstanceId;
		DoRefreshTree();
	}

	// Update status strip
	if (!pPlSql->IsDone())
	{
		SetStatus(L"Parsing\u2026", COLOR_PARSING, true);
	}
	else
	{
		auto cit = m_docContexts.find(m_pActiveDoc);
		bool hasItems = (cit != m_docContexts.end() && !cit->second.items.empty());

		if (!hasItems)
			SetStatus(L"No outline available", COLOR_NONE);
		else
		{
			switch (pPlSql->GetLastOutlineQuality())
			{
			case OpenEditor::PlSqlSupport::oqClean:
				SetStatus(L"", COLOR_FRESH);
				break;
			case OpenEditor::PlSqlSupport::oqTruncated:
				SetStatus(L"File limit reached \u2014 outline may be incomplete", COLOR_ERROR);
				break;
			case OpenEditor::PlSqlSupport::oqSyntaxError:
				SetStatus(L"Syntax error \u2014 showing partial outline", COLOR_ERROR);
				break;
			}
		}
	}

	// Highlight current cursor position — query the active view directly
	OpenEditor::Position cursorPos{ -1, -1 };
	POSITION posView = m_pActiveDoc->GetFirstViewPosition();
	if (posView)
	{
		if (auto* pView = dynamic_cast<COEditorView*>(m_pActiveDoc->GetNextView(posView)))
			cursorPos = pView->GetPosition();
	}
	if (cursorPos != m_lastCursorPos)
		DoHighlightCurrentItem(cursorPos);
}

///////////////////////////////////////////////////////////////////////////////
// Tree population

void CodeOutlineWnd::DoRefreshTree ()
{
	if (!m_pActiveDoc)
		return;

	OpenEditor::Storage& storage = m_pActiveDoc->GetStorage();
	auto* pPlSql = dynamic_cast<OpenEditor::PlSqlSupport*>(
		storage.GetLanguageSupport().get());
	if (!pPlSql)
		return;

	const OpenEditor::OutlineItems& lastOutline = pPlSql->GetLastOutline();

	if (!lastOutline.empty())
	{
		SaveExpandState();
		DocOutlineContext& ctx = m_docContexts[m_pActiveDoc];
		bool rebuilt = MergeOutline(ctx, lastOutline);
		if (rebuilt)
			PopulateTree(ctx);
	}
}

bool CodeOutlineWnd::MergeOutline (DocOutlineContext& ctx,
								   const OpenEditor::OutlineItems& newItems)
{
	const int newSize = static_cast<int>(newItems.size());
	const int oldSize = static_cast<int>(ctx.items.size());

	// -----------------------------------------------------------------------
	// Tier 1: same count and same structure — update positions only, no rebuild
	// -----------------------------------------------------------------------
	if (newSize == oldSize && oldSize > 0)
	{
		bool sameStructure = true;
		for (int i = 0; i < newSize; ++i)
		{
			const auto& n = newItems[i];
			const auto& o = ctx.items[i];
			if (n.depth != o.depth || n.type != o.type || n.label != o.label)
			{
				sameStructure = false;
				break;
			}
		}
		if (sameStructure)
		{
			for (int i = 0; i < newSize; ++i)
				ctx.items[i].pos = newItems[i].pos;
			return false; // no rebuild needed
		}
	}

	// -----------------------------------------------------------------------
	// Tier 2: structure changed — remap expand flags then signal rebuild
	// -----------------------------------------------------------------------
	const bool firstLoad = (oldSize == 0 && ctx.expanded.empty());

	std::vector<bool> newExpanded(newSize, false);

	if (!firstLoad)
	{
		// Forward-scan match on (depth, type, label) to preserve expand state
		// across insertions/deletions; duplicate labels are matched in order.
		int oldCursor = 0;
		for (int ni = 0; ni < newSize; ++ni)
		{
			const auto& nItem = newItems[ni];
			bool matched = false;
			for (int oi = oldCursor; oi < oldSize; ++oi)
			{
				const auto& oItem = ctx.items[oi];
				if (oItem.depth == nItem.depth &&
					oItem.type  == nItem.type  &&
					oItem.label == nItem.label)
				{
					newExpanded[ni] = ctx.expanded[oi];
					oldCursor = oi + 1;
					matched = true;
					break;
				}
			}
			// Brand-new depth-0 item with no old counterpart: expand by default
			if (!matched && nItem.depth == 0)
				newExpanded[ni] = true;
		}
	}
	else
	{
		// First load: expand all depth-0 items by default
		for (int i = 0; i < newSize; ++i)
			if (newItems[i].depth == 0)
				newExpanded[i] = true;
	}

	ctx.items    = newItems;
	ctx.expanded = std::move(newExpanded);
	return true; // caller must call PopulateTree
}

void CodeOutlineWnd::SaveExpandState ()
{
	if (!m_pActiveDoc)
		return;

	auto it = m_docContexts.find(m_pActiveDoc);
	if (it == m_docContexts.end())
		return;

	it->second.expanded.assign(it->second.items.size(), false);

	const auto& items = it->second.items;

	// Walk the whole tree depth-first
	HTREEITEM h = m_tree.GetRootItem();
	while (h)
	{
		if (m_tree.GetItemState(h, TVIS_EXPANDED) & TVIS_EXPANDED)
		{
			int idx = static_cast<int>(m_tree.GetItemData(h));
			if (idx >= 0 && idx < static_cast<int>(items.size()))
				it->second.expanded[idx] = true;
		}
		HTREEITEM hChild = m_tree.GetChildItem(h);
		if (hChild) { h = hChild; continue; }
		HTREEITEM hSib = m_tree.GetNextSiblingItem(h);
		if (hSib) { h = hSib; continue; }
		// Climb up until we find an ancestor with a next sibling
		for (h = m_tree.GetParentItem(h); h; h = m_tree.GetParentItem(h))
		{
			HTREEITEM hParSib = m_tree.GetNextSiblingItem(h);
			if (hParSib) { h = hParSib; break; }
		}
	}
}

void CodeOutlineWnd::PopulateTree (const DocOutlineContext& ctx)
{
	const OpenEditor::OutlineItems& items = ctx.items;

	m_tree.SetRedraw(FALSE);
	m_tree.DeleteAllItems();

	const bool hasImages = (m_images.GetSafeHandle() != nullptr);

	// --- Pass 1: insert all items ---
	std::vector<HTREEITEM> parentStack;
	parentStack.push_back(TVI_ROOT);
	HTREEITEM hLast = TVI_ROOT;

	std::vector<HTREEITEM> hItems; // parallel to items[]
	hItems.reserve(items.size());

	for (int i = 0; i < static_cast<int>(items.size()); ++i)
	{
		const auto& item = items[i];

		while (static_cast<int>(parentStack.size()) <= item.depth)
			parentStack.push_back(hLast);

		HTREEITEM hParent = parentStack[item.depth];

		TVINSERTSTRUCT tvis  = {};
		tvis.hParent         = hParent;
		tvis.hInsertAfter    = TVI_LAST;
		tvis.item.mask       = TVIF_TEXT | TVIF_PARAM;
		tvis.item.pszText    = const_cast<wchar_t*>(item.label.c_str());
		tvis.item.lParam     = static_cast<LPARAM>(i);

		if (hasImages)
		{
			int imgIdx = OutlineTypeToImageIndex(item.type);
			if (imgIdx >= 0)
			{
				tvis.item.mask          |= TVIF_IMAGE | TVIF_SELECTEDIMAGE;
				tvis.item.iImage         = imgIdx;
				tvis.item.iSelectedImage = imgIdx;
			}
		}

		hLast = m_tree.InsertItem(&tvis);
		hItems.push_back(hLast);

		if (item.depth + 1 < static_cast<int>(parentStack.size()))
			parentStack[item.depth + 1] = hLast;
		else
			parentStack.push_back(hLast);
	}

	// --- Pass 2: expand nodes flagged in the parallel expanded vector ---
	for (int i = 0; i < static_cast<int>(hItems.size()); ++i)
	{
		if (i < static_cast<int>(ctx.expanded.size()) && ctx.expanded[i])
			m_tree.Expand(hItems[i], TVE_EXPAND);
	}

	m_tree.SetRedraw(TRUE);
	m_tree.Invalidate();

	m_lastCursorPos = { -1, -1 };
	DoHighlightCurrentItem({ -1, -1 }); // will re-query via OnIdleUpdateCmdUI next tick
}

void CodeOutlineWnd::DoHighlightCurrentItem (OpenEditor::Position cursorPos)
{
	m_lastCursorPos = cursorPos;

	if (cursorPos.line < 0 || !m_pActiveDoc)
		return;

	auto it = m_docContexts.find(m_pActiveDoc);
	if (it == m_docContexts.end() || it->second.items.empty())
		return;

	const OpenEditor::OutlineItems& items = it->second.items;

	// Find the deepest item whose Square contains the cursor position.
	// For the start boundary: cursor must be at or after start (line, then col).
	// For the end boundary:   cursor must be at or before end   (line, then col).
	auto after_or_at_start = [](OpenEditor::Position cursor, OpenEditor::Position start) {
		return start.line < cursor.line ||
			  (start.line == cursor.line && start.column <= cursor.column);
	};
	// end.column == 0 is a "end-of-line" sentinel — any column on that line qualifies.
	auto before_or_at_end = [](OpenEditor::Position cursor, OpenEditor::Position end) {
		if (end.column == 0)
			return cursor.line <= end.line;
		return cursor.line < end.line ||
			  (cursor.line == end.line && cursor.column <= end.column);
	};

	int bestIndex = -1;
	for (int i = 0; i < static_cast<int>(items.size()); ++i)
	{
		const auto& item = items[i];
		if (after_or_at_start(cursorPos, item.pos.start) &&
			before_or_at_end (cursorPos, item.pos.end))
		{
			if (bestIndex < 0 || item.depth >= items[bestIndex].depth)
				bestIndex = i;
		}
	}
	if (bestIndex < 0)
		return;

	HTREEITEM hSelItem = m_tree.GetSelectedItem();
	// Walk all tree items depth-first to find the one with lParam == bestIndex
	for (HTREEITEM hItem = m_tree.GetRootItem(); hItem; )
	{
		if (static_cast<int>(m_tree.GetItemData(hItem)) == bestIndex)
		{
			if (hItem != hSelItem)
			{
				m_tree.SelectItem(hItem);
				m_tree.EnsureVisible(hItem);
			}
			return;
		}
		HTREEITEM hChild = m_tree.GetChildItem(hItem);
		if (hChild) { hItem = hChild; continue; }
		HTREEITEM hSib = m_tree.GetNextSiblingItem(hItem);
		if (hSib)  { hItem = hSib;   continue; }
		// Climb up until we find an ancestor with a next sibling
		for (hItem = m_tree.GetParentItem(hItem); hItem; hItem = m_tree.GetParentItem(hItem))
		{
			HTREEITEM hParSib = m_tree.GetNextSiblingItem(hItem);
			if (hParSib) { hItem = hParSib; break; }
		}
	}
}

void CodeOutlineWnd::SetStatus (LPCTSTR text, COLORREF color, bool animate)
{
	if (!m_status.GetSafeHwnd())
		return;
	m_status.SetText(text, color);
	m_status.StartAnimation(animate);
}

///////////////////////////////////////////////////////////////////////////////
// Navigation

// Convert a Position whose column is a raw buffer index (as stored by the
// parser) to a screen position using tab expansion.
static OpenEditor::Position ToScreenPos (COEditorView* pView, OpenEditor::Position p)
{
    p.column = pView->InxToPos(p.line, p.column);
    return p;
}

void CodeOutlineWnd::NavigateToItem (const OpenEditor::OutlineItem& item, BOOL focus)
{
	if (!m_pActiveDoc || item.pos.start.line < 0)
		return;

	POSITION posView = m_pActiveDoc->GetFirstViewPosition();
	if (!posView)
		return;
	auto* pView = dynamic_cast<COEditorView*>(m_pActiveDoc->GetNextView(posView));
	if (!pView || !pView->GetSafeHwnd())
		return;

	CFrameWnd* pFrame = pView->GetParentFrame();
	if (pFrame)
		pFrame->ActivateFrame();

	pView->MoveToAndCenter(ToScreenPos(pView, item.pos.start));
	if (focus)
		pView->SetFocus();
}

void CodeOutlineWnd::SelectOutline (const OpenEditor::OutlineItem& item, BOOL focus)
{
	const bool canSelect = item.pos.start.line >= 0 && item.pos.start.line <= item.pos.end.line;

	if (canSelect && m_pActiveDoc)
	{
		POSITION posView = m_pActiveDoc->GetFirstViewPosition();
		if (posView)
		{
			auto* pView = dynamic_cast<COEditorView*>(m_pActiveDoc->GetNextView(posView));
			if (pView && pView->GetSafeHwnd())
			{
				pView->MoveToAndCenter(ToScreenPos(pView, item.pos.start));
				OpenEditor::Square sel;
				sel.start = ToScreenPos(pView, item.pos.start);
				sel.end = item.pos.end.column == 0
					? OpenEditor::Position{ pView->GetLineLength(item.pos.end.line), item.pos.end.line }
				: ToScreenPos(pView, item.pos.end);
				pView->SetSelection(sel);
				if (CFrameWnd* pFrame = pView->GetParentFrame())
					pFrame->ActivateFrame();
				if (focus)
					pView->SetFocus();
			}
		}
	}
}

void CodeOutlineWnd::OnDblclkTree (NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
	*pResult = 1;
	HTREEITEM hSel = m_tree.GetSelectedItem();
	if (!hSel || !m_pActiveDoc)
		return;

	int idx = static_cast<int>(m_tree.GetItemData(hSel));
	auto it = m_docContexts.find(m_pActiveDoc);
	if (it == m_docContexts.end())
		return;

	const OpenEditor::OutlineItems& items = it->second.items;
	if (idx >= 0 && idx < static_cast<int>(items.size()))
	{
		if ((GetKeyState(VK_SHIFT) & 0x8000) == 0)
			NavigateToItem(items[idx], TRUE);
		else
			SelectOutline(items[idx], TRUE);
	}
}


void CodeOutlineWnd::OnRClickTree (NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
	*pResult = 1;

	// Hit-test so right-click also selects the item under cursor
	CPoint ptCursor;
	::GetCursorPos(&ptCursor);
	CPoint ptClient = ptCursor;
	m_tree.ScreenToClient(&ptClient);

	UINT uFlags = 0;
	HTREEITEM hHit = m_tree.HitTest(ptClient, &uFlags);
	if (hHit)
	{
		m_tree.SelectItem(hHit);
		m_tree.UpdateWindow();
	}

	OnContextMenu(&m_tree, ptCursor);
}

void CodeOutlineWnd::OnContextMenu (CWnd* /*pWnd*/, CPoint point)
{
	// (-1,-1) means keyboard-invoked (Shift+F10 / context-menu key):
	// align to the bottom-left of the selected item, or tree centre if none.
	if (point.x == -1 && point.y == -1)
	{
		HTREEITEM hSel = m_tree.GetSelectedItem();
		if (hSel)
		{
			CRect rc;
			m_tree.GetItemRect(hSel, &rc, FALSE);
			point = CPoint(rc.left, rc.bottom);
			m_tree.ClientToScreen(&point);
		}
		else
		{
			point = { 0, 0 };
			m_tree.ClientToScreen(&point);
		}
	}

	// Resolve the outline item for the selected node
	HTREEITEM hSel = m_tree.GetSelectedItem();
	const OpenEditor::OutlineItem* pItem = nullptr;
	if (hSel && m_pActiveDoc)
	{
		auto it = m_docContexts.find(m_pActiveDoc);
		if (it != m_docContexts.end())
		{
			int idx = static_cast<int>(m_tree.GetItemData(hSel));
			const auto& items = it->second.items;
			if (idx >= 0 && idx < static_cast<int>(items.size()))
				pItem = &items[idx];
		}
	}

	CMenu menu;
	menu.CreatePopupMenu();

	const UINT MID_GOTO         = 1;
	const UINT MID_SELECT       = 2;
	const UINT MID_COPY         = 3;
	const UINT MID_EXPAND_TOP   = 4;
	const UINT MID_EXPAND_ALL   = 5;
	const UINT MID_COLLAPSE     = 6;
	const UINT MID_SETTINGS     = 7;

	const bool canSelect = pItem && pItem->pos.start.line >= 0 && pItem->pos.start.line <= pItem->pos.end.line;

	menu.AppendMenu(MF_STRING | (pItem     ? MF_ENABLED : MF_GRAYED), MID_GOTO,       L"Go to definition\t<Enter>, <DblClick>");
	menu.AppendMenu(MF_STRING | (canSelect ? MF_ENABLED : MF_GRAYED), MID_SELECT,     L"Select\t<Shift>+<Enter>, <Shift>+<DblClick>");
	menu.AppendMenu(MF_STRING | (pItem     ? MF_ENABLED : MF_GRAYED), MID_COPY,       L"Copy name");
	menu.AppendMenu(MF_SEPARATOR);
	menu.AppendMenu(MF_STRING, MID_EXPAND_TOP, L"Expand top level");
	menu.AppendMenu(MF_STRING, MID_EXPAND_ALL, L"Expand all");
	menu.AppendMenu(MF_STRING, MID_COLLAPSE,   L"Collapse all");
	menu.AppendMenu(MF_SEPARATOR);
	menu.AppendMenu(MF_STRING, MID_SETTINGS,   L"Settings...");

	UINT nCmd = menu.TrackPopupMenu(
		TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_LEFTALIGN,
		point.x, point.y, this);

	switch (nCmd)
	{
	case MID_GOTO:
		if (pItem)
			NavigateToItem(*pItem, TRUE);
		break;

	case MID_SELECT:
		if (pItem)
			SelectOutline(*pItem, TRUE);
		break;

	case MID_COPY:
		if (pItem)
		{
			Common::AppSetClipboardText(pItem->label.c_str(), pItem->label.length(), CF_TEXT);
			Common::AppSetClipboardText("{code}", sizeof("{code}"), CF_PRIVATEFIRST);
		}
		break;

	case MID_EXPAND_TOP:
		for (HTREEITEM h = m_tree.GetRootItem(); h; h = m_tree.GetNextSiblingItem(h))
			m_tree.Expand(h, TVE_EXPAND);
		break;

	case MID_EXPAND_ALL:
		m_tree.SetRedraw(FALSE);
		for (HTREEITEM h = m_tree.GetRootItem(); h; )
		{
			m_tree.Expand(h, TVE_EXPAND);
			HTREEITEM hChild = m_tree.GetChildItem(h);
			if (hChild) { h = hChild; continue; }
			HTREEITEM hSib = m_tree.GetNextSiblingItem(h);
			if (hSib)  { h = hSib;   continue; }
			for (h = m_tree.GetParentItem(h); h; h = m_tree.GetParentItem(h))
			{
				HTREEITEM hParSib = m_tree.GetNextSiblingItem(h);
				if (hParSib) { h = hParSib; break; }
			}
		}
		m_tree.SetRedraw(TRUE);
		m_tree.Invalidate();
		break;

	case MID_COLLAPSE:
		m_tree.SetRedraw(FALSE);
		for (HTREEITEM h = m_tree.GetRootItem(); h; h = m_tree.GetNextSiblingItem(h))
			m_tree.Expand(h, TVE_COLLAPSE);
		m_tree.SetRedraw(TRUE);
		m_tree.Invalidate();
		break;

	case MID_SETTINGS:
		OnSettings();
		break;
	}
}

void CodeOutlineWnd::OnItemExpanded (NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;
	if (!m_pActiveDoc)
		return;

	auto it = m_docContexts.find(m_pActiveDoc);
	if (it == m_docContexts.end())
		return;

	NMTREEVIEW* pNMTV = reinterpret_cast<NMTREEVIEW*>(pNMHDR);
	int idx = static_cast<int>(pNMTV->itemNew.lParam);

	auto& expanded = it->second.expanded;
	if (idx >= 0 && idx < static_cast<int>(expanded.size()))
		expanded[idx] = (pNMTV->action == TVE_EXPAND);
}

BOOL CodeOutlineWnd::PreTranslateMessage (MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN
		&& (pMsg->wParam == VK_RETURN || pMsg->wParam == VK_ESCAPE || pMsg->wParam == VK_F1)
		&& m_tree.GetSafeHwnd()
		&& (pMsg->hwnd == m_tree.GetSafeHwnd() || ::IsChild(m_tree.GetSafeHwnd(), pMsg->hwnd)))
	{
		try { EXCEPTION_FRAME;
			if (pMsg->wParam == VK_RETURN)
			{
				HTREEITEM hSel = m_tree.GetSelectedItem();
				if (hSel && m_pActiveDoc)
				{
					auto it = m_docContexts.find(m_pActiveDoc);
					if (it != m_docContexts.end())
					{
						int idx = static_cast<int>(m_tree.GetItemData(hSel));
						const auto& items = it->second.items;
						if (idx >= 0 && idx < static_cast<int>(items.size()))
							if ((GetKeyState(VK_SHIFT) & 0x8000) == 0)
								NavigateToItem(items[idx], FALSE);
							else
								SelectOutline(items[idx], FALSE);
					}
				}
			}
			else if (pMsg->wParam == VK_ESCAPE)
			{
				if (m_pActiveDoc)
					if (POSITION posView = m_pActiveDoc->GetFirstViewPosition())
						if (auto* pView = m_pActiveDoc->GetNextView(posView))
							if (pView->GetSafeHwnd())
								pView->SetFocus();
			}
			else if (pMsg->wParam == VK_F1)
			{
				return TRUE; // swallows F1 — WM_COMMANDHELP is never generated
			}
		}
		_OE_DEFAULT_HANDLER_;
		return TRUE; // consumed
	}
	return CWnd::PreTranslateMessage(pMsg);
}

LRESULT CodeOutlineWnd::WindowProc (UINT message, WPARAM wParam, LPARAM lParam)
{
	try { 
		EXCEPTION_FRAME;

		return CWnd::WindowProc(message, wParam, lParam);
	}
	_OE_DEFAULT_HANDLER_;

	return 0;
}

#include "OpenEditor/OEPlsSqlOutlinePageTree.h"
#include "COMMON/PropertySheetMem.h"

void CodeOutlineWnd::OnSettings ()
{
	SettingsManager mgr(COEDocument::GetSettingsManager());

	COEPlsSqlOutlinePageTree page(mgr);
	page.m_psp.pszTitle = L"PL/SQL Navigation and Outline";
	page.m_psp.dwFlags |= PSP_USETITLE;

	static UINT gStarPage = 0;
	Common::CPropertySheetMem sheet(L"Settings", gStarPage);
	sheet.m_psh.dwFlags |= PSH_NOAPPLYNOW;
	sheet.m_psh.dwFlags &= ~PSH_HASHELP;
	sheet.AddPage(&page);

	int retVal = sheet.DoModal();

	if (retVal == IDOK)
	{
		COEDocument::GetSettingsManagerForUpdate() = mgr;
		COEDocument::SaveSettingsManager();
	}
}

LRESULT CodeOutlineWnd::OnCommandHelp(WPARAM, LPARAM lParam)
{
	if (lParam == 0)
		lParam = GetWindowContextHelpId();

	if (lParam != 0)
	{
		AfxGetApp()->WinHelpInternal(lParam);
		return TRUE;
	}
	return FALSE;
}