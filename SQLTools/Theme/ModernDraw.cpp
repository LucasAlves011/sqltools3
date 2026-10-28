#include "stdafx.h"
#include "Theme/ModernDraw.h"
#include "Theme/ModernDark.h"
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
                ModernDark::RecolorBitmap(hBmp);
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
            ModernDark::RecolorBitmap(hBmp);
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
                list.Create(16, 16, ILC_COLOR32, 7, 0);
                CBitmap bmp;
                ModernDark::RecolorBitmap(hBmp);
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

    void DrawPeekIcon (HDC hdc, int index, int x, int y)
    {
        static CImageList list;
        if (!list.GetSafeHandle())
        {
            HBITMAP hBmp = (HBITMAP)::LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDB_PEEK_ICONS),
                                                IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
            if (hBmp)
            {
                list.Create(16, 16, ILC_COLOR32, 9, 0);
                ModernDark::RecolorBitmap(hBmp);
                CBitmap bmp;
                bmp.Attach(hBmp);
                list.Add(&bmp, (CBitmap*)NULL);
            }
        }
        if (list.GetSafeHandle() && index >= 0)
            ImageList_Draw(list.GetSafeHandle(), index, hdc, x, y, ILD_TRANSPARENT);
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

    namespace
    {
        // le uma tira de icones (BMP de 32 bits embutido) como DIB; devolve os pixels (BGRA, de baixo para cima)
        HBITMAP LoadStrip (UINT resId, BYTE** bits)
        {
            HBITMAP h = (HBITMAP)::LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(resId), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
            BITMAP bm = {};
            if (!h || !::GetObject(h, sizeof(bm), &bm) || bm.bmBitsPixel != 32 || !bm.bmBits)
            {
                if (h) ::DeleteObject(h);
                return NULL;
            }
            *bits = (BYTE*)bm.bmBits;
            return h;
        }

        // glifos desenhados por codigo, no mesmo estilo dos icones Tabler da tira (traco de 1,5 px, pontas redondas)
        void RoundPen (Gdiplus::Pen& p)
        {
            p.SetStartCap(Gdiplus::LineCapRound);
            p.SetEndCap(Gdiplus::LineCapRound);
            p.SetLineJoin(Gdiplus::LineJoinRound);
        }

        enum Glyph { G_COLUMN, G_PARAM_IN, G_PARAM_OUT, G_PARAM_INOUT, G_PARTITION };

        void DrawGlyph (Gdiplus::Graphics& g, Glyph glyph)
        {
            using namespace Gdiplus;
            const Color slate(255, 71, 84, 103), blue(255, 47, 111, 237), amber(255, 217, 138, 0), green(255, 22, 136, 84);

            switch (glyph)
            {
            case G_COLUMN:
            {
                Pen p(slate, 1.5f); RoundPen(p);
                GraphicsPath box;
                RectF r(2.5f, 3.0f, 11.0f, 10.0f);
                box.AddArc(r.X, r.Y, 3.0f, 3.0f, 180.0f, 90.0f); box.AddArc(r.GetRight() - 3.0f, r.Y, 3.0f, 3.0f, 270.0f, 90.0f);
                box.AddArc(r.GetRight() - 3.0f, r.GetBottom() - 3.0f, 3.0f, 3.0f, 0.0f, 90.0f); box.AddArc(r.X, r.GetBottom() - 3.0f, 3.0f, 3.0f, 90.0f, 90.0f);
                box.CloseFigure();
                g.DrawPath(&p, &box);
                g.DrawLine(&p, 6.2f, 3.0f, 6.2f, 13.0f);
                g.DrawLine(&p, 9.8f, 3.0f, 9.8f, 13.0f);
                break;
            }
            case G_PARAM_IN:    // seta para a direita (entra)
            {
                Pen p(blue, 1.5f); RoundPen(p);
                g.DrawLine(&p, 3.0f, 8.0f, 12.5f, 8.0f);
                g.DrawLine(&p, 9.0f, 4.5f, 12.5f, 8.0f);
                g.DrawLine(&p, 9.0f, 11.5f, 12.5f, 8.0f);
                break;
            }
            case G_PARAM_OUT:   // seta para a esquerda (sai)
            {
                Pen p(amber, 1.5f); RoundPen(p);
                g.DrawLine(&p, 13.0f, 8.0f, 3.5f, 8.0f);
                g.DrawLine(&p, 7.0f, 4.5f, 3.5f, 8.0f);
                g.DrawLine(&p, 7.0f, 11.5f, 3.5f, 8.0f);
                break;
            }
            case G_PARAM_INOUT: // setas nos dois sentidos
            {
                Pen p(green, 1.5f); RoundPen(p);
                g.DrawLine(&p, 2.5f, 8.0f, 13.5f, 8.0f);
                g.DrawLine(&p, 5.5f, 5.0f, 2.5f, 8.0f);
                g.DrawLine(&p, 5.5f, 11.0f, 2.5f, 8.0f);
                g.DrawLine(&p, 10.5f, 5.0f, 13.5f, 8.0f);
                g.DrawLine(&p, 10.5f, 11.0f, 13.5f, 8.0f);
                break;
            }
            case G_PARTITION:   // quatro quadrados
            {
                Pen p(slate, 1.5f); RoundPen(p);
                const float pos[2] = { 2.5f, 9.0f };
                for (int i = 0; i < 2; ++i)
                    for (int j = 0; j < 2; ++j)
                        g.DrawRectangle(&p, pos[i], pos[j], 4.5f, 4.5f);
                break;
            }
            }
        }

        // origem de cada icone da arvore (a ordem e a de IDII_* em ObjectTree_Builder.h)
        enum Source { S_NONE, S_TYPE, S_TYPE_ACCENT, S_PANE, S_GLYPH };
        struct TreeIcon { Source src; int index; };

        const TreeIcon kTreeIcons[] =
        {
            { S_PANE, 0 },          //  0 pasta
            { S_PANE, 1 },          //  1 pasta aberta
            { S_TYPE, 6 },          //  2 tabela
            { S_TYPE, 10 },         //  3 cluster
            { S_TYPE, 11 },         //  4 database link
            { S_TYPE, 16 },         //  5 indice
            { S_TYPE, 2 },          //  6 package
            { S_TYPE, 7 },          //  7 sequence
            { S_TYPE, 8 },          //  8 synonym
            { S_TYPE, 4 },          //  9 trigger
            { S_TYPE, 5 },          // 10 view
            { S_TYPE, 12 },         // 11 schema
            { S_TYPE, 0 },          // 12 function
            { S_TYPE, 1 },          // 13 procedure
            { S_GLYPH, G_COLUMN },  // 14 coluna
            { S_TYPE, 17 },         // 15 check
            { S_TYPE_ACCENT, 18 },  // 16 primary key
            { S_TYPE, 19 },         // 17 unique key
            { S_TYPE, 20 },         // 18 foreign key
            { S_NONE, 0 }, { S_NONE, 0 }, { S_NONE, 0 }, { S_NONE, 0 },
            { S_TYPE, 9 },          // 23 privilegio concedido
            { S_TYPE, 3 },          // 24 package body
            { S_TYPE, 14 },         // 25 type
            { S_TYPE, 15 },         // 26 type body
            { S_NONE, 0 },
            { S_TYPE, 13 },         // 28 desconhecido
            { S_GLYPH, G_PARAM_IN },    // 29
            { S_GLYPH, G_PARAM_OUT },   // 30
            { S_GLYPH, G_PARAM_INOUT }, // 31
            { S_GLYPH, G_PARTITION },   // 32
            { S_NONE, 0 }, { S_NONE, 0 },
        };
    }

    // icones da arvore do Object Viewer: 35 posicoes de 16x16, na ordem de IDII_* (ObjectTree_Builder.h), montadas a partir
    // das tiras ja existentes (tipos de objeto e pastas) e de glifos desenhados por codigo; o tema escuro recolore tudo
    void CreateObjectTreeImageList (CImageList& list)
    {
        const int S = 16, N = (int)(sizeof(kTreeIcons) / sizeof(kTreeIcons[0]));

        BYTE *typeBits = NULL, *paneBits = NULL;
        HBITMAP hTypes = LoadStrip(IDB_OBJLIST_ICONS, &typeBits);
        HBITMAP hPanes = LoadStrip(IDB_PANE_ICONS32, &paneBits);

        BITMAPINFO bi = {};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = S * N;
        bi.bmiHeader.biHeight = S;          // de baixo para cima, como as tiras de origem
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        BYTE* dst = NULL;
        HBITMAP hStrip = ::CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, (void**)&dst, NULL, 0);
        if (!hStrip || !dst)
        {
            if (hTypes) ::DeleteObject(hTypes);
            if (hPanes) ::DeleteObject(hPanes);
            return;
        }
        ::memset(dst, 0, (size_t)S * N * S * 4);

        BITMAP tm = {}, pm = {};
        if (hTypes) ::GetObject(hTypes, sizeof(tm), &tm);
        if (hPanes) ::GetObject(hPanes, sizeof(pm), &pm);

        for (int i = 0; i < N; ++i)
        {
            const TreeIcon& ic = kTreeIcons[i];
            const BYTE* src = NULL;
            int srcWidth = 0, srcCell = 0;
            switch (ic.src)
            {
            case S_TYPE:        src = typeBits; srcWidth = tm.bmWidth; srcCell = ic.index; break;
            case S_TYPE_ACCENT: src = typeBits; srcWidth = tm.bmWidth; srcCell = 24 + ic.index; break;   // 24.. = cor de destaque
            case S_PANE:        src = paneBits; srcWidth = pm.bmWidth; srcCell = ic.index; break;
            default: break;
            }

            if (src && (srcCell + 1) * S <= srcWidth)
            {
                for (int y = 0; y < S; ++y)
                    ::memcpy(dst + ((size_t)y * S * N + (size_t)i * S) * 4, src + ((size_t)y * srcWidth + (size_t)srcCell * S) * 4, S * 4);
            }
            else if (ic.src == S_GLYPH)
            {
                Gdiplus::Bitmap cell(S, S, PixelFormat32bppARGB);   // de cima para baixo, alfa direto
                {
                    Gdiplus::Graphics g(&cell);
                    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                    DrawGlyph(g, (Glyph)ic.index);
                }
                Gdiplus::BitmapData data;
                Gdiplus::Rect all(0, 0, S, S);
                if (cell.LockBits(&all, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &data) == Gdiplus::Ok)
                {
                    for (int y = 0; y < S; ++y)
                        ::memcpy(dst + ((size_t)(S - 1 - y) * S * N + (size_t)i * S) * 4, (BYTE*)data.Scan0 + (size_t)y * data.Stride, S * 4);
                    cell.UnlockBits(&data);
                }
            }
        }

        if (hTypes) ::DeleteObject(hTypes);
        if (hPanes) ::DeleteObject(hPanes);

        ModernDark::RecolorBitmap(hStrip);
        list.Create(S, S, ILC_COLOR32, N, 0);
        CBitmap bmp;
        bmp.Attach(hStrip);
        list.Add(&bmp, (CBitmap*)NULL);
    }

    void DrawTypeIcon (CDC& dc, int imageIndex, int x, int y, bool accent, bool disabled)
    {
        if (imageIndex < 0 || imageIndex > 23)
            return;
        DrawImage(dc, (accent ? OI_TYPE_ACCENT : OI_TYPE_REST) + imageIndex, x, y, disabled);
    }
}
