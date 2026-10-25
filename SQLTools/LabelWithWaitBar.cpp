/* 
	SQLTools is a tool for Oracle database developers and DBAs.
    Copyright (C) 1997-2016 Aleksey Kochetov

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
*/

#include "stdafx.h"
#include "LabelWithWaitBar.h"
#include <gdiplus.h>
#include "Theme/ModernTheme.h"
using namespace Gdiplus;


// LabelWithWaitBar

//IMPLEMENT_DYNAMIC(LabelWithWaitBar, CStatic)

LabelWithWaitBar::LabelWithWaitBar()
    : m_nProgressPos(0), 
    m_nGradientWidth(100), 
    m_nTimerId(777),
    m_nTimerElapse(200),
    m_animation(false),
    m_nTextPosition(DT_CENTER),
    m_flat(false)
{
}

LabelWithWaitBar::~LabelWithWaitBar()
{
}


BEGIN_MESSAGE_MAP(LabelWithWaitBar, CStatic)
    ON_WM_TIMER()
END_MESSAGE_MAP()


// LabelWithWaitBar message handlers

void LabelWithWaitBar::SetText (LPCTSTR pText, COLORREF color) 
{
    m_text = pText;
    m_textColor = color;
    if (m_hWnd) Invalidate(TRUE);
}

void LabelWithWaitBar::StartAnimation (bool animation)
{
    if (m_animation != animation)
    {
        m_animation = animation;

        if (!animation)
        {
            KillTimer(m_nTimerId);
            Invalidate(FALSE);
        }
        else
        {
            m_nProgressPos = -m_nGradientWidth + m_nGradientWidth / 5;
            SetTimer(m_nTimerId, m_nTimerElapse, NULL);
            Invalidate(FALSE);
        }
    }
}

