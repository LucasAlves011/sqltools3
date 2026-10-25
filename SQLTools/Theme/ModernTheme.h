#pragma once

// Visual moderno do SQLTools: paleta, gerenciador visual das barras/abas e atualizacao unica
// das fontes/cores padrao. Somente aparencia: nenhum comando ou comportamento muda.

#include <afxvisualmanagerofficexp.h>

namespace ModernTheme
{
    // Paleta (tema claro)
    const COLORREF Text          = RGB( 31,  35,  40);
    const COLORREF TextMuted     = RGB( 91, 101, 115);
    const COLORREF Surface       = RGB(255, 255, 255);
    const COLORREF SurfaceAlt    = RGB(245, 246, 248);
    const COLORREF Border        = RGB(226, 229, 234);
    const COLORREF BorderStrong  = RGB(207, 213, 221);
    const COLORREF Accent        = RGB( 47, 111, 237);
    const COLORREF AccentTint    = RGB(230, 238, 253);
    const COLORREF AccentStrong  = RGB(207, 224, 253);
    const COLORREF Danger        = RGB(214,  69,  65);
    const COLORREF Success       = RGB( 22, 136,  84);
    const COLORREF Favorite      = RGB(240, 160,   0);

    // Aplica (uma unica vez, com copia de seguranca) as fontes e cores modernas ao editor e as fontes
    // das janelas do SQLTools. Retorna true se alterou as configuracoes do SQLTools (precisam ser salvas).
    bool ApplyOnce ();
}

// Gerenciador visual plano para barras de ferramentas, menus, abas e paineis.
class CModernVisualManager : public CMFCVisualManagerOfficeXP
{
    DECLARE_DYNCREATE(CModernVisualManager)

public:
    CModernVisualManager ();

    virtual void OnUpdateSystemColors ();
    virtual void OnDrawTab (CDC* pDC, CRect rectTab, int iTab, BOOL bIsActive, const CMFCBaseTabCtrl* pTabWnd);

    // barra de status: fundo plano, separadores finos e texto discreto
    virtual void OnFillBarBackground (CDC* pDC, CBasePane* pBar, CRect rectClient, CRect rectClip, BOOL bNCArea = FALSE);
    virtual void OnDrawStatusBarPaneBorder (CDC* pDC, CMFCStatusBar* pBar, CRect rectPane, UINT uiID, UINT nStyle);
    virtual COLORREF GetStatusBarPaneTextColor (CMFCStatusBar* pStatusBar, CMFCStatusBarPaneInfo* pPane);
};
