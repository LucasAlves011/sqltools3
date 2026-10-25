#include "stdafx.h"
#include "DbBrowser/OwnerBar.h"
#include <algorithm>

using namespace ModernTheme;
using namespace ModernDraw;

static const wchar_t* const SECTION = L"ObjectsList";
static const int CTRL_H   = 32;
static const int PILL_H   = 28;
static const int CHIP_H   = 26;
static const int GAP      = 6;

///////////////////////////////////////////////////////////////////////////////
// Painel de owners
///////////////////////////////////////////////////////////////////////////////

#define IDC_OWNER_SEARCH 3001

class COwnerPopup : public CModernPopup
{
public:
    COwnerPopup (COwnerBar& bar, const std::vector<CString>& owners, const std::list<std::wstring>& recents, bool loading)
        : m_bar(bar), m_owners(owners), m_loading(loading), m_scroll(0), m_contentH(0),
          m_hot(-1), m_hl(-1), m_dragName(), m_dragging(false), m_listH(200)
    {
        for (std::list<std::wstring>::const_iterator it = recents.begin(); it != recents.end(); ++it)
            m_recents.insert(m_recents.begin(), CString(it->c_str()));   // mais recente primeiro
        m_rcSearch = CRect(10, 10, WIDTH - 10, 42);
        RebuildRows();
    }

    CSize GetSize () const { return CSize(WIDTH, TOP + m_listH + FOOT); }

    void SetOwners (const std::vector<CString>& owners, bool loading)
    {
        m_owners = owners;
        m_loading = loading;
        if (m_hWnd) { RebuildRows(); Invalidate(FALSE); }
    }

protected:
    static const int WIDTH = 330;
    static const int TOP   = 50;
    static const int FOOT  = 30;
    static const int ROW_H = 30;
    static const int HEAD_H = 26;

    struct Row { int kind; CString name; CRect rc; };    // kind: 0 titulo, 1 favorito, 2 owner

    COwnerBar&           m_bar;
    std::vector<CString> m_owners, m_recents;
    bool                 m_loading;
    std::vector<Row>     m_rows;
    int                  m_scroll, m_contentH, m_hot, m_hl, m_listH;
    CString              m_dragName;
    bool                 m_dragging;
    CRect                m_rcSearch;
    CEdit                m_edit;

    CRect ListRect () const { return CRect(0, TOP, WIDTH, TOP + m_listH); }

