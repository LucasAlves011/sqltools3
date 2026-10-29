/*
    SQLTools is a tool for Oracle database developers and DBAs.

    "SSH Tunnel" settings of a connection profile and the helper that opens the tunnel
    for a profile (asking the user to confirm unknown SSH host keys).
*/
#pragma once

#include <string>
#include "ConnectData\ConnectData.h"
#include "Ssh\SshTunnel.h"

class CSshTunnelDlg : public CDialog
{
    void setupAuth ();
    const wchar_t* validate () const;

public:
    enum { IDD = IDD_SSH_TUNNEL };

    BOOL         m_Enabled;
    std::wstring m_Host, m_Port, m_User, m_Secret, m_KeyFile;
    int          m_Auth;    // 0 - password, 1 - private key

    CSshTunnelDlg (CWnd* pParent = NULL);

protected:
    virtual void DoDataExchange (CDataExchange* pDX);
    virtual BOOL OnInitDialog ();
    virtual void OnOK ();

    DECLARE_MESSAGE_MAP()

    afx_msg void OnAuthChanged ();
    afx_msg void OnEnableChanged ();
    afx_msg void OnBrowse ();
    afx_msg void OnTest ();
};

namespace Ssh
{
    // Opens the tunnel for the profile and returns the local host/port the database client has to use instead of
    // the profile's ones. Shows messages itself; returns false if the user has to correct something or cancelled.
    // mainConnection: the tunnel belongs to the main connection (see StartMainTunnel), otherwise it is started in 'tunnel'.
    bool OpenTunnelFor (const ConnectEntry&, CWnd* parent, bool mainConnection, Tunnel* tunnel, std::wstring& host, std::wstring& port);
}
