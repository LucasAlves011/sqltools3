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
#include "SQLTools.h"
#include "PropAutocompletePage.h"
#include <COMMON/DlgDataExt.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CPropAutocompletePage::CPropAutocompletePage (SQLToolsSettings& settings)
    : CPropertyPage(CPropAutocompletePage::IDD, NULL),
    m_settings(settings)
{
    m_psp.dwFlags &= ~PSP_HASHELP;
}

void CPropAutocompletePage::DoDataExchange (CDataExchange* pDX)
{
    CPropertyPage::DoDataExchange(pDX);

    DDX_Check(pDX, IDC_PROP_AC_TABLE,     m_settings.m_AutocompleteTable    );
    DDX_Check(pDX, IDC_PROP_AC_VIEW,      m_settings.m_AutocompleteView     );
    DDX_Check(pDX, IDC_PROP_AC_PACKAGE,   m_settings.m_AutocompletePackage  );
    DDX_Check(pDX, IDC_PROP_AC_PROCEDURE, m_settings.m_AutocompleteProcedure);
    DDX_Check(pDX, IDC_PROP_AC_FUNCTION,  m_settings.m_AutocompleteFunction );
    DDX_Check(pDX, IDC_PROP_AC_SEQUENCE,  m_settings.m_AutocompleteSequence );
    DDX_Check(pDX, IDC_PROP_AC_SYNONYM,   m_settings.m_AutocompleteSynonym  );
    DDX_Check(pDX, IDC_PROP_AC_TYPE,      m_settings.m_AutocompleteType     );
    DDX_Check(pDX, IDC_PROP_AC_SORT_SUBOBJ,     m_settings.m_AutocompleteSortSubobjects);
    DDX_Check(pDX, IDC_PROP_AC_MULTI_COL_SEL,   m_settings.m_AutocompleteEnableMultiColumnSelection);
    DDX_Check(pDX, IDC_PROP_AC_SEP_LINES,       m_settings.m_AutocompleteInsertColumnsOnSeparateLines);
    DDX_Check(pDX, IDC_PROP_AC_LEADING_COMMA,   m_settings.m_AutocompleteUseLeadingComma);
    DDX_Check(pDX, IDC_PROP_AC_LOWERCASE_ALIAS, m_settings.m_AutocompleteUseLowercaseForAliases);
}

BEGIN_MESSAGE_MAP(CPropAutocompletePage, CPropertyPage)
END_MESSAGE_MAP()