#include "stdafx.h"
#include "Theme/ModernTheme.h"
#include "SQLTools.h"
#include "resource.h"
#include "Theme/ModernDraw.h"
#include "OpenEditor/OEDocument.h"
#include "COMMON/AppGlobal.h"
#include "COMMON/ConfigFilesLocator.h"
#include "COMMON/VisualAttributes.h"
#include <map>
#include <set>
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")
#include <string>

using namespace ModernTheme;

///////////////////////////////////////////////////////////////////////////////
// Visual manager
///////////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNCREATE(CModernVisualManager, CMFCVisualManagerOfficeXP)

CModernVisualManager::CModernVisualManager ()
{
    m_bShadowHighlightedImage = FALSE;
    m_bEmbossDisabledImage    = FALSE;
    m_bFadeInactiveImage      = FALSE;
}

namespace ModernTheme
{
    COLORREF Text = RGB( 31,  35,  40);
    COLORREF TextMuted = RGB( 91, 101, 115);
    COLORREF Surface = RGB(255, 255, 255);
    COLORREF SurfaceAlt = RGB(245, 246, 248);
    COLORREF Border = RGB(226, 229, 234);
    COLORREF BorderStrong = RGB(207, 213, 221);
    COLORREF Accent = RGB( 47, 111, 237);
    COLORREF AccentTint = RGB(230, 238, 253);
    COLORREF AccentStrong = RGB(207, 224, 253);
    COLORREF Danger = RGB(214,  69,  65);
    COLORREF Success = RGB( 22, 136,  84);
    COLORREF Favorite = RGB(240, 160,   0);
    COLORREF TextDisabled = RGB(154, 161, 171);
    COLORREF Hover = RGB(228, 231, 237);
    COLORREF Pressed = RGB(214, 218, 226);
    COLORREF RowAlt = RGB(250, 251, 252);
    COLORREF SelInactive = RGB(238, 240, 244);
    COLORREF GridAlt = RGB(248, 250, 252);
    COLORREF GridFix = RGB(243, 245, 248);
    COLORREF GridFixShadow = RGB(217, 222, 229);
    COLORREF GridLine = RGB(236, 239, 243);
    COLORREF Dim = RGB(190, 196, 205);
    COLORREF DimStrong = RGB(201, 206, 214);
    COLORREF ValidBg = RGB(225, 245, 236);
    COLORREF InvalidBg = RGB(253, 232, 232);
    COLORREF WarnBg = RGB(254, 243, 214);
    COLORREF WarnText = RGB(168, 104,   0);
    COLORREF AccentHover = RGB( 62, 125, 245);
    COLORREF AccentPressed = RGB( 35,  90, 200);
    COLORREF Scope = RGB(  0, 160, 230);
    COLORREF ScopeFill = RGB(220, 235, 252);
    COLORREF ScopeBorder = RGB(  0, 120, 215);
    COLORREF CaptionClose = RGB(236, 238, 242);
    COLORREF GutterText = RGB(138, 146, 156);

    static bool g_dark = false;

    bool IsDark ()
    {
        return g_dark;
    }

