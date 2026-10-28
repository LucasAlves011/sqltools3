/*
	SQLTools is a tool for Oracle database developers and DBAs.
    Copyright (C) 1997-2015 Aleksey Kochetov

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

/*
    13.03.2003 bug fix, if you are not connected and display the Object List the close button is dimmed.
    26.10.2003 bug fix, multiple occurences of PUBLIC in "Schema" list ("Object List" window)
    09.11.2003 bug fix, Cancel window does not work properly on Load DDL
    21.01.2004 bug fix, "Object List" error handling chanded to avoid paranoic bug reporting
    10.02.2004 bug fix, "Object List" a small issue with cancellation of ddl loading
    22.03.2004 bug fix, CreateAs file property is ignored for "Open In Editor", "Query", etc (always in Unix format)
    17.06.2005 B#1119107 - SYS. & dba_ views for DDL reverse-engineering (tMk).
    04.10.2006 B#XXXXXXX - cannot get DDL for db link if its name is longer than 30 chars
*/

#include "stdafx.h"
#include "SQLTools.h"
#include "Theme/ModernDraw.h"
#include "DbSourceWnd.h"
#include "OCI8/BCursor.h"
#include "DbBrowser\DBBrowserTableList.h"
#include "DbBrowser\DBBrowserCodeList.h"
#include "DbBrowser\DBBrowserTypeList.h"
#include "DbBrowser\DBBrowserConstraintList.h"
#include "DbBrowser\DBBrowserIndexList.h"
#include "DbBrowser\DBBrowserSequenceList.h"
#include "DbBrowser\DBBrowserViewList.h"
#include "DbBrowser\DBBrowserTriggerList.h"
#include "DbBrowser\DBBrowserSynonymList.h"
#include "DbBrowser\DBBrowserDbLinkList.h"
#include "DbBrowser\DBBrowserGranteeList.h"
#include "DbBrowser\DBBrowserClusterList.h"
#include "DbBrowser\DBBrowserRecyclebinList.h"
#include "DbBrowser\DBBrowserInvalidObjectList.h"
#include "COMMON/GUICommandDictionary.h"
#include "ServerBackgroundThread\TaskQueue.h"
#include <ActivePrimeExecutionNote.h>
#include "Dlg/ObjectViewerPage.h"
#include "Dlg/FavoriteOwnersPage.h"
#include <Common/PropertySheetMem.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

using namespace OraMetaDict;
using namespace ServerBackgroundThread;

#define IDC_LIST1 111

const int RECENT_SCHEMAS_SIZE = 6;

/////////////////////////////////////////////////////////////////////////////
// CDbSourceWnd

CDbSourceWnd::CDbSourceWnd ()
: CDialog(IDD),
  m_nItems(-1),
  m_nSelItems(-1),
  m_accelTable(NULL),
  m_schemaListStatus(SLS_EMPTY)
{
    m_bValid         = AfxGetApp()->GetProfileInt(L"Code", L"Valid",  TRUE);
    m_bInvalid       = AfxGetApp()->GetProfileInt(L"Code", L"Invalid",TRUE);
    m_nViewAs        = AfxGetApp()->GetProfileInt(L"Code", L"ViewAs", 1);

    auto settings = GetSQLToolsSettings();
    m_bUseCombo = settings.GetObjectsListUseComboSelector() ? TRUE : FALSE;
    m_bShowTabTitles = settings.GetObjectsListTabIconsOnly() ? FALSE : TRUE;
    m_bAllSchemas = settings.GetObjectsListOnlySchemasWithObjects() ? FALSE : TRUE;
}

CDbSourceWnd::~CDbSourceWnd ()
{
    try { EXCEPTION_FRAME;

        for (unsigned int i(0); i < m_wndTabLists.size(); i++)
            delete m_wndTabLists[i];

	    if (m_accelTable)
		    DestroyAcceleratorTable(m_accelTable);
    }
    _DESTRUCTOR_HANDLER_
}

DbBrowserList* CDbSourceWnd::GetCurSel () const
{
    DbBrowserList* wndListCtrl = 0;

    int nTab = GetCurrentTabIndex();

    if (nTab != -1)
        wndListCtrl = m_wndTabLists[nTab];

    _ASSERTE(wndListCtrl);

    return wndListCtrl;
}

int CDbSourceWnd::GetCurrentTabIndex () const
{
    if (m_bUseCombo)
        return m_wndTypeCombo.GetDroppedState() ? -1 : m_wndTypeCombo.GetCurSel();
    return m_wndTab.GetCurSel();
}

BOOL CDbSourceWnd::IsVisible () const
{
    const CWnd* pWnd = GetParent();
    if (!pWnd) pWnd = this;
    return pWnd->IsWindowVisible();
}

BOOL CDbSourceWnd::Create (CWnd* pParentWnd)
{
    return CDialog::Create(IDD, pParentWnd);
}

void CDbSourceWnd::DoDataExchange (CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_DS_SCHEMA,   m_wndSchemaList);
    DDX_Control(pDX, IDC_DS_STATUS,   m_wndStatus);
    DDX_Control(pDX, IDC_DS_TAB, m_wndTab);
    DDX_Check(pDX, IDC_DS_VALID, m_bValid);
    DDX_Check(pDX, IDC_DS_INVALID, m_bInvalid);
    DDX_CBString(pDX, IDC_DS_SCHEMA, m_strSchema);
    DDX_Radio(pDX, IDC_DS_AS_LIST, m_nViewAs);
}

