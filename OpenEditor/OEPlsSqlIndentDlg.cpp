#include "stdafx.h"
#include <algorithm>
#include "OpenEditor/OEPlsSqlIndentDlg.h"
#include "OpenEditor/OEDocument.h"
#include "OpenEditor/OEView.h"
#include "Theme/ModernTheme.h"
#include "Theme/ModernDraw.h"
#include "Theme/ModernDark.h"
#include "Theme/ModernPopup.h"

using namespace OpenEditor;
using namespace ModernTheme;
using ModernDraw::Clr;

namespace
{
    enum { IDP_TABS = 1001, IDP_LIST, IDP_SAMPLE, IDP_DEFAULTS, IDP_PREVIEW = AFX_IDW_PANE_FIRST };

    const wchar_t* const g_tabNames[] = { L"Case", L"Tabs and Indents", L"Wrapping", L"Alignment", L"Spaces" };
    const int g_tabCount = sizeof(g_tabNames) / sizeof(g_tabNames[0]);

    typedef PlSqlIndentOptions O;
    const PlSqlStyleChoice g_case[] = {
        { L"Do not change", O::CASE_KEEP }, { L"UPPER CASE", O::CASE_UPPER }, { L"lower case", O::CASE_LOWER } };
    const PlSqlStyleChoice g_caseAs[] = {
        { L"As keywords", O::CASE_AS_KEYWORDS }, { L"As identifiers", O::CASE_AS_IDENTIFIERS },
        { L"Do not change", O::CASE_KEEP }, { L"UPPER CASE", O::CASE_UPPER }, { L"lower case", O::CASE_LOWER } };
    const PlSqlStyleChoice g_tabs[] = {
        { L"Same as the editor", O::TABS_EDITOR }, { L"Spaces", O::TABS_SPACES }, { L"Tab characters", O::TABS_TABS } };
    const PlSqlStyleChoice g_then[] = {
        { L"Do not change", O::PLACE_KEEP }, { L"End of the condition", O::PLACE_SAME_LINE }, { L"On its own line", O::PLACE_OWN_LINE } };
    const PlSqlStyleChoice g_loop[] = {
        { L"Do not change", O::PLACE_KEEP }, { L"End of FOR / WHILE", O::PLACE_SAME_LINE }, { L"On its own line", O::PLACE_OWN_LINE } };
    const PlSqlStyleChoice g_paren[] = {
        { L"Do not change", O::PLACE_KEEP }, { L"After the last parameter", O::PLACE_SAME_LINE }, { L"On its own line", O::PLACE_OWN_LINE } };

    #define OPT_BOOL(tab, group, label, info, field) \
        { tab, group, label, info, PlSqlStyleOption::BOOL, &O::field, nullptr, nullptr, 0, 0, 0, 0, nullptr }
    #define OPT_INT(tab, group, label, info, field, lo, hi, special, specialText) \
        { tab, group, label, info, PlSqlStyleOption::INT, nullptr, &O::field, nullptr, 0, lo, hi, special, specialText }
    #define OPT_CHOICE(tab, group, label, info, field, choices) \
        { tab, group, label, info, PlSqlStyleOption::CHOICE, nullptr, &O::field, choices, (int)(sizeof(choices) / sizeof(choices[0])), 0, 0, 0, nullptr }

    const PlSqlStyleOption g_opts[] = {
        OPT_CHOICE(0, L"Word case", L"Keywords", L"BEGIN, IF, SELECT, LOOP... Strings and comments are never changed.", keywordCase, g_case),
        OPT_CHOICE(0, L"Word case", L"Identifiers", L"Names of tables, columns, variables, packages and procedures. "
                                                    L"Quoted names, :binds and &substitutions are never changed.", identifierCase, g_case),
        OPT_CHOICE(0, L"Word case", L"Built-in types", L"NUMBER, VARCHAR2, DATE, BOOLEAN...", typeCase, g_caseAs),
        OPT_CHOICE(0, L"Word case", L"Built-in functions", L"NVL, TO_CHAR, SUBSTR, COUNT...", functionCase, g_caseAs),

        OPT_INT   (1, L"Indentation", L"Indent size", L"Spaces per block level. \"Editor\" uses the indent size of the editor.",
                   indentSize, 0, 16, 0, L"Editor"),
        OPT_CHOICE(1, L"Indentation", L"Indent with", L"Indent with spaces or with tab characters.", tabsMode, g_tabs),
        OPT_INT   (1, L"Indentation", L"Continuation indent", L"Extra indent of lines that continue a statement when they are created by "
                   L"wrapping (or always, when the offset of continuation lines is not kept).", continuationIndent, 0, 32, 0, L"= Indent"),
        OPT_BOOL  (1, L"Indentation", L"Keep offset of continuation lines", L"Continuation lines of a statement (a multi-line SELECT, a long "
                   L"IF condition) keep their offset from the first line of the statement, so formatted SQL is not undone.", keepContinuation),
        OPT_BOOL  (1, L"Blocks", L"Indent package contents", L"Indent declarations inside PACKAGE and PACKAGE BODY one level.", indentPackage),
        OPT_BOOL  (1, L"Blocks", L"Indent declarations", L"Indent the declaration section of procedures and functions (after IS / AS) one level.", indentDeclarations),
        OPT_BOOL  (1, L"Blocks", L"Indent WHEN in EXCEPTION", L"Indent the WHEN handlers one level inside EXCEPTION.", indentExceptionWhen),
        OPT_BOOL  (1, L"Blocks", L"Indent WHEN in CASE", L"Indent WHEN and ELSE one level inside a CASE statement.", indentCaseWhen),
        OPT_BOOL  (1, L"Blocks", L"Indent END IF", L"Put END IF at the level of the code inside the IF.", indentEndIf),
        OPT_BOOL  (1, L"Blocks", L"Indent END LOOP", L"Put END LOOP at the level of the code inside the loop.", indentEndLoop),

        OPT_BOOL  (2, L"Statements", L"One statement per line", L"Several statements on the same line are split, one per line.", oneStatementPerLine),
        OPT_BOOL  (2, L"Statements", L"Wrap code after block keywords", L"Code written on the same line after THEN, ELSE, LOOP, BEGIN, "
                   L"DECLARE or EXCEPTION goes to the next line; END, ELSE, ELSIF and WHEN start a new line.", wrapInnerCode),
        OPT_CHOICE(2, L"IF ... THEN ... ELSE", L"Place THEN", L"Position of THEN in IF and ELSIF.", thenPlace, g_then),
        OPT_CHOICE(2, L"Loops", L"Place LOOP", L"Position of LOOP in FOR and WHILE loops.", loopPlace, g_loop),
        OPT_BOOL  (2, L"Routine parameters", L"One parameter per line", L"Parameters of procedures and functions are placed one per line.", paramsOnePerLine),
        OPT_INT   (2, L"Routine parameters", L"Only with at least", L"Minimum number of parameters for \"One parameter per line\".",
                   paramsMinCount, 1, 99, -1, nullptr),
        OPT_CHOICE(2, L"Routine parameters", L"Closing parenthesis", L"Position of the ')' that closes the parameter list.", closeParenPlace, g_paren),

        OPT_BOOL  (3, L"Declared variables", L"Align types", L"Align the data types of consecutive declarations.", alignDeclTypes),
        OPT_BOOL  (3, L"Declared variables", L"Align assignments", L"Align := and DEFAULT of consecutive declarations.", alignDeclAssign),
        OPT_BOOL  (3, L"Routine parameters", L"Align parameters", L"Align names, IN / OUT and types of parameters written one per line.", alignParams),
        OPT_BOOL  (3, L"Code", L"Align consecutive assignments", L"Align := of assignments on consecutive lines.", alignAssignments),

        OPT_BOOL  (4, L"Spaces", L"Around operators", L"Add a space around := => = <> != < > <= >= and || when they touch the code "
                   L"(v:=1 becomes v := 1). Existing spaces are never removed.", spaceAroundOperators),
        OPT_BOOL  (4, L"Spaces", L"After comma", L"Add a space after a comma (a,b becomes a, b).", spaceAfterComma),
        OPT_BOOL  (4, L"Spaces", L"After closing parenthesis", L"Add a space between ')' and the next word ()IS becomes ) IS).", spaceAfterParen),
        OPT_BOOL  (4, L"Spaces", L"Remove trailing spaces", L"Remove spaces at the end of the lines (never inside strings).", trimTrailing),
        OPT_INT   (4, L"Blank lines", L"Keep at most", L"Maximum number of blank lines kept together.", maxBlankLines, -1, 99, -1, L"All"),
    };
    const int g_optCount = sizeof(g_opts) / sizeof(g_opts[0]);

