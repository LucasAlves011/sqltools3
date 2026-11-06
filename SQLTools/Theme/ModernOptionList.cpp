#include "stdafx.h"
#include <algorithm>
#include "Theme/ModernOptionList.h"
#include "Theme/ModernDraw.h"
#include "Theme/ModernDark.h"

using namespace ModernTheme;
using ModernDraw::Clr;

namespace
{
    const int GROUP_H = 32, ROW_H = 36, FOOTER_H = 74, PAD = 6;
    const int TOGGLE_W = 38, TOGGLE_H = 20, BOX_H = 28, STEP_W = 28, STEP_VALUE_W = 58, GAP = 10;
    const UINT IDC_VALUE_EDIT = 100;

    void Chevron (Gdiplus::Graphics& g, float cx, float cy, bool down, COLORREF color)
    {
        const float s = 4.0f;
        Gdiplus::Pen pen(Clr(color), 1.6f);
        pen.SetStartCap(Gdiplus::LineCapRound);
        pen.SetEndCap(Gdiplus::LineCapRound);
        pen.SetLineJoin(Gdiplus::LineJoinRound);
        Gdiplus::PointF pts[3];
        if (down)
        {
            pts[0] = Gdiplus::PointF(cx - s, cy - s / 2); pts[1] = Gdiplus::PointF(cx, cy + s / 2); pts[2] = Gdiplus::PointF(cx + s, cy - s / 2);
        }
        else
        {
            pts[0] = Gdiplus::PointF(cx - s / 2, cy - s); pts[1] = Gdiplus::PointF(cx + s / 2, cy); pts[2] = Gdiplus::PointF(cx - s / 2, cy + s);
        }
        g.DrawLines(&pen, pts, 3);
    }
}

