#include "stdafx.h"
#include "Theme/ModernDark.h"
#include "Theme/ModernTheme.h"
#include <commctrl.h>
#include <uxtheme.h>
#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "comctl32.lib")

using namespace ModernTheme;

namespace
{
    // ---- modo escuro do aplicativo (menus de contexto e controles do sistema) ----
    enum PreferredAppMode { Default, AllowDark, ForceDark, ForceLight };
    typedef PreferredAppMode (WINAPI *SetPreferredAppModeFn)(PreferredAppMode);
    typedef void (WINAPI *FlushMenuThemesFn)();

    // ---- barra de menus (mensagens nao documentadas WM_UAHDRAWMENU / WM_UAHDRAWMENUITEM) ----
    const UINT WM_UAHDRAWMENU = 0x0091;
    const UINT WM_UAHDRAWMENUITEM = 0x0092;

    struct UAHMENU { HMENU hmenu; HDC hdc; DWORD dwFlags; };
    struct UAHMENUITEMMETRICS { DWORD data[4]; };
    struct UAHMENUPOPUPMETRICS { DWORD data[4]; };
    struct UAHMENUITEM { int iPosition; UAHMENUITEMMETRICS umim; UAHMENUPOPUPMETRICS umpm; };
    struct UAHDRAWMENUITEM { DRAWITEMSTRUCT dis; UAHMENU um; UAHMENUITEM umi; };

    void PaintMenuBarLine (HWND hWnd)
    {
        MENUBARINFO mbi = { sizeof(mbi) };
        if (!::GetMenuBarInfo(hWnd, OBJID_MENU, 0, &mbi))
            return;

        CRect client, window;
        ::GetClientRect(hWnd, client);
        ::MapWindowPoints(hWnd, NULL, (LPPOINT)(LPRECT)client, 2);
        ::GetWindowRect(hWnd, window);
        CRect line(client.left, client.top - 1, client.right, client.top);
        line.OffsetRect(-window.left, -window.top);

        HDC hdc = ::GetWindowDC(hWnd);
        CBrush brush(SurfaceAlt);
        ::FillRect(hdc, line, brush);
        ::ReleaseDC(hWnd, hdc);
    }

    LRESULT CALLBACK MenuBarProc (HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR)
    {
        switch (msg)
        {
        case WM_UAHDRAWMENU:
        {
            UAHMENU* um = (UAHMENU*)lp;
            MENUBARINFO mbi = { sizeof(mbi) };
            if (::GetMenuBarInfo(h, OBJID_MENU, 0, &mbi))
            {
                CRect window;
                ::GetWindowRect(h, window);
                CRect bar = mbi.rcBar;
                bar.OffsetRect(-window.left, -window.top);
                CBrush brush(SurfaceAlt);
                ::FillRect(um->hdc, bar, brush);
                return TRUE;
            }
            break;
        }
        case WM_UAHDRAWMENUITEM:
        {
            UAHDRAWMENUITEM* item = (UAHDRAWMENUITEM*)lp;
            wchar_t text[128] = L"";
            MENUITEMINFO mii = { sizeof(mii) };
            mii.fMask = MIIM_STRING;
            mii.dwTypeData = text;
            mii.cch = 127;
            ::GetMenuItemInfo(item->um.hmenu, item->umi.iPosition, TRUE, &mii);

            const DWORD state = item->dis.itemState;
            const bool hot = (state & (ODS_HOTLIGHT | ODS_SELECTED)) != 0;
            const bool disabled = (state & (ODS_DISABLED | ODS_GRAYED)) != 0;

            CBrush back(hot ? Hover : SurfaceAlt);
            ::FillRect(item->um.hdc, &item->dis.rcItem, back);
            ::SetBkMode(item->um.hdc, TRANSPARENT);
            ::SetTextColor(item->um.hdc, disabled ? TextDisabled : Text);

            DWORD flags = DT_CENTER | DT_SINGLELINE | DT_VCENTER;
            if (state & ODS_NOACCEL)
                flags |= DT_HIDEPREFIX;
            HFONT font = (HFONT)::GetCurrentObject(item->um.hdc, OBJ_FONT);
            (void)font;
            ::DrawText(item->um.hdc, text, -1, &item->dis.rcItem, flags);
            return TRUE;
        }
        case WM_NCPAINT:
        case WM_NCACTIVATE:
        {
            LRESULT r = ::DefSubclassProc(h, msg, wp, lp);
            PaintMenuBarLine(h);
            return r;
        }
        case WM_NCDESTROY:
            ::RemoveWindowSubclass(h, MenuBarProc, 1);
            break;
        }
        return ::DefSubclassProc(h, msg, wp, lp);
    }

