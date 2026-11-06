#include "stdafx.h"
#include <COMMON/ExceptionHelper.h>
#include "OpenEditor/OESqlFormatPageTree.h"

COESqlFormatPageTree::COESqlFormatPageTree (SettingsManager& manager)
	: CPropertyPage(COESqlFormatPageTree::IDD)
	, m_manager(manager)
{
	LoadValues();
}

COESqlFormatPageTree::~COESqlFormatPageTree ()
{
}

void COESqlFormatPageTree::DoDataExchange (CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(COESqlFormatPageTree, CPropertyPage)
	ON_WM_SIZE()
END_MESSAGE_MAP()

void COESqlFormatPageTree::LoadValues ()
{
	const OpenEditor::GlobalSettingsPtr s = m_manager.GetGlobalSettings();

	m_simpleStmtOn   = s->GetFmtSimpleStmtEnabled();        m_simpleStmtLen  = s->GetFmtSimpleStmtMaxLen();
	m_selectListOn   = s->GetFmtSelectShortListEnabled();   m_selectListLen  = s->GetFmtSelectShortListMaxLen();
	m_fromListOn     = s->GetFmtFromShortListEnabled();     m_fromListLen    = s->GetFmtFromShortListMaxLen();
	m_groupByOn      = s->GetFmtGroupByShortListEnabled();  m_groupByLen     = s->GetFmtGroupByShortListMaxLen();
	m_orderByOn      = s->GetFmtOrderByShortListEnabled();  m_orderByLen     = s->GetFmtOrderByShortListMaxLen();
	m_selectStmtOn   = s->GetFmtSelectStmtEnabled();        m_selectStmtLen  = s->GetFmtSelectStmtMaxLen();
	m_blockLen       = s->GetFmtBlockShortMaxLen();
	m_caseWhenLen    = s->GetFmtCaseWhenShortMaxLen();
	m_returningLen   = s->GetFmtReturningMaxLen();
	m_returningItems = s->GetFmtReturningMaxItems();

	m_alignAliases        = s->GetFmtAlignColumnAliases();
	m_setOpBlankLine      = s->GetFmtSetOpBlankLine();
	m_alignSetAssignments = s->GetFmtAlignSetAssignments();
	m_alignCaseInSet      = s->GetFmtAlignCaseInSet();
	m_logErrorsLen        = s->GetFmtLogErrorsMaxLen();

	m_brickEnabled      = s->GetFmtBrickListEnabled();
	m_brickLineLen      = s->GetFmtBrickListMaxLineLen();
	m_brickLineItems    = s->GetFmtBrickListMaxLineItems();
	m_brickMinItems     = s->GetFmtBrickListMinItems();
	m_brickLinkedInsert = s->GetFmtBrickListLinkedInsert();
	m_brickLinkedSetOp  = s->GetFmtBrickListLinkedSetOp();
}

BOOL COESqlFormatPageTree::OnInitDialog ()
{
	CPropertyPage::OnInitDialog();

	CRect rc;
	GetClientRect(&rc);
	m_list.Create(this, IDC_OE_SQL_FORMAT_TREE, rc);

	const wchar_t* const shortLines = L"Short-line heuristics";
	const wchar_t* const misc       = L"Miscellaneous";
	const wchar_t* const brick      = L"Brick list";

	typedef ModernOption O;
	std::vector<ModernOption> options;
	auto add = [&](const wchar_t* group, const wchar_t* label, const wchar_t* info, O::EKind kind,
	               bool* on, int* value, int lo = 0, int hi = 0, int step = 1)
	{
		ModernOption o = { group, label, info, kind, on, value, lo, hi, step };
		options.push_back(o);
	};

	add(shortLines, L"Simple statement", L"Keep complete short statements on one line. The number is the maximum line length.",
	    O::TOGGLE_NUMBER, &m_simpleStmtOn, &m_simpleStmtLen, 10, 500, 5);
	add(shortLines, L"SELECT column list", L"Keep a short SELECT column list on one line. The number is the maximum line length.",
	    O::TOGGLE_NUMBER, &m_selectListOn, &m_selectListLen, 10, 500, 5);
	add(shortLines, L"FROM table list", L"Keep a short FROM table list on one line. The number is the maximum line length.",
	    O::TOGGLE_NUMBER, &m_fromListOn, &m_fromListLen, 10, 500, 5);
	add(shortLines, L"GROUP BY list", L"Keep a short GROUP BY list on one line. The number is the maximum line length.",
	    O::TOGGLE_NUMBER, &m_groupByOn, &m_groupByLen, 10, 500, 5);
	add(shortLines, L"ORDER BY list", L"Keep a short ORDER BY list on one line. The number is the maximum line length.",
	    O::TOGGLE_NUMBER, &m_orderByOn, &m_orderByLen, 10, 500, 5);
	add(shortLines, L"SELECT subquery", L"Keep a short SELECT subquery on one line. The number is the maximum line length.",
	    O::TOGGLE_NUMBER, &m_selectStmtOn, &m_selectStmtLen, 10, 500, 5);
	add(shortLines, L"Block max length", L"Maximum length for other parenthesised blocks kept on one line.",
	    O::NUMBER, nullptr, &m_blockLen, 0, 500, 5);
	add(shortLines, L"CASE WHEN max length", L"Keep a short WHEN clause on one line (0 to disable).",
	    O::NUMBER, nullptr, &m_caseWhenLen, 0, 500, 5);
	add(shortLines, L"RETURNING max length", L"Maximum line length for a RETURNING clause kept on one line.",
	    O::NUMBER, nullptr, &m_returningLen, 0, 500, 5);
	add(shortLines, L"RETURNING max items", L"Maximum number of items in a RETURNING clause kept on one line.",
	    O::NUMBER, nullptr, &m_returningItems, 0, 99, 1);

	add(misc, L"Align column aliases", L"Align column aliases in SELECT lists.", O::TOGGLE, &m_alignAliases, nullptr);
	add(misc, L"Blank line before set ops", L"Insert a blank line before UNION / INTERSECT / MINUS.", O::TOGGLE, &m_setOpBlankLine, nullptr);
	add(misc, L"Align SET assignments", L"Align the right-hand sides of assignments in SET clauses.", O::TOGGLE, &m_alignSetAssignments, nullptr);
	add(misc, L"Align CASE in SET", L"Align CASE expressions on the right-hand side of SET assignments.", O::TOGGLE, &m_alignCaseInSet, nullptr);
	add(misc, L"LOG ERRORS max length", L"Maximum line length for LOG ERRORS formatting (default 40).",
	    O::NUMBER, nullptr, &m_logErrorsLen, 0, 500, 5);

	add(brick, L"Enabled", L"Arrange long SELECT column lists and INSERT column / VALUES lists in a compact rectangle "
	    L"(several items per line) instead of one item per line.", O::TOGGLE, &m_brickEnabled, nullptr);
	add(brick, L"Max line length", L"Maximum total line length before wrapping to the next brick row.",
	    O::NUMBER, nullptr, &m_brickLineLen, 20, 500, 5);
	add(brick, L"Max items per line", L"Maximum number of items placed on one brick row.",
	    O::NUMBER, nullptr, &m_brickLineItems, 1, 99, 1);
	add(brick, L"Min items to activate", L"Minimum number of items in the list before the brick layout is used; "
	    L"shorter lists use one item per line.", O::NUMBER, nullptr, &m_brickMinItems, 1, 99, 1);
	add(brick, L"Linked INSERT col / VALUES", L"Wrap the VALUES list at the same positions as the INSERT column list "
	    L"so both lists align vertically.", O::TOGGLE, &m_brickLinkedInsert, nullptr);
	add(brick, L"Linked UNION / INTERSECT / MINUS", L"Wrap each SELECT column list in a compound query at the same "
	    L"positions as the first branch.", O::TOGGLE, &m_brickLinkedSetOp, nullptr);

	m_list.SetOptions(options);
	return TRUE;
}

BOOL COESqlFormatPageTree::OnCommand (WPARAM wParam, LPARAM lParam)
{
	if (LOWORD(wParam) == IDC_OE_SQL_FORMAT_TREE && HIWORD(wParam) == EN_CHANGE)
	{
		SetModified(TRUE);
		return TRUE;
	}
	return CPropertyPage::OnCommand(wParam, lParam);
}

BOOL COESqlFormatPageTree::OnApply ()
{
	try { EXCEPTION_FRAME;

		if (!m_list.GetSafeHwnd()) return TRUE;

		OpenEditor::GlobalSettingsPtr s = m_manager.GetGlobalSettings();

		s->SetFmtSimpleStmtEnabled      (m_simpleStmtOn,   false);  s->SetFmtSimpleStmtMaxLen      (m_simpleStmtLen,  false);
		s->SetFmtSelectShortListEnabled (m_selectListOn,   false);  s->SetFmtSelectShortListMaxLen (m_selectListLen,  false);
		s->SetFmtFromShortListEnabled   (m_fromListOn,     false);  s->SetFmtFromShortListMaxLen   (m_fromListLen,    false);
		s->SetFmtGroupByShortListEnabled(m_groupByOn,      false);  s->SetFmtGroupByShortListMaxLen(m_groupByLen,     false);
		s->SetFmtOrderByShortListEnabled(m_orderByOn,      false);  s->SetFmtOrderByShortListMaxLen(m_orderByLen,     false);
		s->SetFmtSelectStmtEnabled      (m_selectStmtOn,   false);  s->SetFmtSelectStmtMaxLen      (m_selectStmtLen,  false);
		s->SetFmtBlockShortMaxLen       (m_blockLen,       false);
		s->SetFmtCaseWhenShortMaxLen    (m_caseWhenLen,    false);
		s->SetFmtReturningMaxLen        (m_returningLen,   false);
		s->SetFmtReturningMaxItems      (m_returningItems, false);

		s->SetFmtAlignColumnAliases     (m_alignAliases,        false);
		s->SetFmtSetOpBlankLine         (m_setOpBlankLine,      false);
		s->SetFmtAlignSetAssignments    (m_alignSetAssignments, false);
		s->SetFmtAlignCaseInSet         (m_alignCaseInSet,      false);
		s->SetFmtLogErrorsMaxLen        (m_logErrorsLen,        false);

		s->SetFmtBrickListEnabled       (m_brickEnabled,      false);
		s->SetFmtBrickListMaxLineLen    (m_brickLineLen,      false);
		s->SetFmtBrickListMaxLineItems  (m_brickLineItems,    false);
		s->SetFmtBrickListMinItems      (m_brickMinItems,     false);
		s->SetFmtBrickListLinkedInsert  (m_brickLinkedInsert, false);
		s->SetFmtBrickListLinkedSetOp   (m_brickLinkedSetOp,  false);
	}
	_OE_DEFAULT_HANDLER_;

	return TRUE;
}

void COESqlFormatPageTree::OnSize(UINT nType, int cx, int cy)
{
	CPropertyPage::OnSize(nType, cx, cy);

	if (IsWindow(m_list.m_hWnd))
		m_list.MoveWindow(0, 0, cx, cy);
}