    void ApplyPalette (bool dark)
    {
        g_dark = dark;
    Text = dark ? RGB(230, 233, 238) : RGB( 31,  35,  40);
    TextMuted = dark ? RGB(150, 158, 170) : RGB( 91, 101, 115);
    Surface = dark ? RGB( 30,  32,  36) : RGB(255, 255, 255);
    SurfaceAlt = dark ? RGB( 38,  41,  47) : RGB(245, 246, 248);
    Border = dark ? RGB( 54,  58,  66) : RGB(226, 229, 234);
    BorderStrong = dark ? RGB( 74,  79,  90) : RGB(207, 213, 221);
    Accent = dark ? RGB( 96, 152, 255) : RGB( 47, 111, 237);
    AccentTint = dark ? RGB( 36,  52,  84) : RGB(230, 238, 253);
    AccentStrong = dark ? RGB( 48,  72, 120) : RGB(207, 224, 253);
    Danger = dark ? RGB(240,  98,  92) : RGB(214,  69,  65);
    Success = dark ? RGB( 56, 189, 126) : RGB( 22, 136,  84);
    Favorite = dark ? RGB(240, 170,  40) : RGB(240, 160,   0);
    TextDisabled = dark ? RGB(112, 118, 130) : RGB(154, 161, 171);
    Hover = dark ? RGB( 54,  59,  68) : RGB(228, 231, 237);
    Pressed = dark ? RGB( 66,  72,  84) : RGB(214, 218, 226);
    RowAlt = dark ? RGB( 34,  36,  41) : RGB(250, 251, 252);
    SelInactive = dark ? RGB( 48,  52,  60) : RGB(238, 240, 244);
    GridAlt = dark ? RGB( 34,  36,  41) : RGB(248, 250, 252);
    GridFix = dark ? RGB( 41,  44,  50) : RGB(243, 245, 248);
    GridFixShadow = dark ? RGB( 60,  65,  74) : RGB(217, 222, 229);
    GridLine = dark ? RGB( 48,  52,  60) : RGB(236, 239, 243);
    Dim = dark ? RGB(100, 106, 118) : RGB(190, 196, 205);
    DimStrong = dark ? RGB( 84,  90, 102) : RGB(201, 206, 214);
    ValidBg = dark ? RGB( 28,  58,  46) : RGB(225, 245, 236);
    InvalidBg = dark ? RGB( 70,  36,  38) : RGB(253, 232, 232);
    WarnBg = dark ? RGB( 72,  58,  26) : RGB(254, 243, 214);
    WarnText = dark ? RGB(232, 184,  76) : RGB(168, 104,   0);
    AccentHover = dark ? RGB(122, 172, 255) : RGB( 62, 125, 245);
    AccentPressed = dark ? RGB( 72, 124, 226) : RGB( 35,  90, 200);
    Scope = dark ? RGB( 70, 180, 240) : RGB(  0, 160, 230);
    ScopeFill = dark ? RGB( 36,  54,  84) : RGB(220, 235, 252);
    ScopeBorder = dark ? RGB( 70, 140, 230) : RGB(  0, 120, 215);
    CaptionClose = dark ? RGB( 60,  65,  74) : RGB(236, 238, 242);
    GutterText = dark ? RGB(120, 128, 140) : RGB(138, 146, 156);
    }

    COLORREF SysColor (int index)
    {
        if (g_dark)
        {
            switch (index)
            {
            case COLOR_WINDOW:        return Surface;
            case COLOR_WINDOWTEXT:
            case COLOR_BTNTEXT:
            case COLOR_HIGHLIGHTTEXT: return Text;
            case COLOR_BTNFACE:       return SurfaceAlt;
            case COLOR_HIGHLIGHT:     return AccentStrong;
            case COLOR_GRAYTEXT:      return TextMuted;
            case COLOR_BTNSHADOW:     return BorderStrong;   // = COLOR_3DSHADOW
            }
        }
        return ::GetSysColor(index);
    }

    bool LoadThemeSetting ()
    {
        return AfxGetApp()->GetProfileInt(L"Appearance", L"DarkTheme", 0) != 0;
    }

    void SaveThemeSetting (bool dark)
    {
        AfxGetApp()->WriteProfileInt(L"Appearance", L"DarkTheme", dark ? 1 : 0);
    }
}