    // cores dos icones: tons escuros viram claros; as cores de destaque ficam um pouco mais vivas
    struct Remap { BYTE r, g, b; BYTE nr, ng, nb; };
    const Remap REMAPS[] =
    {
        {  71,  84, 103, 200, 206, 216 },   // cinza-azulado (padrao dos icones)
        {  47, 111, 237, 110, 165, 255 },   // azul
        {  22, 136,  84,  70, 200, 130 },   // verde
        { 214,  69,  65, 245, 112, 106 },   // vermelho
        { 217, 138,   0, 245, 178,  52 },   // ambar
    };

    void RecolorPixels (BYTE* px, size_t count, bool premultiplied, bool reverse = false)
    {
        for (size_t i = 0; i < count; ++i, px += 4)
        {
            const int a = px[3];
            if (a == 0)
                continue;
            int b = px[0], g = px[1], r = px[2];
            if (premultiplied && a < 255)
            {
                // a imagem guarda as cores ja multiplicadas pelo alfa: volta ao valor original para comparar
                b = (std::min)(255, b * 255 / a);
                g = (std::min)(255, g * 255 / a);
                r = (std::min)(255, r * 255 / a);
            }
            int best = -1, bestDist = 3 * 70 * 70;
            for (int k = 0; k < (int)(sizeof(REMAPS) / sizeof(REMAPS[0])); ++k)
            {
                // reverse: o icone ja esta nas cores do tema escuro e volta as originais
                const BYTE fr = reverse ? REMAPS[k].nr : REMAPS[k].r, fg = reverse ? REMAPS[k].ng : REMAPS[k].g, fb = reverse ? REMAPS[k].nb : REMAPS[k].b;
                int dr = r - fr, dg = g - fg, db = b - fb;
                int d = dr * dr + dg * dg + db * db;
                if (d < bestDist) { bestDist = d; best = k; }
            }
            if (best >= 0)
            {
                r = reverse ? REMAPS[best].r : REMAPS[best].nr;
                g = reverse ? REMAPS[best].g : REMAPS[best].ng;
                b = reverse ? REMAPS[best].b : REMAPS[best].nb;
            }
            if (premultiplied && a < 255)
            {
                r = r * a / 255;
                g = g * a / 255;
                b = b * a / 255;
            }
            px[2] = (BYTE)r;
            px[1] = (BYTE)g;
            px[0] = (BYTE)b;
        }
    }
}

namespace ModernDark
{
    void InitApp ()
    {
        if (HMODULE ux = ::GetModuleHandleW(L"uxtheme.dll"))
        {
            if (SetPreferredAppModeFn setMode = (SetPreferredAppModeFn)::GetProcAddress(ux, MAKEINTRESOURCEA(135)))
                setMode(ForceDark);
            if (FlushMenuThemesFn flush = (FlushMenuThemesFn)::GetProcAddress(ux, MAKEINTRESOURCEA(136)))
                flush();
        }
    }

    const wchar_t* ControlTheme ()
    {
        return IsDark() ? L"DarkMode_Explorer" : L"Explorer";
    }

    void StyleTitleBar (HWND hWnd)
    {
        const BOOL dark = IsDark() ? TRUE : FALSE;
        ::DwmSetWindowAttribute(hWnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &dark, sizeof(dark));
        // tema claro: cores padrao do Windows (DWMWA_COLOR_DEFAULT)
        COLORREF caption = IsDark() ? SurfaceAlt : 0xFFFFFFFF, text = IsDark() ? Text : 0xFFFFFFFF;
        ::DwmSetWindowAttribute(hWnd, 35 /*DWMWA_CAPTION_COLOR*/, &caption, sizeof(caption));
        ::DwmSetWindowAttribute(hWnd, 36 /*DWMWA_TEXT_COLOR*/, &text, sizeof(text));
    }

    void StyleMenuBar (HWND hFrame)
    {
        if (IsDark())
            ::SetWindowSubclass(hFrame, MenuBarProc, 1, 0);
    }

