#include "stdafx.h"
#include <COMMON/ExceptionHelper.h>
#include "OpenEditor/OESqlFormatPage.h"

COESqlFormatPage::COESqlFormatPage (SettingsManager& manager)
	: CPropertyPage(COESqlFormatPage::IDD)
	, m_manager(manager)
{
	m_psp.dwFlags &= ~PSP_HASHELP;

	const OpenEditor::GlobalSettingsPtr s = m_manager.GetGlobalSettings();

	m_SimpleStmtEnabled  = s->GetFmtSimpleStmtEnabled()        ? TRUE : FALSE;
	m_SimpleStmtMaxLen   = s->GetFmtSimpleStmtMaxLen();

	m_SelectListEnabled  = s->GetFmtSelectShortListEnabled()    ? TRUE : FALSE;
	m_SelectListMaxLen   = s->GetFmtSelectShortListMaxLen();

	m_FromListEnabled    = s->GetFmtFromShortListEnabled()      ? TRUE : FALSE;
	m_FromListMaxLen     = s->GetFmtFromShortListMaxLen();

	m_GroupByListEnabled = s->GetFmtGroupByShortListEnabled()   ? TRUE : FALSE;
	m_GroupByListMaxLen  = s->GetFmtGroupByShortListMaxLen();

	m_OrderByListEnabled = s->GetFmtOrderByShortListEnabled()   ? TRUE : FALSE;
	m_OrderByListMaxLen  = s->GetFmtOrderByShortListMaxLen();

	m_SelectStmtEnabled  = s->GetFmtSelectStmtEnabled()         ? TRUE : FALSE;
	m_SelectStmtMaxLen   = s->GetFmtSelectStmtMaxLen();
	m_BlockMaxLen        = s->GetFmtBlockShortMaxLen();

	m_CaseWhenMaxLen     = s->GetFmtCaseWhenShortMaxLen();
}

COESqlFormatPage::~COESqlFormatPage ()
{
}

void COESqlFormatPage::DoDataExchange (CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);

	DDX_Check(pDX, IDC_OE_FMT_SIMPLE_STMT_ENABLED,  m_SimpleStmtEnabled);
	DDX_Text (pDX, IDC_OE_FMT_SIMPLE_STMT_MAX_LEN,  m_SimpleStmtMaxLen);
	DDV_MinMaxInt(pDX, m_SimpleStmtMaxLen, 10, 999);

	DDX_Check(pDX, IDC_OE_FMT_SELECT_LIST_ENABLED,  m_SelectListEnabled);
	DDX_Text (pDX, IDC_OE_FMT_SELECT_LIST_MAX_LEN,  m_SelectListMaxLen);
	DDV_MinMaxInt(pDX, m_SelectListMaxLen, 10, 999);

	DDX_Check(pDX, IDC_OE_FMT_FROM_LIST_ENABLED,    m_FromListEnabled);
	DDX_Text (pDX, IDC_OE_FMT_FROM_LIST_MAX_LEN,    m_FromListMaxLen);
	DDV_MinMaxInt(pDX, m_FromListMaxLen, 10, 999);

	DDX_Check(pDX, IDC_OE_FMT_GROUPBY_LIST_ENABLED, m_GroupByListEnabled);
	DDX_Text (pDX, IDC_OE_FMT_GROUPBY_LIST_MAX_LEN, m_GroupByListMaxLen);
	DDV_MinMaxInt(pDX, m_GroupByListMaxLen, 10, 999);

	DDX_Check(pDX, IDC_OE_FMT_ORDERBY_LIST_ENABLED, m_OrderByListEnabled);
	DDX_Text (pDX, IDC_OE_FMT_ORDERBY_LIST_MAX_LEN, m_OrderByListMaxLen);
	DDV_MinMaxInt(pDX, m_OrderByListMaxLen, 10, 999);

	DDX_Check(pDX, IDC_OE_FMT_SELECT_STMT_ENABLED,  m_SelectStmtEnabled);
	DDX_Text (pDX, IDC_OE_FMT_SELECT_STMT_MAX_LEN,  m_SelectStmtMaxLen);
	DDV_MinMaxInt(pDX, m_SelectStmtMaxLen, 10, 999);

	DDX_Text (pDX, IDC_OE_FMT_BLOCK_MAX_LEN,        m_BlockMaxLen);
	DDV_MinMaxInt(pDX, m_BlockMaxLen, 10, 999);

	DDX_Text (pDX, IDC_OE_FMT_CASE_WHEN_MAX_LEN,    m_CaseWhenMaxLen);
	DDV_MinMaxInt(pDX, m_CaseWhenMaxLen, 0, 999);
}

BEGIN_MESSAGE_MAP(COESqlFormatPage, CPropertyPage)
END_MESSAGE_MAP()

BOOL COESqlFormatPage::OnApply ()
{
	try { EXCEPTION_FRAME;

		OpenEditor::GlobalSettingsPtr s = m_manager.GetGlobalSettings();

		s->SetFmtSimpleStmtEnabled      (m_SimpleStmtEnabled  ? true : false, false);
		s->SetFmtSimpleStmtMaxLen       (m_SimpleStmtMaxLen,                  false);

		s->SetFmtSelectShortListEnabled (m_SelectListEnabled   ? true : false, false);
		s->SetFmtSelectShortListMaxLen  (m_SelectListMaxLen,                   false);

		s->SetFmtFromShortListEnabled   (m_FromListEnabled     ? true : false, false);
		s->SetFmtFromShortListMaxLen    (m_FromListMaxLen,                     false);

		s->SetFmtGroupByShortListEnabled(m_GroupByListEnabled  ? true : false, false);
		s->SetFmtGroupByShortListMaxLen (m_GroupByListMaxLen,                  false);

		s->SetFmtOrderByShortListEnabled(m_OrderByListEnabled  ? true : false, false);
		s->SetFmtOrderByShortListMaxLen (m_OrderByListMaxLen,                  false);

		s->SetFmtSelectStmtEnabled      (m_SelectStmtEnabled   ? true : false, false);
		s->SetFmtSelectStmtMaxLen       (m_SelectStmtMaxLen,                   false);
		s->SetFmtBlockShortMaxLen       (m_BlockMaxLen,                        false);

		s->SetFmtCaseWhenShortMaxLen    (m_CaseWhenMaxLen,                     false);
	}
	_OE_DEFAULT_HANDLER_;

	return TRUE;
}
