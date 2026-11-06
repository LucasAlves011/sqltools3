#include "stdafx.h"
#include <COMMON/ExceptionHelper.h>
#include "OpenEditor/OESqlFormatPageTree.h"

COESqlFormatPageTree::COESqlFormatPageTree (SettingsManager& manager)
	: CPropertyPage(COESqlFormatPageTree::IDD)
	//, m_recipient(nullptr)
	//, m_recipientCommand(0)
	, m_manager(manager)
	, m_pSimpleStmt(nullptr)
	, m_pSelectList(nullptr)
	, m_pFromList(nullptr)
	, m_pGroupByList(nullptr)
	, m_pOrderByList(nullptr)
	, m_pSelectStmt(nullptr)
	, m_pBlock(nullptr)
	, m_pCaseWhen(nullptr)
	, m_pReturningMaxLen(nullptr)
	, m_pReturningMaxItems(nullptr)
	, m_pAlignAliases(nullptr)
	, m_pSetOpBlankLine(nullptr)
	, m_pAlignSetAssignments(nullptr)
	, m_pAlignCaseInSet(nullptr)
	, m_pLogErrorsMaxLen(nullptr)
	, m_pBrickListEnabled(nullptr)
	, m_pBrickListMaxLineLen(nullptr)
	, m_pBrickListMaxLineItems(nullptr)
	, m_pBrickListMinItems(nullptr)
	, m_pBrickListLinkedInsert(nullptr)
	, m_pBrickListLinkedSetOp(nullptr)
{
	//m_psp.dwFlags &= ~PSP_HASHELP;
}

COESqlFormatPageTree::~COESqlFormatPageTree ()
{
}

//void COESqlFormatPageTree::SetApplyRecipient (CWnd* recipient, UINT command)
//{
//	m_recipient = recipient;
//	m_recipientCommand = command;
//}

