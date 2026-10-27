/*
    Copyright (C) 2004 Aleksey Kochetov

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
#include "Common/PropertySheetMem.h"
#include "Theme/ModernDialog.h"
#include "Theme/ModernDraw.h"
#include "Theme/ModernDark.h"
#include <uxtheme.h>

#pragma comment(lib, "uxtheme.lib")

namespace Common {

    class CustomTreeCtrl : public CTreeCtrl {
    public:
        bool m_modern;
        CustomTreeCtrl (bool modern) : m_modern(modern) {}
    protected:
        DECLARE_MESSAGE_MAP()
        afx_msg int OnCreate (LPCREATESTRUCT cs);
        afx_msg void OnCustomDraw (NMHDR* pNMHDR, LRESULT* pResult);

        virtual BOOL PreCreateWindow(CREATESTRUCT& cs) {
            if (m_modern) {
                cs.style |= TVS_SHOWSELALWAYS|TVS_DISABLEDRAGDROP|TVS_FULLROWSELECT|TVS_NOHSCROLL;
                cs.style &= ~(TVS_TRACKSELECT|TVS_HASLINES|TVS_LINESATROOT|TVS_HASBUTTONS);
                cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
                return CTreeCtrl::PreCreateWindow(cs);
            }
            //cs.style |= TVS_SHOWSELALWAYS|TVS_SINGLEEXPAND|TVS_DISABLEDRAGDROP;
            //cs.style &= ~(TVS_TRACKSELECT|TVS_HASLINES|TVS_LINESATROOT|TVS_HASBUTTONS);
            cs.style |= TVS_SHOWSELALWAYS|TVS_DISABLEDRAGDROP|TVS_LINESATROOT|TVS_HASBUTTONS|TVS_FULLROWSELECT;
            cs.style &= ~(TVS_TRACKSELECT);
            cs.dwExStyle |= WS_EX_CLIENTEDGE;
            return CTreeCtrl::PreCreateWindow(cs);
        }
    };

BEGIN_MESSAGE_MAP(CustomTreeCtrl, CTreeCtrl)
    ON_WM_CREATE()
    ON_NOTIFY_REFLECT(NM_CUSTOMDRAW, OnCustomDraw)
END_MESSAGE_MAP()

int CustomTreeCtrl::OnCreate (LPCREATESTRUCT cs)
{
    if (CTreeCtrl::OnCreate(cs) == -1)
        return -1;
    if (m_modern)
    {
        ::SetWindowTheme(m_hWnd, ModernDark::ControlTheme(), NULL);   // barras de rolagem finas do Windows moderno
        SetBkColor(ModernTheme::Surface);
        SetTextColor(ModernTheme::Text);
        SetIndent(10);
        SetItemHeight(28);
    }
    return 0;
}

// selecao em tom de destaque, com cantos arredondados, no lugar do azul forte do sistema
void CustomTreeCtrl::OnCustomDraw (NMHDR* pNMHDR, LRESULT* pResult)
{
    using namespace ModernTheme;
    LPNMTVCUSTOMDRAW cd = reinterpret_cast<LPNMTVCUSTOMDRAW>(pNMHDR);
    *pResult = CDRF_DODEFAULT;
    if (!m_modern)
        return;

    switch (cd->nmcd.dwDrawStage)
    {
    case CDDS_PREPAINT:
        *pResult = CDRF_NOTIFYITEMDRAW;
        break;

    case CDDS_ITEMPREPAINT:
    {
        HTREEITEM item = (HTREEITEM)cd->nmcd.dwItemSpec;
        const bool selected = (cd->nmcd.uItemState & (CDIS_SELECTED | CDIS_FOCUS)) != 0 ||
                              (GetItemState(item, TVIS_SELECTED) & TVIS_SELECTED) != 0;
        const bool group = GetParentItem(item) == NULL && ItemHasChildren(item);

        CRect row, client;
        GetItemRect(item, row, FALSE);
        GetClientRect(client);
        row.left = client.left + 6;
        row.right = client.right - 6;
        row.DeflateRect(0, 1);

        CDC* dc = CDC::FromHandle(cd->nmcd.hdc);
        CRect line(client.left, row.top - 1, client.right, row.bottom + 1);
        dc->FillSolidRect(line, Surface);
        if (selected)
        {
            Gdiplus::Graphics g(dc->GetSafeHdc());
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            ModernDraw::FillRoundRect(g, row, 6.0f, AccentTint);
        }

        cd->clrText = selected ? Accent : (group ? TextMuted : Text);
        cd->clrTextBk = selected ? AccentTint : Surface;
        cd->nmcd.uItemState &= ~(CDIS_SELECTED | CDIS_FOCUS);
        *pResult = CDRF_NEWFONT;
        break;
    }
    }
}

CPropertySheetMem::CPropertySheetMem(LPCTSTR pszCaption, UINT& selectPage, CWnd* pParentWnd)
    : TreePropSheet::CTreePropSheet(pszCaption, pParentWnd, selectPage), m_selectPage(selectPage), m_modern(true)
{
	SetEmptyPageText(L"Please select a child item of '%s'.");
    SetTreeViewMode(/*bTreeViewMode =*/FALSE, /*bPageCaption =*/FALSE, /*bTreeImages =*/FALSE);
    SetAllExpand(TRUE);
}