BEGIN_MESSAGE_MAP(CDbSourceWnd, CDialog)
    ON_WM_SIZE()
    ON_WM_ERASEBKGND()
    ON_WM_CTLCOLOR()
    ON_NOTIFY_RANGE(NM_CUSTOMDRAW, IDC_DS_AS_LIST, IDC_DS_REFRESH, OnToolbarCustomDraw)
    ON_CONTROL(TBN_TYPE_SELCHANGE, IDC_DS_TYPEBAR, OnTypeBarSelChange)
    ON_CONTROL(2 /*TBN_TYPE_RELAYOUT*/, IDC_DS_TYPEBAR, OnTypeBarRelayout)
    ON_CONTROL(OBN_OWNER_PICK, IDC_DS_OWNERBAR, OnOwnerPick)
    ON_CONTROL(OBN_OWNER_OPEN, IDC_DS_OWNERBAR, OnOwnerOpen)
    ON_CBN_DROPDOWN(IDC_DS_SCHEMA, OnDropDownSchema)
    ON_CBN_SETFOCUS(IDC_DS_SCHEMA, OnSetFocusSchema)
    ON_CBN_SELCHANGE(IDC_DS_SCHEMA, OnSchemaChanged)
    ON_CBN_SELCHANGE(IDC_DS_TYPE_COMBO, OnSelChangeCombo)
    ON_WM_DESTROY()
    ON_NOTIFY(TCN_SELCHANGE, IDC_DS_TAB, OnSelChangeTab)
    ON_MESSAGE_VOID(WM_IDLEUPDATECMDUI, OnIdleUpdateCmdUI)
    ON_MESSAGE(WM_HELPHITTEST, OnHelpHitTest)

    ON_BN_CLICKED(IDC_DS_AS_LIST, OnShowAsList)
    ON_BN_CLICKED(IDC_DS_AS_REPORT, OnShowAsReport)
    ON_BN_CLICKED(IDC_DS_INVALID, OnToolbarFilterChanged)
    ON_BN_CLICKED(IDC_DS_VALID, OnToolbarFilterChanged)
    ON_BN_CLICKED(IDC_DS_SETTINGS, OnSettings)
    ON_BN_CLICKED(IDC_DS_FILTER, OnFilter)
    ON_BN_CLICKED(IDC_DS_REFRESH, OnRefresh)

    ON_COMMAND(ID_DS_REFRESH, OnRefresh)
    ON_COMMAND(ID_DS_SETTINGS, OnSettings)
    ON_COMMAND(ID_DS_TAB_TITLES, OnTabTitles)

	ON_NOTIFY_EX_RANGE(TTN_NEEDTEXTW, 0, 0xFFFF, OnToolTipText)
	ON_NOTIFY_EX_RANGE(TTN_NEEDTEXTA, 0, 0xFFFF, OnToolTipText)
    ON_WM_CONTEXTMENU()
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CDbSourceWnd message handlers


BOOL CDbSourceWnd::OnInitDialog()
{
    CDialog::OnInitDialog();

    UpdateData(FALSE);

    if (m_Images.m_hImageList == NULL)
        m_Images.Attach(ImageList_Duplicate(ModernDraw::Images().GetSafeHandle()));

    m_wndTab.SetImageList(&m_Images);

    m_wndTabLists.push_back(new DBBrowserTableList());
    m_wndTabLists.push_back(new DBBrowserPkConstraintList());
    m_wndTabLists.push_back(new DBBrowserUkConstraintList());
    m_wndTabLists.push_back(new DBBrowserFkConstraintList());
    m_wndTabLists.push_back(new DBBrowserChkConstraintList());
    m_wndTabLists.push_back(new DBBrowserIndexList());
    m_wndTabLists.push_back(new DBBrowserTriggerList());
    m_wndTabLists.push_back(new DBBrowserProcedureList());
    m_wndTabLists.push_back(new DBBrowserFunctionList());
    m_wndTabLists.push_back(new DBBrowserPackageList());
    m_wndTabLists.push_back(new DBBrowserPackageBodyList());
    m_wndTabLists.push_back(new DBBrowserTypeList());
    m_wndTabLists.push_back(new DBBrowserTypeBodyList());
    m_wndTabLists.push_back(new DBBrowserJavaList());
    m_wndTabLists.push_back(new DBBrowserSequenceList());
    m_wndTabLists.push_back(new DBBrowserViewList());
    m_wndTabLists.push_back(new DBBrowserSynonymList());
    m_wndTabLists.push_back(new DBBrowserDbLinkList());
    m_wndTabLists.push_back(new DBBrowserClusterList());
    m_wndTabLists.push_back(new DBBrowserGranteeList());
    m_wndTabLists.push_back(new DBBrowserRecyclebinList());
    m_wndTabLists.push_back(new DBBrowserInvalidObjectList());

    for (unsigned int i(0); i < m_wndTabLists.size(); i++)
    {
        m_wndTabLists[i]->Create(LVS_REPORT|WS_CHILD|WS_BORDER|WS_GROUP|WS_TABSTOP|LVS_SHOWSELALWAYS,
                                                         CRect(0,0,0,0), this, IDC_LIST1);
        //m_wndTabLists[i]->ModifyStyleEx(0, WS_EX_CLIENTEDGE, 0);
        m_wndTabLists[i]->SetOwner(this);

        m_wndTab.InsertItem(m_wndTab.GetItemCount(), 
            m_bShowTabTitles ? Common::wstr(m_wndTabLists[i]->GetTitle()).c_str() : L"", 
            m_wndTabLists[i]->GetImageIndex());
    }

    int nTab = AfxGetApp()->GetProfileInt(L"Code", L"nTab", 0);
    _ASSERTE((unsigned int)nTab < m_wndTabLists.size());

    // ---- create the CComboBoxEx type selector (dynamically, same row as tab) ----
    {
        CRect rcTab;
        m_wndTab.GetWindowRect(rcTab);
        ScreenToClient(&rcTab);
        m_wndTypeCombo.Create(
            WS_CHILD | WS_VSCROLL | CBS_DROPDOWNLIST | WS_TABSTOP,
            CRect(rcTab.left, rcTab.top, rcTab.right, rcTab.top + 400),
            this, IDC_DS_TYPE_COMBO);
        m_wndTypeCombo.SetFont(GetFont());
        m_wndTypeCombo.SetImageList(&m_Images);

        for (unsigned int i = 0; i < m_wndTabLists.size(); i++)
        {
            COMBOBOXEXITEM item = {};
            std::wstring title = Common::wstr(m_wndTabLists[i]->GetTitle());
            item.mask           = CBEIF_TEXT | CBEIF_IMAGE | CBEIF_SELECTEDIMAGE;
            item.iItem          = i;
            item.pszText        = const_cast<wchar_t*>(title.c_str());
            item.iImage         = m_wndTabLists[i]->GetImageIndex();
            item.iSelectedImage = m_wndTabLists[i]->GetImageIndex();
            m_wndTypeCombo.InsertItem(&item);
        }
    }

    // seletores modernos: owner (com favoritos) e barra de tipos
    m_wndOwnerBar.Create(this, IDC_DS_OWNERBAR);
    m_wndTypeBar.Create(this, IDC_DS_TYPEBAR);
    {
        std::vector<CTypeBar::Item> items;
        for (unsigned int i = 0; i < m_wndTabLists.size(); i++)
        {
            CTypeBar::Item it;
            it.title = Common::wstr(m_wndTabLists[i]->GetTitle()).c_str();
            it.imageIndex = m_wndTabLists[i]->GetImageIndex();
            it.category = CTypeBar::CategoryOfType((int)i);
            items.push_back(it);
        }
        m_wndTypeBar.SetItems(items);
        if (!m_wndTypeBar.IsTypeVisible(nTab))
            for (unsigned int i = 0; i < m_wndTabLists.size(); i++)
                if (m_wndTypeBar.IsTypeVisible((int)i)) { nTab = (int)i; break; }
        m_wndTypeBar.SetSel(nTab);
    }
    m_wndSchemaList.ShowWindow(SW_HIDE);   // continua guardando a lista de owners e o atual
    m_wndStatus.SetFlat();

    // Apply selector mode: show one control, hide the other
    if (m_bUseCombo)
    {
        m_wndTab.ShowWindow(SW_HIDE);
        m_wndTypeCombo.ShowWindow(SW_HIDE);
        m_wndTypeBar.ShowWindow(SW_SHOW);
        m_wndTypeCombo.SetCurSel(nTab);
    }
    else
    {
        m_wndTypeCombo.ShowWindow(SW_HIDE);
        m_wndTypeBar.ShowWindow(SW_HIDE);
        m_wndTab.SetCurSel(nTab);
    }

    if (theApp.GetConnectOpen()) EvOnConnect();
    else                         EvOnDisconnect();

    OnSwitchTab(nTab);

    // want to exlude these two controls from tab navigation
    ModifyStyle(::GetDlgItem(*this, IDC_DS_AS_REPORT), WS_TABSTOP, 0, 0);
    ModifyStyle(::GetDlgItem(*this, IDC_DS_AS_LIST), WS_TABSTOP, 0, 0);

    m_wndStatus.ModifyStyle(0, SS_OWNERDRAW);
    m_wndStatus.ModifyStyleEx(WS_EX_STATICEDGE | WS_EX_CLIENTEDGE, 0, SWP_FRAMECHANGED);
    m_wndStatus.AlignLeft();
    //ModifyStyleEx(0, WS_EX_COMPOSITED); // for smoother animation in status

    EnableToolTips();

	if (!m_accelTable)
	{
		CMenu menu;
        menu.CreatePopupMenu();
        menu.AppendMenu(MF_STRING, ID_SQL_OBJ_VIEWER,        L"dummy");
        menu.AppendMenu(MF_STRING, ID_VIEW_PROPERTIES,       L"dummy");
        menu.AppendMenu(MF_STRING, ID_SQL_DB_SOURCE,         L"dummy");
        menu.AppendMenu(MF_STRING, ID_VIEW_CODE_OUTLINE,     L"dummy");
        menu.AppendMenu(MF_STRING, ID_FILE_SHOW_GREP_OUTPUT, L"dummy");
        menu.AppendMenu(MF_STRING, ID_VIEW_FILE_PANEL,       L"dummy");
        menu.AppendMenu(MF_STRING, ID_VIEW_OPEN_FILES,       L"dummy");
        menu.AppendMenu(MF_STRING, ID_VIEW_HISTORY,          L"dummy");
        m_accelTable = Common::GUICommandDictionary::GetMenuAccelTable(menu.m_hMenu);
	}

    return TRUE;
}

