#pragma once

// Settings: owners favoritos globais (aparecem como atalhos na Lista de objetos em todas as conexoes).
// Os favoritos por conexao sao os da estrela do painel de owners e nao passam por aqui.

#include <vector>

class CFavoriteOwnersPage : public CPropertyPage
{
public:
    CFavoriteOwnersPage ();

    enum { IDD = IDD_PROP_FAVORITE_OWNERS };

protected:
    std::vector<CString> m_owners;

    virtual BOOL OnInitDialog ();
    virtual void OnOK ();

    void FillList (int select = -1);
    void UpdateButtons ();

    DECLARE_MESSAGE_MAP()
    afx_msg void OnAdd ();
    afx_msg void OnRemove ();
    afx_msg void OnUp ();
    afx_msg void OnDown ();
    afx_msg void OnNameChange ();
    afx_msg void OnListSelChange ();
};