BEGIN_MESSAGE_MAP(CModernOptionList, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_SIZE()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONDBLCLK()
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSELEAVE()
    ON_WM_MOUSEWHEEL()
    ON_WM_VSCROLL()
    ON_WM_KEYDOWN()
    ON_WM_GETDLGCODE()
    ON_WM_SETFOCUS()
    ON_WM_KILLFOCUS()
    ON_WM_SETCURSOR()
    ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

CModernOptionList::CModernOptionList ()
    : m_scroll(0), m_sel(-1), m_hot(-1), m_hotPart(P_NONE), m_tracking(false), m_focus(false),
      m_editOption(-1), m_editBrushColor(CLR_INVALID) {}

BOOL CModernOptionList::Create (CWnd* parent, UINT id, const CRect& rect)
{
    if (!CWnd::Create(AfxRegisterWndClass(CS_DBLCLKS, ::LoadCursor(NULL, IDC_ARROW)), L"",
                      WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL, rect, parent, id))
        return FALSE;
    if (IsDark())
        ::SetWindowTheme(m_hWnd, ModernDark::ControlTheme(), NULL);    // barra de rolagem escura
    return TRUE;
}

void CModernOptionList::SetOptions (const std::vector<ModernOption>& options)
{
    m_options = options;
    m_sel = -1;
    m_scroll = 0;
    Rebuild();
}

void CModernOptionList::Rebuild ()
{
    m_rows.clear();
    std::wstring group;
    bool collapsed = false;
    for (int n = 0; n < (int)m_options.size(); ++n)
    {
        const ModernOption& o = m_options[n];
        if (group != o.group)
        {
            group = o.group;
            collapsed = std::find(m_collapsed.begin(), m_collapsed.end(), group) != m_collapsed.end();
            Row r = { true, -1, group };
            m_rows.push_back(r);
        }
        if (!collapsed)
        {
            Row r = { false, n, std::wstring() };
            m_rows.push_back(r);
        }
    }
    if (m_sel < 0)
        for (size_t i = 0; i < m_rows.size(); ++i)
            if (!m_rows[i].group) { m_sel = (int)i; break; }
    m_sel = std::min(m_sel, (int)m_rows.size() - 1);
    UpdateScroll();
    if (m_hWnd) Invalidate(FALSE);
}

CRect CModernOptionList::ListRect () const
{
    CRect rc;
    GetClientRect(rc);
    rc.DeflateRect(1, PAD, 1, FOOTER_H);
    return rc;
}

int CModernOptionList::RowHeight (int row) const { return m_rows[row].group ? GROUP_H : ROW_H; }

int CModernOptionList::RowTop (int row) const
{
    int y = ListRect().top - m_scroll;
    for (int i = 0; i < row; ++i) y += RowHeight(i);
    return y;
}

void CModernOptionList::UpdateScroll ()
{
    if (!m_hWnd) return;
    int total = 0;
    for (size_t i = 0; i < m_rows.size(); ++i) total += RowHeight((int)i);
    const int page = std::max(1, ListRect().Height());
    m_scroll = std::max(0, std::min(m_scroll, total - page));
    SCROLLINFO si = { sizeof(si), SIF_ALL, 0, std::max(0, total - 1), (UINT)page, m_scroll, 0 };     // some quando tudo cabe
    SetScrollInfo(SB_VERT, &si, TRUE);
}

bool CModernOptionList::NumberEnabled (const ModernOption& o) const
{
    return o.kind == ModernOption::NUMBER || (o.kind == ModernOption::TOGGLE_NUMBER && *o.on);
}

// a chave fica a direita; com numero, o numero fica a direita e a chave antes dele
CRect CModernOptionList::ToggleRect (int row) const
{
    CRect rc;
    GetClientRect(rc);
    const ModernOption& o = m_options[m_rows[row].option];
    int right = rc.right - 14;
    if (o.kind == ModernOption::TOGGLE_NUMBER)
        right -= STEP_W * 2 + STEP_VALUE_W + GAP;
    const int y = RowTop(row) + (ROW_H - TOGGLE_H) / 2;
    return CRect(right - TOGGLE_W, y, right, y + TOGGLE_H);
}

void CModernOptionList::StepperRects (int row, CRect& minus, CRect& value, CRect& plus) const
{
    CRect rc;
    GetClientRect(rc);
    const int right = rc.right - 14, y = RowTop(row) + (ROW_H - BOX_H) / 2;
    plus  = CRect(right - STEP_W, y, right, y + BOX_H);
    value = CRect(plus.left - STEP_VALUE_W, y, plus.left, y + BOX_H);
    minus = CRect(value.left - STEP_W, y, value.left, y + BOX_H);
}

int CModernOptionList::HitRow (CPoint pt, EPart& part) const
{
    part = P_NONE;
    if (!ListRect().PtInRect(pt))
        return -1;
    for (int i = 0; i < (int)m_rows.size(); ++i)
    {
        const int top = RowTop(i);
        if (pt.y < top || pt.y >= top + RowHeight(i))
            continue;
        if (m_rows[i].group) { part = P_GROUP; return i; }

        const ModernOption& o = m_options[m_rows[i].option];
        part = P_ROW;
        if (o.kind != ModernOption::TOGGLE)
        {
            CRect minus, value, plus;
            StepperRects(i, minus, value, plus);
            if (minus.PtInRect(pt)) { part = P_MINUS; return i; }
            if (plus.PtInRect(pt))  { part = P_PLUS;  return i; }
            if (value.PtInRect(pt)) { part = P_VALUE; return i; }
        }
        if (o.kind == ModernOption::TOGGLE)
            part = P_TOGGLE;                                    // a linha inteira liga e desliga
        else if (o.kind == ModernOption::TOGGLE_NUMBER)
        {
            CRect minus, value, plus;
            StepperRects(i, minus, value, plus);
            if (pt.x < minus.left - 2)                          // tudo antes do numero liga e desliga
                part = P_TOGGLE;
        }
        return i;
    }
    return -1;
}

BOOL CModernOptionList::OnEraseBkgnd (CDC*) { return TRUE; }

void CModernOptionList::OnPaint ()
{
    CPaintDC paint(this);
    CRect rc;
    GetClientRect(rc);
    CDC dc;
    dc.CreateCompatibleDC(&paint);
    CBitmap bmp;
    bmp.CreateCompatibleBitmap(&paint, std::max(1, rc.Width()), std::max(1, rc.Height()));
    CBitmap* oldBmp = dc.SelectObject(&bmp);
    CFont* oldFont = dc.SelectObject(&ModernDraw::UiFont());
    dc.SetBkMode(TRANSPARENT);
    dc.FillSolidRect(rc, Surface);

    {
        Gdiplus::Graphics g(dc.GetSafeHdc());
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

        const CRect list = ListRect();
        CRgn clip;
        clip.CreateRectRgnIndirect(list);
        dc.SelectClipRgn(&clip);
        g.SetClip(Gdiplus::Rect(list.left, list.top, list.Width(), list.Height()));

        for (int i = 0; i < (int)m_rows.size(); ++i)
        {
            const int top = RowTop(i), h = RowHeight(i);
            if (top + h < list.top || top > list.bottom)
                continue;
            CRect row(list.left + 6, top + 2, list.right - 6, top + h - 2);

            if (i == m_sel)
                ModernDraw::FillRoundRect(g, row, 6.0f, m_focus ? AccentTint : SelInactive);
            else if (i == m_hot)
                ModernDraw::FillRoundRect(g, row, 6.0f, Hover);

            if (m_rows[i].group)
            {
                const bool collapsed = std::find(m_collapsed.begin(), m_collapsed.end(), m_rows[i].name) != m_collapsed.end();
                Chevron(g, (float)row.left + 12, (float)row.CenterPoint().y, !collapsed, TextMuted);
                dc.SelectObject(&ModernDraw::UiFontBold());
                dc.SetTextColor(Text);
                CRect tr(row.left + 26, row.top, row.right, row.bottom);
                dc.DrawText(m_rows[i].name.c_str(), tr, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
                continue;
            }

            const ModernOption& o = m_options[m_rows[i].option];
            const bool hotRow = i == m_hot;
            int labelRight = row.right - 10;

            if (o.kind != ModernOption::TOGGLE)
            {
                CRect minus, value, plus;
                StepperRects(i, minus, value, plus);
                const bool enabled = NumberEnabled(o);
                CRect box(minus.left, minus.top, plus.right, plus.bottom);
                ModernDraw::FillRoundRect(g, box, 6.0f, SurfaceAlt);
                ModernDraw::StrokeRoundRect(g, box, 6.0f, Border);
                const int v = *o.value;
                const bool canMinus = enabled && v > o.minValue, canPlus = enabled && v < o.maxValue;
                if (hotRow && m_hotPart == P_MINUS && canMinus)
                    ModernDraw::FillRoundRect(g, CRect(minus.left + 2, minus.top + 2, minus.right - 1, minus.bottom - 2), 5.0f, Hover);
                if (hotRow && m_hotPart == P_PLUS && canPlus)
                    ModernDraw::FillRoundRect(g, CRect(plus.left + 1, plus.top + 2, plus.right - 2, plus.bottom - 2), 5.0f, Hover);
                if (hotRow && m_hotPart == P_VALUE && enabled)
                    ModernDraw::StrokeRoundRect(g, CRect(value.left + 2, value.top + 3, value.right - 2, value.bottom - 3), 4.0f, BorderStrong);
                Gdiplus::Pen on(Clr(Text), 1.6f), off(Clr(TextDisabled), 1.6f);
                const float my = (float)minus.CenterPoint().y, mx = (float)minus.CenterPoint().x, px = (float)plus.CenterPoint().x;
                g.DrawLine(canMinus ? &on : &off, mx - 4.5f, my, mx + 4.5f, my);
                g.DrawLine(canPlus ? &on : &off, px - 4.5f, my, px + 4.5f, my);
                g.DrawLine(canPlus ? &on : &off, px, my - 4.5f, px, my + 4.5f);
                if (!(m_edit.GetSafeHwnd() && m_editOption == m_rows[i].option))
                {
                    dc.SelectObject(&ModernDraw::UiFont());
                    dc.SetTextColor(enabled ? Text : TextDisabled);
                    dc.DrawText(std::to_wstring(v).c_str(), value, DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_NOPREFIX);
                }
                labelRight = minus.left - 10;
            }
            if (o.kind != ModernOption::NUMBER)
            {
                const CRect t = ToggleRect(i);
                const bool on = *o.on;
                ModernDraw::FillRoundRect(g, t, (float)t.Height() / 2, on ? (hotRow ? AccentHover : Accent) : (hotRow ? BorderStrong : Border));
                const int k = t.Height() - 6;
                CRect knob(on ? t.right - 3 - k : t.left + 3, t.top + 3, on ? t.right - 3 : t.left + 3 + k, t.bottom - 3);
                ModernDraw::FillRoundRect(g, knob, (float)k / 2, on ? RGB(255, 255, 255) : Surface);
                labelRight = t.left - 10;
            }

            dc.SelectObject(&ModernDraw::UiFont());
            dc.SetTextColor(Text);
            CRect tr(row.left + 26, row.top, labelRight, row.bottom);
            dc.DrawText(o.label, tr, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
        }

        g.ResetClip();
    }
    dc.SelectClipRgn(NULL);     // o rodape fica fora da area das linhas

    // rodape: descricao da opcao selecionada
    CRect foot(rc.left, rc.bottom - FOOTER_H, rc.right, rc.bottom);
    dc.FillSolidRect(foot.left, foot.top, foot.Width(), 1, Border);
    CRect ft(foot.left + 16, foot.top + 10, foot.right - 16, foot.bottom - 8);
    const wchar_t* title = nullptr;
    const wchar_t* info = L"Select an option to see what it does.";
    if (m_sel >= 0 && m_sel < (int)m_rows.size() && !m_rows[m_sel].group)
    {
        title = m_options[m_rows[m_sel].option].label;
        info = m_options[m_rows[m_sel].option].info;
    }
    int infoTop = ft.top;
    if (title)
    {
        dc.SelectObject(&ModernDraw::UiFontBold());
        dc.SetTextColor(Text);
        dc.DrawText(title, CRect(ft.left, ft.top, ft.right, ft.top + 18), DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
        infoTop += 22;
    }
    dc.SelectObject(&ModernDraw::UiFont());
    dc.SetTextColor(TextMuted);
    dc.DrawText(info, CRect(ft.left, infoTop, ft.right, ft.bottom), DT_WORDBREAK | DT_NOPREFIX | DT_END_ELLIPSIS);

    paint.BitBlt(0, 0, rc.Width(), rc.Height(), &dc, 0, 0, SRCCOPY);
    dc.SelectObject(oldFont);
    dc.SelectObject(oldBmp);
}

void CModernOptionList::OnSize (UINT nType, int cx, int cy)
{
    CWnd::OnSize(nType, cx, cy);
    EndEdit(true);
    UpdateScroll();
}

void CModernOptionList::Changed ()
{
    Invalidate(FALSE);
    GetParent()->SendMessage(WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(), EN_CHANGE), (LPARAM)m_hWnd);
}

void CModernOptionList::Step (int option, int delta)
{
    const ModernOption& o = m_options[option];
    if (!NumberEnabled(o))
        return;
    int& v = *o.value;
    const int nv = std::max(o.minValue, std::min(o.maxValue, v + delta * std::max(1, o.step)));
    if (nv != v) { v = nv; Changed(); }
}

void CModernOptionList::Activate (int row, EPart part)
{
    if (row < 0)
        return;
    if (m_rows[row].group)
    {
        auto it = std::find(m_collapsed.begin(), m_collapsed.end(), m_rows[row].name);
        if (it != m_collapsed.end()) m_collapsed.erase(it); else m_collapsed.push_back(m_rows[row].name);
        Rebuild();
        return;
    }
    const int option = m_rows[row].option;
    const ModernOption& o = m_options[option];
    switch (part)
    {
    case P_TOGGLE:
        if (o.on) { *o.on = !*o.on; Changed(); }
        break;
    case P_MINUS: Step(option, -1); break;
    case P_PLUS:  Step(option, +1); break;
    case P_VALUE: BeginEdit(row); break;
    default: break;
    }
}

void CModernOptionList::BeginEdit (int row)
{
    const int option = m_rows[row].option;
    if (!NumberEnabled(m_options[option]))
        return;
    EndEdit(true);
    CRect minus, value, plus;
    StepperRects(row, minus, value, plus);
    value.DeflateRect(3, 6);
    m_editOption = option;
    m_edit.Create(WS_CHILD | WS_VISIBLE | ES_CENTER | ES_AUTOHSCROLL, value, this, IDC_VALUE_EDIT);
    m_edit.SetFont(&ModernDraw::UiFont());
    m_edit.SetWindowText(std::to_wstring(*m_options[option].value).c_str());
    m_edit.SetSel(0, -1);
    m_edit.SetFocus();
    Invalidate(FALSE);
}

void CModernOptionList::EndEdit (bool commit)
{
    if (!m_edit.GetSafeHwnd())
        return;
    const int option = m_editOption;
    CString text;
    m_edit.GetWindowText(text);
    m_editOption = -1;
    m_edit.DestroyWindow();
    if (commit && option >= 0)
    {
        const ModernOption& o = m_options[option];
        text.Trim();
        if (!text.IsEmpty())
        {
            const int nv = std::max(o.minValue, std::min(o.maxValue, _wtoi(text)));
            if (nv != *o.value) { *o.value = nv; Changed(); }
        }
    }
    Invalidate(FALSE);
}

BOOL CModernOptionList::PreTranslateMessage (MSG* pMsg)
{
    // Enter confirma e Esc cancela a edicao do numero (sem fechar a janela de configuracao)
    if (m_edit.GetSafeHwnd() && pMsg->hwnd == m_edit.m_hWnd && pMsg->message == WM_KEYDOWN)
    {
        if (pMsg->wParam == VK_RETURN) { EndEdit(true);  SetFocus(); return TRUE; }
        if (pMsg->wParam == VK_ESCAPE) { EndEdit(false); SetFocus(); return TRUE; }
    }
    return CWnd::PreTranslateMessage(pMsg);
}

BOOL CModernOptionList::OnCommand (WPARAM wParam, LPARAM lParam)
{
    if (LOWORD(wParam) == IDC_VALUE_EDIT && HIWORD(wParam) == EN_KILLFOCUS)
    {
        EndEdit(true);
        return TRUE;
    }
    return CWnd::OnCommand(wParam, lParam);
}

HBRUSH CModernOptionList::OnCtlColor (CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    if (pWnd && pWnd->m_hWnd == m_edit.GetSafeHwnd())
    {
        pDC->SetTextColor(Text);
        pDC->SetBkColor(SurfaceAlt);
        return CachedBrush(m_editBrush, m_editBrushColor, SurfaceAlt);
    }
    return CWnd::OnCtlColor(pDC, pWnd, nCtlColor);
}

void CModernOptionList::OnLButtonDown (UINT, CPoint pt)
{
    EndEdit(true);
    SetFocus();
    EPart part;
    const int row = HitRow(pt, part);
    if (row < 0)
        return;
    m_sel = row;
    Invalidate(FALSE);
    if (part != P_ROW)
        Activate(row, part);
}

void CModernOptionList::OnLButtonDblClk (UINT nFlags, CPoint pt)
{
    OnLButtonDown(nFlags, pt);      // conta como mais um clique
}

void CModernOptionList::OnMouseMove (UINT, CPoint pt)
{
    if (!m_tracking)
    {
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, m_hWnd, 0 };
        m_tracking = ::TrackMouseEvent(&tme) != FALSE;
    }
    EPart part;
    const int row = HitRow(pt, part);
    if (row != m_hot || part != m_hotPart)
    {
        m_hot = row; m_hotPart = part;
        Invalidate(FALSE);
    }
}

void CModernOptionList::OnMouseLeave ()
{
    m_tracking = false;
    if (m_hot != -1) { m_hot = -1; m_hotPart = P_NONE; Invalidate(FALSE); }
}

BOOL CModernOptionList::OnSetCursor (CWnd* pWnd, UINT nHitTest, UINT message)
{
    if (nHitTest != HTCLIENT)
        return CWnd::OnSetCursor(pWnd, nHitTest, message);
    CPoint pt;
    ::GetCursorPos(&pt);
    ScreenToClient(&pt);
    EPart part;
    const int row = HitRow(pt, part);
    LPCWSTR cursor = IDC_ARROW;
    if (row >= 0 && part == P_VALUE) cursor = IDC_IBEAM;
    else if (row >= 0 && part != P_ROW) cursor = IDC_HAND;
    ::SetCursor(::LoadCursor(NULL, cursor));
    return TRUE;
}

BOOL CModernOptionList::OnMouseWheel (UINT, short zDelta, CPoint)
{
    EndEdit(true);
    m_scroll -= zDelta / WHEEL_DELTA * ROW_H * 2;
    UpdateScroll();
    Invalidate(FALSE);
    return TRUE;
}

void CModernOptionList::OnVScroll (UINT nSBCode, UINT, CScrollBar*)
{
    EndEdit(true);
    SCROLLINFO si = { sizeof(si), SIF_ALL };
    GetScrollInfo(SB_VERT, &si);
    switch (nSBCode)
    {
    case SB_LINEUP:        m_scroll -= ROW_H; break;
    case SB_LINEDOWN:      m_scroll += ROW_H; break;
    case SB_PAGEUP:        m_scroll -= (int)si.nPage; break;
    case SB_PAGEDOWN:      m_scroll += (int)si.nPage; break;
    case SB_THUMBTRACK:
    case SB_THUMBPOSITION: m_scroll = si.nTrackPos; break;
    case SB_TOP:           m_scroll = 0; break;
    case SB_BOTTOM:        m_scroll = si.nMax; break;
    }
    UpdateScroll();
    Invalidate(FALSE);
}

void CModernOptionList::EnsureVisible (int row)
{
    if (row < 0) return;
    const CRect list = ListRect();
    const int top = RowTop(row), h = RowHeight(row);
    if (top < list.top) m_scroll -= list.top - top;
    else if (top + h > list.bottom) m_scroll += top + h - list.bottom;
    UpdateScroll();
}

void CModernOptionList::OnKeyDown (UINT nChar, UINT nRepCnt, UINT nFlags)
{
    if (m_rows.empty())
        return;
    const bool group = m_sel >= 0 && m_rows[m_sel].group;
    const ModernOption* o = (m_sel >= 0 && !group) ? &m_options[m_rows[m_sel].option] : nullptr;
    switch (nChar)
    {
    case VK_UP:   m_sel = std::max(0, m_sel - 1); EnsureVisible(m_sel); Invalidate(FALSE); return;
    case VK_DOWN: m_sel = std::min((int)m_rows.size() - 1, m_sel + 1); EnsureVisible(m_sel); Invalidate(FALSE); return;
    case VK_HOME: m_sel = 0; EnsureVisible(m_sel); Invalidate(FALSE); return;
    case VK_END:  m_sel = (int)m_rows.size() - 1; EnsureVisible(m_sel); Invalidate(FALSE); return;
    case VK_SPACE:
        if (group) Activate(m_sel, P_GROUP);
        else if (o && o->kind != ModernOption::NUMBER) Activate(m_sel, P_TOGGLE);
        return;
    case VK_F2:
        if (o && o->kind != ModernOption::TOGGLE) BeginEdit(m_sel);
        return;
    case VK_LEFT:
    case VK_RIGHT:
        if (group)
        {
            const bool collapsed = std::find(m_collapsed.begin(), m_collapsed.end(), m_rows[m_sel].name) != m_collapsed.end();
            if (collapsed == (nChar == VK_RIGHT)) Activate(m_sel, P_GROUP);
        }
        else if (o && o->kind != ModernOption::TOGGLE)
            Step(m_rows[m_sel].option, nChar == VK_RIGHT ? +1 : -1);
        else if (o && *o->on != (nChar == VK_RIGHT))
            Activate(m_sel, P_TOGGLE);
        return;
    }
    CWnd::OnKeyDown(nChar, nRepCnt, nFlags);
}

UINT CModernOptionList::OnGetDlgCode () { return DLGC_WANTARROWS | DLGC_WANTCHARS; }
void CModernOptionList::OnSetFocus (CWnd*)  { m_focus = true;  Invalidate(FALSE); }
void CModernOptionList::OnKillFocus (CWnd*) { m_focus = false; Invalidate(FALSE); }