    const wchar_t* const g_sample =
        L"create or replace package body pkg_orders is\n"
        L"-- package state\n"
        L"g_count number := 0;\n"
        L"g_last_order date;\n"
        L"c_max_items constant pls_integer := 100;\n"
        L"cursor c_items (p_order number) is\n"
        L"  select item_id, qty\n"
        L"    from order_items\n"
        L"   where order_id = p_order;\n"
        L"\n"
        L"\n"
        L"\n"
        L"procedure log_msg (p_msg in varchar2, p_level in number default 1) is\n"
        L"pragma autonomous_transaction;\n"
        L"begin\n"
        L"insert into app_log (msg, lvl) values (p_msg, p_level); commit;\n"
        L"end log_msg;\n"
        L"function total (p_order in number, p_discount in number default 0, p_rounded out boolean) return number\n"
        L"is\n"
        L"v_total number := 0;\n"
        L"v_items pls_integer := 0;\n"
        L"begin\n"
        L"for r in c_items(p_order)\n"
        L"loop\n"
        L"if r.qty > 10\n"
        L"then v_total := v_total + r.qty * 0.9;\n"
        L"elsif r.qty = 0 then continue;\n"
        L"else\n"
        L"v_total := v_total + r.qty;\n"
        L"end if;\n"
        L"v_items := v_items + 1;\n"
        L"end loop;\n"
        L"case\n"
        L"when p_discount > 0 then\n"
        L"v_total := v_total - p_discount;\n"
        L"else\n"
        L"null;\n"
        L"end case;\n"
        L"p_rounded := false;\n"
        L"return round(nvl(v_total, 0), 2);\n"
        L"exception\n"
        L"when no_data_found then\n"
        L"return 0;\n"
        L"when others then\n"
        L"log_msg('total failed: ' || sqlerrm);\n"
        L"raise;\n"
        L"end total;\n"
        L"begin\n"
        L"g_count := 0;\n"
        L"end pkg_orders;\n"
        L"/\n";

    std::vector<std::wstring> SplitLines (const std::wstring& text)
    {
        std::vector<std::wstring> lines;
        std::wstring cur;
        for (size_t i = 0; i < text.size(); ++i)
        {
            const wchar_t c = text[i];
            if (c == L'\r' || c == L'\n')
            {
                lines.push_back(cur);
                cur.clear();
                if (c == L'\r' && i + 1 < text.size() && text[i + 1] == L'\n') ++i;
            }
            else
                cur += c;
        }
        lines.push_back(cur);
        return lines;
    }

    // pinta com buffer duplo e GDI+ (texto com GDI, na fonte da interface)
    class PaintBuffer
    {
    public:
        PaintBuffer (CWnd* wnd, CPaintDC& dc) : m_dc(dc)
        {
            wnd->GetClientRect(m_rc);
            m_mem.CreateCompatibleDC(&dc);
            m_bmp.CreateCompatibleBitmap(&dc, std::max(1, m_rc.Width()), std::max(1, m_rc.Height()));
            m_old = m_mem.SelectObject(&m_bmp);
            m_oldFont = m_mem.SelectObject(&ModernDraw::UiFont());
            m_mem.SetBkMode(TRANSPARENT);
        }
        ~PaintBuffer ()
        {
            m_dc.BitBlt(0, 0, m_rc.Width(), m_rc.Height(), &m_mem, 0, 0, SRCCOPY);
            m_mem.SelectObject(m_oldFont);
            m_mem.SelectObject(m_old);
        }
        CDC& dc ()           { return m_mem; }
        const CRect& rc ()   { return m_rc; }
    private:
        CPaintDC& m_dc;
        CDC m_mem;
        CBitmap m_bmp;
        CBitmap* m_old;
        CFont* m_oldFont;
        CRect m_rc;
    };