CTreeCtrl* CPropertySheetMem::CreatePageTreeObject () 
{ 
    return new CustomTreeCtrl(m_modern); 
}

TreePropSheet::CPropPageFrame* CPropertySheetMem::CreatePageFrame ()
{
    if (m_modern)
        return new ModernDialog::CPageFrame;
    return TreePropSheet::CTreePropSheet::CreatePageFrame();
}

namespace
{
    // gancho por thread: cada pagina da folha ganha o visual moderno logo apos o WM_INITDIALOG
    HHOOK g_skinHook = NULL;

    LRESULT CALLBACK SkinHookProc (int code, WPARAM wp, LPARAM lp)
    {
        if (code == HC_ACTION)
        {
            const CWPRETSTRUCT* cwp = reinterpret_cast<const CWPRETSTRUCT*>(lp);
            // paginas sao dialogos filhos (WS_CHILD) da janela da folha, que e um dialogo de nivel superior
            if (cwp->message == WM_INITDIALOG && (::GetWindowLong(cwp->hwnd, GWL_STYLE) & WS_CHILD))
            {
                HWND parent = ::GetParent(cwp->hwnd);
                wchar_t cls[16] = L"";
                if (parent && ::GetClassName(parent, cls, 15) && wcscmp(cls, L"#32770") == 0 && !(::GetWindowLong(parent, GWL_STYLE) & WS_CHILD))
                    ModernDialog::SkinPage(cwp->hwnd);
            }
        }
        return ::CallNextHookEx(g_skinHook, code, wp, lp);
    }
}

BOOL CPropertySheetMem::OnInitDialog ()
{
    BOOL result = TreePropSheet::CTreePropSheet::OnInitDialog();
    if (m_modern)
        ModernDialog::SkinSheet(m_hWnd);
    return result;
}

INT_PTR CPropertySheetMem::DoModal ()
{
    if (!m_modern)
        return TreePropSheet::CTreePropSheet::DoModal();

    HHOOK prevHook = g_skinHook;
    g_skinHook = ::SetWindowsHookEx(WH_CALLWNDPROCRET, SkinHookProc, NULL, ::GetCurrentThreadId());
    INT_PTR result = TreePropSheet::CTreePropSheet::DoModal();
    if (g_skinHook)
        ::UnhookWindowsHookEx(g_skinHook);
    g_skinHook = prevHook;
    return result;
}

// CPropertySheetMem
BEGIN_MESSAGE_MAP(CPropertySheetMem, TreePropSheet::CTreePropSheet)
    ON_WM_DESTROY()
END_MESSAGE_MAP()

// CPropertySheetMem message handlers
void CPropertySheetMem::OnDestroy()
{
    m_selectPage = GetActiveIndex();
    CTreePropSheet::OnDestroy();
}

}; // namespace Common
