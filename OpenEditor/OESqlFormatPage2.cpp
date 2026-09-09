#include "stdafx.h"
#include <COMMON/ExceptionHelper.h>
#include "OpenEditor/OESqlFormatPage2.h"

COESqlFormatPage2::COESqlFormatPage2 (SettingsManager& manager)
	: CPropertyPage(COESqlFormatPage2::IDD)
	, m_manager(manager)
{
	m_psp.dwFlags &= ~PSP_HASHELP;

	const OpenEditor::GlobalSettingsPtr s = m_manager.GetGlobalSettings();

	m_AlignAliases       = s->GetFmtAlignColumnAliases()        ? TRUE : FALSE;
	m_SetOpBlankLine     = s->GetFmtSetOpBlankLine()             ? TRUE : FALSE;
}

COESqlFormatPage2::~COESqlFormatPage2 ()
{
}

void COESqlFormatPage2::DoDataExchange (CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);

	DDX_Check(pDX, IDC_OE_FMT_ALIGN_ALIASES,        m_AlignAliases);
	DDX_Check(pDX, IDC_OE_FMT_SET_OP_BLANK_LINE,    m_SetOpBlankLine);
}

BEGIN_MESSAGE_MAP(COESqlFormatPage2, CPropertyPage)
END_MESSAGE_MAP()

BOOL COESqlFormatPage2::OnApply ()
{
	try { EXCEPTION_FRAME;

		OpenEditor::GlobalSettingsPtr s = m_manager.GetGlobalSettings();

		s->SetFmtAlignColumnAliases     (m_AlignAliases        ? true : false, false);
		s->SetFmtSetOpBlankLine         (m_SetOpBlankLine      ? true : false, false);
	}
	_OE_DEFAULT_HANDLER_;

	return TRUE;
}
