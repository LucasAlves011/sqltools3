#pragma once
#include <string>

class CConnectSettingsDlg : public CDialog
{
// Dialog Data
	enum { IDD = IDD_CONNECT_SETTINGS };
	CSize m_minSize;
public:
	CConnectSettingsDlg(CWnd* pParent = NULL);
	virtual ~CConnectSettingsDlg();

protected:
	virtual void DoDataExchange(CDataExchange* pDX); 
	DECLARE_MESSAGE_MAP()

public:
    std::wstring m_Passwords[2];
    bool m_SavePasswords;
    std::wstring m_OnLoginPlSqlBlock;
	bool m_OnLoginPlSqlBlockEnabled;
protected:
	virtual BOOL OnInitDialog();
	virtual void OnOK();
public:
	afx_msg void OnBnClickedCsEnableScripOnLogin();
	afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
};