    void RebuildRows ()
    {
        CString q;
        if (m_edit.GetSafeHwnd())
            m_edit.GetWindowText(q);
        q.Trim();
        q.MakeUpper();

        m_rows.clear();
        int y = 0;

        std::vector<CString> favs, recs, all;
        for (size_t i = 0; i < m_bar.Favorites().size(); ++i)
        {
            CString u = m_bar.Favorites()[i];
            CString up = u; up.MakeUpper();
            if (q.IsEmpty() || up.Find(q) >= 0) favs.push_back(u);
        }
        for (size_t i = 0; i < m_recents.size(); ++i)
        {
            CString u = m_recents[i];
            CString up = u; up.MakeUpper();
            if (!m_bar.IsFavorite(u) && (q.IsEmpty() || up.Find(q) >= 0)) recs.push_back(u);
        }
        for (size_t i = 0; i < m_owners.size(); ++i)
        {
            CString u = m_owners[i];
            CString up = u; up.MakeUpper();
            if (q.IsEmpty() || up.Find(q) >= 0) all.push_back(u);
        }

        if (!favs.empty())
        {
            AddRow(y, 0, L"FAVORITOS");
            for (size_t i = 0; i < favs.size(); ++i) AddRow(y, 1, favs[i]);
        }
        if (!recs.empty())
        {
            AddRow(y, 0, L"RECENTES");
            for (size_t i = 0; i < recs.size(); ++i) AddRow(y, 2, recs[i]);
        }
        CString head;
        head.Format(L"TODOS (%d)", (int)all.size());
        AddRow(y, 0, head);
        for (size_t i = 0; i < all.size(); ++i) AddRow(y, 2, all[i]);
        if (all.empty())
            AddRow(y, 0, m_loading ? L"Carregando owners..." : L"Nenhum owner encontrado");

        m_contentH = y;
        m_listH = (std::max)(120, (std::min)(m_contentH + 4, 360));
        m_scroll = (std::max)(0, (std::min)(m_scroll, m_contentH - m_listH));
        m_hot = -1;
        m_hl = -1;
        if (!q.IsEmpty())
            for (size_t i = 0; i < m_rows.size(); ++i)
                if (m_rows[i].kind != 0) { m_hl = (int)i; break; }

        if (m_hWnd)
        {
            CRect r;
            GetWindowRect(r);
            SetWindowPos(NULL, 0, 0, WIDTH, TOP + m_listH + FOOT, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }

    void AddRow (int& y, int kind, const CString& text)
    {
        Row r;
        r.kind = kind;
        r.name = text;
        int h = (kind == 0) ? HEAD_H : ROW_H;
        r.rc = CRect(0, y, WIDTH, y + h);
        m_rows.push_back(r);
        y += h;
    }

    int RowAt (CPoint pt) const
    {
        CRect lr = ListRect();
        if (!lr.PtInRect(pt))
            return -1;
        int cy = pt.y - lr.top + m_scroll;
        for (size_t i = 0; i < m_rows.size(); ++i)
            if (cy >= m_rows[i].rc.top && cy < m_rows[i].rc.bottom)
                return (int)i;
        return -1;
    }

    void EnsureVisible (int idx)
    {
        if (idx < 0 || idx >= (int)m_rows.size()) return;
        const CRect& r = m_rows[idx].rc;
        if (r.top < m_scroll) m_scroll = r.top;
        else if (r.bottom > m_scroll + m_listH) m_scroll = r.bottom - m_listH;
    }

    void MoveHighlight (int delta)
    {
        int n = (int)m_rows.size();
        for (int i = m_hl + delta; i >= 0 && i < n; i += delta)
        {
            if (m_rows[i].kind != 0)
            {
                m_hl = i;
                EnsureVisible(i);
                Invalidate(FALSE);
                return;
            }
        }
    }

    void Pick (const CString& name)
    {
        CString copy = name;
        COwnerBar& bar = m_bar;
        Close();                        // o painel se destroi: nao usar membros depois
        bar.PickFromPopup(copy);
    }

    virtual void OnPopupCreated ()
    {
        m_edit.Create(WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, CRect(36, 17, WIDTH - 20, 37), this, IDC_OWNER_SEARCH);
        m_edit.SetFont(&UiFont());
        m_edit.SetCueBanner(L"Buscar owner...", TRUE);
        RebuildRows();
    }

    virtual void OnPopupShown ()
    {
        m_edit.SetFocus();
    }

    virtual void OnPopupClosed ()
    {
        m_bar.OnPopupClosed();
    }

    virtual BOOL PreTranslateMessage (MSG* pMsg)
    {
        if (pMsg->message == WM_KEYDOWN && m_edit.GetSafeHwnd() && pMsg->hwnd == m_edit.GetSafeHwnd())
        {
            switch (pMsg->wParam)
            {
            case VK_DOWN:   MoveHighlight(+1); return TRUE;
            case VK_UP:     MoveHighlight(-1); return TRUE;
            case VK_ESCAPE: Close();           return TRUE;
            case VK_RETURN:
                if (m_hl >= 0 && m_hl < (int)m_rows.size() && m_rows[m_hl].kind != 0)
                    Pick(m_rows[m_hl].name);
                return TRUE;
            }
        }
        return CModernPopup::PreTranslateMessage(pMsg);
    }

    virtual void Paint (CDC& dc, Gdiplus::Graphics& g, const CRect& rc)
    {
        dc.SetBkMode(TRANSPARENT);

        // caixa de busca
        FillRoundRect(g, m_rcSearch, 8.0f, Surface);
        StrokeRoundRect(g, m_rcSearch, 8.0f, BorderStrong);
        DrawUiIcon(dc, UI_SEARCH, m_rcSearch.left + 9, m_rcSearch.CenterPoint().y - 8, false);

        // lista (com recorte)
        CRect lr = ListRect();
        int saved = dc.SaveDC();
        dc.IntersectClipRect(lr);
        dc.SelectObject(&UiFont());

        for (size_t i = 0; i < m_rows.size(); ++i)
        {
            const Row& row = m_rows[i];
            CRect r = row.rc;
            r.OffsetRect(0, lr.top - m_scroll);
            if (r.bottom < lr.top || r.top > lr.bottom)
                continue;

            if (row.kind == 0)
            {
                dc.SetTextColor(TextMuted);
                CRect rt(r); rt.left += 14; rt.top += 6;
                dc.DrawText(row.name, rt, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
                continue;
            }

            bool hot = ((int)i == m_hot) || ((int)i == m_hl) || (m_dragging && row.name == m_dragName);
            bool cur = (row.name.CompareNoCase(m_bar.GetCurrent()) == 0);
            bool fav = m_bar.IsFavorite(row.name);
            if (hot)
                FillRoundRect(g, CRect(r.left + 6, r.top + 1, r.right - 6, r.bottom - 1), 6.0f, AccentTint);

            // estrela
            dc.SelectObject(&UiFont());
            dc.SetTextColor(fav ? Favorite : RGB(195, 200, 208));
            dc.DrawText(fav ? L"\x2605" : L"\x2606", CRect(r.left + 12, r.top, r.left + 34, r.bottom), DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            // nome
            dc.SelectObject(cur ? &UiFontBold() : &UiFont());
            dc.SetTextColor(cur ? Accent : Text);
            dc.DrawText(row.name, CRect(r.left + 42, r.top, r.right - 44, r.bottom), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

            // alca de arrastar (so favoritos)
            if (row.kind == 1)
            {
                Gdiplus::SolidBrush dot(Clr(RGB(190, 196, 205)));
                float cx = (float)r.right - 24.0f, cy = (float)r.CenterPoint().y;
                for (int a = 0; a < 2; ++a)
                    for (int b = -1; b <= 1; ++b)
                        g.FillEllipse(&dot, cx + a * 5.0f - 1.2f, cy + b * 5.0f - 1.2f, 2.4f, 2.4f);
            }
        }
        dc.RestoreDC(saved);

        // indicador de rolagem
        if (m_contentH > m_listH)
        {
            float ratio = (float)m_listH / (float)m_contentH;
            int h = (std::max)(24, (int)(m_listH * ratio));
            int y = lr.top + (int)((float)(m_listH - h) * (float)m_scroll / (float)(m_contentH - m_listH));
            FillRoundRect(g, CRect(WIDTH - 8, y + 2, WIDTH - 4, y + h - 2), 2.0f, RGB(200, 205, 213));
        }

        // rodape
        CRect foot(0, TOP + m_listH, WIDTH, TOP + m_listH + FOOT);
        dc.FillSolidRect(foot, SurfaceAlt);
        dc.FillSolidRect(foot.left, foot.top, foot.Width(), 1, Border);
        dc.SelectObject(&UiFont());
        dc.SetTextColor(TextMuted);
        CRect ft(foot); ft.left += 14; ft.right -= 8;
        dc.DrawText(m_loading ? L"Carregando owners..." : L"Digite para filtrar \x00B7 estrela favorita \x00B7 arraste para ordenar",
                    ft, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    }

    DECLARE_MESSAGE_MAP()

    afx_msg void OnSearchChange ()
    {
        m_scroll = 0;
        RebuildRows();
        Invalidate(FALSE);
    }

    afx_msg HBRUSH OnCtlColor (CDC* pDC, CWnd* pWnd, UINT nCtlColor)
    {
        if (nCtlColor == CTLCOLOR_EDIT)
        {
            static CBrush s_brush(Surface);
            pDC->SetBkColor(Surface);
            pDC->SetTextColor(Text);
            return (HBRUSH)s_brush.GetSafeHandle();
        }
        return CModernPopup::OnCtlColor(pDC, pWnd, nCtlColor);
    }

    afx_msg void OnMouseMove (UINT nFlags, CPoint point)
    {
        if (m_dragging)
        {
            // reordena os favoritos enquanto arrasta
            int favFirst = -1, favLast = -1;
            for (size_t i = 0; i < m_rows.size(); ++i)
                if (m_rows[i].kind == 1) { if (favFirst < 0) favFirst = (int)i; favLast = (int)i; }
            if (favFirst >= 0)
            {
                CRect lr = ListRect();
                int cy = point.y - lr.top + m_scroll;
                int target = favFirst;
                for (int i = favFirst; i <= favLast; ++i)
                    if (cy >= m_rows[i].rc.top) target = i;
                int from = -1;
                for (size_t k = 0; k < m_bar.Favorites().size(); ++k)
                    if (m_bar.Favorites()[k].CompareNoCase(m_dragName) == 0) from = (int)k;
                int to = target - favFirst;
                if (from >= 0 && to != from)
                {
                    m_bar.MoveFavorite(from, to);
                    RebuildRows();
                    Invalidate(FALSE);
                }
            }
            return;
        }

        int hot = RowAt(point);
        if (hot >= 0 && m_rows[hot].kind == 0) hot = -1;
        if (hot != m_hot)
        {
            m_hot = hot;
            m_hl = -1;
            Invalidate(FALSE);
        }
        CWnd::OnMouseMove(nFlags, point);
    }

    afx_msg void OnLButtonDown (UINT nFlags, CPoint point)
    {
        int i = RowAt(point);
        if (i >= 0 && m_rows[i].kind != 0)
        {
            const Row row = m_rows[i];
            if (row.kind == 1 && point.x > WIDTH - 44)
            {
                m_dragging = true;
                m_dragName = row.name;
                SetCapture();
                return;
            }
            if (point.x < 40)
            {
                m_bar.ToggleFavorite(row.name);
                RebuildRows();
                Invalidate(FALSE);
                return;
            }
            Pick(row.name);
            return;
        }
        CWnd::OnLButtonDown(nFlags, point);
    }

    afx_msg void OnLButtonUp (UINT nFlags, CPoint point)
    {
        if (m_dragging)
        {
            m_dragging = false;
            ::ReleaseCapture();
            Invalidate(FALSE);
            return;
        }
        CWnd::OnLButtonUp(nFlags, point);
    }

    afx_msg BOOL OnMouseWheel (UINT nFlags, short zDelta, CPoint pt)
    {
        if (m_contentH > m_listH)
        {
            m_scroll = (std::max)(0, (std::min)(m_scroll - zDelta / 3, m_contentH - m_listH));
            Invalidate(FALSE);
        }
        return TRUE;
    }
};

BEGIN_MESSAGE_MAP(COwnerPopup, CModernPopup)
    ON_EN_CHANGE(IDC_OWNER_SEARCH, OnSearchChange)
    ON_WM_CTLCOLOR()
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_MOUSEWHEEL()
END_MESSAGE_MAP()

///////////////////////////////////////////////////////////////////////////////
// COwnerBar
///////////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(COwnerBar, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_SIZE()
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSELEAVE()
    ON_WM_LBUTTONDOWN()
    ON_WM_DESTROY()
END_MESSAGE_MAP()

COwnerBar::COwnerBar ()
    : m_trackingMouse(false), m_layoutWidth(0), m_popup(NULL), m_popupClosedAt(0)
{
    m_rcPill.SetRectEmpty();
    m_rcPlus.SetRectEmpty();
    m_hover.kind = HIT_NONE;
    m_hover.id = -1;
}

COwnerBar::~COwnerBar ()
{
}

BOOL COwnerBar::Create (CWnd* pParent, UINT nID)
{
    HINSTANCE hInst = AfxGetInstanceHandle();
    WNDCLASS wc;
    if (!::GetClassInfo(hInst, L"SQLTools_OwnerBar", &wc))
    {
        memset(&wc, 0, sizeof(wc));
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = ::DefWindowProc;
        wc.hInstance = hInst;
        wc.hCursor = ::LoadCursor(NULL, IDC_ARROW);
        wc.lpszClassName = L"SQLTools_OwnerBar";
        AfxRegisterClass(&wc);
    }
    if (!CWnd::Create(L"SQLTools_OwnerBar", L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, CRect(0, 0, 0, 0), pParent, nID))
        return FALSE;
    LoadFavorites();
    return TRUE;
}

void COwnerBar::OnDestroy ()
{
    if (m_popup)
        m_popup->Close();
    CWnd::OnDestroy();
}

void COwnerBar::OnPopupClosed ()
{
    m_popup = NULL;
    m_popupClosedAt = ::GetTickCount();
    if (m_hWnd)
        Invalidate(FALSE);
}

void COwnerBar::LoadFavorites ()
{
    m_favorites.clear();
    const CString sentinel(L"\x01");
    CString s = AfxGetApp()->GetProfileString(SECTION, L"FavoriteOwners", sentinel);
    if (s == sentinel)
        s = L"DBAMV|MVINTEGRA";           // os dois botoes fixos de antes viram os primeiros favoritos

    int pos = 0;
    CString tok = s.Tokenize(L"|", pos);
    while (!tok.IsEmpty())
    {
        tok.Trim();
        if (!tok.IsEmpty() && !IsFavorite(tok))
            m_favorites.push_back(tok);
        tok = s.Tokenize(L"|", pos);
    }
}

void COwnerBar::SaveFavorites () const
{
    CString s;
    for (size_t i = 0; i < m_favorites.size(); ++i)
    {
        if (i) s += L"|";
        s += m_favorites[i];
    }
    AfxGetApp()->WriteProfileString(SECTION, L"FavoriteOwners", s.IsEmpty() ? L" " : s);
}

bool COwnerBar::IsFavorite (const CString& owner) const
{
    for (size_t i = 0; i < m_favorites.size(); ++i)
        if (m_favorites[i].CompareNoCase(owner) == 0)
            return true;
    return false;
}

void COwnerBar::ToggleFavorite (const CString& owner)
{
    for (size_t i = 0; i < m_favorites.size(); ++i)
    {
        if (m_favorites[i].CompareNoCase(owner) == 0)
        {
            m_favorites.erase(m_favorites.begin() + i);
            SaveFavorites();
            Layout(m_layoutWidth);
            Invalidate(FALSE);
            return;
        }
    }
    m_favorites.push_back(owner);
    SaveFavorites();
    Layout(m_layoutWidth);
    Invalidate(FALSE);
}

void COwnerBar::MoveFavorite (int from, int to)
{
    if (from < 0 || from >= (int)m_favorites.size() || to < 0 || to >= (int)m_favorites.size() || from == to)
        return;
    CString item = m_favorites[from];
    m_favorites.erase(m_favorites.begin() + from);
    m_favorites.insert(m_favorites.begin() + to, item);
    SaveFavorites();
    Layout(m_layoutWidth);
    Invalidate(FALSE);
}

void COwnerBar::SetCurrent (const CString& owner)
{
    if (m_current == owner)
        return;
    m_current = owner;
    Layout(m_layoutWidth);
    if (m_hWnd)
        Invalidate(FALSE);
}

int COwnerBar::MinWidth () const
{
    return 130;
}

void COwnerBar::Notify (UINT code)
{
    if (GetParent())
        GetParent()->SendMessage(WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(), code), (LPARAM)m_hWnd);
}

void COwnerBar::PickFromPopup (const CString& owner)
{
    m_picked = owner;
    Notify(OBN_OWNER_PICK);
}

void COwnerBar::ShowPopup (const std::vector<CString>& owners, const std::list<std::wstring>& recents, bool loading)
{
    if (m_popup)
    {
        m_popup->Close();
        return;
    }
    if (::GetTickCount() - m_popupClosedAt < 250)
        return;

    COwnerPopup* popup = new COwnerPopup(*this, owners, recents, loading);
    CRect pill(m_rcPill);
    ClientToScreen(&pill);
    m_popup = popup;
    if (!popup->CreatePopup(this, CPoint(pill.left, pill.bottom + 4), popup->GetSize()))
    {
        m_popup = NULL;
        delete popup;
        return;
    }
    Invalidate(FALSE);
}

void COwnerBar::UpdatePopup (const std::vector<CString>& owners, bool loading)
{
    if (m_popup)
        m_popup->SetOwners(owners, loading);
}

void COwnerBar::Layout (int width)
{
    m_chips.clear();
    m_layoutWidth = width;
    if (width <= 0 || !m_hWnd)
        return;

    CClientDC dc(this);
    CFont* pOld = dc.SelectObject(&UiFontBold());
    CString name = m_current.IsEmpty() ? CString(L"Owner") : m_current;
    int tw = dc.GetTextExtent(name).cx;
    int pillW = (std::max)(96, (std::min)(8 + 16 + 6 + tw + 8 + 14 + 8, 210));
    int top = (CTRL_H - PILL_H) / 2;
    m_rcPill = CRect(0, top, (std::min)(pillW, width), top + PILL_H);

    dc.SelectObject(&UiFont());
    int plusW = 26;
    int x = m_rcPill.right + 8;
    int limit = width - plusW - GAP;
    int chipTop = (CTRL_H - CHIP_H) / 2;
    for (size_t i = 0; i < m_favorites.size(); ++i)
    {
        int w = dc.GetTextExtent(m_favorites[i]).cx + 24;
        if (x + w > limit)
            break;
        Cell c;
        c.rc = CRect(x, chipTop, x + w, chipTop + CHIP_H);
        c.id = (int)i;
        m_chips.push_back(c);
        x += w + GAP;
    }
    m_rcPlus = CRect(x, chipTop, x + plusW, chipTop + CHIP_H);
    if (m_rcPlus.right > width)
        m_rcPlus.SetRectEmpty();
    dc.SelectObject(pOld);
}

COwnerBar::Hit COwnerBar::HitTest (CPoint pt) const
{
    Hit h;
    h.kind = HIT_NONE;
    h.id = -1;
    if (m_rcPill.PtInRect(pt)) { h.kind = HIT_PILL; return h; }
    for (size_t i = 0; i < m_chips.size(); ++i)
        if (m_chips[i].rc.PtInRect(pt)) { h.kind = HIT_CHIP; h.id = m_chips[i].id; return h; }
    if (!m_rcPlus.IsRectEmpty() && m_rcPlus.PtInRect(pt)) { h.kind = HIT_PLUS; return h; }
    return h;
}

BOOL COwnerBar::OnEraseBkgnd (CDC*)
{
    return TRUE;
}

void COwnerBar::OnSize (UINT nType, int cx, int cy)
{
    CWnd::OnSize(nType, cx, cy);
    Layout(cx);
    Invalidate(FALSE);
}

void COwnerBar::OnPaint ()
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

    bool enabled = IsWindowEnabled() != FALSE;
    Gdiplus::Graphics g(mem.GetSafeHdc());
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

    // botao do owner atual
    {
        bool hot = enabled && (m_hover.kind == HIT_PILL || m_popup != NULL);
        FillRoundRect(g, m_rcPill, 8.0f, Surface);
        StrokeRoundRect(g, m_rcPill, 8.0f, hot ? Accent : BorderStrong);
        DrawUiIcon(mem, UI_USER, m_rcPill.left + 8, m_rcPill.CenterPoint().y - 8, false, !enabled);
        mem.SelectObject(&UiFontBold());
        mem.SetTextColor(!enabled ? TextMuted : m_current.IsEmpty() ? TextMuted : Text);
        CRect rt(m_rcPill.left + 8 + 16 + 6, m_rcPill.top, m_rcPill.right - 26, m_rcPill.bottom);
        mem.DrawText(m_current.IsEmpty() ? CString(L"Owner") : m_current, rt, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        DrawUiIcon(mem, UI_CHEVRON, m_rcPill.right - 22, m_rcPill.CenterPoint().y - 8, hot, !enabled);
    }

    // atalhos dos favoritos
    mem.SelectObject(&UiFont());
    for (size_t i = 0; i < m_chips.size(); ++i)
    {
        const Cell& c = m_chips[i];
        const CString& name = m_favorites[c.id];
        bool sel = (name.CompareNoCase(m_current) == 0);
        bool hot = enabled && m_hover.kind == HIT_CHIP && m_hover.id == c.id;
        FillRoundRect(g, c.rc, c.rc.Height() / 2.0f, (sel || hot) ? AccentTint : SurfaceAlt);
        if (sel)
            StrokeRoundRect(g, c.rc, c.rc.Height() / 2.0f, AccentStrong);
        mem.SelectObject(sel ? &UiFontBold() : &UiFont());
        mem.SetTextColor(!enabled ? TextMuted : sel ? Accent : Text);
        CRect rchip(c.rc);
        mem.DrawText(name, rchip, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    // "+" (gerenciar favoritos)
    if (!m_rcPlus.IsRectEmpty())
    {
        bool hot = enabled && m_hover.kind == HIT_PLUS;
        if (hot)
            FillRoundRect(g, m_rcPlus, 6.0f, AccentTint);
        DrawUiIcon(mem, UI_PLUS, m_rcPlus.CenterPoint().x - 8, m_rcPlus.CenterPoint().y - 8, hot, !enabled);
    }

    dc.BitBlt(0, 0, rc.Width(), rc.Height(), &mem, 0, 0, SRCCOPY);
    mem.SelectObject(pOldBmp);
}

void COwnerBar::OnMouseMove (UINT nFlags, CPoint point)
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

void COwnerBar::OnMouseLeave ()
{
    m_trackingMouse = false;
    m_hover.kind = HIT_NONE;
    m_hover.id = -1;
    Invalidate(FALSE);
}

void COwnerBar::OnLButtonDown (UINT nFlags, CPoint point)
{
    if (!IsWindowEnabled())
        return;

    Hit h = HitTest(point);
    switch (h.kind)
    {
    case HIT_CHIP:
        m_picked = m_favorites[h.id];
        Notify(OBN_OWNER_PICK);
        break;
    case HIT_PILL:
    case HIT_PLUS:
        Notify(OBN_OWNER_OPEN);
        break;
    default:
        break;
    }
    CWnd::OnLButtonDown(nFlags, point);
}