void CDbSourceWnd::OnDestroy()
{
    int nTab = GetCurrentTabIndex();
    if (nTab != -1)
        AfxGetApp()->WriteProfileInt(L"Code", L"nTab",    nTab);

    AfxGetApp()->WriteProfileInt(L"Code", L"Valid",    m_bValid);
    AfxGetApp()->WriteProfileInt(L"Code", L"Invalid",  m_bInvalid);
    AfxGetApp()->WriteProfileInt(L"Code", L"ViewAs",   m_nViewAs);

    CDialog::OnDestroy();
}

void CDbSourceWnd::EvOnConnect ()
{
    if (theApp.GetDatabaseOpen())
    {
        CWnd* wndChild = GetWindow(GW_CHILD);
        while (wndChild)
        {
            wndChild->EnableWindow(TRUE);
            wndChild = wndChild->GetWindow(GW_HWNDNEXT);
        }

        InitSchemaList();
    }
}

void CDbSourceWnd::EvOnDisconnect ()
{
    for (unsigned int i(0); i < m_wndTabLists.size(); i++)
    {
        m_wndTabLists[i]->DeleteAllItems();
        m_wndTabLists[i]->ClearData();
    }

    CWnd* wndChild = GetWindow(GW_CHILD);
    while (wndChild)
    {
        wndChild->EnableWindow(FALSE);
        wndChild = wndChild->GetWindow(GW_HWNDNEXT);
    }

	m_recentSchemas.clear();
    m_wndSchemaList.ResetContent();
    m_schemaListStatus = SLS_EMPTY;
}

void CDbSourceWnd::EvOnSettingsChanged ()
{
    auto settings = GetSQLToolsSettings();

    if (m_hWnd && IsWindow(m_hWnd))
    {
        BOOL bUseCombo = settings.GetObjectsListUseComboSelector() ? TRUE : FALSE;
        if (bUseCombo != m_bUseCombo)
            OnUseCombo();

        BOOL bShowTabTitles = settings.GetObjectsListTabIconsOnly() ? FALSE : TRUE;

        if (bShowTabTitles != m_bShowTabTitles)
            OnTabTitles();

        BOOL bAllSchemas = settings.GetObjectsListOnlySchemasWithObjects() ? FALSE : TRUE;
        if (bAllSchemas != m_bAllSchemas)
        {
            m_bAllSchemas = bAllSchemas;
            InitSchemaList();
        }
    }
    else
    {
        m_bUseCombo = settings.GetObjectsListUseComboSelector() ? TRUE : FALSE;
        m_bShowTabTitles = settings.GetObjectsListTabIconsOnly() ? FALSE : TRUE;
        m_bAllSchemas = settings.GetObjectsListOnlySchemasWithObjects() ? FALSE : TRUE;
    }
}

void CDbSourceWnd::OnSize (UINT nType, int cx, int cy)
{
    CDialog::OnSize(nType, cx, cy);

    if (cx > 0 && cy > 0
    && nType != SIZE_MAXHIDE
    && nType != SIZE_MINIMIZED
    && m_wndOwnerBar.GetSafeHwnd())
        LayoutAll(cx, cy);
}