void CModernVisualManager::OnUpdateSystemColors ()
{
    if (IsDark())
    {
        // tema escuro: as cores de sistema usadas pelo MFC (barras, menus, textos) passam a ser as da paleta
        afxGlobalData.clrBtnFace = afxGlobalData.clrBarFace = SurfaceAlt;
        afxGlobalData.clrBtnText = afxGlobalData.clrBarText = afxGlobalData.clrWindowText = Text;
        afxGlobalData.clrWindow = Surface;
        afxGlobalData.clrBtnLight = Border;
        afxGlobalData.clrBtnHilite = BorderStrong;
        afxGlobalData.clrGrayedText = TextDisabled;
        afxGlobalData.brBtnFace.DeleteObject();  afxGlobalData.brBtnFace.CreateSolidBrush(SurfaceAlt);
        afxGlobalData.brBarFace.DeleteObject();  afxGlobalData.brBarFace.CreateSolidBrush(SurfaceAlt);
        afxGlobalData.brWindow.DeleteObject();   afxGlobalData.brWindow.CreateSolidBrush(Surface);
    }

    CMFCVisualManagerOfficeXP::OnUpdateSystemColors();

    m_clrBarBkgnd            = Surface;
    m_clrMenuRarelyUsed      = SurfaceAlt;
    m_clrMenuLight           = Surface;
    m_clrInactiveTabText     = TextMuted;
    m_clrHighlight           = AccentTint;
    m_clrHighlightDn         = AccentStrong;
    m_clrHighlightChecked    = AccentTint;
    m_clrPressedButtonBorder = Accent;
    m_clrGripper             = BorderStrong;
    m_clrSeparator           = Border;
    m_clrPaneBorder          = Border;
    m_clrMenuBorder          = BorderStrong;
    m_clrMenuItemBorder      = AccentStrong;

    // linhas de separacao dos paineis, das abas e das barras (antes cinza escuro de 3D) ficam finas e claras
    afxGlobalData.clrBtnShadow   = BorderStrong;
    afxGlobalData.clrBtnDkShadow = BorderStrong;
    afxGlobalData.clrBarShadow   = BorderStrong;
    afxGlobalData.clrBarDkShadow = BorderStrong;
    afxGlobalData.clrBarLight    = SurfaceAlt;
    afxGlobalData.clrBarHilite   = Surface;
}

void CModernVisualManager::OnDrawTab (CDC* pDC, CRect rectTab, int iTab, BOOL bIsActive, const CMFCBaseTabCtrl* pTabWnd)
{
    ASSERT_VALID(pTabWnd);

    pDC->FillSolidRect(rectTab, SurfaceAlt);

    if (bIsActive)
    {
        // aba ativa: cartao branco com os cantos de fora arredondados e sublinhado de destaque
        const bool top = pTabWnd->GetLocation() == CMFCBaseTabCtrl::LOCATION_TOP;
        const int r = 7;
        CRect card(rectTab);
        card.DeflateRect(2, 0);
        CRect shape(card);
        if (top) shape.bottom += r; else shape.top -= r;

        int saved = pDC->SaveDC();
        pDC->IntersectClipRect(rectTab);
        Gdiplus::Graphics g(pDC->GetSafeHdc());
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        ModernDraw::FillRoundRect(g, shape, (float)r, Surface);
        pDC->RestoreDC(saved);

        CRect line(card);
        if (top) line.top = line.bottom - 2; else line.bottom = line.top + 2;
        pDC->FillSolidRect(line, Accent);
    }

    OnDrawTabContent(pDC, rectTab, iTab, bIsActive, pTabWnd, bIsActive ? Text : TextMuted);
}

void CModernVisualManager::OnEraseTabsArea (CDC* pDC, CRect rect, const CMFCBaseTabCtrl* pTabWnd)
{
    pDC->FillSolidRect(rect, SurfaceAlt);
    // linha fina do lado do conteudo
    if (pTabWnd && pTabWnd->GetLocation() == CMFCBaseTabCtrl::LOCATION_TOP)
        pDC->FillSolidRect(rect.left, rect.bottom - 1, rect.Width(), 1, Border);
    else
        pDC->FillSolidRect(rect.left, rect.top, rect.Width(), 1, Border);
}

