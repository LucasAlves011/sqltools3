/*
    SQLTools is a tool for Oracle database developers and DBAs.
*/
#include "stdafx.h"
#include "resource.h"
#include "SshTunnelDlg.h"
#include <COMMON/DlgDataExt.h>
#include <COMMON/MyUtf.h>
#include <COMMON/StrHelpers.h>

BEGIN_MESSAGE_MAP(CSshTunnelDlg, CDialog)
    ON_CBN_SELCHANGE(IDC_SSH_AUTH, OnAuthChanged)
    ON_BN_CLICKED(IDC_SSH_ENABLE, OnEnableChanged)
    ON_BN_CLICKED(IDC_SSH_BROWSE, OnBrowse)
    ON_BN_CLICKED(IDC_SSH_TEST, OnTest)
END_MESSAGE_MAP()

CSshTunnelDlg::CSshTunnelDlg (CWnd* pParent)
    : CDialog(IDD, pParent), m_Enabled(FALSE), m_Auth(0)
{
}

void CSshTunnelDlg::DoDataExchange (CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Check(pDX, IDC_SSH_ENABLE, m_Enabled);
    DDX_Text(pDX, IDC_SSH_HOST, m_Host);
    DDX_Text(pDX, IDC_SSH_PORT, m_Port);
    DDX_Text(pDX, IDC_SSH_USER, m_User);
    DDX_CBIndex(pDX, IDC_SSH_AUTH, m_Auth);
    DDX_Text(pDX, IDC_SSH_SECRET, m_Secret);
    DDX_Text(pDX, IDC_SSH_KEYFILE, m_KeyFile);
}

BOOL CSshTunnelDlg::OnInitDialog ()
{
    CDialog::OnInitDialog();
    CComboBox* combo = (CComboBox*)GetDlgItem(IDC_SSH_AUTH);
    combo->AddString(L"Password");
    combo->AddString(L"Private key");
    if (m_Port.empty()) m_Port = L"22";
    UpdateData(FALSE);
    setupAuth();
    return TRUE;
}

void CSshTunnelDlg::setupAuth ()
{
    UpdateData(TRUE);
    const BOOL on = m_Enabled;
    const BOOL key = m_Auth == 1;
    for (UINT id : { IDC_SSH_HOST, IDC_SSH_PORT, IDC_SSH_USER, IDC_SSH_AUTH, IDC_SSH_SECRET })
        GetDlgItem(id)->EnableWindow(on);
    GetDlgItem(IDC_SSH_KEYFILE)->EnableWindow(on && key);
    GetDlgItem(IDC_SSH_BROWSE)->EnableWindow(on && key);
    GetDlgItem(IDC_SSH_TEST)->EnableWindow(on);
    SetDlgItemText(IDC_SSH_SECRET_LABEL, key ? L"&Passphrase:" : L"Pass&word:");
}

void CSshTunnelDlg::OnAuthChanged () { setupAuth(); }
void CSshTunnelDlg::OnEnableChanged () { setupAuth(); }

void CSshTunnelDlg::OnBrowse ()
{
    CFileDialog dlg(TRUE, NULL, NULL, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
        L"Private key files|*.pem;*.key;id_*|All files (*.*)|*.*||", this);
    if (dlg.DoModal() == IDOK)
        SetDlgItemText(IDC_SSH_KEYFILE, dlg.GetPathName());
}

const wchar_t* CSshTunnelDlg::validate () const
{
    int port = _wtoi(m_Port.c_str());
    if (m_Host.empty())                 return L"Enter the SSH host.";
    if (port < 1 || port > 65535)       return L"Enter a valid SSH port (1-65535).";
    if (m_User.empty())                 return L"Enter the SSH user name.";
    if (m_Auth == 1 && m_KeyFile.empty()) return L"Select the private key file.";
    if (m_Auth == 1 && m_KeyFile.size() > 4 && !_wcsicmp(m_KeyFile.c_str() + m_KeyFile.size() - 4, L".ppk"))
        return L"PuTTY .ppk keys are not supported. Export the key in OpenSSH format with PuTTYgen (Conversions > Export OpenSSH key).";
    return 0;
}

