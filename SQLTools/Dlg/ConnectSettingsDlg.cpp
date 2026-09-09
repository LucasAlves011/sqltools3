#include "stdafx.h"
#include "SQLTools.h"
#include "Dlg\ConnectSettingsDlg.h"
#include <COMMON\DlgDataExt.h>


CConnectSettingsDlg::CConnectSettingsDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CConnectSettingsDlg::IDD, pParent)
    , m_SavePasswords(false)
    , m_OnLoginPlSqlBlock()
{
}

CConnectSettingsDlg::~CConnectSettingsDlg()
{
}

void CConnectSettingsDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Text(pDX, IDC_CS_PASSWORD1, m_Passwords[0]);
    DDX_Text(pDX, IDC_CS_PASSWORD2, m_Passwords[1]);
    DDX_Check(pDX, IDC_CS_SAVE_PASSWORD, m_SavePasswords);
    DDX_Check(pDX, IDC_CS_ENABLE_SCRIP_ON_LOGIN, m_OnLoginPlSqlBlockEnabled);
    DDX_Text(pDX, IDC_CS_SCRIPT_ON_LOGON, m_OnLoginPlSqlBlock);

    if (pDX && !pDX->m_bSaveAndValidate)
        if (auto* item = GetDlgItem(IDC_CS_SCRIPT_ON_LOGON))
            item->EnableWindow(IsDlgButtonChecked(IDC_CS_ENABLE_SCRIP_ON_LOGIN) == BST_CHECKED);
}

BEGIN_MESSAGE_MAP(CConnectSettingsDlg, CDialog)
    ON_BN_CLICKED(IDC_CS_ENABLE_SCRIP_ON_LOGIN, &CConnectSettingsDlg::OnBnClickedCsEnableScripOnLogin)
    ON_WM_GETMINMAXINFO()
    ON_BN_CLICKED(IDHELP, OnHelp)
END_MESSAGE_MAP()

BOOL CConnectSettingsDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    CRect rc;
    GetWindowRect(rc);
    m_minSize = rc.Size();

    return TRUE;
}

void CConnectSettingsDlg::OnOK()
{
    UpdateData();

    if (m_Passwords[0] != m_Passwords[1])
    {
        MessageBeep((UINT)-1);
        AfxMessageBox(L"Master password and confirmation are not identical!", MB_OK|MB_ICONSTOP);
        ::SetFocus(::GetDlgItem(m_hWnd, IDC_CS_PASSWORD1));
        return;
    }
    
    CDialog::OnOK();
}

void CConnectSettingsDlg::OnBnClickedCsEnableScripOnLogin()
{
    if (auto* item = GetDlgItem(IDC_CS_SCRIPT_ON_LOGON))
        item->EnableWindow(IsDlgButtonChecked(IDC_CS_ENABLE_SCRIP_ON_LOGIN) == BST_CHECKED);
}

void CConnectSettingsDlg::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    lpMMI->ptMinTrackSize.x = m_minSize.cx;
    lpMMI->ptMinTrackSize.y = m_minSize.cy;
}