void CModernVisualManager::OnDrawTabCloseButton (CDC* pDC, CRect rect, const CMFCBaseTabCtrl* /*pTabWnd*/, BOOL bIsHighlighted, BOOL bIsPressed, BOOL bIsDisabled)
{
    // "x" fino; ao passar o mouse, fundo arredondado discreto
    Gdiplus::Graphics g(pDC->GetSafeHdc());
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    if (bIsHighlighted && !bIsDisabled)
        ModernDraw::FillRoundRect(g, rect, 4.0f, bIsPressed ? Pressed : Hover);

    Gdiplus::Pen pen(ModernDraw::Clr(bIsDisabled ? BorderStrong : (bIsHighlighted ? Text : TextMuted)), 1.4f);
    pen.SetStartCap(Gdiplus::LineCapRound);
    pen.SetEndCap(Gdiplus::LineCapRound);
    const float cx = rect.CenterPoint().x + 0.0f, cy = rect.CenterPoint().y + 0.0f;
    g.DrawLine(&pen, cx - 3.0f, cy - 3.0f, cx + 3.0f, cy + 3.0f);
    g.DrawLine(&pen, cx - 3.0f, cy + 3.0f, cx + 3.0f, cy - 3.0f);
}

namespace
{
    // icone de chevron 16x16 com alfa, na cor pedida
    HICON MakeChevronIcon (bool left, COLORREF color)
    {
        Gdiplus::Bitmap bmp(16, 16, PixelFormat32bppARGB);
        {
            Gdiplus::Graphics g(&bmp);
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            g.Clear(Gdiplus::Color(0, 0, 0, 0));
            Gdiplus::Pen pen(ModernDraw::Clr(color), 1.7f);
            pen.SetStartCap(Gdiplus::LineCapRound);
            pen.SetEndCap(Gdiplus::LineCapRound);
            pen.SetLineJoin(Gdiplus::LineJoinRound);
            const float d = left ? -1.0f : 1.0f;
            Gdiplus::PointF p[3] = { Gdiplus::PointF(8.0f - 1.8f * d, 4.0f), Gdiplus::PointF(8.0f + 1.8f * d, 8.0f), Gdiplus::PointF(8.0f - 1.8f * d, 12.0f) };
            g.DrawLines(&pen, p, 3);
        }
        HICON icon = NULL;
        bmp.GetHICON(&icon);
        return icon;
    }

    bool IsLeftTabButton (CMFCButton* pButton)
    {
        bool left = true;
        if (CWnd* parent = pButton->GetParent())
        {
            CRect pr, br;
            parent->GetClientRect(pr);
            pButton->GetWindowRect(br);
            parent->ScreenToClient(br);
            left = br.CenterPoint().x < pr.CenterPoint().x;
        }
        return left;
    }
}

void CModernVisualManager::OnEraseTabsButton (CDC* pDC, CRect rect, CMFCButton* pButton, CMFCBaseTabCtrl* /*pWndTab*/)
{
    // troca, uma vez, o triangulo padrao por um chevron fino (o icone e desenhado pelo proprio botao)
    // (a direcao so e conhecida depois que o botao e posicionado, entao reavalia a cada desenho)
    static std::map<HWND, bool> direction;
    if (pButton)
    {
        const bool left = IsLeftTabButton(pButton);
        auto it = direction.find(pButton->GetSafeHwnd());
        if (it == direction.end() || it->second != left)
        {
            direction[pButton->GetSafeHwnd()] = left;
            pButton->SetImage(MakeChevronIcon(left, TextMuted), TRUE, MakeChevronIcon(left, Accent), MakeChevronIcon(left, BorderStrong), TRUE);
        }
    }

    pDC->FillSolidRect(rect, SurfaceAlt);
    if (pButton && pButton->IsWindowEnabled() && (pButton->IsHighlighted() || pButton->IsPushed()))
    {
        Gdiplus::Graphics g(pDC->GetSafeHdc());
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        CRect pad(rect);
        pad.DeflateRect(2, 3);
        ModernDraw::FillRoundRect(g, pad, 5.0f, pButton->IsPressed() ? Pressed : Hover);
    }
}

void CModernVisualManager::OnDrawTabsButtonBorder (CDC* /*pDC*/, CRect& /*rect*/, CMFCButton* /*pButton*/, UINT /*uiState*/, CMFCBaseTabCtrl* /*pWndTab*/)
{
    // sem moldura: o destaque ao passar o mouse e o fundo arredondado desenhado ao apagar o botao
}

