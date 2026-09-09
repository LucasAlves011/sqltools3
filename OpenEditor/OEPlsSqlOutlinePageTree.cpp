#include "stdafx.h"
#include <COMMON/ExceptionHelper.h>
#include "OpenEditor/OEPlsSqlOutlinePageTree.h"

COEPlsSqlOutlinePageTree::COEPlsSqlOutlinePageTree (SettingsManager& manager)
	: CPropertyPage(COEPlsSqlOutlinePageTree::IDD)
	, m_manager(manager)
	, m_pKeywordNavigation(nullptr)
	, m_pFileMaxTokens(nullptr)
	, m_pFileMaxLines(nullptr)
	, m_pOutlineEnabled(nullptr)
	, m_pOutlineSelectEnabled(nullptr)
	, m_pOutlineSelectOnly(nullptr)
	, m_pOutlineMaxTopItems(nullptr)
{
}

COEPlsSqlOutlinePageTree::~COEPlsSqlOutlinePageTree ()
{
}

void COEPlsSqlOutlinePageTree::DoDataExchange (CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(COEPlsSqlOutlinePageTree, CPropertyPage)
	ON_NOTIFY(PTN_ITEMCHANGED, IDC_OE_PLSQL_OUTLINE_TREE, OnPropTreeNotify)
	ON_NOTIFY(PTN_CHECKCLICK,  IDC_OE_PLSQL_OUTLINE_TREE, OnPropTreeNotify)
	ON_WM_SIZE()
END_MESSAGE_MAP()

BOOL COEPlsSqlOutlinePageTree::OnInitDialog ()
{
	CPropertyPage::OnInitDialog();

	CRect rc;
	GetClientRect(&rc);

	m_tree.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | PTS_NOTIFY, rc, this, IDC_OE_PLSQL_OUTLINE_TREE);
	m_tree.SetInfoHeight(72);
	m_tree.SetColumn(200);

	BuildTree();

	return TRUE;
}

