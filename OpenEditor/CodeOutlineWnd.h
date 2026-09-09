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

#pragma once

#include "LabelWithWaitBar.h"
#include "COutlineTreeCtrl.h"
#include "OpenEditor/OEHelpers.h"   // OutlineItem, OutlineItems

class COEDocument;

// Per-document outline context — items plus user's expand choices
struct DocOutlineContext
{
	OpenEditor::OutlineItems items;
	std::vector<bool>        expanded; // parallel to items: true = node is expanded
};

// CodeOutlineWnd — composite dockable pane for code outline.
//
// Layout:  [  CTreeCtrlEx (fills top, resizes)  ]
//          [  LabelWithWaitBar (fixed height)   ]
//
// Polls PlSqlSupport on every WM_IDLEUPDATECMDUI tick by comparing
// Storage::GetActionId() — no cross-lifetime observer pointers.
//
class CodeOutlineWnd : public CWnd
{
public:
	CodeOutlineWnd ();
	virtual ~CodeOutlineWnd ();

	BOOL Create (CWnd* pParent, UINT nID);

	void SetDocument         (COEDocument* pDoc);
	void OnDocumentDestroyed (COEDocument* pDoc);

private:
	COutlineTreeCtrl m_tree;
	LabelWithWaitBar m_status;
	CImageList       m_images;

	COEDocument* m_pActiveDoc;

	std::map<COEDocument*, DocOutlineContext> m_docContexts;

	OpenEditor::SequenceId m_lastParsedActionId;
	int                    m_lastParserInstanceId;
	OpenEditor::Position   m_lastCursorPos;

	// Save expand state of current tree into the active doc's context before
	// clearing the tree.
	void SaveExpandState ();
	void DoRefreshTree   ();

	// Merge newItems into ctx:
	// - Tier 1: same structure → update positions only, no tree rebuild
	// - Tier 2: structure changed → remap expand flags, rebuild tree
	// Returns true if the tree was rebuilt.
	bool MergeOutline (DocOutlineContext& ctx, const OpenEditor::OutlineItems& newItems);

	void PopulateTree    (const DocOutlineContext& ctx);
	void DoHighlightCurrentItem (OpenEditor::Position cursorPos);
	void SetStatus       (LPCTSTR text, COLORREF color, bool animate = false);

	DECLARE_MESSAGE_MAP()
	afx_msg int  OnCreate (LPCREATESTRUCT lpcs);
	afx_msg void OnSize   (UINT nType, int cx, int cy);
	afx_msg void OnDestroy ();
	afx_msg void OnIdleUpdateCmdUI ();
	afx_msg void OnItemExpanded  (NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDblclkTree    (NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnRClickTree    (NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnContextMenu   (CWnd* /*pWnd*/, CPoint /*point*/);
	afx_msg void OnSettings ();
	afx_msg LRESULT OnCommandHelp(WPARAM, LPARAM);


	void NavigateToItem (const OpenEditor::OutlineItem& item, BOOL focus);
	void SelectOutline  (const OpenEditor::OutlineItem& item, BOOL focus);

	BOOL PreTranslateMessage (MSG* pMsg);
	LRESULT WindowProc (UINT message, WPARAM wParam, LPARAM lParam);
};