    void Chevron (Gdiplus::Graphics& g, float cx, float cy, bool down, COLORREF color, float size = 4.0f)
    {
        Gdiplus::Pen pen(Clr(color), 1.6f);
        pen.SetStartCap(Gdiplus::LineCapRound);
        pen.SetEndCap(Gdiplus::LineCapRound);
        pen.SetLineJoin(Gdiplus::LineJoinRound);
        Gdiplus::PointF pts[3];
        if (down)
        {
            pts[0] = Gdiplus::PointF(cx - size, cy - size / 2); pts[1] = Gdiplus::PointF(cx, cy + size / 2);
            pts[2] = Gdiplus::PointF(cx + size, cy - size / 2);
        }
        else
        {
            pts[0] = Gdiplus::PointF(cx - size / 2, cy - size); pts[1] = Gdiplus::PointF(cx + size / 2, cy);
            pts[2] = Gdiplus::PointF(cx - size / 2, cy + size);
        }
        g.DrawLines(&pen, pts, 3);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// Painel flutuante com as escolhas de uma opcao

class CPlSqlChoicePopup : public CModernPopup
{
public:
    CPlSqlChoicePopup (CPlSqlStyleOptionList* owner, int option, const PlSqlStyleOption& def, int current)
        : m_owner(owner), m_option(option), m_def(def), m_current(current), m_hot(-1) {}

    enum { ROW = 30, PAD = 4 };

protected:
    void Paint (CDC& dc, Gdiplus::Graphics& g, const CRect& client) override
    {
        CFont* old = dc.SelectObject(&ModernDraw::UiFont());
        dc.SetBkMode(TRANSPARENT);
        for (int i = 0; i < m_def.choiceCount; ++i)
        {
            CRect r(PAD, PAD + i * ROW, client.right - PAD, PAD + (i + 1) * ROW);
            const bool cur = m_def.choices[i].value == m_current;
            if (i == m_hot)
                ModernDraw::FillRoundRect(g, r, 5.0f, Hover);
            if (cur)
            {
                Gdiplus::Pen pen(Clr(Accent), 1.8f);
                pen.SetStartCap(Gdiplus::LineCapRound); pen.SetEndCap(Gdiplus::LineCapRound);
                const float x = (float)r.left + 10, y = (float)r.CenterPoint().y;
                Gdiplus::PointF pts[3] = { Gdiplus::PointF(x, y), Gdiplus::PointF(x + 3.5f, y + 3.5f), Gdiplus::PointF(x + 10, y - 4) };
                g.DrawLines(&pen, pts, 3);
            }
            CRect tr(r.left + 28, r.top, r.right - 8, r.bottom);
            dc.SetTextColor(cur ? Accent : Text);
            dc.DrawText(m_def.choices[i].text, tr, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
        }
        dc.SelectObject(old);
    }

    void OnPopupShown () override
    {
        for (int i = 0; i < m_def.choiceCount; ++i)
            if (m_def.choices[i].value == m_current) m_hot = i;
        SetFocus();
    }

    int HitTest (CPoint pt) const
    {
        const int i = (pt.y - PAD) / ROW;
        return (pt.y >= PAD && i >= 0 && i < m_def.choiceCount) ? i : -1;
    }

    void Choose (int i)
    {
        if (i >= 0 && i < m_def.choiceCount)
        {
            CPlSqlStyleOptionList* owner = m_owner;
            const int option = m_option, value = m_def.choices[i].value;
            Close();
            owner->SetChoice(option, value);
        }
    }

    DECLARE_MESSAGE_MAP()
    afx_msg void OnMouseMove (UINT, CPoint pt)
    {
        const int h = HitTest(pt);
        if (h != m_hot) { m_hot = h; Invalidate(FALSE); }
    }
    afx_msg void OnLButtonUp (UINT, CPoint pt)
    {
        Choose(HitTest(pt));
    }
    afx_msg void OnKeyDown (UINT nChar, UINT nRepCnt, UINT nFlags)
    {
        if (nChar == VK_DOWN)       { m_hot = std::min(m_def.choiceCount - 1, m_hot + 1); Invalidate(FALSE); }
        else if (nChar == VK_UP)    { m_hot = std::max(0, m_hot - 1); Invalidate(FALSE); }
        else if (nChar == VK_RETURN || nChar == VK_SPACE) Choose(m_hot);
        else CModernPopup::OnKeyDown(nChar, nRepCnt, nFlags);
    }

private:
    CPlSqlStyleOptionList* m_owner;
    int m_option;
    const PlSqlStyleOption& m_def;
    int m_current, m_hot;
};

BEGIN_MESSAGE_MAP(CPlSqlChoicePopup, CModernPopup)
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONUP()
    ON_WM_KEYDOWN()
END_MESSAGE_MAP()

// ---------------------------------------------------------------------------------------------------------------
// Abas

BEGIN_MESSAGE_MAP(CPlSqlStyleTabs, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_LBUTTONDOWN()
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSELEAVE()
    ON_WM_SETCURSOR()
END_MESSAGE_MAP()

CPlSqlStyleTabs::CPlSqlStyleTabs () : m_sel(0), m_hot(-1), m_tracking(false) {}

BOOL CPlSqlStyleTabs::Create (CWnd* parent, UINT id, const wchar_t* const* names, int count)
{
    m_names.assign(names, names + count);
    return CWnd::Create(AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW, ::LoadCursor(NULL, IDC_ARROW)), L"",
                        WS_CHILD | WS_VISIBLE, CRect(0, 0, 10, 10), parent, id);
}

int CPlSqlStyleTabs::HitTest (CPoint pt) const
{
    for (size_t i = 0; i < m_rects.size(); ++i)
        if (m_rects[i].PtInRect(pt)) return (int)i;
    return -1;
}

BOOL CPlSqlStyleTabs::OnEraseBkgnd (CDC*) { return TRUE; }

void CPlSqlStyleTabs::OnPaint ()
{
    CPaintDC paint(this);
    PaintBuffer buf(this, paint);
    CDC& dc = buf.dc();
    const CRect& rc = buf.rc();
    dc.FillSolidRect(rc, SurfaceAlt);

    Gdiplus::Graphics g(dc.GetSafeHdc());
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

    // linha de base de toda a faixa
    dc.FillSolidRect(rc.left, rc.bottom - 1, rc.Width(), 1, Border);

    m_rects.clear();
    int x = rc.left + 2;
    for (size_t i = 0; i < m_names.size(); ++i)
    {
        const bool sel = (int)i == m_sel, hot = (int)i == m_hot;
        dc.SelectObject(sel ? &ModernDraw::UiFontBold() : &ModernDraw::UiFont());
        CSize sz = dc.GetTextExtent(m_names[i].c_str(), (int)m_names[i].size());
        CRect r(x, rc.top, x + sz.cx + 24, rc.bottom);
        m_rects.push_back(r);

        if (hot && !sel)
        {
            CRect h(r.left + 2, r.top + 4, r.right - 2, r.bottom - 6);
            ModernDraw::FillRoundRect(g, h, 5.0f, Hover);
        }
        dc.SetTextColor(sel ? Text : (hot ? Text : TextMuted));
        CRect tr(r.left, r.top, r.right, r.bottom - 3);
        dc.DrawText(m_names[i].c_str(), tr, DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_NOPREFIX);
        if (sel)
        {
            CRect u(r.left + 8, r.bottom - 3, r.right - 8, r.bottom);
            ModernDraw::FillRoundRect(g, u, 1.5f, Accent);
        }
        x = r.right + 2;
    }
}

void CPlSqlStyleTabs::OnLButtonDown (UINT, CPoint pt)
{
    const int h = HitTest(pt);
    if (h >= 0 && h != m_sel)
    {
        m_sel = h;
        Invalidate(FALSE);
        GetParent()->SendMessage(WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(), BN_CLICKED), (LPARAM)m_hWnd);
    }
}

void CPlSqlStyleTabs::OnMouseMove (UINT, CPoint pt)
{
    if (!m_tracking)
    {
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, m_hWnd, 0 };
        m_tracking = ::TrackMouseEvent(&tme) != FALSE;
    }
    const int h = HitTest(pt);
    if (h != m_hot) { m_hot = h; Invalidate(FALSE); }
}

void CPlSqlStyleTabs::OnMouseLeave ()
{
    m_tracking = false;
    if (m_hot != -1) { m_hot = -1; Invalidate(FALSE); }
}

BOOL CPlSqlStyleTabs::OnSetCursor (CWnd*, UINT, UINT)
{
    CPoint pt;
    ::GetCursorPos(&pt);
    ScreenToClient(&pt);
    ::SetCursor(::LoadCursor(NULL, HitTest(pt) >= 0 ? IDC_HAND : IDC_ARROW));
    return TRUE;
}

// ---------------------------------------------------------------------------------------------------------------
// Lista de opcoes

namespace
{
    const int GROUP_H = 32, ROW_H = 36, FOOTER_H = 78, PAD = 6, RADIUS = 8;
    const int TOGGLE_W = 38, TOGGLE_H = 20, CHOICE_W = 196, BOX_H = 28, STEP_W = 28, STEP_VALUE_W = 74;
}

BEGIN_MESSAGE_MAP(CPlSqlStyleOptionList, CWnd)
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
END_MESSAGE_MAP()

CPlSqlStyleOptionList::CPlSqlStyleOptionList ()
    : m_options(nullptr), m_count(0), m_values(nullptr), m_tab(0), m_scroll(0), m_sel(-1), m_hot(-1),
      m_hotPart(P_NONE), m_tracking(false), m_focus(false) {}

BOOL CPlSqlStyleOptionList::Create (CWnd* parent, UINT id, const PlSqlStyleOption* options, int count, PlSqlIndentOptions* values)
{
    m_options = options; m_count = count; m_values = values;
    if (!CWnd::Create(AfxRegisterWndClass(CS_DBLCLKS, ::LoadCursor(NULL, IDC_ARROW)), L"",
                      WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL, CRect(0, 0, 10, 10), parent, id))
        return FALSE;
    if (IsDark())
        ::SetWindowTheme(m_hWnd, ModernDark::ControlTheme(), NULL);    // barra de rolagem escura
    Rebuild();
    return TRUE;
}

void CPlSqlStyleOptionList::SetTab (int tab)
{
    m_tab = tab;
    m_scroll = 0;
    m_sel = -1;
    Rebuild();
}

void CPlSqlStyleOptionList::Rebuild ()
{
    m_rows.clear();
    std::wstring group;
    bool collapsed = false;
    for (int n = 0; n < m_count; ++n)
    {
        const PlSqlStyleOption& d = m_options[n];
        if (d.tab != m_tab)
            continue;
        if (group != d.group)
        {
            group = d.group;
            const std::wstring key = std::to_wstring(m_tab) + L"|" + group;
            collapsed = std::find(m_collapsed.begin(), m_collapsed.end(), key) != m_collapsed.end();
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
    Invalidate(FALSE);
}

CRect CPlSqlStyleOptionList::ListRect () const
{
    CRect rc;
    GetClientRect(rc);
    rc.DeflateRect(1, PAD, 1, FOOTER_H);
    return rc;
}

int CPlSqlStyleOptionList::RowHeight (int row) const { return m_rows[row].group ? GROUP_H : ROW_H; }

int CPlSqlStyleOptionList::RowTop (int row) const
{
    int y = ListRect().top - m_scroll;
    for (int i = 0; i < row; ++i) y += RowHeight(i);
    return y;
}

void CPlSqlStyleOptionList::UpdateScroll ()
{
    if (!m_hWnd) return;
    int total = 0;
    for (size_t i = 0; i < m_rows.size(); ++i) total += RowHeight((int)i);
    const int page = std::max(1, ListRect().Height());
    m_scroll = std::max(0, std::min(m_scroll, total - page));
    SCROLLINFO si = { sizeof(si), SIF_ALL, 0, std::max(0, total - 1), (UINT)page, m_scroll, 0 };     // some quando tudo cabe
    SetScrollInfo(SB_VERT, &si, TRUE);
}

CRect CPlSqlStyleOptionList::ControlRect (int row) const
{
    CRect rc;
    GetClientRect(rc);
    const PlSqlStyleOption& d = m_options[m_rows[row].option];
    const int top = RowTop(row), right = rc.right - 14;
    int w = 0, h = BOX_H;
    switch (d.kind)
    {
    case PlSqlStyleOption::BOOL:   w = TOGGLE_W; h = TOGGLE_H; break;
    case PlSqlStyleOption::CHOICE: w = std::min(CHOICE_W, std::max(120, rc.Width() / 2 - 20)); break;
    case PlSqlStyleOption::INT:    w = STEP_W * 2 + STEP_VALUE_W; break;
    }
    const int y = top + (ROW_H - h) / 2;
    return CRect(right - w, y, right, y + h);
}

void CPlSqlStyleOptionList::PartRects (int row, CRect& minus, CRect& value, CRect& plus) const
{
    const CRect c = ControlRect(row);
    minus = CRect(c.left, c.top, c.left + STEP_W, c.bottom);
    plus  = CRect(c.right - STEP_W, c.top, c.right, c.bottom);
    value = CRect(minus.right, c.top, plus.left, c.bottom);
}

int CPlSqlStyleOptionList::HitRow (CPoint pt, EPart& part) const
{
    part = P_NONE;
    const CRect list = ListRect();
    if (!list.PtInRect(pt))
        return -1;
    for (int i = 0; i < (int)m_rows.size(); ++i)
    {
        const int top = RowTop(i), h = RowHeight(i);
        if (pt.y < top || pt.y >= top + h)
            continue;
        if (m_rows[i].group) { part = P_GROUP; return i; }

        const PlSqlStyleOption& d = m_options[m_rows[i].option];
        part = P_ROW;
        CRect c = ControlRect(i);
        if (d.kind == PlSqlStyleOption::INT)
        {
            CRect minus, value, plus;
            PartRects(i, minus, value, plus);
            if (minus.PtInRect(pt)) part = P_MINUS;
            else if (plus.PtInRect(pt)) part = P_PLUS;
        }
        else if (d.kind == PlSqlStyleOption::CHOICE)
        {
            if (c.PtInRect(pt)) part = P_CHOICE;
        }
        else
        {
            part = P_TOGGLE;    // a linha inteira liga e desliga
        }
        return i;
    }
    return -1;
}

std::wstring CPlSqlStyleOptionList::ValueText (int option) const
{
    const PlSqlStyleOption& d = m_options[option];
    if (d.kind == PlSqlStyleOption::CHOICE)
    {
        const int v = m_values->*d.i;
        for (int c = 0; c < d.choiceCount; ++c)
            if (d.choices[c].value == v) return d.choices[c].text;
        return L"";
    }
    if (d.kind == PlSqlStyleOption::INT)
    {
        const int v = m_values->*d.i;
        if (d.specialText && v == d.special)
            return d.specialText;
        return std::to_wstring(v);
    }
    return L"";
}

BOOL CPlSqlStyleOptionList::OnEraseBkgnd (CDC*) { return TRUE; }

void CPlSqlStyleOptionList::OnPaint ()
{
    CPaintDC paint(this);
    PaintBuffer buf(this, paint);
    CDC& dc = buf.dc();
    const CRect& rc = buf.rc();
    dc.FillSolidRect(rc, Surface);

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
            const std::wstring key = std::to_wstring(m_tab) + L"|" + m_rows[i].groupName;
            const bool collapsed = std::find(m_collapsed.begin(), m_collapsed.end(), key) != m_collapsed.end();
            Chevron(g, (float)row.left + 12, (float)row.CenterPoint().y, !collapsed, TextMuted);
            dc.SelectObject(&ModernDraw::UiFontBold());
            dc.SetTextColor(Text);
            CRect tr(row.left + 26, row.top, row.right, row.bottom);
            dc.DrawText(m_rows[i].groupName.c_str(), tr, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
            continue;
        }

        const int option = m_rows[i].option;
        const PlSqlStyleOption& d = m_options[option];
        const CRect c = ControlRect(i);

        dc.SelectObject(&ModernDraw::UiFont());
        dc.SetTextColor(Text);
        CRect tr(row.left + 26, row.top, c.left - 10, row.bottom);
        dc.DrawText(d.label, tr, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);

        const bool hotRow = i == m_hot;
        switch (d.kind)
        {
        case PlSqlStyleOption::BOOL:
            {
                const bool on = m_values->*d.b;
                ModernDraw::FillRoundRect(g, c, (float)c.Height() / 2, on ? (hotRow ? AccentHover : Accent) : (hotRow ? BorderStrong : Border));
                const int k = c.Height() - 6;
                CRect knob(on ? c.right - 3 - k : c.left + 3, c.top + 3, on ? c.right - 3 : c.left + 3 + k, c.bottom - 3);
                ModernDraw::FillRoundRect(g, knob, (float)k / 2, on ? RGB(255, 255, 255) : Surface);
            }
            break;
        case PlSqlStyleOption::CHOICE:
            {
                const bool hot = hotRow && m_hotPart == P_CHOICE;
                ModernDraw::FillRoundRect(g, c, 6.0f, hot ? Hover : SurfaceAlt);
                ModernDraw::StrokeRoundRect(g, c, 6.0f, hot ? BorderStrong : Border);
                CRect vt(c.left + 10, c.top, c.right - 26, c.bottom);
                dc.SetTextColor(Text);
                dc.DrawText(ValueText(option).c_str(), vt, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
                Chevron(g, (float)c.right - 14, (float)c.CenterPoint().y, true, TextMuted, 3.5f);
            }
            break;
        case PlSqlStyleOption::INT:
            {
                CRect minus, value, plus;
                PartRects(i, minus, value, plus);
                ModernDraw::FillRoundRect(g, c, 6.0f, SurfaceAlt);
                ModernDraw::StrokeRoundRect(g, c, 6.0f, Border);
                const int v = m_values->*d.i;
                const bool canMinus = v > d.minValue, canPlus = v < d.maxValue;
                if (hotRow && m_hotPart == P_MINUS && canMinus)
                    ModernDraw::FillRoundRect(g, CRect(minus.left + 2, minus.top + 2, minus.right - 1, minus.bottom - 2), 5.0f, Hover);
                if (hotRow && m_hotPart == P_PLUS && canPlus)
                    ModernDraw::FillRoundRect(g, CRect(plus.left + 1, plus.top + 2, plus.right - 2, plus.bottom - 2), 5.0f, Hover);
                Gdiplus::Pen on(Clr(Text), 1.6f), off(Clr(TextDisabled), 1.6f);
                const float my = (float)minus.CenterPoint().y, mx = (float)minus.CenterPoint().x, px = (float)plus.CenterPoint().x;
                g.DrawLine(canMinus ? &on : &off, mx - 4.5f, my, mx + 4.5f, my);
                g.DrawLine(canPlus ? &on : &off, px - 4.5f, my, px + 4.5f, my);
                g.DrawLine(canPlus ? &on : &off, px, my - 4.5f, px, my + 4.5f);
                dc.SetTextColor(Text);
                dc.DrawText(ValueText(option).c_str(), value, DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_NOPREFIX);
            }
            break;
        }
    }

    dc.SelectClipRgn(NULL);
    g.ResetClip();

    // rodape: descricao da opcao selecionada
    CRect foot(rc.left, rc.bottom - FOOTER_H, rc.right, rc.bottom);
    dc.FillSolidRect(foot.left, foot.top, foot.Width(), 1, Border);
    CRect ft(foot.left + 16, foot.top + 10, foot.right - 16, foot.bottom - 8);
    if (m_sel >= 0 && m_sel < (int)m_rows.size() && !m_rows[m_sel].group)
    {
        const PlSqlStyleOption& d = m_options[m_rows[m_sel].option];
        dc.SelectObject(&ModernDraw::UiFontBold());
        dc.SetTextColor(Text);
        dc.DrawText(d.label, CRect(ft.left, ft.top, ft.right, ft.top + 18), DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
        dc.SelectObject(&ModernDraw::UiFont());
        dc.SetTextColor(TextMuted);
        dc.DrawText(d.info, CRect(ft.left, ft.top + 22, ft.right, ft.bottom), DT_WORDBREAK | DT_NOPREFIX | DT_END_ELLIPSIS);
    }
    else
    {
        dc.SetTextColor(TextMuted);
        dc.DrawText(L"Select an option to see what it does.", ft, DT_WORDBREAK | DT_NOPREFIX);
    }
}

void CPlSqlStyleOptionList::OnSize (UINT nType, int cx, int cy)
{
    CWnd::OnSize(nType, cx, cy);
    UpdateScroll();
}

void CPlSqlStyleOptionList::Changed ()
{
    Invalidate(FALSE);
    GetParent()->SendMessage(WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(), EN_CHANGE), (LPARAM)m_hWnd);
}

void CPlSqlStyleOptionList::Step (int option, int delta)
{
    const PlSqlStyleOption& d = m_options[option];
    if (d.kind == PlSqlStyleOption::INT)
    {
        int& v = m_values->*d.i;
        const int nv = std::max(d.minValue, std::min(d.maxValue, v + delta));
        if (nv != v) { v = nv; Changed(); }
    }
    else if (d.kind == PlSqlStyleOption::CHOICE)
    {
        int& v = m_values->*d.i;
        int idx = 0;
        for (int c = 0; c < d.choiceCount; ++c) if (d.choices[c].value == v) idx = c;
        const int ni = std::max(0, std::min(d.choiceCount - 1, idx + delta));
        if (ni != idx) { v = d.choices[ni].value; Changed(); }
    }
    else if (d.kind == PlSqlStyleOption::BOOL)
    {
        bool& v = m_values->*d.b;
        const bool nv = delta > 0;
        if (nv != v) { v = nv; Changed(); }
    }
}

void CPlSqlStyleOptionList::SetChoice (int option, int value)
{
    const PlSqlStyleOption& d = m_options[option];
    if (d.kind == PlSqlStyleOption::CHOICE && m_values->*d.i != value)
    {
        m_values->*d.i = value;
        Changed();
    }
    SetFocus();
}

void CPlSqlStyleOptionList::Activate (int row, EPart part)
{
    if (row < 0)
        return;
    if (m_rows[row].group)
    {
        const std::wstring key = std::to_wstring(m_tab) + L"|" + m_rows[row].groupName;
        auto it = std::find(m_collapsed.begin(), m_collapsed.end(), key);
        if (it != m_collapsed.end()) m_collapsed.erase(it); else m_collapsed.push_back(key);
        Rebuild();
        return;
    }
    const int option = m_rows[row].option;
    const PlSqlStyleOption& d = m_options[option];
    switch (part)
    {
    case P_TOGGLE:
        m_values->*d.b = !(m_values->*d.b);
        Changed();
        break;
    case P_MINUS: Step(option, -1); break;
    case P_PLUS:  Step(option, +1); break;
    case P_CHOICE:
        {
            const CRect c = ControlRect(row);
            CPoint pos(c.left, c.bottom + 4);
            ClientToScreen(&pos);
            CPlSqlChoicePopup* popup = new CPlSqlChoicePopup(this, option, d, m_values->*d.i);
            const int w = std::max(c.Width(), 200);
            if (!popup->CreatePopup(this, pos, CSize(w, d.choiceCount * CPlSqlChoicePopup::ROW + CPlSqlChoicePopup::PAD * 2)))
                delete popup;
        }
        break;
    default:
        break;
    }
}

void CPlSqlStyleOptionList::OnLButtonDown (UINT, CPoint pt)
{
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

void CPlSqlStyleOptionList::OnLButtonDblClk (UINT nFlags, CPoint pt)
{
    // um duplo clique conta como dois cliques (liga e desliga, +2 ...)
    OnLButtonDown(nFlags, pt);
}

void CPlSqlStyleOptionList::OnMouseMove (UINT, CPoint pt)
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

void CPlSqlStyleOptionList::OnMouseLeave ()
{
    m_tracking = false;
    if (m_hot != -1) { m_hot = -1; m_hotPart = P_NONE; Invalidate(FALSE); }
}

BOOL CPlSqlStyleOptionList::OnSetCursor (CWnd* pWnd, UINT nHitTest, UINT message)
{
    if (nHitTest != HTCLIENT)
        return CWnd::OnSetCursor(pWnd, nHitTest, message);
    CPoint pt;
    ::GetCursorPos(&pt);
    ScreenToClient(&pt);
    EPart part;
    const int row = HitRow(pt, part);
    ::SetCursor(::LoadCursor(NULL, (row >= 0 && part != P_ROW) ? IDC_HAND : IDC_ARROW));
    return TRUE;
}

BOOL CPlSqlStyleOptionList::OnMouseWheel (UINT, short zDelta, CPoint)
{
    m_scroll -= zDelta / WHEEL_DELTA * ROW_H * 2;
    UpdateScroll();
    Invalidate(FALSE);
    return TRUE;
}

void CPlSqlStyleOptionList::OnVScroll (UINT nSBCode, UINT, CScrollBar*)
{
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

void CPlSqlStyleOptionList::EnsureVisible (int row)
{
    if (row < 0) return;
    const CRect list = ListRect();
    const int top = RowTop(row), h = RowHeight(row);
    if (top < list.top) m_scroll -= list.top - top;
    else if (top + h > list.bottom) m_scroll += top + h - list.bottom;
    UpdateScroll();
}

void CPlSqlStyleOptionList::OnKeyDown (UINT nChar, UINT nRepCnt, UINT nFlags)
{
    if (m_rows.empty())
        return;
    const bool group = m_sel >= 0 && m_rows[m_sel].group;
    const PlSqlStyleOption* d = (m_sel >= 0 && !group) ? &m_options[m_rows[m_sel].option] : nullptr;
    switch (nChar)
    {
    case VK_UP:   m_sel = std::max(0, m_sel - 1); EnsureVisible(m_sel); Invalidate(FALSE); return;
    case VK_DOWN: m_sel = std::min((int)m_rows.size() - 1, m_sel + 1); EnsureVisible(m_sel); Invalidate(FALSE); return;
    case VK_HOME: m_sel = 0; EnsureVisible(m_sel); Invalidate(FALSE); return;
    case VK_END:  m_sel = (int)m_rows.size() - 1; EnsureVisible(m_sel); Invalidate(FALSE); return;
    case VK_SPACE:
    case VK_RETURN:
        if (group) Activate(m_sel, P_GROUP);
        else if (d && d->kind == PlSqlStyleOption::BOOL) Activate(m_sel, P_TOGGLE);
        else if (d && d->kind == PlSqlStyleOption::CHOICE) Activate(m_sel, P_CHOICE);
        return;
    case VK_LEFT:
    case VK_RIGHT:
        if (group)
        {
            const std::wstring key = std::to_wstring(m_tab) + L"|" + m_rows[m_sel].groupName;
            const bool collapsed = std::find(m_collapsed.begin(), m_collapsed.end(), key) != m_collapsed.end();
            if (collapsed == (nChar == VK_RIGHT)) Activate(m_sel, P_GROUP);
        }
        else if (d)
            Step(m_rows[m_sel].option, nChar == VK_RIGHT ? +1 : -1);
        return;
    }
    CWnd::OnKeyDown(nChar, nRepCnt, nFlags);
}

UINT CPlSqlStyleOptionList::OnGetDlgCode () { return DLGC_WANTARROWS | DLGC_WANTCHARS; }
void CPlSqlStyleOptionList::OnSetFocus (CWnd*)  { m_focus = true;  Invalidate(FALSE); }
void CPlSqlStyleOptionList::OnKillFocus (CWnd*) { m_focus = false; Invalidate(FALSE); }

// ---------------------------------------------------------------------------------------------------------------
// Janela

BEGIN_MESSAGE_MAP(COEPlsSqlIndentDlg, CDialog)
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_WM_DESTROY()
    ON_WM_PAINT()
END_MESSAGE_MAP()

COEPlsSqlIndentDlg::COEPlsSqlIndentDlg (CWnd* parent, const PlSqlIndentOptions& options, const std::wstring& initialText)
    : CDialog(), m_options(options), m_initialText(initialText), m_previewDoc(nullptr), m_preview(nullptr), m_ready(false)
{
    m_pParentWnd = parent;
}

COEPlsSqlIndentDlg::~COEPlsSqlIndentDlg ()
{
    delete static_cast<CDocument*>(m_previewDoc);   // o destrutor de COEDocument e protegido (virtual em CDocument)
}

INT_PTR COEPlsSqlIndentDlg::DoModal ()
{
    // modelo na memoria: so a moldura (redimensionavel); os controles sao criados em OnInitDialog
    std::vector<WORD> tpl;
    auto put = [&](WORD w) { tpl.push_back(w); };
    auto putStr = [&](const wchar_t* s) { do { put(*s); } while (*s++); };
    const DWORD style = DS_SETFONT | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_CLIPCHILDREN;
    put(LOWORD(style)); put(HIWORD(style));
    put(0); put(0);
    put(0);
    put(0); put(0); put(600); put(360);
    put(0);
    put(0);
    putStr(L"Code Style - PL/SQL");
    put(9);
    putStr(L"Segoe UI");
    while (tpl.size() % 2) put(0);

    m_template.assign((const unsigned char*)tpl.data(), (const unsigned char*)(tpl.data() + tpl.size()));
    InitModalIndirect((LPCDLGTEMPLATE)m_template.data(), m_pParentWnd);
    return CDialog::DoModal();
}

BOOL COEPlsSqlIndentDlg::OnInitDialog ()
{
    CDialog::OnInitDialog();

    CFont* font = GetFont();
    const CRect r0(0, 0, 10, 10);
    const DWORD vis = WS_CHILD | WS_VISIBLE;

    m_tabs.Create(this, IDP_TABS, g_tabNames, g_tabCount);
    m_list.Create(this, IDP_LIST, g_opts, g_optCount, &m_options);

    // a previa e o proprio editor do SQLTools, com um documento so dela (nao aparece nas janelas abertas)
    m_previewDoc = (COEDocument*)RUNTIME_CLASS(COEDocument)->CreateObject();
    m_previewDoc->m_bAutoDelete = FALSE;
    m_previewDoc->OnNewDocument();
    m_previewDoc->SetClassSetting("PL/SQL");

    CCreateContext ctx;
    ctx.m_pCurrentDoc = m_previewDoc;
    ctx.m_pNewViewClass = RUNTIME_CLASS(COEditorView);
    m_preview = (COEditorView*)RUNTIME_CLASS(COEditorView)->CreateObject();
    if (!m_preview->Create(NULL, NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | WS_TABSTOP, r0, this, IDP_PREVIEW, &ctx))
    {
        m_preview = nullptr;        // a janela destruida ja apagou o objeto
        EndDialog(IDCANCEL);
        return FALSE;
    }
    m_preview->SendMessage(WM_INITIALUPDATE);

    m_btnSample.Create(L"&Sample code", vis | WS_TABSTOP | BS_PUSHBUTTON, r0, this, IDP_SAMPLE);
    m_btnSample.SetFont(font);
    m_btnDefaults.Create(L"&Reset to defaults", vis | WS_TABSTOP | BS_PUSHBUTTON, r0, this, IDP_DEFAULTS);
    m_btnDefaults.SetFont(font);
    m_btnOk.Create(L"OK", vis | WS_TABSTOP | BS_DEFPUSHBUTTON, r0, this, IDOK);
    m_btnOk.SetFont(font);
    m_btnCancel.Create(L"Cancel", vis | WS_TABSTOP | BS_PUSHBUTTON, r0, this, IDCANCEL);
    m_btnCancel.SetFont(font);

    SetWindowPos(NULL, 0, 0, 1180, 760, SWP_NOMOVE | SWP_NOZORDER);
    CenterWindow(m_pParentWnd);
    CRect rc;
    GetClientRect(rc);
    Layout(rc.Width(), rc.Height());

    m_ready = true;
    SetPreviewText(m_initialText.empty() ? std::wstring(g_sample) : m_initialText);
    Reformat();

    m_list.SetFocus();
    return FALSE;
}

void COEPlsSqlIndentDlg::Layout (int cx, int cy)
{
    if (!m_btnOk.GetSafeHwnd())
        return;

    const int m = 16, btnH = 32, btnW = 100, tabsH = 36, gap = 16;
    const int leftW = std::max(400, std::min(500, cx * 40 / 100));
    const int bottom = cy - m - btnH - m;

    m_tabs.MoveWindow(m, m, leftW, tabsH);
    m_list.MoveWindow(m, m + tabsH + 10, leftW, std::max(100, bottom - (m + tabsH + 10)));

    const int px = m + leftW + gap, pw = std::max(100, cx - px - m);
    m_previewCard = CRect(px, m + tabsH + 10, px + pw, bottom);
    if (m_preview)
        m_preview->MoveWindow(m_previewCard.left + 1, m_previewCard.top + 1, m_previewCard.Width() - 2, m_previewCard.Height() - 2);

    m_btnSample.MoveWindow(m, cy - m - btnH, 128, btnH);
    m_btnDefaults.MoveWindow(m + 136, cy - m - btnH, 150, btnH);
    m_btnCancel.MoveWindow(cx - m - btnW, cy - m - btnH, btnW, btnH);
    m_btnOk.MoveWindow(cx - m - btnW * 2 - 8, cy - m - btnH, btnW, btnH);

    Invalidate();
}

void COEPlsSqlIndentDlg::OnPaint ()
{
    CPaintDC dc(this);
    CRect rc;
    GetClientRect(rc);

    // titulo da previa, na altura das abas, e moldura fina em volta do editor
    CFont* old = dc.SelectObject(&ModernDraw::UiFontBold());
    dc.SetBkMode(TRANSPARENT);
    dc.SetTextColor(Text);
    CRect title(m_previewCard.left + 2, 16, m_previewCard.right, 16 + 36);
    dc.DrawText(L"Preview", title, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    const int tw = dc.GetTextExtent(L"Preview").cx;
    dc.SelectObject(&ModernDraw::UiFont());
    dc.SetTextColor(TextMuted);
    CRect hint(title.left + tw + 14, title.top, title.right, title.bottom);
    dc.DrawText(L"Type or paste your own code here. It is reformatted whenever an option changes.", hint,
                DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
    dc.FillSolidRect(m_previewCard.left, title.bottom - 1, m_previewCard.Width(), 1, Border);
    dc.SelectObject(old);

    CBrush border(Border);
    dc.FrameRect(m_previewCard, &border);
    CRect lr;
    m_list.GetWindowRect(lr);
    ScreenToClient(lr);
    lr.InflateRect(1, 1);
    dc.FrameRect(lr, &border);
}

void COEPlsSqlIndentDlg::OnSize (UINT nType, int cx, int cy)
{
    CDialog::OnSize(nType, cx, cy);
    Layout(cx, cy);
}

void COEPlsSqlIndentDlg::OnGetMinMaxInfo (MINMAXINFO* lpMMI)
{
    lpMMI->ptMinTrackSize.x = 940;
    lpMMI->ptMinTrackSize.y = 560;
    CDialog::OnGetMinMaxInfo(lpMMI);
}

void COEPlsSqlIndentDlg::OnDestroy ()
{
    // a view se apaga sozinha quando a janela e destruida; o documento e apagado no destrutor do dialogo
    if (m_preview && ::IsWindow(m_preview->m_hWnd))
        m_preview->DestroyWindow();
    m_preview = nullptr;
    CDialog::OnDestroy();
}

void COEPlsSqlIndentDlg::SetPreviewText (const std::wstring& text)
{
    if (!m_preview)
        return;

    std::wstring t;
    const std::vector<std::wstring> lines = SplitLines(text);
    for (size_t i = 0; i < lines.size(); ++i)
    {
        if (i) t += L'\n';
        t += lines[i];
    }

    const Position caret = m_preview->GetPosition();
    const int topmost = m_preview->GetTopmostLine();

    // InsertBlock insere no cursor sem apagar a selecao: o texto antigo e apagado antes
    m_preview->SetBlockMode(ebtStream);
    if (m_preview->GetLineCount() > 0)
    {
        m_preview->SelectAll();
        m_preview->DeleteBlock(true);
    }
    Position start;
    m_preview->MoveTo(start);
    m_preview->InsertBlock(t.c_str(), true, true);
    m_preview->ClearSelection(true);

    Position pos;
    pos.line = std::min(caret.line, std::max(0, m_preview->GetLineCount() - 1));
    pos.column = 0;
    m_preview->MoveTo(pos);
    m_preview->ScrollTo(std::min(topmost, std::max(0, m_preview->GetLineCount() - 1)));
}

std::vector<std::wstring> COEPlsSqlIndentDlg::GetPreviewLines () const
{
    std::vector<std::wstring> lines;
    if (m_preview)
        for (int i = 0, n = m_preview->GetLineCount(); i < n; ++i)
        {
            OEStringW buff;
            m_preview->GetLineW(i, buff);
            lines.emplace_back(buff.data(), buff.length());
        }
    if (lines.empty())
        lines.push_back(std::wstring());
    return lines;
}

void COEPlsSqlIndentDlg::Reformat ()
{
    if (!m_ready || !m_preview)
        return;

    // texto mudado a mao desde a ultima formatacao (ou primeira vez): vira o novo original
    const std::vector<std::wstring> lines = GetPreviewLines();
    if (m_source.empty() || lines != m_shown)
        m_source = lines;

    const std::vector<std::wstring> result = PlSqlIndenter::Reindent(m_source, 0, (int)m_source.size() - 1, m_options);
    if (result != lines)
    {
        std::wstring text;
        for (size_t i = 0; i < result.size(); ++i)
        {
            if (i) text += L'\n';
            text += result[i];
        }
        SetPreviewText(text);
    }
    m_shown = GetPreviewLines();
}

BOOL COEPlsSqlIndentDlg::OnCommand (WPARAM wParam, LPARAM lParam)
{
    const UINT id = LOWORD(wParam), code = HIWORD(wParam);
    if (m_ready)
    {
        if (id == IDP_TABS && code == BN_CLICKED)
        {
            m_list.SetTab(m_tabs.GetCurSel());
            return TRUE;
        }
        if (id == IDP_LIST && code == EN_CHANGE)
        {
            Reformat();
            return TRUE;
        }
        if (id == IDP_SAMPLE && code == BN_CLICKED)
        {
            SetPreviewText(g_sample);
            Reformat();
            m_preview->SetFocus();
            return TRUE;
        }
        if (id == IDP_DEFAULTS && code == BN_CLICKED)
        {
            PlSqlIndentOptions defaults;
            defaults.editorIndent   = m_options.editorIndent;
            defaults.tabSize        = m_options.tabSize;
            defaults.editorUsesTabs = m_options.editorUsesTabs;
            m_options = defaults;
            m_list.Refresh();
            Reformat();
            return TRUE;
        }
    }
    return CDialog::OnCommand(wParam, lParam);
}

BOOL COEPlsSqlIndentDlg::PreTranslateMessage (MSG* pMsg)
{
    // o editor da previa recebe todas as teclas (Enter, Tab, setas), menos Esc;
    // os atalhos de edicao viram comandos, como no editor principal
    if (m_preview && pMsg->hwnd == m_preview->m_hWnd && pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST)
    {
        if (pMsg->message == WM_KEYDOWN)
        {
            if (pMsg->wParam == VK_ESCAPE)
                return CDialog::PreTranslateMessage(pMsg);

            const bool ctrl  = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
            const bool shift = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;
            UINT cmd = 0;
            if (ctrl && !shift)
            {
                switch (pMsg->wParam)
                {
                case 'C': case VK_INSERT: cmd = ID_EDIT_COPY; break;
                case 'X': cmd = ID_EDIT_CUT; break;
                case 'V': cmd = ID_EDIT_PASTE; break;
                case 'Z': cmd = ID_EDIT_UNDO; break;
                case 'Y': cmd = ID_EDIT_REDO; break;
                case 'A': cmd = ID_EDIT_SELECT_ALL; break;
                }
            }
            else if (shift && !ctrl)
            {
                if (pMsg->wParam == VK_INSERT) cmd = ID_EDIT_PASTE;
                else if (pMsg->wParam == VK_DELETE) cmd = ID_EDIT_CUT;
            }
            if (cmd)
            {
                m_preview->SendMessage(WM_COMMAND, cmd);
                return TRUE;
            }
        }
        ::TranslateMessage(pMsg);
        ::DispatchMessage(pMsg);
        return TRUE;
    }
    return CDialog::PreTranslateMessage(pMsg);
}

void COEPlsSqlIndentDlg::OnOK ()
{
    CDialog::OnOK();
}
