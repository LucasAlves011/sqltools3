#include "stdafx.h"
#include "Theme/ModernDialog.h"
#include "Theme/ModernDraw.h"
#include <commctrl.h>
#include <uxtheme.h>
#include <dwmapi.h>
#include "Theme/ModernDark.h"

#pragma comment(lib, "dwmapi.lib")

#pragma comment(lib, "comctl32.lib")

using namespace ModernTheme;
using namespace ModernDraw;

namespace
{
    HBRUSH BrushFor (COLORREF color)
    {
        static CBrush surface, alt;
        CBrush& b = (color == Surface) ? surface : alt;
        if (!b.GetSafeHandle())
            b.CreateSolidBrush(color);
        return (HBRUSH)b.GetSafeHandle();
    }

    CString ClassOf (HWND h)
    {
        wchar_t name[64] = L"";
        ::GetClassName(h, name, 63);
        return name;
    }

    CString StripMnemonic (const CString& text);

    // ---- fundo do dialogo / pagina ----
    LRESULT CALLBACK DialogProc (HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR ref)
    {
        const COLORREF bg = (COLORREF)ref;
        switch (msg)
        {
        case WM_ERASEBKGND:
        {
            CRect rc;
            ::GetClientRect(h, rc);
            ::FillRect((HDC)wp, rc, BrushFor(bg));
            // contorno fino em volta de arvores, listas e caixas de lista (a borda nativa e removida)
            for (HWND c = ::GetWindow(h, GW_CHILD); c; c = ::GetWindow(c, GW_HWNDNEXT))
            {
                if (!::IsWindowVisible(c))
                    continue;
                const CString cls = ClassOf(c);
                if (cls == L"SysTreeView32" || cls == L"SysListView32" || cls == L"ListBox")
                {
                    CRect tr;
                    ::GetWindowRect(c, tr);
                    ::MapWindowPoints(NULL, h, (LPPOINT)(LPRECT)tr, 2);
                    tr.InflateRect(1, 1);
                    CBrush border(Border);
                    ::FrameRect((HDC)wp, tr, border);
                }
            }
            return 1;
        }
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN:
        {
            // cores proprias do dialogo (ex.: texto de erro em vermelho) tem prioridade sobre o padrao
            LRESULT r = ::DefSubclassProc(h, msg, wp, lp);
            if (r != 0 && r != (LRESULT)::GetSysColorBrush(COLOR_3DFACE) && r != (LRESULT)::GetSysColorBrush(COLOR_BTNFACE) &&
                r != (LRESULT)::GetSysColorBrush(COLOR_WINDOW))
                return r;
            ::SetBkColor((HDC)wp, bg);
            ::SetTextColor((HDC)wp, Text);
            return (LRESULT)BrushFor(bg);
        }
        case WM_PAINT:
        {
            LRESULT r = ::DefSubclassProc(h, msg, wp, lp);
            // caixa de mensagem do sistema: a faixa clara dos botoes e pintada pelo user32; cobre com a cor do dialogo
            if ((HINSTANCE)::GetWindowLongPtr(h, GWLP_HINSTANCE) == ::GetModuleHandleW(L"user32.dll"))
            {
                int top = INT_MAX;
                for (HWND c = ::GetWindow(h, GW_CHILD); c; c = ::GetWindow(c, GW_HWNDNEXT))
                    if (ClassOf(c).CompareNoCase(L"Button") == 0)
                    {
                        CRect br;
                        ::GetWindowRect(c, br);
                        ::MapWindowPoints(NULL, h, (LPPOINT)(LPRECT)br, 2);
                        top = (std::min)(top, (int)br.top);
                    }
                if (top != INT_MAX)
                {
                    CRect rc;
                    ::GetClientRect(h, rc);
                    rc.top = (std::max)(0, top - 18);
                    HDC hdc = ::GetDC(h);
                    ::FillRect(hdc, rc, BrushFor(bg));
                    ::ReleaseDC(h, hdc);
                    // os botoes ficam por cima; repinta-os
                    for (HWND c = ::GetWindow(h, GW_CHILD); c; c = ::GetWindow(c, GW_HWNDNEXT))
                        ::RedrawWindow(c, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
                }
            }
            return r;
        }
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX:
        {
            // campos de texto e listas: fundo e texto da paleta (so quando o dialogo nao define cores proprias)
            LRESULT r = ::DefSubclassProc(h, msg, wp, lp);
            if (IsDark() && (r == 0 || r == (LRESULT)::GetSysColorBrush(COLOR_WINDOW) || r == (LRESULT)::GetSysColorBrush(COLOR_3DFACE)))
            {
                ::SetBkColor((HDC)wp, Surface);
                ::SetTextColor((HDC)wp, Text);
                return (LRESULT)BrushFor(Surface);
            }
            return r;
        }
        case WM_SHOWWINDOW:
        {
            LRESULT r = ::DefSubclassProc(h, msg, wp, lp);
            if (wp && !(::GetWindowLong(h, GWL_STYLE) & WS_CHILD))
                ModernDark::StyleTitleBar(h);       // a barra de titulo escura precisa ser reaplicada ao exibir
            return r;
        }
        case WM_NCDESTROY:
            ::RemoveProp(h, L"ModernSkin");
            ::RemoveWindowSubclass(h, DialogProc, 1);
            break;
        }
        return ::DefSubclassProc(h, msg, wp, lp);
    }

    // ---- botoes (caixa de selecao, opcao, botao e grupo) ----
    struct ButtonData
    {
        COLORREF bg;
        bool hover;
        int icon;           // -1 = sem icone
        COLORREF tint;
    };

    // mistura duas cores: percentual 'pct' de a e o resto de b
    COLORREF Mix (COLORREF a, COLORREF b, int pct)
    {
        return RGB((GetRValue(a) * pct + GetRValue(b) * (100 - pct)) / 100,
                   (GetGValue(a) * pct + GetGValue(b) * (100 - pct)) / 100,
                   (GetBValue(a) * pct + GetBValue(b) * (100 - pct)) / 100);
    }

    // quadro de grupo: so a borda e o titulo sao pintados; o interior fica intacto porque os controles do
    // grupo podem estar por baixo dele na ordem das janelas
    void PaintGroup (HWND h, HDC hdc, const CRect& rc, const ButtonData& data)
    {
        CString text;
        ::GetWindowText(h, text.GetBuffer(256), 256);
        text.ReleaseBuffer();
        text = StripMnemonic(text);

        CDC dc;
        dc.Attach(hdc);
        HFONT font = (HFONT)::SendMessage(h, WM_GETFONT, 0, 0);
        HGDIOBJ oldFont = font ? dc.SelectObject(font) : NULL;
        CSize ts = dc.GetTextExtent(text);

        // faixas de fundo: a borda e o titulo ficam sobre o fundo do dialogo (as faixas da borda sao finas)
        Gdiplus::Graphics g(hdc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        CRect frame(rc.left, rc.top + ts.cy / 2, rc.right - 1, rc.bottom - 1);
        const CRect title(rc.left + 10, rc.top, rc.left + 10 + ts.cx + 8, rc.top + ts.cy);
        dc.FillSolidRect(title, data.bg);
        // a linha da borda passa por tras do titulo: desenha a borda e depois repinta o titulo
        {
            Gdiplus::GraphicsPath path;
            RoundRectPath(path, Gdiplus::RectF((float)frame.left + 0.5f, (float)frame.top + 0.5f, (float)frame.Width(), (float)frame.Height()), 6.0f);
            Gdiplus::Region keep(Gdiplus::Rect(rc.left, rc.top, rc.Width(), rc.Height()));
            keep.Exclude(Gdiplus::Rect(title.left, title.top, title.Width(), title.Height()));
            g.SetClip(&keep);
            Gdiplus::Pen pen(Clr(Border), 1.0f);
            g.DrawPath(&pen, &path);
            g.ResetClip();
        }
        dc.SetBkMode(TRANSPARENT);
        dc.SetTextColor(TextMuted);
        dc.DrawText(text, CRect(title.left + 4, title.top, title.right, title.bottom), DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        if (oldFont)
            dc.SelectObject(oldFont);
        dc.Detach();
    }

    void PaintButton (HWND h, HDC hdc, const ButtonData& data)
    {
        CRect rc;
        ::GetClientRect(h, rc);
        if (rc.IsRectEmpty())
            return;

        const LONG style = ::GetWindowLong(h, GWL_STYLE);
        const LONG type = style & BS_TYPEMASK;
        if (type == BS_GROUPBOX)
        {
            PaintGroup(h, hdc, rc, data);
            return;
        }
        const LRESULT state = ::SendMessage(h, BM_GETSTATE, 0, 0);
        const bool enabled = ::IsWindowEnabled(h) != FALSE;
        const bool pressed = (state & BST_PUSHED) != 0;
        const bool checked = (state & BST_CHECKED) != 0;
        const bool uiState = ::SendMessage(h, WM_QUERYUISTATE, 0, 0) != 0;
        const bool showFocus = (state & BST_FOCUS) && !(::SendMessage(h, WM_QUERYUISTATE, 0, 0) & UISF_HIDEFOCUS);
        (void)uiState;
        const bool hot = data.hover && enabled;
        const UINT prefix = (::SendMessage(h, WM_QUERYUISTATE, 0, 0) & UISF_HIDEACCEL) ? DT_HIDEPREFIX : 0;

        CDC mem;
        mem.Attach(hdc);
        CDC buf;
        buf.CreateCompatibleDC(&mem);
        CBitmap bmp;
        bmp.CreateCompatibleBitmap(&mem, rc.Width(), rc.Height());
        CBitmap* pOldBmp = buf.SelectObject(&bmp);

        buf.FillSolidRect(rc, data.bg);
        buf.SetBkMode(TRANSPARENT);
        HFONT font = (HFONT)::SendMessage(h, WM_GETFONT, 0, 0);
        HGDIOBJ oldFont = font ? buf.SelectObject(font) : NULL;

        CString text;
        ::GetWindowText(h, text.GetBuffer(512), 512);
        text.ReleaseBuffer();

        Gdiplus::Graphics g(buf.GetSafeHdc());
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

        const bool isCheck = type == BS_CHECKBOX || type == BS_AUTOCHECKBOX || type == BS_3STATE || type == BS_AUTO3STATE;
        const bool isRadio = type == BS_RADIOBUTTON || type == BS_AUTORADIOBUTTON;
        const bool pushLike = (style & BS_PUSHLIKE) != 0;

        if ((isCheck || isRadio) && !pushLike)
        {
            const int box = 16;
            const int bx = rc.left + 1;
            const int by = rc.top + (rc.Height() - box) / 2;
            CRect br(bx, by, bx + box, by + box);
            const bool indeterminate = (state & BST_INDETERMINATE) != 0;
            const bool on = checked || indeterminate;

            COLORREF fill = on ? (enabled ? Accent : BorderStrong) : (enabled ? Surface : SurfaceAlt);
            COLORREF edge = on ? fill : (hot || pressed ? Accent : BorderStrong);
            if (pressed && enabled && !on)
                fill = AccentTint;

            if (showFocus)
                StrokeRoundRect(g, CRect(br.left - 2, br.top - 2, br.right + 2, br.bottom + 2), isRadio ? 10.0f : 6.0f, AccentStrong, 2.0f);

            if (isRadio)
            {
                Gdiplus::SolidBrush fb(Clr(fill));
                g.FillEllipse(&fb, (Gdiplus::REAL)br.left, (Gdiplus::REAL)br.top, (Gdiplus::REAL)box - 1, (Gdiplus::REAL)box - 1);
                Gdiplus::Pen pen(Clr(edge), 1.4f);
                g.DrawEllipse(&pen, br.left + 0.7f, br.top + 0.7f, (Gdiplus::REAL)box - 2.4f, (Gdiplus::REAL)box - 2.4f);
                if (checked)
                {
                    Gdiplus::SolidBrush dot(Clr(RGB(255, 255, 255)));
                    g.FillEllipse(&dot, br.left + 4.5f, br.top + 4.5f, 6.0f, 6.0f);
                }
            }
            else
            {
                FillRoundRect(g, br, 4.5f, fill);
                StrokeRoundRect(g, br, 4.5f, edge, 1.4f);
                if (checked || indeterminate)
                {
                    Gdiplus::Pen mark(Clr(RGB(255, 255, 255)), 2.0f);
                    mark.SetStartCap(Gdiplus::LineCapRound);
                    mark.SetEndCap(Gdiplus::LineCapRound);
                    mark.SetLineJoin(Gdiplus::LineJoinRound);
                    if (indeterminate && !checked)
                        g.DrawLine(&mark, br.left + 4.0f, br.top + 8.0f, br.right - 5.0f, br.top + 8.0f);
                    else
                    {
                        Gdiplus::PointF p[3] = { Gdiplus::PointF(br.left + 3.8f, br.top + 8.2f),
                                                 Gdiplus::PointF(br.left + 6.8f, br.top + 11.0f),
                                                 Gdiplus::PointF(br.left + 12.0f, br.top + 5.0f) };
                        g.DrawLines(&mark, p, 3);
                    }
                }
            }

            CRect tr(rc.left + box + 9, rc.top, rc.right, rc.bottom);
            buf.SetTextColor(enabled ? Text : TextDisabled);
            UINT fmt = DT_LEFT | DT_END_ELLIPSIS | prefix;
            fmt |= (style & BS_MULTILINE) ? (DT_WORDBREAK | DT_VCENTER) : (DT_SINGLELINE | DT_VCENTER);
            buf.DrawText(text, tr, fmt);
        }
        else
        {
            // botao de comando (ou botao de duas posicoes)
            const bool isDefault = type == BS_DEFPUSHBUTTON;
            CRect br = rc;
            br.DeflateRect(1, 1);

            COLORREF fill, edge, fore;
            if (!enabled)
            {
                fill = SurfaceAlt; edge = Border; fore = TextDisabled;
            }
            else if (isDefault)
            {
                fill = pressed ? AccentPressed : (hot ? AccentHover : Accent);
                edge = fill; fore = RGB(255, 255, 255);
            }
            else if (data.icon >= 0)
            {
                // botao colorido: contorno suave na cor do botao, realce ao passar o mouse
                fill = pressed ? Mix(data.tint, Surface, 22) : (hot ? Mix(data.tint, Surface, 11) : Surface);
                edge = (hot || pressed) ? data.tint : Mix(data.tint, Surface, 45);
                fore = Text;
            }
            else
            {
                fill = pressed ? AccentStrong : (hot ? AccentTint : (checked && pushLike ? AccentTint : Surface));
                edge = (hot || pressed || checked) ? Accent : BorderStrong;
                fore = (hot || pressed || checked) ? Accent : Text;
            }

            // botoes pequenos (ex.: so uma seta) usam cantos e margens menores
            const float radius = (float)(std::min)(7, (std::min)(br.Width(), br.Height()) / 3);
            FillRoundRect(g, br, radius, fill);
            StrokeRoundRect(g, br, radius, edge, 1.0f);
            if (showFocus && !isDefault)
                StrokeRoundRect(g, CRect(br.left + 2, br.top + 2, br.right - 2, br.bottom - 2), (std::max)(1.0f, radius - 2.0f), AccentStrong, 1.5f);

            buf.SetTextColor(fore);
            CRect tr = br;
            const int textWidth = buf.GetTextExtent(StripMnemonic(text)).cx;
            tr.DeflateRect((std::max)(1, (std::min)(6, (br.Width() - textWidth) / 2)), 0);
            if (data.icon >= 0)
            {
                // icone + texto, centralizados juntos
                CSize ts = buf.GetTextExtent(StripMnemonic(text));
                const int gap = 6, iconSize = 16;
                int total = iconSize + gap + ts.cx;
                int x = tr.left + (std::max)(0, (tr.Width() - total) / 2);
                const int icon = (data.icon == ModernDraw::DI_CONNECT && !isDefault) ? ModernDraw::DI_CONNECT_DARK : data.icon;
                ModernDraw::DrawDialogIcon(buf.GetSafeHdc(), icon, x, br.top + (br.Height() - iconSize) / 2, !enabled);
                tr.left = x + iconSize + gap;
                buf.DrawText(text, tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | prefix);
            }
            else
                buf.DrawText(text, tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | prefix);
        }

        if (oldFont)
            buf.SelectObject(oldFont);
        mem.BitBlt(0, 0, rc.Width(), rc.Height(), &buf, 0, 0, SRCCOPY);
        buf.SelectObject(pOldBmp);
        mem.Detach();
    }

    LRESULT CALLBACK ButtonProc (HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR ref)
    {
        ButtonData* data = (ButtonData*)ref;
        switch (msg)
        {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = ::BeginPaint(h, &ps);
            PaintButton(h, hdc, *data);
            ::EndPaint(h, &ps);
            return 0;
        }
        case WM_PRINTCLIENT:
            PaintButton(h, (HDC)wp, *data);
            return 0;
        case WM_MOUSEMOVE:
            if (!data->hover)
            {
                data->hover = true;
                TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, h, 0 };
                ::TrackMouseEvent(&tme);
                ::InvalidateRect(h, NULL, FALSE);
            }
            break;
        case WM_MOUSELEAVE:
            data->hover = false;
            ::InvalidateRect(h, NULL, FALSE);
            break;
        case WM_SETFOCUS:
        case WM_KILLFOCUS:
        case WM_ENABLE:
        case WM_SETTEXT:
        case WM_UPDATEUISTATE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        case BM_SETCHECK:
        case BM_SETSTATE:
        case BM_SETSTYLE:
        case BM_CLICK:
        {
            LRESULT r = ::DefSubclassProc(h, msg, wp, lp);
            ::InvalidateRect(h, NULL, FALSE);
            return r;
        }
        case WM_NCDESTROY:
            ::RemoveWindowSubclass(h, ButtonProc, 1);
            delete data;
            break;
        }
        return ::DefSubclassProc(h, msg, wp, lp);
    }

    // ---- setas de incremento (spin): duas metades com chevrons finos ----
    struct SpinData
    {
        COLORREF bg;
        int hot;            // 0 nenhuma, 1 metade de cima (ou esquerda), 2 metade de baixo (ou direita)
        int pressed;
    };

    int SpinHalf (HWND h, POINT pt)
    {
        CRect rc;
        ::GetClientRect(h, rc);
        if (::GetWindowLong(h, GWL_STYLE) & UDS_HORZ)
            return pt.x < rc.CenterPoint().x ? 1 : 2;
        return pt.y < rc.CenterPoint().y ? 1 : 2;
    }

    void PaintSpin (HWND h, HDC hdc, const SpinData& data)
    {
        CRect rc;
        ::GetClientRect(h, rc);
        if (rc.IsRectEmpty())
            return;

        const bool horz = (::GetWindowLong(h, GWL_STYLE) & UDS_HORZ) != 0;
        const bool enabled = ::IsWindowEnabled(h) != FALSE;

        CDC mem;
        mem.Attach(hdc);
        CDC buf;
        buf.CreateCompatibleDC(&mem);
        CBitmap bmp;
        bmp.CreateCompatibleBitmap(&mem, rc.Width(), rc.Height());
        CBitmap* pOld = buf.SelectObject(&bmp);

        buf.FillSolidRect(rc, enabled ? Surface : SurfaceAlt);
        CBrush border(enabled ? BorderStrong : Border);
        buf.FrameRect(rc, &border);

        CRect halves[2] = { rc, rc };
        if (horz)
        {
            halves[0].right = rc.CenterPoint().x;
            halves[1].left = rc.CenterPoint().x;
        }
        else
        {
            halves[0].bottom = rc.CenterPoint().y;
            halves[1].top = rc.CenterPoint().y;
        }

        Gdiplus::Graphics g(buf.GetSafeHdc());
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        for (int i = 0; i < 2; ++i)
        {
            CRect hr = halves[i];
            hr.DeflateRect(1, 1);
            const bool hot = enabled && data.hot == i + 1;
            const bool down = enabled && data.pressed == i + 1;
            if (hot || down)
                buf.FillSolidRect(hr, down ? Pressed : Hover);

            Gdiplus::Pen pen(Clr(!enabled ? BorderStrong : (hot ? Accent : TextMuted)), 1.5f);
            pen.SetStartCap(Gdiplus::LineCapRound);
            pen.SetEndCap(Gdiplus::LineCapRound);
            pen.SetLineJoin(Gdiplus::LineJoinRound);
            const float cx = (float)hr.CenterPoint().x, cy = (float)hr.CenterPoint().y;
            const float d = (i == 0) ? -1.0f : 1.0f;      // cima/esquerda, baixo/direita
            Gdiplus::PointF pts[3];
            if (horz)
            {
                pts[0] = Gdiplus::PointF(cx - 1.5f * d, cy - 3.0f);
                pts[1] = Gdiplus::PointF(cx + 1.5f * d, cy);
                pts[2] = Gdiplus::PointF(cx - 1.5f * d, cy + 3.0f);
            }
            else
            {
                pts[0] = Gdiplus::PointF(cx - 3.0f, cy - 1.2f * d);
                pts[1] = Gdiplus::PointF(cx, cy + 1.2f * d);
                pts[2] = Gdiplus::PointF(cx + 3.0f, cy - 1.2f * d);
            }
            g.DrawLines(&pen, pts, 3);
        }
        // divisor entre as metades
        if (horz)
            buf.FillSolidRect(rc.CenterPoint().x, rc.top + 1, 1, rc.Height() - 2, Border);
        else
            buf.FillSolidRect(rc.left + 1, rc.CenterPoint().y, rc.Width() - 2, 1, Border);

        mem.BitBlt(0, 0, rc.Width(), rc.Height(), &buf, 0, 0, SRCCOPY);
        buf.SelectObject(pOld);
        mem.Detach();
    }

    LRESULT CALLBACK SpinProc (HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR ref)
    {
        SpinData* data = (SpinData*)ref;
        switch (msg)
        {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = ::BeginPaint(h, &ps);
            PaintSpin(h, hdc, *data);
            ::EndPaint(h, &ps);
            return 0;
        }
        case WM_PRINTCLIENT:
            PaintSpin(h, (HDC)wp, *data);
            return 0;
        case WM_MOUSEMOVE:
        {
            POINT pt = { (short)LOWORD(lp), (short)HIWORD(lp) };
            const int half = SpinHalf(h, pt);
            if (data->hot == 0)
            {
                TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, h, 0 };
                ::TrackMouseEvent(&tme);
            }
            if (data->hot != half)
            {
                data->hot = half;
                ::InvalidateRect(h, NULL, FALSE);
            }
            break;
        }
        case WM_MOUSELEAVE:
            data->hot = 0;
            ::InvalidateRect(h, NULL, FALSE);
            break;
        case WM_LBUTTONDOWN:
        {
            POINT pt = { (short)LOWORD(lp), (short)HIWORD(lp) };
            data->pressed = SpinHalf(h, pt);
            ::InvalidateRect(h, NULL, FALSE);
            break;
        }
        case WM_LBUTTONUP:
        case WM_CAPTURECHANGED:
            data->pressed = 0;
            ::InvalidateRect(h, NULL, FALSE);
            break;
        case WM_ENABLE:
            ::InvalidateRect(h, NULL, FALSE);
            break;
        case WM_NCDESTROY:
            ::RemoveWindowSubclass(h, SpinProc, 1);
            delete data;
            break;
        }
        return ::DefSubclassProc(h, msg, wp, lp);
    }

    // ---- campos de texto: borda fina que muda de cor com o foco ----
    LRESULT CALLBACK EditProc (HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR)
    {
        switch (msg)
        {
        case WM_NCPAINT:
        {
            ::DefSubclassProc(h, msg, wp, lp);      // barras de rolagem e area nao cliente padrao
            HDC hdc = ::GetWindowDC(h);
            CRect rc;
            ::GetWindowRect(h, rc);
            rc.OffsetRect(-rc.left, -rc.top);
            const bool focus = ::GetFocus() == h;
            CBrush border(!::IsWindowEnabled(h) ? Border : (focus ? Accent : BorderStrong));
            ::FrameRect(hdc, rc, border);
            ::ReleaseDC(h, hdc);
            return 0;
        }
        case WM_SETFOCUS:
        case WM_KILLFOCUS:
        {
            LRESULT r = ::DefSubclassProc(h, msg, wp, lp);
            ::RedrawWindow(h, NULL, NULL, RDW_FRAME | RDW_INVALIDATE);
            return r;
        }
        case WM_NCDESTROY:
            ::RemoveWindowSubclass(h, EditProc, 1);
            break;
        }
        return ::DefSubclassProc(h, msg, wp, lp);
    }

    // ---- caixas de combinacao ----
    struct ComboData
    {
        COLORREF bg;
        bool hover;
    };

    bool ComboHasFocus (HWND h)
    {
        HWND f = ::GetFocus();
        return f && (f == h || ::IsChild(h, f));
    }

    void PaintCombo (HWND h, HDC hdc, const ComboData& data)
    {
        CRect rc;
        ::GetClientRect(h, rc);
        if (rc.IsRectEmpty())
            return;

        COMBOBOXINFO ci = { sizeof(ci) };
        ::GetComboBoxInfo(h, &ci);
        const bool dropList = (::GetWindowLong(h, GWL_STYLE) & 3) == CBS_DROPDOWNLIST;
        const bool enabled = ::IsWindowEnabled(h) != FALSE;
        const bool open = ::SendMessage(h, CB_GETDROPPEDSTATE, 0, 0) != 0;
        const bool focus = ComboHasFocus(h) || open;

        CDC mem;
        mem.Attach(hdc);
        CDC buf;
        buf.CreateCompatibleDC(&mem);
        CBitmap bmp;
        bmp.CreateCompatibleBitmap(&mem, rc.Width(), rc.Height());
        CBitmap* pOld = buf.SelectObject(&bmp);

        buf.FillSolidRect(rc, enabled ? Surface : SurfaceAlt);
        CBrush border(!enabled ? Border : (focus ? Accent : (data.hover ? Mix(Accent, BorderStrong, 50) : BorderStrong)));
        buf.FrameRect(rc, &border);

        // seta
        CRect ar = ci.rcButton;
        if (ar.IsRectEmpty())
            ar = CRect(rc.right - 22, rc.top, rc.right - 1, rc.bottom);
        Gdiplus::Graphics g(buf.GetSafeHdc());
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        Gdiplus::Pen pen(Clr(enabled ? (focus ? Accent : TextMuted) : TextDisabled), 1.5f);
        pen.SetStartCap(Gdiplus::LineCapRound);
        pen.SetEndCap(Gdiplus::LineCapRound);
        pen.SetLineJoin(Gdiplus::LineJoinRound);
        const float cx = (float)(rc.right - 12), cy = (float)rc.CenterPoint().y;
        Gdiplus::PointF p[3] = { Gdiplus::PointF(cx - 3.5f, cy - 1.5f), Gdiplus::PointF(cx, cy + 2.0f), Gdiplus::PointF(cx + 3.5f, cy - 1.5f) };
        g.DrawLines(&pen, p, 3);

        if (dropList)
        {
            HFONT font = (HFONT)::SendMessage(h, WM_GETFONT, 0, 0);
            HGDIOBJ oldFont = font ? buf.SelectObject(font) : NULL;
            CString text;
            int sel = (int)::SendMessage(h, CB_GETCURSEL, 0, 0);
            if (sel >= 0)
            {
                int len = (int)::SendMessage(h, CB_GETLBTEXTLEN, sel, 0);
                if (len > 0)
                    ::SendMessage(h, CB_GETLBTEXT, sel, (LPARAM)text.GetBuffer(len + 1));
                text.ReleaseBuffer();
            }
            buf.SetBkMode(TRANSPARENT);
            buf.SetTextColor(enabled ? Text : TextDisabled);
            CRect tr(rc.left + 6, rc.top, ar.left - 2, rc.bottom);
            buf.DrawText(text, tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            if (oldFont)
                buf.SelectObject(oldFont);
        }

        // com campo de edicao, a area do campo pertence ao proprio controle de edicao
        if (!dropList && !::IsRectEmpty(&ci.rcItem))
            ::ExcludeClipRect(hdc, ci.rcItem.left, ci.rcItem.top, ci.rcItem.right, ci.rcItem.bottom);
        mem.BitBlt(0, 0, rc.Width(), rc.Height(), &buf, 0, 0, SRCCOPY);
        buf.SelectObject(pOld);
        mem.Detach();
    }

    // o campo de edicao do combo avisa o combo quando o foco muda (para redesenhar a borda)
    LRESULT CALLBACK ComboEditProc (HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR)
    {
        if (msg == WM_SETFOCUS || msg == WM_KILLFOCUS)
        {
            LRESULT r = ::DefSubclassProc(h, msg, wp, lp);
            if (HWND combo = ::GetParent(h))
                ::InvalidateRect(combo, NULL, FALSE);
            return r;
        }
        if (msg == WM_NCDESTROY)
            ::RemoveWindowSubclass(h, ComboEditProc, 1);
        return ::DefSubclassProc(h, msg, wp, lp);
    }

    LRESULT CALLBACK ComboProc (HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR ref)
    {
        ComboData* data = (ComboData*)ref;
        switch (msg)
        {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = ::BeginPaint(h, &ps);
            PaintCombo(h, hdc, *data);
            ::EndPaint(h, &ps);
            return 0;
        }
        case WM_PRINTCLIENT:
            PaintCombo(h, (HDC)wp, *data);
            return 0;
        case WM_MOUSEMOVE:
            if (!data->hover)
            {
                data->hover = true;
                TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, h, 0 };
                ::TrackMouseEvent(&tme);
                ::InvalidateRect(h, NULL, FALSE);
            }
            break;
        case WM_MOUSELEAVE:
            data->hover = false;
            ::InvalidateRect(h, NULL, FALSE);
            break;
        case WM_SETFOCUS:
        case WM_KILLFOCUS:
        case WM_ENABLE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_KEYDOWN:
        case CB_SETCURSEL:
        case CB_SHOWDROPDOWN:
        case WM_SETTEXT:
        case WM_COMMAND:
        {
            LRESULT r = ::DefSubclassProc(h, msg, wp, lp);
            ::InvalidateRect(h, NULL, FALSE);
            return r;
        }
        case WM_NCDESTROY:
            ::RemoveWindowSubclass(h, ComboProc, 1);
            delete data;
            break;
        }
        return ::DefSubclassProc(h, msg, wp, lp);
    }

    // remove os marcadores de tecla de atalho ('&'), mantendo '&&' como um '&'
    CString StripMnemonic (const CString& text)
    {
        CString plain;
        for (int k = 0; k < text.GetLength(); ++k)
            if (text[k] != L'&' || (k + 1 < text.GetLength() && text[k + 1] == L'&' && (++k, true)))
                plain += text[k];
        return plain;
    }

    // a caixa moderna e o espaco ate o texto sao maiores que os do controle nativo: aumenta a largura
    // (nunca diminui) para o texto caber, sem passar da borda da pagina
    void FitCheckWidth (HWND c, HWND hParent)
    {
        CString text;
        ::GetWindowText(c, text.GetBuffer(512), 512);
        text.ReleaseBuffer();
        text = StripMnemonic(text);

        HDC hdc = ::GetDC(c);
        HFONT font = (HFONT)::SendMessage(c, WM_GETFONT, 0, 0);
        HGDIOBJ old = font ? ::SelectObject(hdc, font) : NULL;
        SIZE ext = {};
        ::GetTextExtentPoint32(hdc, text, text.GetLength(), &ext);
        if (old)
            ::SelectObject(hdc, old);
        ::ReleaseDC(c, hdc);

        CRect rc, pc;
        ::GetWindowRect(c, rc);
        ::MapWindowPoints(NULL, hParent, (LPPOINT)(LPRECT)rc, 2);

        // a caixa tem 16 px: controles mais baixos (ex.: 10 DLU) cortariam a caixa, entao cresce a altura
        // para os dois lados, mantendo o centro
        const int minHeight = 18;
        if (rc.Height() < minHeight)
        {
            const int grow = minHeight - rc.Height();
            rc.top -= grow / 2;
            rc.bottom = rc.top + minHeight;
            ::SetWindowPos(c, NULL, rc.left, rc.top, rc.Width(), minHeight, SWP_NOZORDER | SWP_NOACTIVATE);
        }
        ::GetClientRect(hParent, pc);
        int width = (std::max)((int)rc.Width(), 16 + 9 + (int)ext.cx + 8);
        width = (std::min)(width, (int)pc.right - 8 - (int)rc.left);
        if (width <= rc.Width())
            return;
        ::SetWindowPos(c, NULL, 0, 0, width, rc.Height(), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

        // controle logo a direita, na mesma linha (ex.: campo ao lado da caixa): empurra para nao sobrepor
        const int newRight = rc.left + width;
        for (HWND s = ::GetWindow(hParent, GW_CHILD); s; s = ::GetWindow(s, GW_HWNDNEXT))
        {
            if (s == c)
                continue;
            CRect sr;
            ::GetWindowRect(s, sr);
            ::MapWindowPoints(NULL, hParent, (LPPOINT)(LPRECT)sr, 2);
            if (sr.top < rc.bottom && sr.bottom > rc.top && sr.left >= rc.right - 2 && sr.left < newRight + 4)
                ::SetWindowPos(s, NULL, newRight + 6, sr.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }

    void SkinControls (HWND hParent, COLORREF bg, const ModernDialog::ButtonIcon* icons = NULL, int iconCount = 0)
    {
        for (HWND c = ::GetWindow(hParent, GW_CHILD); c; c = ::GetWindow(c, GW_HWNDNEXT))
        {
            const CString cls = ClassOf(c);
            const LONG style = ::GetWindowLong(c, GWL_STYLE);

            if (cls.CompareNoCase(L"Button") == 0)
            {
                const LONG type = style & BS_TYPEMASK;
                if (type == BS_OWNERDRAW || (style & (BS_ICON | BS_BITMAP)) || type == BS_SPLITBUTTON || type == BS_DEFSPLITBUTTON ||
                    type == BS_COMMANDLINK || type == BS_DEFCOMMANDLINK)
                    continue;
                if ((type == BS_CHECKBOX || type == BS_AUTOCHECKBOX || type == BS_3STATE || type == BS_AUTO3STATE ||
                     type == BS_RADIOBUTTON || type == BS_AUTORADIOBUTTON) && !(style & (BS_PUSHLIKE | BS_MULTILINE)))
                    FitCheckWidth(c, hParent);
                ButtonData* bd = new ButtonData{ bg, false, -1, 0 };
                if (type == BS_PUSHBUTTON || type == BS_DEFPUSHBUTTON)
                    for (int k = 0; k < iconCount; ++k)
                        if (icons[k].id == (UINT)::GetDlgCtrlID(c))
                        {
                            bd->icon = icons[k].icon;
                            bd->tint = icons[k].tint;
                        }
                ::SetWindowSubclass(c, ButtonProc, 1, (DWORD_PTR)bd);
                ::InvalidateRect(c, NULL, TRUE);
            }
            else if (cls.CompareNoCase(L"ComboBox") == 0)
            {
                // combos desenhados pelo proprio dialogo (owner-draw) ou do tipo "simple" ficam como estao
                if ((style & (CBS_OWNERDRAWFIXED | CBS_OWNERDRAWVARIABLE)) || (style & 3) == CBS_SIMPLE)
                    continue;
                ::SetWindowSubclass(c, ComboProc, 1, (DWORD_PTR) new ComboData{ bg, false });
                COMBOBOXINFO ci = { sizeof(ci) };
                if (::GetComboBoxInfo(c, &ci) && ci.hwndItem)
                {
                    ::SetWindowSubclass(ci.hwndItem, ComboEditProc, 1, 0);
                    ::SendMessage(ci.hwndItem, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(5, 3));
                }
                ::InvalidateRect(c, NULL, TRUE);
            }
            else if (cls.CompareNoCase(L"msctls_updown32") == 0)
            {
                ::SetWindowSubclass(c, SpinProc, 1, (DWORD_PTR) new SpinData{ bg, 0, 0 });
                ::InvalidateRect(c, NULL, TRUE);
            }
            else if (cls.CompareNoCase(L"msctls_trackbar32") == 0
                  || cls.CompareNoCase(L"SysTabControl32") == 0 || cls.CompareNoCase(L"SysDateTimePick32") == 0)
            {
                ::SetWindowTheme(c, ModernDark::ControlTheme(), NULL);
            }
            else if (cls.CompareNoCase(L"SysTreeView32") == 0)
            {
                ::SetWindowTheme(c, ModernDark::ControlTheme(), NULL);
                TreeView_SetBkColor(c, Surface);
                TreeView_SetTextColor(c, Text);
                ::SetWindowLong(c, GWL_STYLE, style & ~WS_BORDER);
                ::SetWindowLong(c, GWL_EXSTYLE, ::GetWindowLong(c, GWL_EXSTYLE) & ~WS_EX_CLIENTEDGE);
                ::SetWindowPos(c, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
            }
            else if (cls.CompareNoCase(L"SysListView32") == 0 || cls.CompareNoCase(L"ListBox") == 0)
            {
                // sem a borda preta: o contorno fino e desenhado pelo fundo do dialogo
                ::SetWindowTheme(c, ModernDark::ControlTheme(), NULL);     // barras de rolagem finas do Windows moderno
                if (cls.CompareNoCase(L"SysListView32") == 0)
                {
                    ListView_SetBkColor(c, Surface);
                    ListView_SetTextBkColor(c, Surface);
                    ListView_SetTextColor(c, Text);
                }
                ::SetWindowLong(c, GWL_STYLE, style & ~WS_BORDER);
                ::SetWindowLong(c, GWL_EXSTYLE, ::GetWindowLong(c, GWL_EXSTYLE) & ~WS_EX_CLIENTEDGE);
                ::SetWindowPos(c, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
            }
            else if (cls.CompareNoCase(L"Edit") == 0)
            {
                ::SetWindowLong(c, GWL_EXSTYLE, ::GetWindowLong(c, GWL_EXSTYLE) & ~WS_EX_CLIENTEDGE);
                ::SetWindowLong(c, GWL_STYLE, style | WS_BORDER);
                ::SetWindowSubclass(c, EditProc, 1, 0);
                ::SendMessage(c, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(5, 5));
                ::SetWindowPos(c, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
            }
        }
    }
}

namespace ModernDialog
{
    void SkinPage (HWND hPage)
    {
        if (::GetProp(hPage, L"ModernSkin"))
            return;
        ::SetProp(hPage, L"ModernSkin", (HANDLE)1);
        ::SetWindowSubclass(hPage, DialogProc, 1, (DWORD_PTR)Surface);
        SkinControls(hPage, Surface);
        ::InvalidateRect(hPage, NULL, TRUE);
    }

    void SkinDialog (HWND hDlg, const ButtonIcon* icons, int iconCount)
    {
        if (::GetProp(hDlg, L"ModernSkin"))
            return;
        ::SetProp(hDlg, L"ModernSkin", (HANDLE)1);
        ::SetWindowSubclass(hDlg, DialogProc, 1, (DWORD_PTR)SurfaceAlt);
        SkinControls(hDlg, SurfaceAlt, icons, iconCount);
        ::InvalidateRect(hDlg, NULL, TRUE);
    }

    void SkinSheet (HWND hSheet)
    {
        if (::GetProp(hSheet, L"ModernSkin"))
            return;
        ::SetProp(hSheet, L"ModernSkin", (HANDLE)1);
        ::SetWindowSubclass(hSheet, DialogProc, 1, (DWORD_PTR)SurfaceAlt);
        SkinControls(hSheet, SurfaceAlt);
        ::InvalidateRect(hSheet, NULL, TRUE);
    }

    BEGIN_MESSAGE_MAP(CPageFrame, TreePropSheet::CPropPageFrameDefault)
        ON_WM_ERASEBKGND()
    END_MESSAGE_MAP()

    BOOL CPageFrame::OnEraseBkgnd (CDC* pDC)
    {
        CRect rc;
        GetClientRect(rc);
        pDC->FillSolidRect(rc, Surface);
        CBrush border(Border);
        pDC->FrameRect(rc, &border);
        return TRUE;
    }

    namespace
    {
        HHOOK g_hook = NULL;

        // uma folha de propriedades tem o controle de abas padrao (id 0x3020)
        bool IsSheet (HWND h)
        {
            return ::GetDlgItem(h, 0x3020) != NULL;
        }

        LRESULT CALLBACK GlobalHookProc (int code, WPARAM wp, LPARAM lp)
        {
            if (code == HC_ACTION)
            {
                const CWPRETSTRUCT* cwp = reinterpret_cast<const CWPRETSTRUCT*>(lp);

                // editor, grades e barras de rolagem: barras de rolagem finas do Windows moderno
                if (cwp->message == WM_CREATE)
                {
                    const CString cls = ClassOf(cwp->hwnd);

                    // janelas do MFC e a area MDI nascem com o pincel de fundo do sistema (branco/cinza) e so ficam
                    // na cor da paleta depois de pintadas: troca o pincel da classe para nao piscar na partida
                    if (cls == L"MDIClient" || cls.Left(3) == L"Afx")
                    {
                        static HBRUSH s_surface = ::CreateSolidBrush(Surface);
                        ::SetClassLongPtr(cwp->hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)s_surface);
                    }
                    if (cls == L"ScrollBar" || cls.Left(15) == L"Kochware.OpenGr" || (IsDark() && cls.Left(8) == L"RICHEDIT") || (IsDark() && cls == L"OpenEditorWindow"))
                        ::SetWindowTheme(cwp->hwnd, ModernDark::ControlTheme(), NULL);

                    // listas e arvores criadas em qualquer lugar: cores da paleta e barras de rolagem escuras
                    if (IsDark())
                    {
                        if (cls == L"SysListView32")
                        {
                            ListView_SetBkColor(cwp->hwnd, Surface);
                            ListView_SetTextBkColor(cwp->hwnd, Surface);
                            ListView_SetTextColor(cwp->hwnd, Text);
                            ::SetWindowTheme(cwp->hwnd, ModernDark::ControlTheme(), NULL);
                        }
                        else if (cls == L"SysTreeView32")
                        {
                            TreeView_SetBkColor(cwp->hwnd, Surface);
                            TreeView_SetTextColor(cwp->hwnd, Text);
                            ::SetWindowTheme(cwp->hwnd, ModernDark::ControlTheme(), NULL);
                        }
                    }

                    // barra de titulo escura e menu principal escuro
                    if (IsDark())
                    {
                        if (cls == L"Kochware.SQLTools.MainFrame")
                        {
                            ModernDark::StyleTitleBar(cwp->hwnd);
                            ModernDark::StyleMenuBar(cwp->hwnd);
                        }
                    }
                }
                // dialogos do programa e caixas de mensagem do sistema (MessageBox, criadas pelo user32)
                if (cwp->message == WM_INITDIALOG && ClassOf(cwp->hwnd) == L"#32770" &&
                    ((HINSTANCE)::GetWindowLongPtr(cwp->hwnd, GWLP_HINSTANCE) == AfxGetInstanceHandle() ||
                     (HINSTANCE)::GetWindowLongPtr(cwp->hwnd, GWLP_HINSTANCE) == ::GetModuleHandleW(L"user32.dll")))
                {
                    HWND h = cwp->hwnd;
                    if (!(::GetWindowLong(h, GWL_STYLE) & WS_CHILD))
                    {
                        ModernDark::StyleTitleBar(h);
                        // sem a animacao de aparecer (fade): enquanto ela roda, o dialogo escuro fica translucido
                        // e parece claro sobre o fundo da janela
                        const BOOL off = TRUE;
                        ::DwmSetWindowAttribute(h, 3 /*DWMWA_TRANSITIONS_FORCEDISABLED*/, &off, sizeof(off));
                    }
                    if (::GetWindowLong(h, GWL_STYLE) & WS_CHILD)
                    {
                        // somente paginas de folhas de propriedades; paineis acoplados ja tem visual proprio
                        HWND parent = ::GetParent(h);
                        if (parent && IsSheet(parent))
                            SkinPage(h);
                    }
                    else if (IsSheet(h))
                        SkinSheet(h);
                    else
                        SkinDialog(h);
                }
            }
            return ::CallNextHookEx(g_hook, code, wp, lp);
        }
    }

    void InstallGlobalSkin ()
    {
        if (!g_hook)
            g_hook = ::SetWindowsHookEx(WH_CALLWNDPROCRET, GlobalHookProc, NULL, ::GetCurrentThreadId());
    }
}