void CModernVisualManager::OnFillBarBackground (CDC* pDC, CBasePane* pBar, CRect rectClient, CRect rectClip, BOOL bNCArea)
{
    CMFCVisualManagerOfficeXP::OnFillBarBackground(pDC, pBar, rectClient, rectClip, bNCArea);

    // linha fina no topo da barra de status
    if (pBar && pBar->IsKindOf(RUNTIME_CLASS(CMFCStatusBar)))
        pDC->FillSolidRect(rectClient.left, rectClient.top, rectClient.Width(), 1, Border);
}

void CModernVisualManager::OnDrawStatusBarPaneBorder (CDC* pDC, CMFCStatusBar* /*pBar*/, CRect rectPane, UINT uiID, UINT /*nStyle*/)
{
    // sem relevo: so um separador fino a esquerda de cada indicador (a mensagem principal nao tem)
    if (uiID != ID_SEPARATOR)
        pDC->FillSolidRect(rectPane.left, rectPane.top + 5, 1, (std::max)(0, rectPane.Height() - 10), Border);
}

COLORREF CModernVisualManager::GetStatusBarPaneTextColor (CMFCStatusBar* /*pStatusBar*/, CMFCStatusBarPaneInfo* pPane)
{
    if (!pPane)
        return TextMuted;
    if (pPane->nID == ID_INDICATOR_OCIGRID)
        return Accent;
    if (pPane->nID == ID_SEPARATOR || pPane->nID == 0)
        return Text;
    return TextMuted;
}

COLORREF CModernVisualManager::OnDrawPaneCaption (CDC* pDC, CDockablePane* /*pBar*/, BOOL /*bActive*/, CRect rectCaption, CRect /*rectButtons*/)
{
    pDC->FillSolidRect(rectCaption, Surface);
    pDC->FillSolidRect(rectCaption.left, rectCaption.bottom - 1, rectCaption.Width(), 1, Border);
    return Text;
}

namespace
{
    const UINT WM_APPLY_SHADOW = WM_APP + 77;

    LRESULT CALLBACK ShadowProc (HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR)
    {
        if (msg == WM_APPLY_SHADOW)
        {
            if (HMODULE hDwm = ::LoadLibraryW(L"dwmapi.dll"))
            {
                typedef HRESULT (WINAPI *SetAttr)(HWND, DWORD, LPCVOID, DWORD);
                typedef HRESULT (WINAPI *ExtendFrame)(HWND, const void*);
                if (SetAttr setAttr = (SetAttr)::GetProcAddress(hDwm, "DwmSetWindowAttribute"))
                {
                    const int policy = 2;   // DWMNCRP_ENABLED
                    setAttr(h, 2 /*DWMWA_NCRENDERING_POLICY*/, &policy, sizeof(policy));
                    const int corner = 2;   // DWMWCP_ROUND
                    setAttr(h, 33 /*DWMWA_WINDOW_CORNER_PREFERENCE*/, &corner, sizeof(corner));
                }
                if (ExtendFrame extend = (ExtendFrame)::GetProcAddress(hDwm, "DwmExtendFrameIntoClientArea"))
                {
                    const int margins[4] = { 0, 0, 0, 0 };
                    extend(h, margins);
                }
                ::FreeLibrary(hDwm);
            }
            // a sombra e criada quando a janela e exibida: reexibe a janela uma vez, sem ativar
            const UINT keep = SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE;
            ::SetWindowPos(h, NULL, 0, 0, 0, 0, keep | SWP_HIDEWINDOW);
            ::SetWindowPos(h, NULL, 0, 0, 0, 0, keep | SWP_SHOWWINDOW);
            return 0;
        }
        if (msg == WM_NCDESTROY)
            ::RemoveWindowSubclass(h, ShadowProc, 1);
        return ::DefSubclassProc(h, msg, wp, lp);
    }