    // troca de tema com o programa aberto: liga ou desliga o desenho escuro da barra de menus
    void RestyleMenuBar (HWND hFrame)
    {
        ::RemoveWindowSubclass(hFrame, MenuBarProc, 1);
        if (IsDark())
            ::SetWindowSubclass(hFrame, MenuBarProc, 1, 0);
        ::DrawMenuBar(hFrame);
        ::SetWindowPos(hFrame, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }

    // modo do aplicativo no Windows (menus de contexto, barras de rolagem e controles do sistema)
    void ApplyAppMode ()
    {
        if (HMODULE ux = ::GetModuleHandleW(L"uxtheme.dll"))
        {
            if (SetPreferredAppModeFn setMode = (SetPreferredAppModeFn)::GetProcAddress(ux, MAKEINTRESOURCEA(135)))
                setMode(IsDark() ? ForceDark : ForceLight);
            if (FlushMenuThemesFn flush = (FlushMenuThemesFn)::GetProcAddress(ux, MAKEINTRESOURCEA(136)))
                flush();
        }
    }

    // converte os icones de um bitmap entre os tons do tema claro (original) e do escuro, independente do tema atual
    void ConvertBitmap (HBITMAP hBitmap, bool premultiplied, bool toDark)
    {
        if (!hBitmap)
            return;
        BITMAP bm = {};
        if (!::GetObject(hBitmap, sizeof(bm), &bm) || bm.bmBitsPixel != 32 || bm.bmWidth <= 0 || bm.bmHeight == 0 || !bm.bmBits)
            return;
        RecolorPixels((BYTE*)bm.bmBits, (size_t)bm.bmWidth * (size_t)abs(bm.bmHeight), premultiplied, !toDark);
    }

    void RecolorBitmap (HBITMAP hBitmap, bool premultiplied)
    {
        if (!IsDark() || !hBitmap)
            return;

        BITMAP bm = {};
        if (!::GetObject(hBitmap, sizeof(bm), &bm) || bm.bmBitsPixel != 32 || bm.bmWidth <= 0 || bm.bmHeight == 0)
            return;

        const size_t count = (size_t)bm.bmWidth * (size_t)abs(bm.bmHeight);
        if (bm.bmBits)
        {
            RecolorPixels((BYTE*)bm.bmBits, count, premultiplied);     // secao DIB: altera no lugar
            return;
        }

        // bitmap dependente de dispositivo: copia os pixels, altera e devolve
        BITMAPINFO bi = {};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = bm.bmWidth;
        bi.bmiHeader.biHeight = -abs(bm.bmHeight);
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        std::vector<BYTE> bits(count * 4);
        HDC hdc = ::GetDC(NULL);
        if (::GetDIBits(hdc, hBitmap, 0, abs(bm.bmHeight), bits.data(), &bi, DIB_RGB_COLORS))
        {
            RecolorPixels(bits.data(), count, premultiplied);
            ::SetDIBits(hdc, hBitmap, 0, abs(bm.bmHeight), bits.data(), &bi, DIB_RGB_COLORS);
        }
        ::ReleaseDC(NULL, hdc);
    }

    LRESULT CustomDrawToolbar (NMHDR* pNMHDR)
    {
        LPNMTBCUSTOMDRAW cd = reinterpret_cast<LPNMTBCUSTOMDRAW>(pNMHDR);
        switch (cd->nmcd.dwDrawStage)
        {
        case CDDS_PREPAINT:
        {
            if (IsDark())
            {
                CBrush brush(SurfaceAlt);
                ::FillRect(cd->nmcd.hdc, &cd->nmcd.rc, brush);
            }
            return CDRF_NOTIFYITEMDRAW;
        }
        case CDDS_ITEMPREPAINT:
            if (IsDark())
            {
                cd->clrText = Text;
                cd->clrBtnFace = SurfaceAlt;
                cd->clrBtnHighlight = Hover;
                cd->clrHighlightHotTrack = Hover;
                cd->clrTextHighlight = Text;
                cd->clrMark = Accent;

                // fundo dos estados marcado / sob o mouse desenhado aqui (o do sistema e claro)
                const UINT st = cd->nmcd.uItemState;
                if (st & (CDIS_CHECKED | CDIS_HOT | CDIS_SELECTED))
                {
                    CRect r(cd->nmcd.rc);
                    r.DeflateRect(0, 1);
                    Gdiplus::Graphics g(cd->nmcd.hdc);
                    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                    COLORREF fill = (st & CDIS_SELECTED) ? Pressed : ((st & CDIS_CHECKED) ? AccentTint : Hover);
                    Gdiplus::SolidBrush brush(Gdiplus::Color(255, GetRValue(fill), GetGValue(fill), GetBValue(fill)));
                    Gdiplus::GraphicsPath path;
                    const float rad = 5.0f;
                    Gdiplus::RectF rf((float)r.left, (float)r.top, (float)r.Width(), (float)r.Height());
                    path.AddArc(rf.X, rf.Y, rad * 2, rad * 2, 180, 90);
                    path.AddArc(rf.GetRight() - rad * 2, rf.Y, rad * 2, rad * 2, 270, 90);
                    path.AddArc(rf.GetRight() - rad * 2, rf.GetBottom() - rad * 2, rad * 2, rad * 2, 0, 90);
                    path.AddArc(rf.X, rf.GetBottom() - rad * 2, rad * 2, rad * 2, 90, 90);
                    path.CloseFigure();
                    g.FillPath(&brush, &path);
                    cd->nmcd.uItemState &= ~(CDIS_CHECKED | CDIS_HOT | CDIS_SELECTED);
                    return TBCDRF_NOBACKGROUND | TBCDRF_NOOFFSET | TBCDRF_USECDCOLORS;
                }
                return TBCDRF_USECDCOLORS;
            }
            return CDRF_DODEFAULT;
        }
        return CDRF_DODEFAULT;
    }
}