void COEPlsSqlOutlinePageTree::BuildTree ()
{
	const OpenEditor::GlobalSettingsPtr s = m_manager.GetGlobalSettings();

	// ── Navigation ────────────────────────────────────────────────────────
	CPropTreeItem* pRoot1 = m_tree.InsertItem(new CPropTreeItem());
	pRoot1->SetLabelText(_T("Navigation"));
	pRoot1->SetInfoText(_T("PL/SQL keyword navigation and outline settings."));
	pRoot1->Expand();

	m_pKeywordNavigation = (CPropTreeItemStatic*)m_tree.InsertItem(new CPropTreeItemStatic(), pRoot1);
	m_pKeywordNavigation->SetLabelText(_T("Keyword navigation"));
	m_pKeywordNavigation->SetInfoText(
		_T("Enable navigation between SELECT/FROM/WHERE and IF/ELSE/END IF keywords. ")
		_T("Also required for the PL/SQL Outline pane."));
	m_pKeywordNavigation->HasCheckBox(TRUE);
	m_pKeywordNavigation->Check(s->GetPlSqlKeywordNavigation() ? TRUE : FALSE);

	// ── Outline ───────────────────────────────────────────────────────────
	CPropTreeItem* pRoot3 = m_tree.InsertItem(new CPropTreeItem());
	pRoot3->SetLabelText(_T("Outline"));
	pRoot3->SetInfoText(_T("Controls what appears in the PL/SQL Outline pane."));
	pRoot3->Expand();

	m_pOutlineEnabled = (CPropTreeItemStatic*)m_tree.InsertItem(new CPropTreeItemStatic(), pRoot3);
	m_pOutlineEnabled->SetLabelText(_T("Enabled"));
	m_pOutlineEnabled->SetInfoText(_T("Enable the PL/SQL Outline pane. Requires keyword navigation to be on."));
	m_pOutlineEnabled->HasCheckBox(TRUE);
	m_pOutlineEnabled->Check(s->GetPlSqlOutlineEnabled() ? TRUE : FALSE);

	m_pOutlineSelectEnabled = (CPropTreeItemStatic*)m_tree.InsertItem(new CPropTreeItemStatic(), pRoot3);
	m_pOutlineSelectEnabled->SetLabelText(_T("Show SELECT statements"));
	m_pOutlineSelectEnabled->SetInfoText(_T("Include top-level SELECT statements in the outline."));
	m_pOutlineSelectEnabled->HasCheckBox(TRUE);
	m_pOutlineSelectEnabled->Check(s->GetPlSqlOutlineSelectEnabled() ? TRUE : FALSE);

	m_pOutlineSelectOnly = (CPropTreeItemStatic*)m_tree.InsertItem(new CPropTreeItemStatic(), pRoot3);
	m_pOutlineSelectOnly->SetLabelText(_T("SELECT only (no FROM/WHERE)"));
	m_pOutlineSelectOnly->SetInfoText(
		_T("When enabled, only the SELECT leg label is shown; FROM and WHERE clause nodes are hidden."));
	m_pOutlineSelectOnly->HasCheckBox(TRUE);
	m_pOutlineSelectOnly->Check(s->GetPlSqlOutlineSelectOnly() ? TRUE : FALSE);

	m_pOutlineMaxTopItems = (CPropTreeItemEdit*)m_tree.InsertItem(new CPropTreeItemEdit(), pRoot3);
	m_pOutlineMaxTopItems->SetLabelText(_T("Max top-level items"));
	m_pOutlineMaxTopItems->SetInfoText(
		_T("Stop adding top-level outline items after this many have been collected (0 = unlimited, default = 500)."));
	m_pOutlineMaxTopItems->SetValueFormat(CPropTreeItemEdit::ValueFormatInteger);
	m_pOutlineMaxTopItems->SetItemValue(s->GetPlSqlOutlineMaxTopItems());

	// ── Parser limits ─────────────────────────────────────────────────────
	CPropTreeItem* pRoot2 = m_tree.InsertItem(new CPropTreeItem());
	pRoot2->SetLabelText(_T("Parser limits"));
	pRoot2->SetInfoText(_T("Limits applied to background PL/SQL parsing to avoid slowdowns on very large files."));
	pRoot2->Expand();

	m_pFileMaxTokens = (CPropTreeItemEdit*)m_tree.InsertItem(new CPropTreeItemEdit(), pRoot2);
	m_pFileMaxTokens->SetLabelText(_T("Max tokens"));
	m_pFileMaxTokens->SetInfoText(_T("Stop parsing after this many tokens have been processed (0 = unlimited, default = 15000)."));
	m_pFileMaxTokens->SetValueFormat(CPropTreeItemEdit::ValueFormatInteger);
	m_pFileMaxTokens->SetItemValue(s->GetPlSqlFileMaxTokens());

	m_pFileMaxLines = (CPropTreeItemEdit*)m_tree.InsertItem(new CPropTreeItemEdit(), pRoot2);
	m_pFileMaxLines->SetLabelText(_T("Max lines"));
	m_pFileMaxLines->SetInfoText(_T("Stop parsing after this many lines have been read (0 = unlimited, default = 3000)."));
	m_pFileMaxLines->SetValueFormat(CPropTreeItemEdit::ValueFormatInteger);
	m_pFileMaxLines->SetItemValue(s->GetPlSqlFileMaxLines());
}

void COEPlsSqlOutlinePageTree::OnPropTreeNotify (NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
	SetModified(TRUE);
	*pResult = 0;
}

BOOL COEPlsSqlOutlinePageTree::OnApply ()
{
	try { EXCEPTION_FRAME;

		if (!m_pKeywordNavigation) return TRUE;

		// Commit any value currently being edited in the active cell
		CPropTreeItem* pFocused = m_tree.GetFocusedItem();
		if (pFocused)
			pFocused->CommitChanges();

		OpenEditor::GlobalSettingsPtr s = m_manager.GetGlobalSettings();

		s->SetPlSqlKeywordNavigation(m_pKeywordNavigation->IsChecked() ? true : false, false);

		int v = 0;

		m_pFileMaxTokens->GetItemValue(v);
		s->SetPlSqlFileMaxTokens(v, false);

		m_pFileMaxLines->GetItemValue(v);
		s->SetPlSqlFileMaxLines(v, false);

		s->SetPlSqlOutlineEnabled      (m_pOutlineEnabled->IsChecked()       ? true : false, false);
		s->SetPlSqlOutlineSelectEnabled(m_pOutlineSelectEnabled->IsChecked() ? true : false, false);
		s->SetPlSqlOutlineSelectOnly   (m_pOutlineSelectOnly->IsChecked()    ? true : false, false);

		m_pOutlineMaxTopItems->GetItemValue(v);
		s->SetPlSqlOutlineMaxTopItems(v, false);

	}
	_OE_DEFAULT_HANDLER_;

	return TRUE;
}

void COEPlsSqlOutlinePageTree::OnSize (UINT nType, int cx, int cy)
{
	CPropertyPage::OnSize(nType, cx, cy);

	if (IsWindow(m_tree.m_hWnd))
		m_tree.MoveWindow(0, 0, cx, cy);
}
