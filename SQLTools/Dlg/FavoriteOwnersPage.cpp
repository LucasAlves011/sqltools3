#include "stdafx.h"
#include "SQLTools.h"
#include "FavoriteOwnersPage.h"
#include "DbBrowser/OwnerFavorites.h"

BEGIN_MESSAGE_MAP(CFavoriteOwnersPage, CPropertyPage)
    ON_BN_CLICKED(IDC_FAVO_ADD,    OnAdd)
    ON_BN_CLICKED(IDC_FAVO_REMOVE, OnRemove)
    ON_BN_CLICKED(IDC_FAVO_UP,     OnUp)
    ON_BN_CLICKED(IDC_FAVO_DOWN,   OnDown)
    ON_EN_CHANGE(IDC_FAVO_NAME,    OnNameChange)
    ON_LBN_SELCHANGE(IDC_FAVO_LIST, OnListSelChange)
END_MESSAGE_MAP()

CFavoriteOwnersPage::CFavoriteOwnersPage ()
    : CPropertyPage(CFavoriteOwnersPage::IDD, NULL)
{
    m_psp.dwFlags &= ~PSP_HASHELP;
    m_owners = OwnerFavorites::LoadGlobal();
}

BOOL CFavoriteOwnersPage::OnInitDialog ()
{
    CPropertyPage::OnInitDialog();
    FillList();
    UpdateButtons();
    return TRUE;
}

void CFavoriteOwnersPage::OnOK ()
{
    OwnerFavorites::SaveGlobal(m_owners);
    CPropertyPage::OnOK();
}

void CFavoriteOwnersPage::FillList (int select)
{
    CListBox* list = (CListBox*)GetDlgItem(IDC_FAVO_LIST);
    list->SetRedraw(FALSE);
    list->ResetContent();
    for (size_t i = 0; i < m_owners.size(); ++i)
        list->AddString(m_owners[i]);
    if (select >= 0 && select < (int)m_owners.size())
        list->SetCurSel(select);
    list->SetRedraw(TRUE);
    list->Invalidate();
    UpdateButtons();
}

void CFavoriteOwnersPage::UpdateButtons ()
{
    CListBox* list = (CListBox*)GetDlgItem(IDC_FAVO_LIST);
    int sel = list->GetCurSel();
    CString name;
    GetDlgItemText(IDC_FAVO_NAME, name);
    name.Trim();

    GetDlgItem(IDC_FAVO_ADD)->EnableWindow(!name.IsEmpty());
    GetDlgItem(IDC_FAVO_REMOVE)->EnableWindow(sel >= 0);
    GetDlgItem(IDC_FAVO_UP)->EnableWindow(sel > 0);
    GetDlgItem(IDC_FAVO_DOWN)->EnableWindow(sel >= 0 && sel + 1 < (int)m_owners.size());
}

void CFavoriteOwnersPage::OnNameChange ()
{
    UpdateButtons();
}

void CFavoriteOwnersPage::OnListSelChange ()
{
    UpdateButtons();
}

void CFavoriteOwnersPage::OnAdd ()
{
    CString name;
    GetDlgItemText(IDC_FAVO_NAME, name);
    name.Trim();
    if (name.IsEmpty())
        return;

    int at = -1;
    for (size_t i = 0; i < m_owners.size(); ++i)
        if (m_owners[i].CompareNoCase(name) == 0)
            at = (int)i;
    if (at < 0)
    {
        m_owners.push_back(name);
        at = (int)m_owners.size() - 1;
    }
    SetDlgItemText(IDC_FAVO_NAME, L"");
    FillList(at);
    GetDlgItem(IDC_FAVO_NAME)->SetFocus();
    SetModified();
}

void CFavoriteOwnersPage::OnRemove ()
{
    CListBox* list = (CListBox*)GetDlgItem(IDC_FAVO_LIST);
    int sel = list->GetCurSel();
    if (sel < 0 || sel >= (int)m_owners.size())
        return;
    m_owners.erase(m_owners.begin() + sel);
    FillList((std::min)(sel, (int)m_owners.size() - 1));
    SetModified();
}

void CFavoriteOwnersPage::OnUp ()
{
    CListBox* list = (CListBox*)GetDlgItem(IDC_FAVO_LIST);
    int sel = list->GetCurSel();
    if (sel <= 0 || sel >= (int)m_owners.size())
        return;
    std::swap(m_owners[sel], m_owners[sel - 1]);
    FillList(sel - 1);
    SetModified();
}

void CFavoriteOwnersPage::OnDown ()
{
    CListBox* list = (CListBox*)GetDlgItem(IDC_FAVO_LIST);
    int sel = list->GetCurSel();
    if (sel < 0 || sel + 1 >= (int)m_owners.size())
        return;
    std::swap(m_owners[sel], m_owners[sel + 1]);
    FillList(sel + 1);
    SetModified();
}