void CDbSourceWnd::LayoutAll (int cx, int cy)
{
    const int pad = 8, btn = 28, rowTop = 6, statusH = 24;

    auto place = [this](UINT id, int x, int y, int w, int h)
    {
        if (CWnd* pWnd = GetDlgItem(id))
            pWnd->SetWindowPos(0, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
    };

    // barra de status no rodape
    place(IDC_DS_STATUS, 0, cy - statusH, cx, statusH);

    // linha de botoes: atualizar | lista curta/detalhada | owner ... validos invalidos filtro configuracoes
    int x = pad;
    place(IDC_DS_REFRESH, x, rowTop, btn, btn);
    x += btn + 6;
    m_rcSeg = CRect(x, rowTop, x + 62, rowTop + btn);
    place(IDC_DS_AS_LIST,   x + 2,      rowTop + 2, 28, btn - 4);
    place(IDC_DS_AS_REPORT, x + 2 + 30, rowTop + 2, 28, btn - 4);
    x = m_rcSeg.right + 8;

    int rx = cx - pad - btn;
    place(IDC_DS_SETTINGS, rx, rowTop, btn, btn);  rx -= btn + 2;
    place(IDC_DS_FILTER,   rx, rowTop, btn, btn);  rx -= btn + 2;
    place(IDC_DS_INVALID,  rx, rowTop, btn, btn);  rx -= btn + 2;
    place(IDC_DS_VALID,    rx, rowTop, btn, btn);

    int ownerRight = rx - 8;
    int ownerW = (std::max)(m_wndOwnerBar.MinWidth(), ownerRight - x);
    m_wndOwnerBar.SetWindowPos(0, x, rowTop - 2, ownerW, 32, SWP_NOZORDER | SWP_NOACTIVATE);

    // seletor de tipo e listas
    int top = rowTop + btn + 6;
    int listTop;
    if (m_bUseCombo)
    {
        int h = m_wndTypeBar.CalcHeight(cx);
        m_wndTypeBar.SetWindowPos(0, 0, top, cx, h, SWP_NOZORDER | SWP_NOACTIVATE);
        listTop = top + h + 2;
    }
    else
    {
        m_wndTab.SetWindowPos(0, 0, top, cx, cy - statusH - top - 4, SWP_NOZORDER);

        CRect listRect;
        m_wndTab.GetWindowRect(&listRect);
        m_wndTab.AdjustRect(FALSE, &listRect);
        ScreenToClient(listRect);
        listRect.InflateRect(1, 0);

        for (unsigned int i(0); i < m_wndTabLists.size(); i++)
            m_wndTabLists[i]->SetWindowPos(&CWnd::wndTop, listRect.left, listRect.top, listRect.Width(), listRect.Height(), 0);
        Invalidate(FALSE);
        return;
    }

    int listHeight = cy - statusH - 4 - listTop;
    if (listHeight > 0)
        for (unsigned int i(0); i < m_wndTabLists.size(); i++)
            m_wndTabLists[i]->SetWindowPos(&CWnd::wndTop, 0, listTop, cx, listHeight, 0);

    Invalidate(FALSE);
}

BOOL CDbSourceWnd::OnEraseBkgnd (CDC* pDC)
{
    CRect rc;
    GetClientRect(rc);
    pDC->FillSolidRect(rc, ModernTheme::Surface);

    // fundo do par "lista curta / detalhada"
    if (!m_rcSeg.IsRectEmpty())
    {
        Gdiplus::Graphics g(pDC->GetSafeHdc());
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        ModernDraw::FillRoundRect(g, m_rcSeg, 7.0f, ModernTheme::SurfaceAlt);
    }
    return TRUE;
}

HBRUSH CDbSourceWnd::OnCtlColor (CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    if (nCtlColor == CTLCOLOR_DLG || nCtlColor == CTLCOLOR_STATIC)
    {
        static CBrush s_surface;
        static COLORREF s_color = CLR_INVALID;
        pDC->SetBkColor(ModernTheme::Surface);
        return ModernTheme::CachedBrush(s_surface, s_color, ModernTheme::Surface);
    }
    return CDialog::OnCtlColor(pDC, pWnd, nCtlColor);
}

void CDbSourceWnd::OnToolbarCustomDraw (UINT id, NMHDR* pNMHDR, LRESULT* pResult)
{
    *pResult = CDRF_DODEFAULT;
    NMCUSTOMDRAW* cd = reinterpret_cast<NMCUSTOMDRAW*>(pNMHDR);
    if (cd->dwDrawStage != CDDS_PREPAINT)
        return;

    ModernDraw::UiIcon icon;
    switch (id)
    {
    case IDC_DS_REFRESH:   icon = ModernDraw::UI_REFRESH;  break;
    case IDC_DS_AS_LIST:   icon = ModernDraw::UI_LIST;     break;
    case IDC_DS_AS_REPORT: icon = ModernDraw::UI_TABLE;    break;
    case IDC_DS_VALID:     icon = ModernDraw::UI_CHECK;    break;
    case IDC_DS_INVALID:   icon = ModernDraw::UI_X;        break;
    case IDC_DS_FILTER:    icon = ModernDraw::UI_FILTER;   break;
    case IDC_DS_SETTINGS:  icon = ModernDraw::UI_SETTINGS; break;
    default: return;
    }

    CDC* dc = CDC::FromHandle(cd->hdc);
    CRect rc(cd->rc);
    bool disabled = (cd->uItemState & CDIS_DISABLED) != 0;
    bool hot      = (cd->uItemState & CDIS_HOT) != 0 && !disabled;
    bool pressed  = (cd->uItemState & CDIS_SELECTED) != 0;
    bool checked  = ::SendMessage(cd->hdr.hwndFrom, BM_GETCHECK, 0, 0) == BST_CHECKED;
    bool seg      = (id == IDC_DS_AS_LIST || id == IDC_DS_AS_REPORT);

    dc->FillSolidRect(rc, seg ? ModernTheme::SurfaceAlt : ModernTheme::Surface);

    Gdiplus::Graphics g(dc->GetSafeHdc());
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

    if (seg && checked)
    {
        // opcao ativa do par: "pastilha" branca com borda fina
        ModernDraw::FillRoundRect(g, rc, 6.0f, ModernTheme::Surface);
        ModernDraw::StrokeRoundRect(g, rc, 6.0f, ModernTheme::BorderStrong);
    }
    else if (!seg && !disabled && (pressed || hot || checked))
    {
        ModernDraw::FillRoundRect(g, rc, 6.0f, pressed ? ModernTheme::AccentStrong : ModernTheme::AccentTint);
    }
    else if (seg && hot)
    {
        ModernDraw::FillRoundRect(g, rc, 6.0f, ModernTheme::AccentTint);
    }

    // validos/invalidos ja sao coloridos; os demais ganham a cor de destaque quando ativos
    bool accent = !disabled && (hot || checked || pressed);
    ModernDraw::DrawUiIcon(*dc, icon, rc.CenterPoint().x - 8, rc.CenterPoint().y - 8, accent, disabled);

    *pResult = CDRF_SKIPDEFAULT;
}

void CDbSourceWnd::OnTypeBarSelChange ()
{
    int nSel = m_wndTypeBar.GetSel();
    if (nSel >= 0)
    {
        m_wndTypeCombo.SetCurSel(nSel);
        OnSwitchTab(nSel);
    }
}

void CDbSourceWnd::OnTypeBarRelayout ()
{
    CRect rc;
    GetClientRect(rc);
    LayoutAll(rc.Width(), rc.Height());
}

void CDbSourceWnd::GetOwnerList (std::vector<CString>& owners) const
{
    owners.clear();
    for (int i = 0; i < m_wndSchemaList.GetCount(); ++i)
    {
        CString s;
        m_wndSchemaList.GetLBText(i, s);
        owners.push_back(s);
    }
}

void CDbSourceWnd::RefreshOwnerPopup ()
{
    if (m_wndOwnerBar.IsPopupOpen())
    {
        std::vector<CString> owners;
        GetOwnerList(owners);
        m_wndOwnerBar.UpdatePopup(owners, m_schemaListStatus == SLS_LOADING);
    }
}

void CDbSourceWnd::OnOwnerOpen ()
{
    PopulateSchemaList();          // carrega a lista completa de owners em segundo plano

    std::vector<CString> owners;
    GetOwnerList(owners);
    m_wndOwnerBar.ShowPopup(owners, m_recentSchemas, m_schemaListStatus == SLS_LOADING);
}

void CDbSourceWnd::OnOwnerPick ()
{
    const CString& owner = m_wndOwnerBar.GetPicked();
    if (!owner.IsEmpty())
        SelectSchema(owner);
}

void CDbSourceWnd::OnSwitchTab (unsigned int nTab)
{
    _ASSERTE((unsigned int)nTab < m_wndTabLists.size());

    if (nTab < m_wndTabLists.size())
    {
        if (m_bUseCombo && m_wndTypeBar.GetSafeHwnd() && m_wndTypeBar.GetSel() != (int)nTab)
            m_wndTypeBar.SetSel((int)nTab);

        for (unsigned int i(0); i < m_wndTabLists.size(); i++)
            m_wndTabLists[i]->ShowWindow(i == nTab ? SW_SHOW : SW_HIDE);

        if (m_nViewAs == 0)
            m_wndTabLists[nTab]->ModifyStyle(LVS_REPORT, LVS_LIST, 0);
        else
            m_wndTabLists[nTab]->ModifyStyle(LVS_LIST, LVS_REPORT, 0);

        if (theApp.GetConnectOpen())
        {
            CWaitCursor wait;

            m_wndTabLists[nTab]->Refresh();
            m_wndTabLists[nTab]->ApplyQuickFilter(m_bValid ? true : false, m_bInvalid ? true : false);

            BOOL enable = m_wndTabLists[nTab]->IsQuickFilterSupported() ? TRUE : FALSE;
            ::EnableWindow(::GetDlgItem(*this, IDC_DS_VALID), enable);
            ::EnableWindow(::GetDlgItem(*this, IDC_DS_INVALID), enable);

            HWND hFocus = ::GetFocus();
            bool selectorHasFocus = m_bUseCombo
                ? (m_wndTypeBar.m_hWnd == hFocus)
                : (m_wndTab.m_hWnd == hFocus);
            if (!selectorHasFocus)
                m_wndTabLists[nTab]->SetFocus();
        }

    }
}

void CDbSourceWnd::OnSelChangeTab (NMHDR*, LRESULT* pResult)
{
    OnSwitchTab(m_wndTab.GetCurSel());
    *pResult = 0;
}

void CDbSourceWnd::OnSelChangeCombo ()
{
    // Ignore intermediate changes while the dropdown is open (mouse tracking);
    // act only when the dropdown is already closed (keyboard navigation).
    if (m_wndTypeCombo.GetDroppedState())
        return;
    int nSel = m_wndTypeCombo.GetCurSel();
    if (nSel != -1)
        OnSwitchTab(nSel);
}

void CDbSourceWnd::OnUseCombo ()
{
    // Sync selection across both controls before switching visibility
    int nSel = !m_bUseCombo ? m_wndTab.GetCurSel() : m_wndTypeCombo.GetCurSel();

    m_bUseCombo = !m_bUseCombo;

    if (m_bUseCombo)
    {
        m_wndTab.ShowWindow(SW_HIDE);
        m_wndTypeCombo.ShowWindow(SW_HIDE);
        m_wndTypeBar.ShowWindow(SW_SHOW);
        if (nSel != -1)
        {
            m_wndTypeCombo.SetCurSel(nSel);
            m_wndTypeBar.SetSel(nSel);
        }
    }
    else
    {
        m_wndTypeCombo.ShowWindow(SW_HIDE);
        m_wndTypeBar.ShowWindow(SW_HIDE);
        m_wndTab.ShowWindow(SW_SHOW);
        if (nSel != -1)
            m_wndTab.SetCurSel(nSel);
    }

    // Reflow the layout
    CRect rc;
    GetClientRect(rc);
    OnSize(SIZE_RESTORED, rc.Width(), rc.Height());
}

void CDbSourceWnd::InitSchemaList ()
{
    m_wndSchemaList.ResetContent();

    try { EXCEPTION_FRAME;

        m_defSchema = theApp.GetCurrentSchema();
        m_strSchema = m_defSchema.c_str();
		addToRecentSchemas(Common::wstr(m_defSchema).c_str());
        m_wndSchemaList.SetCurSel(m_wndSchemaList.AddString(m_strSchema));
        m_schemaListStatus = SLS_CURRENT_ONLY;

        SetSchemaForObjectLists();
    }
    _DEFAULT_HANDLER_
}

    LPSCSTR cszSchemasSttm =
        "select username from sys.all_users a_u"
		" where username <> :schema"
        " and exists (select null from all_objects a_s where a_s.owner = a_u.username)";

    LPSCSTR cszAllSchemasSttm =
        "select username from sys.all_users a_u"
		" where username <> :schema";

	struct BackgroundTask_PopulateSchemaList : Task
    {
        CDbSourceWnd& m_dbSourceWnd;
        string m_defSchema;
        bool m_allSchemas;
        vector<string> m_schemas;

        BackgroundTask_PopulateSchemaList (CDbSourceWnd& dbSourceWnd)
            : Task("Populate SchemaList", (CWnd*)&dbSourceWnd),
            m_dbSourceWnd(dbSourceWnd),
            m_defSchema(dbSourceWnd.m_defSchema),
            m_allSchemas(dbSourceWnd.m_bAllSchemas)
        {
            m_dbSourceWnd.m_schemaListStatus = CDbSourceWnd::SLS_LOADING;
            m_dbSourceWnd.m_wndSchemaList.SetBusy(true);
        }

        void DoInBackground (OciConnect& connect)
        {
            ActivePrimeExecutionOnOff onOff;

            SetCursor(LoadCursor(NULL, IDC_APPSTARTING));
            OciCursor cursor(connect);
            cursor.Prepare(m_allSchemas ? cszAllSchemasSttm : cszSchemasSttm);
            cursor.Bind(":schema", m_defSchema.c_str());
            cursor.Execute();
            while (cursor.Fetch())
                m_schemas.push_back(cursor.ToString(0));
            //Sleep(1000);
        }

        void ReturnInForeground ()
        {
            if (m_dbSourceWnd.m_schemaListStatus == CDbSourceWnd::SLS_LOADING)
            {
                BOOL dropped = m_dbSourceWnd.m_wndSchemaList.GetDroppedState();
                if (dropped) 
                    m_dbSourceWnd.m_wndSchemaList.ShowDropDown(FALSE);

                vector<string>::const_iterator it = m_schemas.begin();
            
                for (; it != m_schemas.end(); ++it)
                {
                    CString ws = Common::wstr(*it).c_str();
                    if (m_dbSourceWnd.m_wndSchemaList.FindStringExact(-1, ws) == CB_ERR)
                        m_dbSourceWnd.m_wndSchemaList.AddString(ws);
                }

                // TODO: reimplement PUBLIC support! Forgot how :(
                if (m_dbSourceWnd.m_wndSchemaList.FindStringExact(-1, L"PUBLIC") == CB_ERR)
                    m_dbSourceWnd.m_wndSchemaList.AddString(L"PUBLIC");

                int curInx = m_dbSourceWnd.m_wndSchemaList.FindStringExact(-1, m_dbSourceWnd.m_strSchema);
                if (curInx != CB_ERR)
                    m_dbSourceWnd.m_wndSchemaList.SetCurSel(curInx);

                if (dropped) 
                    m_dbSourceWnd.m_wndSchemaList.ShowDropDown(TRUE);

                m_dbSourceWnd.m_schemaListStatus = CDbSourceWnd::SLS_LOADED;
            }
            m_dbSourceWnd.m_wndSchemaList.SetBusy(false);
            m_dbSourceWnd.RefreshOwnerPopup();
        }

        void ReturnErrorInForeground () 
        {
            m_dbSourceWnd.m_schemaListStatus = CDbSourceWnd::SLS_CURRENT_ONLY;
            m_dbSourceWnd.m_wndSchemaList.SetBusy(false);
        }
    };


void CDbSourceWnd::PopulateSchemaList ()
{
    if (m_schemaListStatus == SLS_CURRENT_ONLY)
        BkgdRequestQueue::Get().Push(TaskPtr(new BackgroundTask_PopulateSchemaList(*this)));
}

void CDbSourceWnd::OnDropDownSchema()
{
    PopulateSchemaList();
}

// need to populate the list before user presses any key on it
void CDbSourceWnd::OnSetFocusSchema ()
{
    PopulateSchemaList();
}

void CDbSourceWnd::SetSchemaForObjectLists ()
{
    for (unsigned int i(0); i < m_wndTabLists.size(); i++)
        m_wndTabLists[i]->SetSchema(Common::str(m_strSchema).c_str());
}

void CDbSourceWnd::addToRecentSchemas (const wchar_t* schema)
{
    //std::wstring schema = Common::wstr(_schema);

	auto it = std::find(m_recentSchemas.begin(), m_recentSchemas.end(), schema);

	if (it != m_recentSchemas.end())
		m_recentSchemas.erase(it);

	m_recentSchemas.push_back(schema);

	while (m_recentSchemas.size() > RECENT_SCHEMAS_SIZE)
		m_recentSchemas.erase(m_recentSchemas.begin());
}

void CDbSourceWnd::OnSchemaChanged ()
{
    CWaitCursor wait;

    UpdateData();
	addToRecentSchemas(m_strSchema);
    SetSchemaForObjectLists();

    int nTab = GetCurrentTabIndex();
    if (nTab != -1)
        m_wndTabLists[nTab]->Refresh();
}

void CDbSourceWnd::SelectSchema (const wchar_t* schemaName)
{
    CWaitCursor wait;

    m_strSchema = schemaName;
    int inx = m_wndSchemaList.FindStringExact(-1, schemaName);
    if (inx == CB_ERR)
    {
        inx = m_wndSchemaList.AddString(schemaName);
    }
    if (inx != CB_ERR)
    {
        m_wndSchemaList.SetCurSel(inx);
    }

    m_wndSchemaList.Invalidate();
    m_wndSchemaList.UpdateWindow();

    addToRecentSchemas(m_strSchema);
    SetSchemaForObjectLists();

    int nTab = GetCurrentTabIndex();
    if (nTab != -1 && (unsigned int)nTab < m_wndTabLists.size())
        m_wndTabLists[nTab]->Refresh();
}

void CDbSourceWnd::OnToolbarFilterChanged ()
{
    UpdateData();

    int nTab = GetCurrentTabIndex();
    if (nTab != -1)
        m_wndTabLists[nTab]->ApplyQuickFilter(m_bValid ? true : false, m_bInvalid ? true : false);
}

void CDbSourceWnd::OnRefresh ()
{
    CWaitCursor wait;

    if ((0xFF00 & GetKeyState(VK_CONTROL)))
        for (unsigned int i(0); i < m_wndTabLists.size(); i++)
            m_wndTabLists[i]->SetDirty(true);

    int nTab = GetCurrentTabIndex();
    if (nTab != -1)
        m_wndTabLists[nTab]->Refresh(true);
}

void CDbSourceWnd::OnShowAsList ()
{
    m_nViewAs = 0;
    int nTab = GetCurrentTabIndex();
    if (nTab != -1)
        m_wndTabLists[nTab]->ModifyStyle(LVS_REPORT, LVS_LIST, 0);

    ::SendMessage(::GetDlgItem(*this, IDC_DS_AS_REPORT), BM_SETCHECK, BST_UNCHECKED, 0L);
    ::SendMessage(::GetDlgItem(*this, IDC_DS_AS_LIST), BM_SETCHECK, BST_CHECKED, 0L);

    // want to exlude these two controls from tab navigation
    ModifyStyle(::GetDlgItem(*this, IDC_DS_AS_REPORT), WS_TABSTOP, 0, 0);
    ModifyStyle(::GetDlgItem(*this, IDC_DS_AS_LIST), WS_TABSTOP, 0, 0);
}

void CDbSourceWnd::OnShowAsReport ()
{
    m_nViewAs = 1;
    for (unsigned int i(0); i < m_wndTabLists.size(); i++)
        m_wndTabLists[i]->ModifyStyle(LVS_LIST, LVS_REPORT, 0);

    ::SendMessage(::GetDlgItem(*this, IDC_DS_AS_REPORT), BM_SETCHECK, BST_CHECKED, 0L);
    ::SendMessage(::GetDlgItem(*this, IDC_DS_AS_LIST), BM_SETCHECK, BST_UNCHECKED, 0L);

    // want to exlude these two controls from tab navigation
    ModifyStyle(::GetDlgItem(*this, IDC_DS_AS_REPORT), WS_TABSTOP, 0, 0);
    ModifyStyle(::GetDlgItem(*this, IDC_DS_AS_LIST), WS_TABSTOP, 0, 0);
}

void CDbSourceWnd::OnIdleUpdateCmdUI ()
{
    //TRACE("CDbSourceWnd::OnIdleUpdateCmdUI\n");

    if (!IsVisible())
        return;

    // favoritos por conexao: a chave e o texto de conexao (usuario@alias)
    m_wndOwnerBar.SetConnection(theApp.GetConnectOpen() ? CString(theApp.GetConnectDisplayString().c_str()) : CString());
    m_wndOwnerBar.SetCurrent(m_strSchema);

    int nTab = GetCurrentTabIndex();
    if (nTab != -1)
    {
        int nItems    = m_wndTabLists[nTab]->GetItemCount();
        int nSelItems = m_wndTabLists[nTab]->GetSelectedCount();

        if (m_nItems != nItems
        || m_nSelItems != nSelItems)
        {
            wchar_t buff[80]; 
            const int buff_size = sizeof(buff)/sizeof(buff[0]);
            buff[buff_size-1] = 0;
            m_nItems    = nItems;
            m_nSelItems = nSelItems;
            _snwprintf(buff, buff_size-1, L"Selected %d of %d", nSelItems, nItems);
            m_wndStatus.SetText(buff, RGB(0, 0, 0));
        }

        // TODO: find another way to refresh the current list
        if (m_wndTabLists[nTab]->IsDirty() 
            && theApp.GetConnectOpen() && !theApp.GetConnectSlow())
        {
            try {
                CWaitCursor wait;
                m_wndTabLists[nTab]->Refresh();
            }
            catch (...)
            {
                m_wndTabLists[nTab]->SetDirty(false); // otherwise infinite loop happens
            }
            m_wndTabLists[nTab]->ApplyQuickFilter(m_bValid ? true : false, m_bInvalid ? true : false);

            BOOL enable = m_wndTabLists[nTab]->IsQuickFilterSupported() ? TRUE : FALSE;
            ::EnableWindow(::GetDlgItem(*this, IDC_DS_VALID), enable);
            ::EnableWindow(::GetDlgItem(*this, IDC_DS_INVALID), enable);
        }

        bool busy = BkgdRequestQueue::Get().HasAnyByOwner(this);
        m_wndTabLists[nTab]->SetBusy(busy);
        m_wndStatus.StartAnimation(busy);
    }
}

//void CDbSourceWnd::OnSettings()
//{
//    SQLToolsSettings settings = GetSQLToolsSettings();
//    ShowDDLPreferences(settings, false);
//}
void CDbSourceWnd::OnSettings()
{
    SQLToolsSettings settings = GetSQLToolsSettings();

    CObjectViewerPage page(settings);
    page.m_psp.pszTitle = L"Object Viewer / List";
    page.m_psp.dwFlags |= PSP_USETITLE;

    static UINT gStarPage = 0;
    Common::CPropertySheetMem sheet(L"Settings", gStarPage);
    sheet.SetTreeViewMode(TRUE, FALSE, FALSE);   // mesmo visual do Settings principal
    sheet.SetModernStyle(true);
    sheet.SetTreeWidth(200);
    sheet.m_psh.dwFlags |= PSH_NOAPPLYNOW;
    sheet.m_psh.dwFlags &= ~PSH_HASHELP;
    sheet.AddPage(&page);
    CFavoriteOwnersPage ownersPage;
    sheet.AddPage(&ownersPage);

    int retVal = sheet.DoModal();

    if (retVal == IDOK)
    {
        GetSQLToolsSettingsForUpdate() = settings;
        GetSQLToolsSettingsForUpdate().NotifySettingsChanged();
    }
}


void CDbSourceWnd::OnFilter ()
{
    m_nViewAs = 0;
    int nTab = GetCurrentTabIndex();
    if (nTab != -1)
        m_wndTabLists[nTab]->OnFilter();
}

LRESULT CDbSourceWnd::OnHelpHitTest(WPARAM, LPARAM lParam)
{
    ASSERT_VALID(this);

    int nID = OnToolHitTest((DWORD)lParam, NULL);
    if (nID != -1)
        return HID_BASE_COMMAND+nID;

    return HID_BASE_CONTROL + IDD;
}

// See CControlBar::WindowProc
LRESULT CDbSourceWnd::WindowProc(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
    LRESULT lResult = 0;

    try { EXCEPTION_FRAME;

        if (nMsg == WM_NOTIFY)
        {
            // send these messages to the owner if not handled
            if (OnWndMsg(nMsg, wParam, lParam, &lResult))
                return lResult;
            else
            {
                // try owner next
                lResult = GetOwner()->SendMessage(nMsg, wParam, lParam);

                // special case for TTN_NEEDTEXTA and TTN_NEEDTEXTW
                if(nMsg == WM_NOTIFY)
                {
                    NMHDR* pNMHDR = (NMHDR*)lParam;
                    if (pNMHDR->code == TTN_NEEDTEXTA || pNMHDR->code == TTN_NEEDTEXTW)
                    {
                        TOOLTIPTEXTA* pTTTA = (TOOLTIPTEXTA*)pNMHDR;
                        TOOLTIPTEXTW* pTTTW = (TOOLTIPTEXTW*)pNMHDR;
                        if ((pNMHDR->code == TTN_NEEDTEXTA && (!pTTTA->lpszText || !*pTTTA->lpszText)) ||
                            (pNMHDR->code == TTN_NEEDTEXTW && (!pTTTW->lpszText || !*pTTTW->lpszText)))
                        {
                            // not handled by owner, so let bar itself handle it
                            lResult = CDialog::WindowProc(nMsg, wParam, lParam);
                        }
                    }
                }
                return lResult;
            }
        }
        // otherwise, just handle in default way
        lResult = CDialog::WindowProc(nMsg, wParam, lParam);
    }
    _DEFAULT_HANDLER_

    return lResult;
}

void CDbSourceWnd::OnOK ()
{
    int nTab = GetCurrentTabIndex();
    if (nTab != -1)
        m_wndTabLists[nTab]->SendMessage(WM_COMMAND, m_wndTabLists[nTab]->GetDefaultContextCommand(false));
}

void CDbSourceWnd::OnCancel ()
{
    AfxGetMainWnd()->SetFocus();
}

void CDbSourceWnd::OnTabTitles ()
{
    m_bShowTabTitles = !m_bShowTabTitles;

    // Update tab item labels (only meaningful when tab control is visible)
    if (!m_bUseCombo)
    {
        TCITEM item;
        item.mask = TCIF_TEXT;
        for (unsigned i = 0; i < m_wndTabLists.size(); i++)
        {
            std::wstring buff = Common::wstr(m_bShowTabTitles ? m_wndTabLists[i]->GetTitle() : "");
            item.pszText = const_cast<wchar_t*>(buff.c_str());
            m_wndTab.SetItem(i, &item);
        }

        CRect rect;
        m_wndTab.GetWindowRect(&rect);
        m_wndTab.AdjustRect(FALSE, &rect);
        ScreenToClient(rect);
        rect.InflateRect(1, 0);

        for (int i = 0; i < (int)m_wndTabLists.size(); i++)
            m_wndTabLists[i]->SetWindowPos(0, rect.left, rect.top, rect.Width(), rect.Height(), SWP_NOZORDER);
    }
}

#define COUNT_OF(array)         (sizeof(array)/sizeof(array[0]))
#define _AfxGetDlgCtrlID(hWnd)  ((UINT)(WORD)::GetDlgCtrlID(hWnd))
// based on CWorkbookMDIFrame::OnToolTipText
BOOL CDbSourceWnd::OnToolTipText (UINT, NMHDR* pNMHDR, LRESULT* pResult)
{
    ASSERT(pNMHDR->code == TTN_NEEDTEXTA || pNMHDR->code == TTN_NEEDTEXTW);

    // need to handle both ANSI and UNICODE versions of the message
    TOOLTIPTEXTA* pTTTA = (TOOLTIPTEXTA*)pNMHDR;
    TOOLTIPTEXTW* pTTTW = (TOOLTIPTEXTW*)pNMHDR;

    CString strTipText;
    UINT_PTR nID = pNMHDR->idFrom;
    if (pNMHDR->code == TTN_NEEDTEXTA && (pTTTA->uFlags & TTF_IDISHWND) ||
        pNMHDR->code == TTN_NEEDTEXTW && (pTTTW->uFlags & TTF_IDISHWND))
    {
        // idFrom is actually the HWND of the tool
        nID = _AfxGetDlgCtrlID((HWND)nID);
    }

    switch (nID)
    {
    case IDC_DS_SCHEMA    : strTipText = "Schema (Alt-S)";         break;
    case IDC_DS_VALID     : strTipText = "Include Valid (Alt-V)";  break;
    case IDC_DS_INVALID   : strTipText = "Include Invalid (Alt-V)"; break;
    case IDC_DS_AS_LIST   : strTipText = "Short List (Alt-L)";     break;
    case IDC_DS_AS_REPORT : strTipText = "Detail List (Alt-D)";    break;
    case IDC_DS_FILTER    : strTipText = "Filter (Alt-F)";         break;
    case IDC_DS_REFRESH   : strTipText = "Refresh (Alt-R) / Refresh All (Ctrl+Click)"; break;
    case IDC_DS_SETTINGS  : strTipText = "Settings (Alt-T)";       break;

    default:
        return FALSE;
    }

#ifndef _UNICODE
    if (pNMHDR->code == TTN_NEEDTEXTA)
        lstrcpyn(pTTTA->szText, strTipText, COUNT_OF(pTTTA->szText));
    else
        _mbstowcsz(pTTTW->szText, strTipText, COUNT_OF(pTTTW->szText));
#else
    if (pNMHDR->code == TTN_NEEDTEXTA)
        _wcstombsz(pTTTA->szText, strTipText, COUNT_OF(pTTTA->szText));
    else
        lstrcpyn(pTTTW->szText, strTipText, COUNT_OF(pTTTW->szText));
#endif
    *pResult = 0;

    // bring the tooltip window above other popup windows
    ::SetWindowPos(pNMHDR->hwndFrom, HWND_TOP, 0, 0, 0, 0,
        SWP_NOACTIVATE|SWP_NOSIZE|SWP_NOMOVE|SWP_NOOWNERZORDER);

    return TRUE;    // message was handled
}

void CDbSourceWnd::OnContextMenu(CWnd* pWnd, CPoint point)
{
	const int SCHEMA_COMMAND_BASE = 100;

    if (pWnd == this)
    {
        if (point.x == -1 && point.y == -1)
        {
            CRect rc;
            GetWindowRect(rc);
            point.x = rc.left;
            point.y = rc.top;
        }

        CMenu menu;
        menu.CreatePopupMenu();
        menu.AppendMenu(MF_STRING, ID_DS_REFRESH, L"&Refresh");
        menu.AppendMenu(MF_SEPARATOR);
        menu.AppendMenu(MF_STRING, ID_DS_SETTINGS, L"&Setting...");

        menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
    }
    else if (pWnd == &m_wndSchemaList)
    {
        if (point.x == -1 && point.y == -1)
        {
            CRect rc;
            m_wndSchemaList.GetWindowRect(rc);
            point.x = rc.left;
            point.y = rc.top;
        }

        CMenu menu;
        menu.CreatePopupMenu();
        menu.AppendMenu(MF_STRING, 1, L"&Refresh the Schema list");
        menu.AppendMenu(MF_SEPARATOR, (UINT_PTR)-1);

		if (m_recentSchemas.size() > 1)
		{
			auto it = m_recentSchemas.rbegin();
			auto end = m_recentSchemas.rend();
			for (int i = m_recentSchemas.size()-1; it != end; ++it, --i)
			{
				if (it->c_str() != m_strSchema)
					menu.AppendMenu(MF_STRING, SCHEMA_COMMAND_BASE + i, it->c_str());
			}

			menu.AppendMenu(MF_SEPARATOR, (UINT_PTR)-1);
		}
        menu.AppendMenu(MF_STRING, 2, L"&Settings...");
        int choice = (int)menu.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
        switch (choice)
        {
        case 1:
            InitSchemaList();
            PopulateSchemaList();
            break;
        case 2:
            OnSettings();
            break;
        default:
			{
				unsigned int index = choice - SCHEMA_COMMAND_BASE;
				if (index < m_recentSchemas.size())
				{                                           
                    auto it = m_recentSchemas.begin();
                    std::advance(it, index);
					int cbIndex = m_wndSchemaList.FindStringExact(0, it->c_str());
					if (cbIndex != LB_ERR)
					{
						m_wndSchemaList.SetCurSel(cbIndex);
						OnSchemaChanged();
					}
				}
			}
        }
    }
}

BOOL CDbSourceWnd::PreTranslateMessage(MSG* pMsg)
{
    if (m_accelTable
    && TranslateAccelerator(AfxGetMainWnd()->m_hWnd, m_accelTable, pMsg))
        return TRUE;
    return CDialog::PreTranslateMessage(pMsg);
}

BEGIN_MESSAGE_MAP(SchemaComboBox, CComboBox)
    ON_WM_SETCURSOR()
END_MESSAGE_MAP()

BOOL SchemaComboBox::OnSetCursor (CWnd* pWnd, UINT nHitTest, UINT message)
{
    if (nHitTest == HTCLIENT && IsBusy())
    {
        SetCursor(LoadCursor(NULL, IDC_APPSTARTING/*IDC_WAIT*/));
        return TRUE;
    }

    return CComboBox::OnSetCursor(pWnd, nHitTest, message);
}

void SchemaComboBox::SetBusy (bool busy)
{
    if (m_busy != busy)
    {
        m_busy = busy; 

        CPoint pt;
        GetCursorPos(&pt);
        ScreenToClient(&pt);

        CRect rc;
        GetClientRect(&rc);

        if (rc.PtInRect(pt))
            SetCursor(LoadCursor(NULL, m_busy ? IDC_APPSTARTING : IDC_ARROW));
    }
}