    // janelas flutuantes (painel destacado) ganham a mesma sombra dos dialogos do sistema
    void EnsureWindowShadow (HWND hWnd)
    {
        static std::set<HWND> done;
        if (!hWnd || !done.insert(hWnd).second)
            return;
        ::SetClassLongPtr(hWnd, GCL_STYLE, ::GetClassLongPtr(hWnd, GCL_STYLE) | CS_DROPSHADOW);
        ::SetWindowSubclass(hWnd, ShadowProc, 1, 0);
        ::PostMessage(hWnd, WM_APPLY_SHADOW, 0, 0);
    }
}

COLORREF CModernVisualManager::OnFillMiniFrameCaption (CDC* pDC, CRect rectCaption, CPaneFrameWnd* pFrameWnd, BOOL /*bActive*/)
{
    if (pFrameWnd)
        if (!IsDark()) EnsureWindowShadow(pFrameWnd->GetSafeHwnd());
    pDC->FillSolidRect(rectCaption, Surface);
    pDC->FillSolidRect(rectCaption.left, rectCaption.bottom - 1, rectCaption.Width(), 1, Border);
    return Text;
}

void CModernVisualManager::OnDrawMiniFrameBorder (CDC* pDC, CPaneFrameWnd* /*pFrameWnd*/, CRect rectBorder, CRect rectBorderSize)
{
    // faixas da moldura em branco e um contorno fino por fora
    pDC->FillSolidRect(rectBorder.left, rectBorder.top, rectBorder.Width(), rectBorderSize.top, Surface);
    pDC->FillSolidRect(rectBorder.left, rectBorder.bottom - rectBorderSize.bottom, rectBorder.Width(), rectBorderSize.bottom, Surface);
    pDC->FillSolidRect(rectBorder.left, rectBorder.top, rectBorderSize.left, rectBorder.Height(), Surface);
    pDC->FillSolidRect(rectBorder.right - rectBorderSize.right, rectBorder.top, rectBorderSize.right, rectBorder.Height(), Surface);
    pDC->Draw3dRect(rectBorder, BorderStrong, BorderStrong);
}

void CModernVisualManager::OnDrawCaptionButton (CDC* pDC, CMFCCaptionButton* pButton, BOOL bActive, BOOL bHorz, BOOL bMaximized, BOOL bDisabled, int nImageID)
{
    // so o botao de fechar e redesenhado; os demais (fixar etc.) seguem o estilo base
    if (pButton->GetHit() != HTCLOSE)
    {
        CMFCVisualManagerOfficeXP::OnDrawCaptionButton(pDC, pButton, bActive, bHorz, bMaximized, bDisabled, nImageID);
        return;
    }

    // igual ao botao de fechar das janelas do sistema: "X" fino e, ao passar o mouse, fundo vermelho com "X" branco
    CRect rc = pButton->GetRect();
    const bool hot = (pButton->m_bFocused || pButton->m_bPushed) && !bDisabled;

    if (hot)
        pDC->FillSolidRect(rc, pButton->m_bPushed ? RGB(176, 36, 24) : RGB(196, 43, 28));

    Gdiplus::Graphics g(pDC->GetSafeHdc());
    g.SetSmoothingMode(Gdiplus::SmoothingModeNone);
    Gdiplus::Pen pen(hot ? Gdiplus::Color(255, 255, 255, 255) : ModernDraw::Clr(bDisabled ? TextMuted : Text), 1.0f);
    const int cx = rc.CenterPoint().x, cy = rc.CenterPoint().y;
    g.DrawLine(&pen, cx - 5, cy - 5, cx + 4, cy + 4);
    g.DrawLine(&pen, cx - 5, cy + 4, cx + 4, cy - 5);
}

///////////////////////////////////////////////////////////////////////////////
// Atualizacao unica das fontes e cores
///////////////////////////////////////////////////////////////////////////////

