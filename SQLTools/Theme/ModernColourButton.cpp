#include "stdafx.h"
#include "Theme/ModernColourButton.h"
#include "Theme/ModernDraw.h"

using namespace ModernTheme;
using ModernDraw::Clr;

void CModernColourButton::DrawItem (LPDRAWITEMSTRUCT lpDrawItemStruct)
{
    CDC* target = CDC::FromHandle(lpDrawItemStruct->hDC);
    const CRect rc = lpDrawItemStruct->rcItem;
    const UINT state = lpDrawItemStruct->itemState;
    const bool disabled = (state & ODS_DISABLED) != 0;
    const bool hot = !disabled && (m_bMouseOver || m_bPopupActive);
    const bool focus = !disabled && (state & ODS_FOCUS) != 0;

    // buffer com o fundo do dialogo (os cantos arredondados ficam sobre ele)
    CDC dc;
    dc.CreateCompatibleDC(target);
    CBitmap bmp;
    bmp.CreateCompatibleBitmap(target, rc.Width(), rc.Height());
    CBitmap* old = dc.SelectObject(&bmp);
    const CRect r(0, 0, rc.Width(), rc.Height());
    dc.FillSolidRect(r, SurfaceAlt);
    if (CWnd* parent = GetParent())
    {
        // o fundo real e o do pai (pagina de configuracao ou caixa de grupo)
        HBRUSH brush = (HBRUSH)parent->SendMessage(WM_CTLCOLORBTN, (WPARAM)dc.GetSafeHdc(), (LPARAM)m_hWnd);
        if (brush)
            ::FillRect(dc.GetSafeHdc(), r, brush);
    }

    {
        Gdiplus::Graphics g(dc.GetSafeHdc());
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

        CRect box(r.left, r.top, r.right - 1, r.bottom - 1);
        ModernDraw::FillRoundRect(g, box, 6.0f, disabled ? SurfaceAlt : (hot ? Hover : Surface));
        ModernDraw::StrokeRoundRect(g, box, 6.0f, focus ? Accent : (hot ? BorderStrong : Border));

        // amostra da cor
        const int arrowW = 22;
        CRect swatch(box.left + 6, box.top + 5, box.right - arrowW, box.bottom - 5);
        COLORREF color = GetColor(TRUE);
        if (disabled)
            color = TextDisabled;
        ModernDraw::FillRoundRect(g, swatch, 3.0f, color);
        ModernDraw::StrokeRoundRect(g, swatch, 3.0f, BorderStrong);

        // seta da lista
        Gdiplus::Pen pen(Clr(disabled ? TextDisabled : TextMuted), 1.6f);
        pen.SetStartCap(Gdiplus::LineCapRound);
        pen.SetEndCap(Gdiplus::LineCapRound);
        const float cx = (float)box.right - arrowW / 2.0f, cy = (float)box.CenterPoint().y;
        Gdiplus::PointF pts[3] = { Gdiplus::PointF(cx - 3.5f, cy - 1.8f), Gdiplus::PointF(cx, cy + 1.8f), Gdiplus::PointF(cx + 3.5f, cy - 1.8f) };
        g.DrawLines(&pen, pts, 3);
    }

    target->BitBlt(rc.left, rc.top, rc.Width(), rc.Height(), &dc, 0, 0, SRCCOPY);
    dc.SelectObject(old);
}

void CModernColourButton::DrawHotArrow (BOOL)
{
    // o realce e todo redesenhado em DrawItem
    Invalidate(FALSE);
}
