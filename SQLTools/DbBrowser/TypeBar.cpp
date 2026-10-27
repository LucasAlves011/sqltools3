#include "stdafx.h"
#include "DbBrowser/TypeBar.h"
#include <sstream>

using namespace ModernTheme;
using namespace ModernDraw;

#define TBN_TYPE_RELAYOUT 2

static const wchar_t* const SECTION = L"ObjectsList";

static const int PAD_X  = 10;
static const int TAB_H  = 28;
static const int CHIP_H = 26;
static const int GAP    = 6;
static const int GEAR_W = 28;

///////////////////////////////////////////////////////////////////////////////
// Painel "Tipos exibidos na barra"
///////////////////////////////////////////////////////////////////////////////

class CTypeBarPopup : public CModernPopup
{
public:
    CTypeBarPopup (CTypeBar& bar) : m_bar(bar), m_hot(-1), m_hotKind(0)
    {
        Build();
    }

    CSize GetSize () const { return CSize(WIDTH, m_height); }

    void Refresh () { Invalidate(FALSE); }

protected:
    static const int WIDTH = 500;
    static const int ROW_H = 30;

    struct Row { CRect rc; int type; };

    CTypeBar&        m_bar;
    std::vector<Row> m_rows;
    struct Sec { CRect rc; CString text; };
    std::vector<Sec> m_secs;
    CRect m_rcHeader, m_rcGroup, m_rcReset;
    int   m_height, m_hot, m_hotKind;      // m_hotKind: 1 linha, 2 agrupar, 3 restaurar
    CString m_error;

    void Build ()
    {
        int y = 0;
        m_rcHeader = CRect(0, 0, WIDTH, 38);
        y = 38;
        const int colW = (WIDTH - 16 - 6) / 2;

        for (int cat = 0; cat < CTypeBar::CATEGORY_COUNT; ++cat)
        {
            Sec s;
            s.rc = CRect(0, y, WIDTH, y + 24);
            s.text = CTypeBar::CategoryName(cat);
            s.text.MakeUpper();
            m_secs.push_back(s);
            y += 24;

            int n = 0;
            for (size_t i = 0; i < m_bar.Items().size(); ++i)
            {
                if (m_bar.Items()[i].category != cat)
                    continue;
                int col = n % 2, rowN = n / 2;
                Row r;
                r.type = (int)i;
                r.rc = CRect(8 + col * (colW + 6), y + rowN * ROW_H, 8 + col * (colW + 6) + colW, y + rowN * ROW_H + ROW_H);
                m_rows.push_back(r);
                ++n;
            }
            y += ((n + 1) / 2) * ROW_H + 4;
        }

        m_rcGroup = CRect(14, y + 10, 14 + 200, y + 34);
        m_rcReset = CRect(WIDTH - 14 - 130, y + 8, WIDTH - 14, y + 36);
        m_height = y + 44;
    }