void COESqlFormatPageTree::DoDataExchange (CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(COESqlFormatPageTree, CPropertyPage)
	ON_NOTIFY(PTN_ITEMCHANGED, IDC_OE_SQL_FORMAT_TREE, OnPropTreeNotify)
	ON_NOTIFY(PTN_CHECKCLICK,  IDC_OE_SQL_FORMAT_TREE, OnPropTreeNotify)
	ON_WM_SIZE()
END_MESSAGE_MAP()

BOOL COESqlFormatPageTree::OnInitDialog ()
{
	CPropertyPage::OnInitDialog();

	CRect rc;
	GetClientRect(&rc);
	//rc.DeflateRect(4, 4);

	m_tree.Create(WS_CHILD | WS_VISIBLE /*| WS_BORDER*/ | WS_TABSTOP | PTS_NOTIFY, rc, this, IDC_OE_SQL_FORMAT_TREE);
	m_tree.SetInfoHeight(72);
	m_tree.SetColumn(200);

	BuildTree();

	return TRUE;
}

// helpers — static functions to reduce repetition in BuildTree

static CPropTreeItemEdit* InsertCheckedEdit (CPropTree& tree, CPropTreeItem* parent,
	LPCTSTR label, LPCTSTR info, bool checked, int value)
{
	CPropTreeItemEdit* p = (CPropTreeItemEdit*)tree.InsertItem(new CPropTreeItemEdit(), parent);
	p->SetLabelText(label);
	p->SetInfoText(info);
	p->HasCheckBox(TRUE);
	p->Check(checked ? TRUE : FALSE);
	p->SetValueFormat(CPropTreeItemEdit::ValueFormatInteger);
	p->SetItemValue(value);
	return p;
}

static CPropTreeItemEdit* InsertEdit (CPropTree& tree, CPropTreeItem* parent,
	LPCTSTR label, LPCTSTR info, int value)
{
	CPropTreeItemEdit* p = (CPropTreeItemEdit*)tree.InsertItem(new CPropTreeItemEdit(), parent);
	p->SetLabelText(label);
	p->SetInfoText(info);
	p->SetValueFormat(CPropTreeItemEdit::ValueFormatInteger);
	p->SetItemValue(value);
	return p;
}

static CPropTreeItemStatic* InsertCheckedStatic (CPropTree& tree, CPropTreeItem* parent,
	LPCTSTR label, LPCTSTR info, bool checked)
{
	CPropTreeItemStatic* p = (CPropTreeItemStatic*)tree.InsertItem(new CPropTreeItemStatic(), parent);
	p->SetLabelText(label);
	p->SetInfoText(info);
	p->HasCheckBox(TRUE);
	p->Check(checked ? TRUE : FALSE);
	return p;
}

void COESqlFormatPageTree::BuildTree ()
{
	const OpenEditor::GlobalSettingsPtr s = m_manager.GetGlobalSettings();

	// ── Short-line heuristics ──────────────────────────────────────────────
	CPropTreeItem* pRoot1 = m_tree.InsertItem(new CPropTreeItem());
	pRoot1->SetLabelText(_T("Short-line heuristics"));
	pRoot1->SetInfoText(_T("Controls when SQL clauses are kept on a single line. "
						   "Check to enable; the number is the maximum line length."));
	pRoot1->Expand();

	m_pSimpleStmt  = InsertCheckedEdit(m_tree, pRoot1,
		_T("Simple statement"),
		_T("Keep complete short statements on one line, max length"),
		s->GetFmtSimpleStmtEnabled(),        s->GetFmtSimpleStmtMaxLen());

	m_pSelectList  = InsertCheckedEdit(m_tree, pRoot1,
		_T("SELECT column list"),
		_T("Keep short SELECT column list on one line, max length"),
		s->GetFmtSelectShortListEnabled(),   s->GetFmtSelectShortListMaxLen());

	m_pFromList    = InsertCheckedEdit(m_tree, pRoot1,
		_T("FROM table list"),
		_T("Keep short FROM table list on one line, max length"),
		s->GetFmtFromShortListEnabled(),     s->GetFmtFromShortListMaxLen());

	m_pGroupByList = InsertCheckedEdit(m_tree, pRoot1,
		_T("GROUP BY list"),
		_T("Keep short GROUP BY list on one line, max length"),
		s->GetFmtGroupByShortListEnabled(),  s->GetFmtGroupByShortListMaxLen());

	m_pOrderByList = InsertCheckedEdit(m_tree, pRoot1,
		_T("ORDER BY list"),
		_T("Keep short ORDER BY list on one line, max length"),
		s->GetFmtOrderByShortListEnabled(),  s->GetFmtOrderByShortListMaxLen());

	m_pSelectStmt  = InsertCheckedEdit(m_tree, pRoot1,
		_T("SELECT subquery"),
		_T("Keep short SELECT subquery on one line, max length"),
		s->GetFmtSelectStmtEnabled(),        s->GetFmtSelectStmtMaxLen());

	m_pBlock       = InsertEdit(m_tree, pRoot1,
		_T("Block max length"),
		_T("Max length for other parenthesised blocks"),
		s->GetFmtBlockShortMaxLen());

	m_pCaseWhen    = InsertEdit(m_tree, pRoot1,
		_T("CASE WHEN max length"),
		_T("Keep short WHEN clause on one line (0 to disable)"),
		s->GetFmtCaseWhenShortMaxLen());

	m_pReturningMaxLen   = InsertEdit(m_tree, pRoot1,
		_T("RETURNING max length"),
		_T("Maximum line length for a RETURNING clause kept on one line"),
		s->GetFmtReturningMaxLen());

	m_pReturningMaxItems = InsertEdit(m_tree, pRoot1,
		_T("RETURNING max items"),
		_T("Maximum number of items in a RETURNING clause kept on one line"),
		s->GetFmtReturningMaxItems());

	// ── Miscellaneous
	CPropTreeItem* pRoot2 = m_tree.InsertItem(new CPropTreeItem());
	pRoot2->SetLabelText(_T("Miscellaneous"));
	pRoot2->SetInfoText(_T("Additional SQL formatting options"));
	pRoot2->Expand();

	m_pAlignAliases  = InsertCheckedStatic(m_tree, pRoot2,
		_T("Align column aliases"),
		_T("Align column aliases in SELECT lists"),
		s->GetFmtAlignColumnAliases());

	m_pSetOpBlankLine = InsertCheckedStatic(m_tree, pRoot2,
		_T("Blank line before set ops"),
		_T("Insert blank line before UNION / INTERSECT / MINUS"),
		s->GetFmtSetOpBlankLine());

	m_pAlignSetAssignments = InsertCheckedStatic(m_tree, pRoot2,
		_T("Align SET assignments"),
		_T("Align right-hand sides of assignments in SET clauses"),
		s->GetFmtAlignSetAssignments());

	m_pAlignCaseInSet = InsertCheckedStatic(m_tree, pRoot2,
		_T("Align CASE in SET"),
		_T("Align CASE expressions on the right-hand side of SET assignments"),
		s->GetFmtAlignCaseInSet());

	m_pLogErrorsMaxLen = InsertEdit(m_tree, pRoot2,
		_T("LOG ERRORS max length"),
		_T("Maximum line length for LOG ERRORS formatting (default 40)"),
		s->GetFmtLogErrorsMaxLen());

	// ── Brick list ────────────────────────────────────────────────────────
	CPropTreeItem* pRoot3 = m_tree.InsertItem(new CPropTreeItem());
	pRoot3->SetLabelText(_T("Brick list"));
	pRoot3->SetInfoText(_T("Arranges long column lists in a compact rectangle (multiple items per line) "
						   "instead of one item per line. "
						   "Applies to SELECT column lists and INSERT column/VALUES lists."));
	pRoot3->Expand();

	m_pBrickListEnabled = InsertCheckedStatic(m_tree, pRoot3,
		_T("Enabled"),
		_T("Enable brick-list layout for SELECT column lists and INSERT column/VALUES lists"),
		s->GetFmtBrickListEnabled());

	m_pBrickListMaxLineLen = InsertEdit(m_tree, pRoot3,
		_T("Max line length"),
		_T("Maximum total line length before wrapping to the next brick row"),
		s->GetFmtBrickListMaxLineLen());

	m_pBrickListMaxLineItems = InsertEdit(m_tree, pRoot3,
		_T("Max items per line"),
		_T("Maximum number of items placed on one brick row"),
		s->GetFmtBrickListMaxLineItems());

	m_pBrickListMinItems = InsertEdit(m_tree, pRoot3,
		_T("Min items to activate"),
		_T("Minimum number of items in the list before brick layout is applied; shorter lists use normal one-per-line layout"),
		s->GetFmtBrickListMinItems());

	m_pBrickListLinkedInsert = InsertCheckedStatic(m_tree, pRoot3,
		_T("Linked INSERT col / VALUES"),
		_T("Wrap the VALUES list at the same positions as the INSERT column list so both lists align vertically"),
		s->GetFmtBrickListLinkedInsert());

	m_pBrickListLinkedSetOp = InsertCheckedStatic(m_tree, pRoot3,
		_T("Linked UNION / INTERSECT / MINUS"),
		_T("Wrap each SELECT column list in a compound query at the same positions as the first branch"),
		s->GetFmtBrickListLinkedSetOp());
}

void COESqlFormatPageTree::OnPropTreeNotify (NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
	SetModified(TRUE);
	*pResult = 0;
}

BOOL COESqlFormatPageTree::OnApply ()
{
	try { EXCEPTION_FRAME;

		if (!m_pSimpleStmt) return TRUE;

		// Commit any value that is currently being edited in the active cell
		CPropTreeItem* pFocused = m_tree.GetFocusedItem();
		if (pFocused)
			pFocused->CommitChanges();

		OpenEditor::GlobalSettingsPtr s = m_manager.GetGlobalSettings();

		int v = 0;

		m_pSimpleStmt->GetItemValue(v);
		s->SetFmtSimpleStmtEnabled      (m_pSimpleStmt->IsChecked()   ? true : false, false);
		s->SetFmtSimpleStmtMaxLen       (v,                                            false);

		m_pSelectList->GetItemValue(v);
		s->SetFmtSelectShortListEnabled (m_pSelectList->IsChecked()   ? true : false, false);
		s->SetFmtSelectShortListMaxLen  (v,                                            false);

		m_pFromList->GetItemValue(v);
		s->SetFmtFromShortListEnabled   (m_pFromList->IsChecked()     ? true : false, false);
		s->SetFmtFromShortListMaxLen    (v,                                            false);

		m_pGroupByList->GetItemValue(v);
		s->SetFmtGroupByShortListEnabled(m_pGroupByList->IsChecked()  ? true : false, false);
		s->SetFmtGroupByShortListMaxLen (v,                                            false);

		m_pOrderByList->GetItemValue(v);
		s->SetFmtOrderByShortListEnabled(m_pOrderByList->IsChecked()  ? true : false, false);
		s->SetFmtOrderByShortListMaxLen (v,                                            false);

		m_pSelectStmt->GetItemValue(v);
		s->SetFmtSelectStmtEnabled      (m_pSelectStmt->IsChecked()   ? true : false, false);
		s->SetFmtSelectStmtMaxLen       (v,                                            false);

		m_pBlock->GetItemValue(v);
		s->SetFmtBlockShortMaxLen       (v,                                            false);

		m_pCaseWhen->GetItemValue(v);
		s->SetFmtCaseWhenShortMaxLen    (v,                                            false);

		m_pReturningMaxLen->GetItemValue(v);
		s->SetFmtReturningMaxLen        (v,                                            false);

		m_pReturningMaxItems->GetItemValue(v);
		s->SetFmtReturningMaxItems      (v,                                            false);

		s->SetFmtAlignColumnAliases     (m_pAlignAliases->IsChecked()        ? true : false, false);
		s->SetFmtSetOpBlankLine         (m_pSetOpBlankLine->IsChecked()      ? true : false, false);
		s->SetFmtAlignSetAssignments    (m_pAlignSetAssignments->IsChecked() ? true : false, false);
		s->SetFmtAlignCaseInSet         (m_pAlignCaseInSet->IsChecked()      ? true : false, false);

		m_pLogErrorsMaxLen->GetItemValue(v);
		s->SetFmtLogErrorsMaxLen        (v,                                                  false);

		s->SetFmtBrickListEnabled       (m_pBrickListEnabled->IsChecked()    ? true : false, false);

		m_pBrickListMaxLineLen->GetItemValue(v);
		s->SetFmtBrickListMaxLineLen    (v,                                            false);

		m_pBrickListMaxLineItems->GetItemValue(v);
		s->SetFmtBrickListMaxLineItems  (v,                                            false);

		m_pBrickListMinItems->GetItemValue(v);
		s->SetFmtBrickListMinItems      (v,                                            false);

		s->SetFmtBrickListLinkedInsert  (m_pBrickListLinkedInsert->IsChecked() ? true : false, false);
		s->SetFmtBrickListLinkedSetOp   (m_pBrickListLinkedSetOp->IsChecked()  ? true : false, false);

		//if (m_recipient)
		//	m_recipient->OnCmdMsg(m_recipientCommand, CN_COMMAND, NULL, NULL);

	}
	_OE_DEFAULT_HANDLER_;

	return TRUE;
}

void COESqlFormatPageTree::OnSize(UINT nType, int cx, int cy)
{
	CPropertyPage::OnSize(nType, cx, cy);

	if (IsWindow(m_tree.m_hWnd))
		m_tree.MoveWindow(0, 0, cx, cy);
}