void LabelWithWaitBar::DrawItem (LPDRAWITEMSTRUCT d)
{
    using namespace ModernTheme;

    if (!d)
        return;

    CDC dc;
    dc.Attach(d->hDC);
    CRect rc = d->rcItem;
    int w = rc.Width(), h = rc.Height();

    CDC mem;
    mem.CreateCompatibleDC(&dc);
    CBitmap bmp;
    bmp.CreateCompatibleBitmap(&dc, w, h);
    CBitmap* pOldBmp = mem.SelectObject(&bmp);

    // fundo da barra e "chip" arredondado com borda fina
    mem.FillSolidRect(0, 0, w, h, Surface);

    if (m_flat)
    {
        mem.FillSolidRect(0, 0, w, 1, Border);
        if (m_animation)
        {
            Graphics gf(mem.GetSafeHdc());
            BYTE r8 = GetRValue(Accent), g8 = GetGValue(Accent), b8 = GetBValue(Accent);
            LinearGradientBrush bar(
                PointF((REAL)m_nProgressPos, 0), PointF((REAL)(m_nProgressPos + m_nGradientWidth), 0),
                Color(0, r8, g8, b8), Color(255, r8, g8, b8));
            REAL intensities[] = { 0.0f, 1.0f, 0.0f };
            REAL positions[]   = { 0.0f, 0.5f, 1.0f };
            bar.SetBlend(intensities, positions, 3);
            gf.FillRectangle(&bar, (REAL)m_nProgressPos, (REAL)h - 2.0f, (REAL)m_nGradientWidth, 2.0f);
        }
        static CFont s_flatFont;
        if (!s_flatFont.GetSafeHandle())
            s_flatFont.CreatePointFont(90, L"Segoe UI");
        CFont* pOldFlat = mem.SelectObject(&s_flatFont);
        mem.SetBkMode(TRANSPARENT);
        mem.SetTextColor(TextMuted);
        CRect rt(12, 0, w - 8, h);
        mem.DrawText(m_text, rt, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        mem.SelectObject(pOldFlat);
        dc.BitBlt(rc.left, rc.top, w, h, &mem, 0, 0, SRCCOPY);
        mem.SelectObject(pOldBmp);
        dc.Detach();
        return;
    }

    Graphics g(mem.GetSafeHdc());
    g.SetSmoothingMode(SmoothingModeAntiAlias);

    GraphicsPath chip;
    {
        RectF r(0.5f, 1.5f, (REAL)w - 1.0f, (REAL)h - 3.0f);
        REAL rad = 7.0f, dia = rad * 2;
        chip.AddArc(r.X, r.Y, dia, dia, 180, 90);
        chip.AddArc(r.GetRight() - dia, r.Y, dia, dia, 270, 90);
        chip.AddArc(r.GetRight() - dia, r.GetBottom() - dia, dia, dia, 0, 90);
        chip.AddArc(r.X, r.GetBottom() - dia, dia, dia, 90, 90);
        chip.CloseFigure();
    }
    SolidBrush fill(Color(255, GetRValue(Surface), GetGValue(Surface), GetBValue(Surface)));
    g.FillPath(&fill, &chip);

    // indicador de atividade: linha de destaque que corre pela base do campo
    if (m_animation)
    {
        GraphicsPath* clipPath = chip.Clone();
        Region clip(clipPath);
        g.SetClip(&clip);
        BYTE r8 = GetRValue(Accent), g8 = GetGValue(Accent), b8 = GetBValue(Accent);
        LinearGradientBrush bar(
            PointF((REAL)m_nProgressPos, 0), PointF((REAL)(m_nProgressPos + m_nGradientWidth), 0),
            Color(0, r8, g8, b8), Color(255, r8, g8, b8));
        REAL intensities[] = { 0.0f, 1.0f, 0.0f };
        REAL positions[]   = { 0.0f, 0.5f, 1.0f };
        bar.SetBlend(intensities, positions, 3);
        g.FillRectangle(&bar, (REAL)m_nProgressPos, (REAL)h - 4.5f, (REAL)m_nGradientWidth, 3.0f);
        g.ResetClip();
        delete clipPath;
    }

    Pen border(Color(255, GetRValue(Border), GetGValue(Border), GetBValue(Border)), 1.0f);
    g.DrawPath(&border, &chip);

    // texto: "usuario@alias:host as papel [versao]"; a cor da conexao vale para o nome
    static CFont s_font, s_fontBold;
    if (!s_font.GetSafeHandle())
    {
        s_font.CreatePointFont(90, L"Segoe UI");
        s_fontBold.CreatePointFont(90, L"Segoe UI Semibold");
    }

    COLORREF mainColor = (m_textColor == RGB(0, 0, 0)) ? Text : m_textColor;

    CString mainPart = m_text, rolePart, versionPart;
    int v = mainPart.ReverseFind(L'[');
    if (v > 0 && mainPart.Right(1) == L"]")
    {
        versionPart = mainPart.Mid(v);
        mainPart = mainPart.Left(v);
        mainPart.TrimRight();
    }
    int a2 = mainPart.Find(L" as ");
    if (a2 > 0)
    {
        rolePart = mainPart.Mid(a2);
        mainPart = mainPart.Left(a2);
    }

    mem.SetBkMode(TRANSPARENT);
    CRect rcText(10, 0, w - 10, h);

    CFont* pOld = mem.SelectObject(&s_fontBold);
    CSize szMain = mem.GetTextExtent(mainPart);
    mem.SelectObject(&s_font);
    CSize szRole = mem.GetTextExtent(rolePart);
    CSize szVer  = mem.GetTextExtent(versionPart);
    int gap = versionPart.IsEmpty() ? 0 : 8;
    int total = szMain.cx + szRole.cx + gap + szVer.cx;

    if (total <= rcText.Width())
    {
        int x = (m_nTextPosition == DT_CENTER) ? rcText.left + (rcText.Width() - total) / 2 : rcText.left;
        int ty = (h - szMain.cy) / 2;

        mem.SelectObject(&s_fontBold);
        mem.SetTextColor(mainColor);
        mem.TextOut(x, ty, mainPart);
        x += szMain.cx;

        mem.SelectObject(&s_font);
        mem.SetTextColor(TextMuted);
        mem.TextOut(x, ty, rolePart);
        x += szRole.cx + gap;
        mem.TextOut(x, ty, versionPart);
    }
    else
    {
        // nao cabe: texto unico com reticencias, como antes
        mem.SelectObject(&s_font);
        mem.SetTextColor(mainColor);
        mem.DrawText(m_text, rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
    mem.SelectObject(pOld);

    dc.BitBlt(rc.left, rc.top, w, h, &mem, 0, 0, SRCCOPY);
    mem.SelectObject(pOldBmp);
    dc.Detach();
}


void LabelWithWaitBar::OnTimer(UINT_PTR nIDEvent)
{
    CStatic::OnTimer(nIDEvent);

    if (nIDEvent == m_nTimerId)
    {
        CRect rc;
        GetClientRect(rc);
        m_nProgressPos += m_nGradientWidth / 10;
        if (m_nProgressPos > rc.Width())
            m_nProgressPos = -m_nGradientWidth + m_nGradientWidth / 10;

        Invalidate(FALSE);
    }
}