    virtual void Paint (CDC& dc, Gdiplus::Graphics& g, const CRect& rc)
    {
        dc.SetBkMode(TRANSPARENT);

        // cabecalho
        dc.SelectObject(&UiFontBold());
        dc.SetTextColor(Text);
        dc.DrawText(L"Tipos exibidos na barra", CRect(14, 0, WIDTH - 140, m_rcHeader.bottom), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        CString cnt;
        cnt.Format(L"%d de %d visíveis", m_bar.VisibleCount(), (int)m_bar.Items().size());
        dc.SelectObject(&UiFont());
        dc.SetTextColor(TextMuted);
        dc.DrawText(cnt, CRect(WIDTH - 200, 0, WIDTH - 14, m_rcHeader.bottom), DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

        // secoes
        dc.SelectObject(&UiFont());
        dc.SetTextColor(TextMuted);
        for (size_t i = 0; i < m_secs.size(); ++i)
        {
            CRect r = m_secs[i].rc;
            r.left += 14;
            dc.DrawText(m_secs[i].text, r, DT_LEFT | DT_BOTTOM | DT_SINGLELINE);
        }

        // linhas
        for (size_t i = 0; i < m_rows.size(); ++i)
        {
            const Row& row = m_rows[i];
            bool on = m_bar.IsTypeVisible(row.type);
            bool hot = (m_hotKind == 1 && m_hot == (int)i);
            if (hot)
                FillRoundRect(g, row.rc, 6.0f, AccentTint);

            DrawTypeIcon(dc, m_bar.Items()[row.type].imageIndex, row.rc.left + 8, row.rc.CenterPoint().y - 8, false, !on);
            dc.SetTextColor(on ? Text : TextMuted);
            dc.DrawText(m_bar.Items()[row.type].title, CRect(row.rc.left + 32, row.rc.top, row.rc.right - 46, row.rc.bottom), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            DrawSwitch(g, CRect(row.rc.right - 40, row.rc.CenterPoint().y - 8, row.rc.right - 12, row.rc.CenterPoint().y + 8), on);
        }

        // rodape
        dc.FillSolidRect(0, m_rcGroup.top - 12, WIDTH, 1, Border);
        DrawSwitch(g, CRect(m_rcGroup.left, m_rcGroup.CenterPoint().y - 8, m_rcGroup.left + 28, m_rcGroup.CenterPoint().y + 8), m_bar.GroupByCategory());
        dc.SetTextColor(Text);
        dc.DrawText(L"Agrupar por categoria", CRect(m_rcGroup.left + 36, m_rcGroup.top, m_rcGroup.right + 60, m_rcGroup.bottom), DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        if (!m_error.IsEmpty())
        {
            dc.SetTextColor(Danger);
            dc.DrawText(m_error, CRect(m_rcGroup.right + 70, m_rcGroup.top, m_rcReset.left - 8, m_rcGroup.bottom), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }

        bool hotReset = (m_hotKind == 3);
        if (hotReset)
            FillRoundRect(g, m_rcReset, 8.0f, AccentTint);
        StrokeRoundRect(g, m_rcReset, 8.0f, hotReset ? Accent : BorderStrong);
        dc.SetTextColor(Text);
        dc.DrawText(L"Restaurar padrão", m_rcReset, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    static void DrawSwitch (Gdiplus::Graphics& g, const CRect& rc, bool on)
    {
        FillRoundRect(g, rc, rc.Height() / 2.0f, on ? Accent : DimStrong);
        int d = rc.Height() - 4;
        int x = on ? rc.right - 2 - d : rc.left + 2;
        Gdiplus::SolidBrush white(Gdiplus::Color(255, 255, 255, 255));
        g.FillEllipse(&white, x, rc.top + 2, d, d);
    }

    virtual void OnPopupClosed ()
    {
        m_bar.OnPopupClosed();
    }

    DECLARE_MESSAGE_MAP()
    afx_msg void OnMouseMove (UINT nFlags, CPoint point)
    {
        int kind = 0, hot = -1;
        for (size_t i = 0; i < m_rows.size(); ++i)
            if (m_rows[i].rc.PtInRect(point)) { kind = 1; hot = (int)i; break; }
        if (!kind && m_rcGroup.PtInRect(point)) kind = 2;
        if (!kind && m_rcReset.PtInRect(point)) kind = 3;
        if (kind != m_hotKind || hot != m_hot)
        {
            m_hotKind = kind; m_hot = hot;
            Invalidate(FALSE);
        }
        CWnd::OnMouseMove(nFlags, point);
    }

    afx_msg void OnLButtonDown (UINT nFlags, CPoint point)
    {
        m_error.Empty();
        for (size_t i = 0; i < m_rows.size(); ++i)
        {
            if (m_rows[i].rc.PtInRect(point))
            {
                int type = m_rows[i].type;
                if (!m_bar.SetTypeVisible(type, !m_bar.IsTypeVisible(type)))
                    m_error = L"Mantenha ao menos um tipo.";
                Invalidate(FALSE);
                return;
            }
        }
        if (m_rcGroup.PtInRect(point))
        {
            m_bar.SetGroupByCategory(!m_bar.GroupByCategory());
            Invalidate(FALSE);
            return;
        }
        if (m_rcReset.PtInRect(point))
        {
            m_bar.ResetConfig();
            Invalidate(FALSE);
            return;
        }
        CWnd::OnLButtonDown(nFlags, point);
    }
};

BEGIN_MESSAGE_MAP(CTypeBarPopup, CModernPopup)
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONDOWN()
END_MESSAGE_MAP()

///////////////////////////////////////////////////////////////////////////////
// CTypeBar
///////////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CTypeBar, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_SIZE()
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSELEAVE()
    ON_WM_LBUTTONDOWN()
    ON_WM_KEYDOWN()
    ON_WM_GETDLGCODE()
    ON_WM_SETFOCUS()
    ON_WM_KILLFOCUS()
    ON_WM_DESTROY()
END_MESSAGE_MAP()

const wchar_t* CTypeBar::CategoryName (int category)
{
    static const wchar_t* names[CATEGORY_COUNT] = { L"Tabelas", L"Código", L"Objetos", L"Administração" };
    return (category >= 0 && category < CATEGORY_COUNT) ? names[category] : L"";
}

// ordem das listas em CDbSourceWnd::OnInitDialog
int CTypeBar::CategoryOfType (int t)
{
    if (t <= 5)  return 0;   // Tables, PKs, UKs, FKs, CHKs, Indexes
    if (t <= 13) return 1;   // Triggers .. Java
    if (t <= 18) return 2;   // Sequences, Views, Synonyms, DB Links, Clusters
    return 3;                // Grantee, Recyclebin, Invalid Objects
}

CTypeBar::CTypeBar ()
    : m_sel(0), m_cat(0), m_height(40), m_layoutWidth(0), m_grouped(true), m_trackingMouse(false),
      m_popup(NULL), m_popupClosedAt(0)
{
    m_rcGear.SetRectEmpty();
    m_hover.kind = HIT_NONE;
    m_hover.id = -1;
}

CTypeBar::~CTypeBar ()
{
}

BOOL CTypeBar::Create (CWnd* pParent, UINT nID)
{
    HINSTANCE hInst = AfxGetInstanceHandle();
    WNDCLASS wc;
    if (!::GetClassInfo(hInst, L"SQLTools_TypeBar", &wc))
    {
        memset(&wc, 0, sizeof(wc));
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = ::DefWindowProc;
        wc.hInstance = hInst;
        wc.hCursor = ::LoadCursor(NULL, IDC_ARROW);
        wc.lpszClassName = L"SQLTools_TypeBar";
        AfxRegisterClass(&wc);
    }
    return CWnd::Create(L"SQLTools_TypeBar", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS,
                        CRect(0, 0, 0, 0), pParent, nID);
}

void CTypeBar::OnDestroy ()
{
    if (m_popup)
        m_popup->Close();
    CWnd::OnDestroy();
}

void CTypeBar::OnPopupClosed ()
{
    m_popup = NULL;
    m_popupClosedAt = ::GetTickCount();
    Invalidate(FALSE);
}

void CTypeBar::SetItems (const std::vector<Item>& items)
{
    m_items = items;
    m_hidden.assign(items.size(), false);
    LoadConfig();
    if (m_sel >= (int)m_items.size()) m_sel = 0;
    Layout(m_layoutWidth);
}

void CTypeBar::LoadConfig ()
{
    CWinApp* app = AfxGetApp();
    m_grouped = app->GetProfileInt(SECTION, L"GroupTypes", 1) != 0;

    CString hidden = app->GetProfileString(SECTION, L"HiddenTypes", L"");
    int pos = 0;
    CString tok = hidden.Tokenize(L",", pos);
    while (!tok.IsEmpty())
    {
        int idx = _wtoi(tok);
        if (idx >= 0 && idx < (int)m_hidden.size())
            m_hidden[idx] = true;
        tok = hidden.Tokenize(L",", pos);
    }
    if (VisibleCount() == 0)
        m_hidden.assign(m_hidden.size(), false);
}

void CTypeBar::SaveConfig () const
{
    CString hidden;
    for (size_t i = 0; i < m_hidden.size(); ++i)
    {
        if (m_hidden[i])
        {
            CString s;
            s.Format(L"%s%d", hidden.IsEmpty() ? L"" : L",", (int)i);
            hidden += s;
        }
    }
    CWinApp* app = AfxGetApp();
    app->WriteProfileString(SECTION, L"HiddenTypes", hidden);
    app->WriteProfileInt(SECTION, L"GroupTypes", m_grouped ? 1 : 0);
}

bool CTypeBar::IsTypeVisible (int index) const
{
    return index >= 0 && index < (int)m_hidden.size() && !m_hidden[index];
}

int CTypeBar::VisibleCount () const
{
    int n = 0;
    for (size_t i = 0; i < m_hidden.size(); ++i)
        if (!m_hidden[i]) ++n;
    return n;
}

bool CTypeBar::SetTypeVisible (int index, bool visible)
{
    if (index < 0 || index >= (int)m_hidden.size())
        return false;
    if (!visible && VisibleCount() <= 1 && !m_hidden[index])
        return false;

    m_hidden[index] = !visible;
    SaveConfig();

    // se o tipo selecionado foi escondido, passa para o primeiro visivel
    bool selChanged = false;
    if (m_hidden[m_sel])
    {
        for (size_t i = 0; i < m_items.size(); ++i)
            if (!m_hidden[i]) { m_sel = (int)i; selChanged = true; break; }
        m_cat = m_items[m_sel].category;
    }
    Reflow();
    if (selChanged)
        Notify();
    return true;
}

void CTypeBar::SetGroupByCategory (bool grouped)
{
    m_grouped = grouped;
    SaveConfig();
    Reflow();
}

void CTypeBar::ResetConfig ()
{
    m_hidden.assign(m_hidden.size(), false);
    m_grouped = true;
    SaveConfig();
    Reflow();
}

void CTypeBar::Reflow ()
{
    CRect rc;
    GetClientRect(rc);
    Layout(rc.Width());
    Invalidate(FALSE);
    if (GetParent())
        GetParent()->SendMessage(WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(), TBN_TYPE_RELAYOUT), (LPARAM)m_hWnd);
}

void CTypeBar::Notify ()
{
    if (GetParent())
        GetParent()->SendMessage(WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(), TBN_TYPE_SELCHANGE), (LPARAM)m_hWnd);
}

void CTypeBar::SetSel (int index)
{
    if (index < 0 || index >= (int)m_items.size())
        return;
    m_sel = index;
    m_cat = m_items[index].category;
    Layout(m_layoutWidth);
    if (m_hWnd)
        Invalidate(FALSE);
}

bool CTypeBar::ShowTabs () const
{
    if (!m_grouped)
        return false;
    int cats = 0;
    for (int c = 0; c < CATEGORY_COUNT; ++c)
        for (size_t i = 0; i < m_items.size(); ++i)
            if (m_items[i].category == c && !m_hidden[i]) { ++cats; break; }
    return cats > 1;
}

int CTypeBar::CalcHeight (int width)
{
    Layout(width);
    return m_height;
}

void CTypeBar::Layout (int width)
{
    m_tabs.clear();
    m_chips.clear();
    if (width <= 0 || m_items.empty() || !m_hWnd)
    {
        m_layoutWidth = width;
        return;
    }
    m_layoutWidth = width;

    CClientDC dc(this);
    CFont* pOld = dc.SelectObject(&UiFont());

    bool tabs = ShowTabs();
    int y = 8;

    if (tabs)
    {
        // categoria atual precisa ter ao menos um tipo visivel
        bool ok = false;
        for (size_t i = 0; i < m_items.size(); ++i)
            if (m_items[i].category == m_cat && !m_hidden[i]) { ok = true; break; }
        if (!ok)
            for (size_t i = 0; i < m_items.size(); ++i)
                if (!m_hidden[i]) { m_cat = m_items[i].category; break; }

        int x = PAD_X;
        for (int c = 0; c < CATEGORY_COUNT; ++c)
        {
            bool any = false;
            for (size_t i = 0; i < m_items.size(); ++i)
                if (m_items[i].category == c && !m_hidden[i]) { any = true; break; }
            if (!any)
                continue;
            CString name = CategoryName(c);
            int w = dc.GetTextExtent(name).cx + 26;
            Cell cell;
            cell.rc = CRect(x, 0, x + w, TAB_H);
            cell.id = c;
            m_tabs.push_back(cell);
            x += w + 2;
        }
        y = TAB_H + 8;
    }

    int chipsTop = y;
    int avail = width - PAD_X * 2 - GEAR_W - GAP;
    int x = PAD_X, row = 0;
    for (size_t i = 0; i < m_items.size(); ++i)
    {
        if (m_hidden[i] || (tabs && m_items[i].category != m_cat))
            continue;
        int w = 11 + 16 + 6 + dc.GetTextExtent(m_items[i].title).cx + 12;
        if (x > PAD_X && x + w > PAD_X + avail)
        {
            x = PAD_X;
            ++row;
        }
        Cell cell;
        cell.rc = CRect(x, chipsTop + row * (CHIP_H + GAP), x + w, chipsTop + row * (CHIP_H + GAP) + CHIP_H);
        cell.id = (int)i;
        m_chips.push_back(cell);
        x += w + GAP;
    }
    int rows = row + 1;

    m_rcGear = CRect(width - PAD_X - GEAR_W, chipsTop - 1, width - PAD_X, chipsTop - 1 + GEAR_W);
    m_height = chipsTop + rows * CHIP_H + (rows - 1) * GAP + 8;
    if (m_height < chipsTop + GEAR_W + 6)
        m_height = chipsTop + GEAR_W + 6;

    dc.SelectObject(pOld);
}

CTypeBar::Hit CTypeBar::HitTest (CPoint pt) const
{
    Hit h;
    h.kind = HIT_NONE;
    h.id = -1;
    if (m_rcGear.PtInRect(pt)) { h.kind = HIT_GEAR; return h; }
    for (size_t i = 0; i < m_chips.size(); ++i)
        if (m_chips[i].rc.PtInRect(pt)) { h.kind = HIT_TYPE; h.id = m_chips[i].id; return h; }
    for (size_t i = 0; i < m_tabs.size(); ++i)
        if (m_tabs[i].rc.PtInRect(pt)) { h.kind = HIT_CATEGORY; h.id = m_tabs[i].id; return h; }
    return h;
}

BOOL CTypeBar::OnEraseBkgnd (CDC*)
{
    return TRUE;
}

void CTypeBar::OnSize (UINT nType, int cx, int cy)
{
    CWnd::OnSize(nType, cx, cy);
    Layout(cx);
    Invalidate(FALSE);
}

void CTypeBar::OnPaint ()
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

    bool enabled = IsWindowEnabled() != FALSE;
    bool focused = (::GetFocus() == m_hWnd);
    Gdiplus::Graphics g(mem.GetSafeHdc());
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    mem.SetBkMode(TRANSPARENT);
    CFont* pOldFont = mem.SelectObject(&UiFont());

    // abas de categoria
    if (!m_tabs.empty())
    {
        mem.FillSolidRect(0, TAB_H - 1, rc.Width(), 1, Border);
        for (size_t i = 0; i < m_tabs.size(); ++i)
        {
            const Cell& c = m_tabs[i];
            bool active = (c.id == m_cat);
            bool hot = enabled && m_hover.kind == HIT_CATEGORY && m_hover.id == c.id;
            mem.SelectObject(active ? &UiFontBold() : &UiFont());
            mem.SetTextColor(!enabled ? TextMuted : (active || hot) ? Text : TextMuted);
            CRect rtab(c.rc);
            mem.DrawText(CategoryName(c.id), rtab, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            if (active)
                mem.FillSolidRect(c.rc.left + 6, TAB_H - 2, c.rc.Width() - 12, 2, Accent);
        }
    }

    // botoes de tipo
    for (size_t i = 0; i < m_chips.size(); ++i)
    {
        const Cell& c = m_chips[i];
        bool sel = (c.id == m_sel);
        bool hot = enabled && m_hover.kind == HIT_TYPE && m_hover.id == c.id;

        COLORREF fill = sel ? AccentTint : hot ? AccentTint : SurfaceAlt;
        FillRoundRect(g, c.rc, c.rc.Height() / 2.0f, fill);
        if (sel)
            StrokeRoundRect(g, c.rc, c.rc.Height() / 2.0f, AccentStrong);
        if (sel && focused)
            StrokeRoundRect(g, CRect(c.rc.left - 1, c.rc.top - 1, c.rc.right + 1, c.rc.bottom + 1), c.rc.Height() / 2.0f + 1, Accent);

        DrawTypeIcon(mem, m_items[c.id].imageIndex, c.rc.left + 9, c.rc.CenterPoint().y - 8, sel, !enabled);
        mem.SelectObject(sel ? &UiFontBold() : &UiFont());
        mem.SetTextColor(!enabled ? TextMuted : sel ? Accent : Text);
        CRect rt(c.rc.left + 11 + 16 + 6, c.rc.top, c.rc.right - 8, c.rc.bottom);
        mem.DrawText(m_items[c.id].title, rt, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    // botao de personalizar
    {
        bool hot = enabled && (m_hover.kind == HIT_GEAR || m_popup != NULL);
        if (hot)
            FillRoundRect(g, m_rcGear, 6.0f, AccentTint);
        DrawUiIcon(mem, UI_ADJUST, m_rcGear.CenterPoint().x - 8, m_rcGear.CenterPoint().y - 8, hot, !enabled);
    }

    mem.SelectObject(pOldFont);
    dc.BitBlt(0, 0, rc.Width(), rc.Height(), &mem, 0, 0, SRCCOPY);
    mem.SelectObject(pOldBmp);
}

void CTypeBar::OnMouseMove (UINT nFlags, CPoint point)
{
    if (!m_trackingMouse)
    {
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, m_hWnd, 0 };
        _TrackMouseEvent(&tme);
        m_trackingMouse = true;
    }
    Hit h = HitTest(point);
    if (h.kind != m_hover.kind || h.id != m_hover.id)
    {
        m_hover = h;
        Invalidate(FALSE);
    }
    CWnd::OnMouseMove(nFlags, point);
}

void CTypeBar::OnMouseLeave ()
{
    m_trackingMouse = false;
    m_hover.kind = HIT_NONE;
    m_hover.id = -1;
    Invalidate(FALSE);
}

void CTypeBar::OpenConfigPopup ()
{
    if (m_popup)
    {
        m_popup->Close();
        return;
    }
    // o clique que fechou o painel (por perda de foco) nao deve reabri-lo
    if (::GetTickCount() - m_popupClosedAt < 250)
        return;

    CTypeBarPopup* popup = new CTypeBarPopup(*this);
    CRect gear(m_rcGear);
    ClientToScreen(&gear);
    CSize sz = popup->GetSize();
    CPoint pos(gear.right - sz.cx, gear.bottom + 4);
    m_popup = popup;
    if (!popup->CreatePopup(this, pos, sz))
    {
        m_popup = NULL;
        delete popup;
        return;
    }
    Invalidate(FALSE);
}

void CTypeBar::OnLButtonDown (UINT nFlags, CPoint point)
{
    if (!IsWindowEnabled())
        return;

    SetFocus();
    Hit h = HitTest(point);
    switch (h.kind)
    {
    case HIT_CATEGORY:
        if (h.id != m_cat)
        {
            m_cat = h.id;
            Reflow();
        }
        break;
    case HIT_TYPE:
        if (h.id != m_sel)
        {
            m_sel = h.id;
            Invalidate(FALSE);
            Notify();
        }
        break;
    case HIT_GEAR:
        OpenConfigPopup();
        break;
    default:
        break;
    }
    CWnd::OnLButtonDown(nFlags, point);
}

void CTypeBar::MoveSel (int delta)
{
    int n = (int)m_items.size();
    for (int i = m_sel + delta; i >= 0 && i < n; i += delta)
    {
        if (!m_hidden[i])
        {
            bool catChanged = (m_items[i].category != m_cat);
            m_sel = i;
            m_cat = m_items[i].category;
            if (catChanged)
                Reflow();
            else
                Invalidate(FALSE);
            Notify();
            return;
        }
    }
}

void CTypeBar::OnKeyDown (UINT nChar, UINT nRepCnt, UINT nFlags)
{
    switch (nChar)
    {
    case VK_LEFT:
    case VK_UP:    MoveSel(-1); return;
    case VK_RIGHT:
    case VK_DOWN:  MoveSel(+1); return;
    }
    CWnd::OnKeyDown(nChar, nRepCnt, nFlags);
}

UINT CTypeBar::OnGetDlgCode ()
{
    return DLGC_WANTARROWS;
}

void CTypeBar::OnSetFocus (CWnd* pOld)
{
    CWnd::OnSetFocus(pOld);
    Invalidate(FALSE);
}

void CTypeBar::OnKillFocus (CWnd* pNew)
{
    CWnd::OnKillFocus(pNew);
    Invalidate(FALSE);
}
