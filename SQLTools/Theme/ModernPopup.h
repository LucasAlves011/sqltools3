#pragma once

// Base de paineis flutuantes (popovers) do visual moderno: janela de topo sem moldura, com cantos
// arredondados (Windows 11), sombra, desenho com buffer duplo e fechamento ao perder a ativacao.

#include "Theme/ModernDraw.h"

class CModernPopup : public CWnd
{
public:
    CModernPopup ();
    virtual ~CModernPopup ();

    // cria o painel perto de screenPos (canto superior esquerdo desejado), mantendo-o na area visivel
    BOOL CreatePopup (CWnd* owner, CPoint screenPos, CSize size);
    void Close ();

protected:
    virtual void Paint (CDC& dc, Gdiplus::Graphics& g, const CRect& client) = 0;
    virtual void OnPopupCreated () {}
    virtual void OnPopupShown () { SetFocus(); }
    virtual void OnPopupClosed () {}

    DECLARE_MESSAGE_MAP()
    afx_msg void OnPaint ();
    afx_msg BOOL OnEraseBkgnd (CDC*);
    afx_msg void OnActivate (UINT nState, CWnd* pWndOther, BOOL bMinimized);
    afx_msg void OnKeyDown (UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg UINT OnGetDlgCode ();
    afx_msg int  OnMouseActivate (CWnd*, UINT, UINT);

    virtual void PostNcDestroy ();
};
