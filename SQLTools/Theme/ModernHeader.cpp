#include "stdafx.h"
#include "Theme/ModernHeader.h"
#include "Theme/ModernDraw.h"

using namespace ModernTheme;
using namespace ModernDraw;

BEGIN_MESSAGE_MAP(CModernHeader, CHeaderCtrl)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSELEAVE()
END_MESSAGE_MAP()

CModernHeader::CModernHeader ()
    : m_hotItem(-1), m_tracking(false)
{
}

void CModernHeader::Attach (HWND hHeader)
{
    if (!m_hWnd && hHeader)
    {
        SubclassWindow(hHeader);
        SetFont(&UiFont());     // a caixa de edicao do filtro herda esta fonte
        Invalidate();
    }
}

BOOL CModernHeader::OnEraseBkgnd (CDC*)
{
    return TRUE;
}

void CModernHeader::OnMouseMove (UINT nFlags, CPoint point)
{
    if (!m_tracking)
    {
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, m_hWnd, 0 };
        _TrackMouseEvent(&tme);
        m_tracking = true;
    }

    int hot = -1;
    HDHITTESTINFO ht = {};
    ht.pt = point;
    int idx = (int)SendMessage(HDM_HITTEST, 0, (LPARAM)&ht);
    if (idx >= 0 && (ht.flags & HHT_ONHEADER))
        hot = idx;
    if (hot != m_hotItem)
    {
        m_hotItem = hot;
        Invalidate(FALSE);
    }
    CHeaderCtrl::OnMouseMove(nFlags, point);
}

void CModernHeader::OnMouseLeave ()
{
    m_tracking = false;
    if (m_hotItem != -1)
    {
        m_hotItem = -1;
        Invalidate(FALSE);
    }
}

void CModernHeader::OnPaint ()
{
    CPaintDC dc(this);
    CRect rc;
    GetClientRect(rc);

    CDC mem;
    mem.CreateCompatibleDC(&dc);
    CBitmap bmp;
    bmp.CreateCompatibleBitmap(&dc, rc.Width(), rc.Height());
    CBitmap* pOldBmp = mem.SelectObject(&bmp);
    mem.FillSolidRect(rc, Surface);
    mem.SetBkMode(TRANSPARENT);

    Gdiplus::Graphics g(mem.GetSafeHdc());
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

    const bool hasFilter = (GetStyle() & HDS_FILTERBAR) != 0;
    const int titleH = hasFilter ? (rc.Height() + 1) / 2 : rc.Height();
    const int count = GetItemCount();

    for (int n = 0; n < count; ++n)
    {
        int i = (int)SendMessage(HDM_ORDERTOINDEX, n, 0);
        CRect ir;
        if (!GetItemRect(i, &ir))
            continue;

        wchar_t text[256] = L"";
        HDITEM hdi = {};
        hdi.mask = HDI_TEXT | HDI_FORMAT;
        hdi.pszText = text;
        hdi.cchTextMax = 255;
        GetItem(i, &hdi);

        // ---- titulo ----
        CRect tr(ir.left, 0, ir.right, titleH);
        if (i == m_hotItem)
            FillRoundRect(g, CRect(tr.left + 2, tr.top + 2, tr.right - 2, tr.bottom - 1), 5.0f, SurfaceAlt);

        mem.SelectObject(&UiFontBold());
        mem.SetTextColor(TextMuted);
        UINT align = DT_LEFT;
        if ((hdi.fmt & HDF_JUSTIFYMASK) == HDF_RIGHT)  align = DT_RIGHT;
        if ((hdi.fmt & HDF_JUSTIFYMASK) == HDF_CENTER) align = DT_CENTER;
        CRect tt(tr.left + 8, tr.top, tr.right - 8, tr.bottom);
        if (hdi.fmt & (HDF_SORTUP | HDF_SORTDOWN))
            tt.right -= 14;
        mem.DrawText(text, tt, align | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        // seta de ordenacao
        if (hdi.fmt & (HDF_SORTUP | HDF_SORTDOWN))
        {
            float cx = (float)tr.right - 12.0f, cy = (float)tr.CenterPoint().y;
            bool up = (hdi.fmt & HDF_SORTUP) != 0;
            Gdiplus::Pen pen(Clr(Accent), 1.6f);
            pen.SetStartCap(Gdiplus::LineCapRound);
            pen.SetEndCap(Gdiplus::LineCapRound);
            pen.SetLineJoin(Gdiplus::LineJoinRound);
            float d = up ? -1.0f : 1.0f;
            Gdiplus::PointF p[3] = { Gdiplus::PointF(cx - 3.5f, cy - 1.5f * d), Gdiplus::PointF(cx, cy + 2.0f * d), Gdiplus::PointF(cx + 3.5f, cy - 1.5f * d) };
            g.DrawLines(&pen, p, 3);
        }

        // separador entre colunas
        mem.FillSolidRect(ir.right - 1, tr.top + 6, 1, tr.Height() - 12, Border);

        // ---- filtro ----
        if (hasFilter)
        {
            CRect fr(ir.left + 4, titleH + 2, ir.right - 4, rc.Height() - 3);
            if (fr.Width() > 8 && fr.Height() > 6)
            {
                wchar_t filter[256] = L"";
                HD_TEXTFILTER tf = {};
                tf.pszText = filter;
                tf.cchTextMax = 255;
                HDITEM fi = {};
                fi.mask = HDI_FILTER;
                fi.type = HDFT_ISSTRING;
                fi.pvFilter = &tf;
                GetItem(i, &fi);

                bool active = filter[0] != 0;
                FillRoundRect(g, fr, 6.0f, Surface);
                StrokeRoundRect(g, fr, 6.0f, active ? Accent : Border);

                mem.SelectObject(&UiFont());
                mem.SetTextColor(active ? Text : RGB(154, 161, 171));
                CRect ft(fr.left + 8, fr.top, fr.right - 24, fr.bottom);
                mem.DrawText(active ? filter : L"Filtrar...", ft, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

                DrawUiIcon(mem, UI_FILTER, fr.right - 20, fr.CenterPoint().y - 8, active);
            }
        }
    }

    // linha inferior do cabecalho
    mem.FillSolidRect(0, rc.Height() - 1, rc.Width(), 1, Border);

    dc.BitBlt(0, 0, rc.Width(), rc.Height(), &mem, 0, 0, SRCCOPY);
    mem.SelectObject(pOldBmp);
}
