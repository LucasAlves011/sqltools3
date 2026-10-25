#pragma once

// Auxiliares de desenho do visual moderno (GDI+ e tira de icones Tabler), compartilhados pelos controles.

#include <gdiplus.h>
#include "Theme/ModernTheme.h"

namespace ModernDraw
{
    // posicoes na tira ObjectListIcons.bmp (gerada por tools/GenToolbarIcons)
    enum
    {
        OI_TYPE_REST   = 0,     // 0..23  tipos de objeto (ordem de IDB_DB_OBJ_IMAGE_INDEX)
        OI_TYPE_ACCENT = 24,    // 24..47 os mesmos, na cor de destaque
        OI_UI_REST     = 48,    // 48..59 interface
        OI_UI_ACCENT   = 60     // 60..71 interface, cor de destaque
    };

    enum UiIcon
    {
        UI_REFRESH = 0, UI_LIST, UI_TABLE, UI_CHECK, UI_X, UI_FILTER,
        UI_SETTINGS, UI_ADJUST, UI_CHEVRON, UI_USER, UI_PLUS, UI_SEARCH
    };

    Gdiplus::Color Clr (COLORREF c, BYTE alpha = 255);
    void RoundRectPath (Gdiplus::GraphicsPath& path, const Gdiplus::RectF& r, float radius);
    void FillRoundRect (Gdiplus::Graphics& g, const CRect& rc, float radius, COLORREF color);
    void StrokeRoundRect (Gdiplus::Graphics& g, const CRect& rc, float radius, COLORREF color, float width = 1.0f);

    // fontes da interface (Segoe UI 9 e semibold)
    CFont& UiFont ();
    CFont& UiFontBold ();

    // tira de icones (16x16, 32 bits com alfa); a lista tem os 72 icones na ordem acima
    CImageList& Images ();
    void DrawImage (CDC& dc, int index, int x, int y, bool disabled = false);
    void DrawUiIcon (CDC& dc, UiIcon icon, int x, int y, bool accent, bool disabled = false);
    void DrawTypeIcon (CDC& dc, int imageIndex, int x, int y, bool accent, bool disabled = false);
}