static Ssh::ConfirmHostKeyFn makeConfirm (HWND owner)
{
    return [owner] (const std::string& endpoint, const std::string& type, const std::string& fp) -> bool
    {
        std::wstring msg = L"The authenticity of the SSH server " + Common::wstr(endpoint) + L" can't be established.\n\n"
            L"Key type: " + Common::wstr(type) + L"\nFingerprint: " + Common::wstr(fp) + L"\n\n"
            L"Only continue if this fingerprint matches the one of your server (ask the administrator).\n"
            L"Trust this server and remember its key?";
        return ::MessageBox(owner, msg.c_str(), L"SSH Tunnel - unknown server", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDYES;
    };
}

// connects and authenticates only (no database involved) to let the user check the typed values
void CSshTunnelDlg::OnTest ()
{
    UpdateData(TRUE);
    if (const wchar_t* err = validate())
    {
        MessageBox(err, L"SSH Tunnel", MB_OK | MB_ICONEXCLAMATION);
        return;
    }

    Ssh::Config cfg;
    cfg.host    = Common::str(m_Host);
    cfg.port    = _wtoi(m_Port.c_str());
    cfg.user    = Common::str(m_User);
    cfg.useKey  = m_Auth == 1;
    cfg.keyFile = Common::str(m_KeyFile);
    cfg.secret  = Common::str(m_Secret);
    cfg.targetHost = "127.0.0.1";   // never used, no local connection is made
    cfg.targetPort = 1;

    if (!Ssh::Tunnel::IsAvailable())
    {
        MessageBox(L"SSH tunnels are not available in this build of SQLTools.", L"SSH Tunnel", MB_OK | MB_ICONEXCLAMATION);
        return;
    }

    try
    {
        CWaitCursor wait;
        Ssh::Tunnel tunnel;
        tunnel.Start(cfg, makeConfirm(m_hWnd));
        tunnel.Stop();
        MessageBeep(MB_OK);
        MessageBox((L"SSH connection to " + m_Host + L":" + m_Port + L" as \"" + m_User + L"\" is OK.").c_str(), 
            L"SSH Tunnel", MB_OK | MB_ICONASTERISK);
    }
    catch (const Ssh::Error& x)
    {
        MessageBeep((UINT)-1);
        MessageBox(Common::wstr(x.what()).c_str(), L"SSH Tunnel", MB_OK | MB_ICONHAND);
    }
}

void CSshTunnelDlg::OnOK ()
{
    UpdateData(TRUE);
    if (m_Enabled)
    {
        if (const wchar_t* err = validate())
        {
            MessageBox(err, L"SSH Tunnel", MB_OK | MB_ICONEXCLAMATION);
            return;
        }
    }
    CDialog::OnOK();
}

namespace Ssh
{
    bool OpenTunnelFor (const ConnectEntry& entry, CWnd* parent, bool mainConnection, Tunnel* tunnel, std::wstring& host, std::wstring& port)
    {
        HWND owner = parent ? parent->GetSafeHwnd() : 0;

        if (!Tunnel::IsAvailable())
        {
            ::MessageBox(owner, L"SSH tunnels are not available in this build of SQLTools.", L"SSH Tunnel", MB_OK | MB_ICONEXCLAMATION);
            return false;
        }
        if (entry.GetHost().empty() || entry.GetPort().empty())
        {
            ::MessageBox(owner, L"The SSH tunnel needs the database Host and TCP Port (as seen from the SSH server).\n\n"
                L"Check \"Bypass tnsnames.ora\" and fill in Host and TCP Port.", L"SSH Tunnel", MB_OK | MB_ICONEXCLAMATION);
            return false;
        }

        Config cfg;
        cfg.host    = Common::str(entry.GetSshHost());
        cfg.port    = _wtoi(entry.GetSshPort().c_str());
        cfg.user    = Common::str(entry.GetSshUser());
        cfg.useKey  = entry.GetSshAuth() == 1;
        cfg.keyFile = Common::str(entry.GetSshKeyFile());
        cfg.secret  = Common::str(entry.m_DecryptedSshPassword);
        cfg.targetHost = Common::str(entry.GetHost());
        cfg.targetPort = _wtoi(entry.GetPort().c_str());

        ConfirmHostKeyFn confirm = makeConfirm(owner);

        try
        {
            CWaitCursor wait;
            unsigned short local = 0;
            if (mainConnection)
                local = StartMainTunnel(cfg, confirm);
            else
            {
                tunnel->Start(cfg, confirm);
                local = tunnel->LocalPort();
            }
            host = L"127.0.0.1";
            port = std::to_wstring(local);
            return true;
        }
        catch (const Error& x)
        {
            ::MessageBox(owner, Common::wstr(x.what()).c_str(), L"SSH Tunnel", MB_OK | MB_ICONERROR);
        }
        return false;
    }
}