namespace
{
    const wchar_t* const SECTION = L"Appearance";
    const wchar_t* const KEY_THEME_VERSION = L"ModernThemeVersion";
    const int THEME_VERSION = 5;
    const wchar_t* const KEY_APPLIED_DARK = L"AppliedDark";

    void copyIfAbsent (const wchar_t* fileName)
    {
        std::wstring path;
        Common::ConfigFilesLocator::GetFilePath(fileName, path);
        std::wstring backup = path + L".pre-modern-theme";
        if (::PathFileExistsW(path.c_str()) && !::PathFileExistsW(backup.c_str()))
            ::CopyFileW(path.c_str(), backup.c_str(), TRUE);
    }

    struct EditorStyle { const char* name; COLORREF fg; bool bold, italic, underline; };

    // Editor, tema escuro
    const EditorStyle EDITOR_STYLES_DARK[] =
    {
        { "SQL Keywords",     RGB(198, 140, 255), false, false, false },
        { "SQL Functions",    RGB(110, 170, 240), false, false, false },
        { "PL/SQL Keywords",  RGB(198, 140, 255), false, false, false },
        { "SYS Packages",     RGB( 80, 200, 215), false, false, false },
        { "Operators",        RGB(210, 214, 222), false, false, false },
        { "String",           RGB(120, 205, 150), false, false, false },
        { "Number",           RGB(240, 165,  95), false, false, false },
        { "Comment",          RGB(120, 128, 140), false, true,  false },
        { "Line Comment",     RGB(120, 128, 140), false, true,  false },
        { "Bind Variable",    RGB(245, 140,  90), false, false, false },
        { "Substitution",     RGB(245, 140,  90), false, false, false },
        { "File name (@ & @@)", RGB( 80, 200, 215), false, false, false },
        { "Other Keywords",   RGB(198, 140, 255), false, false, false },
        { "User Objects",     RGB( 80, 200, 215), false, false, false },
        { "Keywords",         RGB(198, 140, 255), false, false, false },
        { "End of File",      RGB(100, 106, 118), false, false, false },
    };

    // Editor: apenas cores e estilo de fonte dos atributos que ja existem
    const EditorStyle EDITOR_STYLES[] =
    {
        { "SQL Keywords",     RGB(122,  63, 181), false, false, false },
        { "SQL Functions",    RGB( 27, 110, 194), false, false, false },
        { "PL/SQL Keywords",  RGB(122,  63, 181), false, false, false },
        { "SYS Packages",     RGB( 14, 116, 144), false, false, false },
        { "Operators",        RGB( 31,  35,  40), false, false, false },
        { "String",           RGB( 10, 125,  79), false, false, false },
        { "Number",           RGB(180,  83,   9), false, false, false },
        { "Comment",          RGB(138, 146, 156), false, true,  false },
        { "Line Comment",     RGB(138, 146, 156), false, true,  false },
        { "Bind Variable",    RGB(194,  65,  12), false, false, false },
        { "Substitution",     RGB(194,  65,  12), false, false, false },
        { "File name (@ & @@)", RGB(14, 116, 144), false, false, false },
        { "Other Keywords",   RGB(122,  63, 181), false, false, false },
        { "User Objects",     RGB( 14, 116, 144), false, false, false },
        { "Keywords",         RGB(122,  63, 181), false, false, false },
        { "End of File",      RGB(156, 163, 175), false, false, false },
    };

