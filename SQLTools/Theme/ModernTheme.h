#pragma once

// Visual moderno do SQLTools: paleta, gerenciador visual das barras/abas e atualizacao unica
// das fontes/cores padrao. Somente aparencia: nenhum comando ou comportamento muda.

#include <afxvisualmanagerofficexp.h>
#include <functional>

namespace ModernTheme
{
    // Paleta (tema claro)
    // (variaveis: o tema claro ou escuro e escolhido uma vez, na partida, por ApplyPalette)
    extern COLORREF Text;
    extern COLORREF TextMuted;
    extern COLORREF Surface;
    extern COLORREF SurfaceAlt;
    extern COLORREF Border;
    extern COLORREF BorderStrong;
    extern COLORREF Accent;
    extern COLORREF AccentTint;
    extern COLORREF AccentStrong;
    extern COLORREF Danger;
    extern COLORREF Success;
    extern COLORREF Favorite;
    extern COLORREF TextDisabled;
    extern COLORREF Hover;
    extern COLORREF Pressed;
    extern COLORREF RowAlt;
    extern COLORREF SelInactive;
    extern COLORREF GridAlt;
    extern COLORREF GridFix;
    extern COLORREF GridFixShadow;
    extern COLORREF GridLine;
    extern COLORREF Dim;
    extern COLORREF DimStrong;
    extern COLORREF ValidBg;
    extern COLORREF InvalidBg;
    extern COLORREF WarnBg;
    extern COLORREF WarnText;
    extern COLORREF AccentHover;
    extern COLORREF AccentPressed;
    extern COLORREF Scope;
    extern COLORREF ScopeFill;
    extern COLORREF ScopeBorder;
    extern COLORREF CaptionClose;
    extern COLORREF GutterText;

    // escolhe a paleta (true = tema escuro); chamar antes de criar qualquer janela
    void ApplyPalette (bool dark);

    // cor de sistema (GetSysColor) ou a equivalente da paleta no tema escuro
    COLORREF SysColor (int index);
    bool IsDark ();

    // troca de tema com o programa aberto (reaplica paleta, editor, icones e todas as janelas existentes)
    void SwitchTheme (bool dark);

    // quem guarda icones, pinceis ou cores em cache registra aqui como atualizar-se na troca de tema
    void AddThemeListener (std::function<void()> listener);

    // mensagem enviada a todas as janelas depois da troca (ON_REGISTERED_MESSAGE)
    extern const UINT UM_THEME_CHANGED;
    UINT ThemeChangedMessage ();

    // numero da paleta em uso: sobe a cada troca; caches de icones e pinceis comparam e se refazem
    unsigned Generation ();

    // pincel de cor solida que se refaz sozinho quando a cor muda (para o cache de WM_CTLCOLOR e classes)
    HBRUSH CachedBrush (CBrush& brush, COLORREF& cachedColor, COLORREF color);

    // tema escolhido pelo usuario (guardado no registro)
    bool LoadThemeSetting ();
    void SaveThemeSetting (bool dark);

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
    virtual void OnEraseTabsArea (CDC* pDC, CRect rect, const CMFCBaseTabCtrl* pTabWnd);
    virtual void OnDrawTabCloseButton (CDC* pDC, CRect rect, const CMFCBaseTabCtrl* pTabWnd, BOOL bIsHighlighted, BOOL bIsPressed, BOOL bIsDisabled);
    virtual void OnEraseTabsButton (CDC* pDC, CRect rect, CMFCButton* pButton, CMFCBaseTabCtrl* pWndTab);
    virtual void OnDrawTabsButtonBorder (CDC* pDC, CRect& rect, CMFCButton* pButton, UINT uiState, CMFCBaseTabCtrl* pWndTab);

    // barra de status: fundo plano, separadores finos e texto discreto
    virtual void OnFillBarBackground (CDC* pDC, CBasePane* pBar, CRect rectClient, CRect rectClip, BOOL bNCArea = FALSE);
    virtual void OnDrawStatusBarPaneBorder (CDC* pDC, CMFCStatusBar* pBar, CRect rectPane, UINT uiID, UINT nStyle);
    virtual COLORREF GetStatusBarPaneTextColor (CMFCStatusBar* pStatusBar, CMFCStatusBarPaneInfo* pPane);

    // legendas dos paineis (acoplados e flutuantes): planas, com borda fina
    virtual COLORREF OnDrawPaneCaption (CDC* pDC, CDockablePane* pBar, BOOL bActive, CRect rectCaption, CRect rectButtons);
    virtual void OnDrawCaptionButton (CDC* pDC, CMFCCaptionButton* pButton, BOOL bActive, BOOL bHorz, BOOL bMaximized, BOOL bDisabled, int nImageID = -1);
    virtual COLORREF OnFillMiniFrameCaption (CDC* pDC, CRect rectCaption, CPaneFrameWnd* pFrameWnd, BOOL bActive);
    virtual void OnDrawMiniFrameBorder (CDC* pDC, CPaneFrameWnd* pFrameWnd, CRect rectBorder, CRect rectBorderSize);
};
