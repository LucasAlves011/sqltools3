#include "stdafx.h"
#include "Theme/ModernDraw.h"
#include "resource.h"

namespace ModernDraw
{
    Gdiplus::Color Clr (COLORREF c, BYTE alpha)
    {
        return Gdiplus::Color(alpha, GetRValue(c), GetGValue(c), GetBValue(c));
    }

    void RoundRectPath (Gdiplus::GraphicsPath& path, const Gdiplus::RectF& r, float radius)
    {
        float d = radius * 2;
        path.AddArc(r.X, r.Y, d, d, 180, 90);
        path.AddArc(r.GetRight() - d, r.Y, d, d, 270, 90);
        path.AddArc(r.GetRight() - d, r.GetBottom() - d, d, d, 0, 90);
        path.AddArc(r.X, r.GetBottom() - d, d, d, 90, 90);
        path.CloseFigure();
    }

    void FillRoundRect (Gdiplus::Graphics& g, const CRect& rc, float radius, COLORREF color)
    {
        Gdiplus::GraphicsPath path;
        RoundRectPath(path, Gdiplus::RectF((float)rc.left, (float)rc.top, (float)rc.Width(), (float)rc.Height()), radius);
        Gdiplus::SolidBrush brush(Clr(color));
        g.FillPath(&brush, &path);
    }

    void StrokeRoundRect (Gdiplus::Graphics& g, const CRect& rc, float radius, COLORREF color, float width)
    {
        Gdiplus::GraphicsPath path;
        RoundRectPath(path, Gdiplus::RectF(rc.left + 0.5f, rc.top + 0.5f, (float)rc.Width() - 1.0f, (float)rc.Height() - 1.0f), radius);
        Gdiplus::Pen pen(Clr(color), width);
        g.DrawPath(&pen, &path);
    }

    CFont& UiFont ()
    {
        static CFont font;
        if (!font.GetSafeHandle())
            font.CreatePointFont(90, L"Segoe UI");
        return font;
    }

    CFont& UiFontBold ()
    {
        static CFont font;
        if (!font.GetSafeHandle())
            font.CreatePointFont(90, L"Segoe UI Semibold");
        return font;
    }

    CImageList& Images ()
    {
        static CImageList list;
        if (!list.GetSafeHandle())
        {
            HBITMAP hBmp = (HBITMAP)::LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDB_OBJLIST_ICONS),
                                                IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
            if (hBmp)
            {
                list.Create(16, 16, ILC_COLOR32, 72, 0);
                CBitmap bmp;
                bmp.Attach(hBmp);
                list.Add(&bmp, (CBitmap*)NULL);
            }
        }
        return list;
    }

    void CreatePaneImageList (CImageList& list)
    {
        HBITMAP hBmp = (HBITMAP)::LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDB_PANE_ICONS32),
                                            IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
        if (hBmp)
        {
            list.Create(16, 16, ILC_COLOR32, 7, 0);
            CBitmap bmp;
            bmp.Attach(hBmp);
            list.Add(&bmp, (CBitmap*)NULL);
        }
    }

    void DrawDialogIcon (HDC hdc, int index, int x, int y, bool disabled)
    {
        static CImageList list;
        if (!list.GetSafeHandle())
        {
            HBITMAP hBmp = (HBITMAP)::LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDB_DIALOG_ICONS),
                                                IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
            if (hBmp)
            {
                list.Create(16, 16, ILC_COLOR32, 6, 0);
                CBitmap bmp;
                bmp.Attach(hBmp);
                list.Add(&bmp, (CBitmap*)NULL);
            }
        }
        if (!list.GetSafeHandle() || index < 0)
            return;

        IMAGELISTDRAWPARAMS p = {};
        p.cbSize = sizeof(p);
        p.himl = list.GetSafeHandle();
        p.i = index;
        p.hdcDst = hdc;
        p.x = x;
        p.y = y;
        p.rgbBk = CLR_NONE;
        p.rgbFg = CLR_NONE;
        p.fStyle = ILD_TRANSPARENT;
        if (disabled)
        {
            p.fState = ILS_ALPHA;
            p.Frame = 90;
        }
        ImageList_DrawIndirect(&p);
    }

    void DrawImage (CDC& dc, int index, int x, int y, bool disabled)
    {
        CImageList& list = Images();
        if (!list.GetSafeHandle() || index < 0)
            return;

        if (!disabled)
        {
            list.Draw(&dc, index, CPoint(x, y), ILD_TRANSPARENT);
            return;
        }

        IMAGELISTDRAWPARAMS p = {};
        p.cbSize = sizeof(p);
        p.himl = list.GetSafeHandle();
        p.i = index;
        p.hdcDst = dc.GetSafeHdc();
        p.x = x;
        p.y = y;
        p.rgbBk = CLR_NONE;
        p.rgbFg = CLR_NONE;
        p.fStyle = ILD_TRANSPARENT;
        p.fState = ILS_ALPHA;
        p.Frame = 90;
        ImageList_DrawIndirect(&p);
    }

    void DrawUiIcon (CDC& dc, UiIcon icon, int x, int y, bool accent, bool disabled)
    {
        DrawImage(dc, (accent ? OI_UI_ACCENT : OI_UI_REST) + (int)icon, x, y, disabled);
    }

    void DrawTypeIcon (CDC& dc, int imageIndex, int x, int y, bool accent, bool disabled)
    {
        if (imageIndex < 0 || imageIndex > 23)
            return;
        DrawImage(dc, (accent ? OI_TYPE_ACCENT : OI_TYPE_REST) + imageIndex, x, y, disabled);
    }
}