    void applyEditorTheme ()
    {
        OpenEditor::SettingsManager& mgr = COEDocument::GetSettingsManagerForUpdate();
        mgr.GetGlobalSettings()->SetRuler(false);   // a regua do editor sai por padrao (continua em Settings)

        for (int c = 0, climit = mgr.GetClassCount(); c < climit; ++c)
        {
            Common::VisualAttributesSet& set = mgr.GetClassByPos(c)->GetVisualAttributesSet();

            for (int i = 0, ilimit = set.GetCount(); i < ilimit; ++i)
            {
                Common::VisualAttribute& attr = set[i];
                const std::string& name = attr.m_Name;

                if (name == "Text")
                {
                    attr.m_FontName = "Consolas";
                    attr.m_FontSize = 12;
                    attr.m_Foreground = Text;
                    attr.m_Background = Surface;
                }
                else if (name == "Selected Text")
                {
                    attr.m_Foreground = Text;
                    attr.m_Background = AccentStrong;
                }
                else if (name == "Current Line")
                {
                    attr.m_Background = IsDark() ? RGB(36, 39, 46) : RGB(246, 248, 252);
                }
                else if (name == "Bookmark")
                {
                    attr.m_Background = RGB(59, 130, 246);
                }
                else if (name == "Random Bookmark")
                {
                    attr.m_Background = RGB(99, 102, 241);
                    attr.m_Foreground = RGB(255, 255, 255);
                }
                else if (name == "Highlighting")
                {
                    attr.m_Background = IsDark() ? RGB(36, 52, 84) : RGB(236, 243, 254);
                }
                else if (name == "Error Highlighting")
                {
                    attr.m_Background = IsDark() ? RGB(88, 40, 44) : RGB(253, 226, 226);
                }
                else
                {
                    const EditorStyle* styles = IsDark() ? EDITOR_STYLES_DARK : EDITOR_STYLES;
                    for (size_t k = 0; k < sizeof(EDITOR_STYLES) / sizeof(EDITOR_STYLES[0]); ++k)
                    {
                        if (name == styles[k].name)
                        {
                            attr.m_Foreground = styles[k].fg;
                            attr.m_FontBold = styles[k].bold;
                            attr.m_FontItalic = styles[k].italic;
                            attr.m_FontUnderline = styles[k].underline;
                            break;
                        }
                    }
                }
            }
        }
        COEDocument::SaveSettingsManager();
    }

    bool applyWindowFonts ()
    {
        struct WindowFont { const char* window; const char* font; int size; };
        static const WindowFont FONTS[] =
        {
            { "Data Grid Window",  "Segoe UI", 9 },
            { "Statistics Window", "Segoe UI", 9 },
            { "Bind Window",       "Segoe UI", 9 },
            { "Output Window",     "Consolas", 10 },
            { "History Window",    "Consolas", 10 },
        };

        bool changed = false;
        std::vector<Common::VisualAttributesSet*> sets = GetSQLToolsSettingsForUpdate().GetVASets();
        for (size_t s = 0; s < sets.size(); ++s)
        {
            for (size_t f = 0; f < sizeof(FONTS) / sizeof(FONTS[0]); ++f)
            {
                if (std::string(sets[s]->GetName()) != FONTS[f].window)
                    continue;

                for (int i = 0; i < sets[s]->GetCount(); ++i)
                {
                    Common::VisualAttribute& attr = (*sets[s])[i];
                    if (attr.m_Name == "Text")
                    {
                        attr.m_FontName = FONTS[f].font;
                        attr.m_FontSize = FONTS[f].size;
                        attr.m_Foreground = Text;
                        attr.m_Background = Surface;
                        changed = true;
                    }
                }
            }
        }
        return changed;
    }
}

bool ModernTheme::ApplyOnce ()
{
    CWinApp* app = AfxGetApp();

    try
    {
        // reaplica quando a versao do tema muda ou quando o usuario troca entre claro e escuro
        if (app->GetProfileInt(SECTION, KEY_THEME_VERSION, 0) >= THEME_VERSION
        && app->GetProfileInt(SECTION, KEY_APPLIED_DARK, 0) == (IsDark() ? 1 : 0))
            return false;

        // copia de seguranca das configuracoes de aparencia (so na primeira vez)
        copyIfAbsent(L"settings.xml");
        copyIfAbsent(L"sqltools.xml");

        applyEditorTheme();
        bool changed = applyWindowFonts();

        app->WriteProfileInt(SECTION, KEY_THEME_VERSION, THEME_VERSION);
        app->WriteProfileInt(SECTION, KEY_APPLIED_DARK, IsDark() ? 1 : 0);
        return changed;
    }
    catch (...)
    {
        return false;
    }
}
