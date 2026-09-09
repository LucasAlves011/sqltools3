/* 
	SQLTools is a tool for Oracle database developers and DBAs.
    Copyright (C) 2011 Aleksey Kochetov

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
#include "ObjectViewerPage.h"
#include <COMMON/DlgDataExt.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CObjectViewerPage dialog


CObjectViewerPage::CObjectViewerPage(SQLToolsSettings& settings)
	: CPropertyPage(CObjectViewerPage::IDD, NULL),
  m_settings(settings)
{
    m_psp.dwFlags &= ~PSP_HASHELP;
}


void CObjectViewerPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);

    DDX_Check(pDX, IDC_OVP_RETAIN_FOCUS_IN_EDITOR          , m_settings.m_ObjectViewerRetainFocusInEditor         );
    DDX_Check(pDX, IDC_OVP_USE_CACHE                       , m_settings.m_ObjectViewerUseCache                    );
    DDX_Check(pDX, IDC_OVP_SORT_TAB_COL_ALPHABETICALLY     , m_settings.m_ObjectViewerSortTabColAlphabetically    );
    DDX_Check(pDX, IDC_OVP_SORT_PKG_PROC_ALPHABETICALLY    , m_settings.m_ObjectViewerSortPkgProcAlphabetically   );
    DDX_Check(pDX, IDC_OVP_COPY_PROCEDURE_WITH_PACKAGE_NAME, m_settings.m_ObjectViewerCopyProcedureWithPackageName);
    DDX_Check(pDX, IDC_OVP_SHOW_COMMENTS                   , m_settings.m_ObjectViewerShowComments                );
    DDX_Check(pDX, IDC_OVP_CLASSIC_TREE_LOOK               , m_settings.m_ObjectViewerClassicTreeLook             );
    DDX_Check(pDX, IDC_OVP_INFOTIPS                        , m_settings.m_ObjectViewerInfotips                    );
    DDX_Check(pDX, IDC_OVP_COPY_PASTE_ON_DOUBLE_CLICK      , m_settings.m_ObjectViewerCopyPasteOnDoubleClick      );

    DDX_Text(pDX, IDC_OVP_MAX_LINE_LENGTH,    m_settings.m_ObjectViewerMaxLineLength);
	DDV_MinMaxUInt(pDX, m_settings.m_ObjectViewerMaxLineLength, 0, 1000);
    SendDlgItemMessage(IDC_OVP_MAX_LINE_LENGTH_SPIN, UDM_SETRANGE32, 0, 1000);

    DDX_Check(pDX, IDC_OVP_OV_QUERY_IN_ACTIVE_WINDOW       , m_settings.m_ObjectViewerQueryInActiveWindow         );
    DDX_Check(pDX, IDC_OVP_OL_QUERY_IN_ACTIVE_WINDOW       , m_settings.m_ObjectsListQueryInActiveWindow          );

    DDX_Check(pDX, IDC_OVP_SYNONYM_WO_OBJECT_INVALID       , m_settings.m_ObjectsListSynonymWithoutObjectInvalid  );

    DDX_Check(pDX, IDC_OVP_COMBO_SELECTOR                  , m_settings.m_ObjectsListUseComboSelector             );
    DDX_Check(pDX, IDC_OVP_TAB_ICONS_ONLY                  , m_settings.m_ObjectsListTabIconsOnly                 );
    DDX_Check(pDX, IDC_OVP_SCHEMAS_WITH_OBJECTS            , m_settings.m_ObjectsListOnlySchemasWithObjects       );

    if (m_hWnd && pDX && !pDX->m_bSaveAndValidate)
    {
        ::EnableWindow(::GetDlgItem(m_hWnd, IDC_OVP_USE_CACHE), m_settings.GetObjectCache());
        OnBnClicked_ComboSelector();
    }
}

BEGIN_MESSAGE_MAP(CObjectViewerPage, CPropertyPage)
    ON_BN_CLICKED(IDC_OVP_COMBO_SELECTOR, OnBnClicked_ComboSelector)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CObjectViewerPage message handlers

void CObjectViewerPage::OnBnClicked_ComboSelector ()
{
    try {
        EXCEPTION_FRAME;
        ASSERT(::GetDlgItem(m_hWnd, IDC_OVP_COMBO_SELECTOR));
        ASSERT(::GetDlgItem(m_hWnd, IDC_OVP_TAB_ICONS_ONLY));
        HWND hButton1 = ::GetDlgItem(m_hWnd, IDC_OVP_COMBO_SELECTOR);
        HWND hButton2 = ::GetDlgItem(m_hWnd, IDC_OVP_TAB_ICONS_ONLY);
        if (hButton1 && hButton2)
            ::EnableWindow(hButton2, ::SendMessage(hButton1, BM_GETCHECK, 0, 0L) != BST_CHECKED);
    }
    _OE_DEFAULT_HANDLER_;
}