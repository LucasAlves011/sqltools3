#include "stdafx.h"
#include "ValuePanelWnd.h"
#include "OCIGridView.h"
#include <COMMCTRL.H>
#include <gdiplus.h>
#include <algorithm>
#include <regex>
#include <COMMON/MyUtf.h>
#include <COMMON/StrHelpers.h>
#include "Theme/ModernTheme.h"
#include "Theme/ModernDark.h"
#include "resource.h"

namespace OG2
{

// IDs de controles
enum
{
    IDC_VP_BTN_FORMAT = 10101,
    IDC_VP_BTN_COPY   = 10102,
    IDC_VP_BTN_SAVE   = 10103,
    IDC_VP_GUTTER     = 10104,
    IDC_VP_EDIT       = 10105,
    IDC_VP_FIND       = 10106
};

// IDs de menu
enum
{
    IDM_FORMAT_BINARY  = 10201,
    IDM_FORMAT_JSON    = 10202,
    IDM_FORMAT_TEXT    = 10203,
    IDM_FORMAT_XML     = 10204,
    IDM_OPT_WORDWRAP   = 10205,
    IDM_OPT_AUTOFORMAT = 10206,
    IDM_OPT_COMPACT    = 10207,
    IDM_ENC_UTF8       = 10208,
    IDM_ENC_ANSI       = 10209,
    IDM_OPT_CDATA      = 10210
};

static const TCHAR* s_szSplitterClass = _T("SQLTools_ValueSplitter");
static const TCHAR* s_szGutterClass   = _T("SQLTools_ValueGutter");
static const TCHAR* s_szPanelClass    = _T("SQLTools_ValuePanel");
static const TCHAR* s_szFindClass     = _T("SQLTools_ValueFindBar");

// zoom do texto (Ctrl + roda): lembrado ate fechar o programa; o RTF carregado a cada valor o devolveria a 100%
static int s_zoomNum = 0, s_zoomDen = 0;

static const TCHAR* s_szProfileSection = _T("GridValuePanel");


/////////////////////////////////////////////////////////////////////////////
// Visual moderno: paleta (ModernTheme) e icones vetoriais suaves (GDI+)
/////////////////////////////////////////////////////////////////////////////

static const int kHeaderH  = 30;
static const int kToolbarH = 36;

static Gdiplus::Color Clr (COLORREF c, BYTE a = 255)
{
    return Gdiplus::Color(a, GetRValue(c), GetGValue(c), GetBValue(c));
}

static void RoundRectPath (Gdiplus::GraphicsPath& path, const Gdiplus::RectF& r, float rad)
{
    float d = rad * 2;
    path.AddArc(r.X, r.Y, d, d, 180, 90);
    path.AddArc(r.GetRight() - d, r.Y, d, d, 270, 90);
    path.AddArc(r.GetRight() - d, r.GetBottom() - d, d, d, 0, 90);
    path.AddArc(r.X, r.GetBottom() - d, d, d, 90, 90);
    path.CloseFigure();
}

static void FillRoundRect (Gdiplus::Graphics& g, const CRect& rc, float rad, COLORREF color)
{
    Gdiplus::GraphicsPath path;
    RoundRectPath(path, Gdiplus::RectF((float)rc.left, (float)rc.top, (float)rc.Width(), (float)rc.Height()), rad);
    Gdiplus::SolidBrush brush(Clr(color));
    g.FillPath(&brush, &path);
}

static void MakeRoundPen (Gdiplus::Pen& pen)
{
    pen.SetStartCap(Gdiplus::LineCapRound);
    pen.SetEndCap(Gdiplus::LineCapRound);
    pen.SetLineJoin(Gdiplus::LineJoinRound);
}

// icones de 16x16 centrados em (cx, cy)
static void DrawChevron (Gdiplus::Graphics& g, float cx, float cy, int dir /*0 esq,1 dir,2 baixo*/, COLORREF color, float w, float s = 1.0f)
{
    Gdiplus::Pen pen(Clr(color), w); MakeRoundPen(pen);
    Gdiplus::PointF p[3];
    if (dir == 0)      { p[0] = Gdiplus::PointF(cx + 2*s, cy - 4*s); p[1] = Gdiplus::PointF(cx - 2*s, cy); p[2] = Gdiplus::PointF(cx + 2*s, cy + 4*s); }
    else if (dir == 1) { p[0] = Gdiplus::PointF(cx - 2*s, cy - 4*s); p[1] = Gdiplus::PointF(cx + 2*s, cy); p[2] = Gdiplus::PointF(cx - 2*s, cy + 4*s); }
    else if (dir == 3) { p[0] = Gdiplus::PointF(cx - 3.5f*s, cy + 1.5f*s); p[1] = Gdiplus::PointF(cx, cy - 2*s); p[2] = Gdiplus::PointF(cx + 3.5f*s, cy + 1.5f*s); }
    else               { p[0] = Gdiplus::PointF(cx - 3.5f*s, cy - 1.5f*s); p[1] = Gdiplus::PointF(cx, cy + 2*s); p[2] = Gdiplus::PointF(cx + 3.5f*s, cy - 1.5f*s); }
    g.DrawLines(&pen, p, 3);
}

static void DrawCloseGlyph (Gdiplus::Graphics& g, float cx, float cy, COLORREF color)
{
    Gdiplus::Pen pen(Clr(color), 1.5f); MakeRoundPen(pen);
    g.DrawLine(&pen, cx - 4, cy - 4, cx + 4, cy + 4);
    g.DrawLine(&pen, cx - 4, cy + 4, cx + 4, cy - 4);
}

// Icones Tabler da tira ValuePanelIcons.bmp (gerada por tools/GenToolbarIcons), os mesmos das barras.
// Posicoes: 0 code, 1 copy, 2 salvar, 3 seta do menu; 4..7 = os mesmos na cor de realce.
enum { VI_CODE = 0, VI_COPY = 1, VI_SAVE = 2, VI_DROPDOWN = 3, VI_HILITE = 4 };

static CImageList& ValueIcons ()
{
    static CImageList s_list;
    static unsigned s_generation = 0;
    if (s_generation != ModernTheme::Generation() && s_list.GetSafeHandle())
        s_list.DeleteImageList();           // a paleta mudou: icones refeitos com os tons do tema atual
    s_generation = ModernTheme::Generation();
    if (!s_list.GetSafeHandle())
    {
        HBITMAP hBmp = (HBITMAP)::LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDB_VALUE_ICONS),
                                            IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
        if (hBmp)
        {
            ModernDark::RecolorBitmap(hBmp);      // tema escuro: icones claros
            s_list.Create(16, 16, ILC_COLOR32, 8, 0);
            CBitmap bmp;
            bmp.Attach(hBmp);
            s_list.Add(&bmp, (CBitmap*)NULL);
        }
    }
    return s_list;
}

static void DrawValueIcon (CDC& dc, int index, float cx, float cy, bool hilite)
{
    CImageList& list = ValueIcons();
    if (list.GetSafeHandle())
        list.Draw(&dc, index + (hilite ? VI_HILITE : 0), CPoint((int)(cx - 8), (int)(cy - 8)), ILD_TRANSPARENT);
}

/////////////////////////////////////////////////////////////////////////////
// CModernButton
/////////////////////////////////////////////////////////////////////////////

CModernButton::CModernButton()
    : m_iconType(ICON_NONE)
    , m_bDropdown(false)
    , m_bHover(false)
    , m_bTrackingMouse(false)
{
}

CModernButton::~CModernButton()
{
}

BEGIN_MESSAGE_MAP(CModernButton, CButton)
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSELEAVE()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

BOOL CModernButton::OnEraseBkgnd(CDC*)
{
    return TRUE;
}

void CModernButton::OnMouseMove(UINT nFlags, CPoint point)
{
    if (!m_bTrackingMouse)
    {
        TRACKMOUSEEVENT tme;
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = m_hWnd;
        _TrackMouseEvent(&tme);
        m_bTrackingMouse = true;
    }

    if (!m_bHover)
    {
        m_bHover = true;
        Invalidate();
    }

    CButton::OnMouseMove(nFlags, point);
}

void CModernButton::OnMouseLeave()
{
    m_bTrackingMouse = false;
    if (m_bHover)
    {
        m_bHover = false;
        Invalidate();
    }
    CButton::OnMouseLeave();
}

void CModernButton::DrawItem(LPDRAWITEMSTRUCT lpDIS)
{
    using namespace ModernTheme;

    CDC dc;
    dc.Attach(lpDIS->hDC);
    CRect rc = lpDIS->rcItem;

    bool isPressed = (lpDIS->itemState & ODS_SELECTED) != 0;
    bool isDisabled = (lpDIS->itemState & ODS_DISABLED) != 0;
    bool isHover = m_bHover && !isDisabled;

    CDC memDC;
    memDC.CreateCompatibleDC(&dc);
    CBitmap memBmp;
    memBmp.CreateCompatibleBitmap(&dc, rc.Width(), rc.Height());
    CBitmap* pOldBmp = memDC.SelectObject(&memBmp);

    // fundo da toolbar atras do botao
    memDC.FillSolidRect(0, 0, rc.Width(), rc.Height(), Surface);

    // botao "chip": tom suave em repouso, destaque no hover e ao pressionar
    COLORREF bgCol = SurfaceAlt;
    if (isPressed)    bgCol = AccentStrong;
    else if (isHover) bgCol = AccentTint;

    CRect rcBtn(0, 0, rc.Width(), rc.Height());

    Gdiplus::Graphics g(memDC.GetSafeHdc());
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    FillRoundRect(g, rcBtn, 6.0f, bgCol);

    float centerY = rcBtn.Height() / 2.0f;
    int textLeft = 10;
    bool hiliteIcon = !isDisabled && (isHover || isPressed);

    switch (m_iconType)
    {
    case ICON_FORMAT: DrawValueIcon(memDC, VI_CODE, 18.0f, centerY, !isDisabled);   textLeft = 32; break;
    case ICON_COPY:   DrawValueIcon(memDC, VI_COPY, 18.0f, centerY, hiliteIcon);    textLeft = 32; break;
    case ICON_SAVE:   DrawValueIcon(memDC, VI_SAVE, 18.0f, centerY, hiliteIcon);    textLeft = 32; break;
    default: break;
    }

    // texto do botao
    CString text;
    GetWindowText(text);

    int arrowIdx = text.Find(_T('\x25BC'));
    if (arrowIdx != -1)
    {
        text = text.Left(arrowIdx);
        text.TrimRight();
    }

    CFont* pFont = GetFont();
    CFont* pOldF = memDC.SelectObject(pFont ? pFont : CFont::FromHandle((HFONT)::GetStockObject(DEFAULT_GUI_FONT)));
    memDC.SetBkMode(TRANSPARENT);
    memDC.SetTextColor(isDisabled ? TextMuted : Text);

    int textRight = m_bDropdown ? rcBtn.right - 18 : rcBtn.right - 6;
    CRect rcText(textLeft, rcBtn.top, textRight, rcBtn.bottom);
    memDC.DrawText(text, rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    memDC.SelectObject(pOldF);

    // seta do dropdown
    if (m_bDropdown)
        DrawValueIcon(memDC, VI_DROPDOWN, rcBtn.right - 14.0f, centerY, isHover);

    dc.BitBlt(rc.left, rc.top, rc.Width(), rc.Height(), &memDC, 0, 0, SRCCOPY);
    memDC.SelectObject(pOldBmp);
    dc.Detach();
}

/////////////////////////////////////////////////////////////////////////////
// CValueSplitterBar
/////////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CValueSplitterBar, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_CAPTURECHANGED()
    ON_WM_SETCURSOR()
    ON_WM_MOUSELEAVE()
END_MESSAGE_MAP()

CValueSplitterBar::CValueSplitterBar(OciGridView* pGrid)
    : m_pGrid(pGrid)
    , m_bCollapsed(false)
    , m_bDragging(false)
    , m_bHoverPill(false)
    , m_bTrackingMouse(false)
    , m_dragStartX(0)
    , m_dragStartWidth(0)
{
}

CValueSplitterBar::~CValueSplitterBar()
{
}

BOOL CValueSplitterBar::Create(CWnd* pParentWnd, UINT nID)
{
    HINSTANCE hInst = AfxGetInstanceHandle();
    WNDCLASS wc;
    if (!::GetClassInfo(hInst, s_szSplitterClass, &wc))
    {
        memset(&wc, 0, sizeof(wc));
        wc.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = ::DefWindowProc;
        wc.hInstance = hInst;
        wc.hCursor = ::LoadCursor(NULL, IDC_ARROW);
        wc.lpszClassName = s_szSplitterClass;
        AfxRegisterClass(&wc);
    }

    return CWnd::Create(s_szSplitterClass, _T(""),
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        CRect(0, 0, 0, 0), pParentWnd, nID);
}

void CValueSplitterBar::SetCollapsed(bool bCollapsed)
{
    if (m_bCollapsed != bCollapsed)
    {
        m_bCollapsed = bCollapsed;
        Invalidate();
    }
}

CRect CValueSplitterBar::GetPillRect() const
{
    CRect rc;
    GetClientRect(rc);
    int pillWidth = (std::max)(6, rc.Width() - 2);
    int pillHeight = 46;
    int y = (rc.Height() - pillHeight) / 2;
    return CRect(rc.left + 1, y, rc.left + 1 + pillWidth, y + pillHeight);
}

BOOL CValueSplitterBar::OnEraseBkgnd(CDC*)
{
    return TRUE;
}

void CValueSplitterBar::OnPaint()
{
    using namespace ModernTheme;

    CPaintDC dc(this);
    CRect rc;
    GetClientRect(rc);

    CDC memDC;
    memDC.CreateCompatibleDC(&dc);
    CBitmap memBmp;
    memBmp.CreateCompatibleBitmap(&dc, rc.Width(), rc.Height());
    CBitmap* pOldBmp = memDC.SelectObject(&memBmp);

    // fundo e linha lateral esquerda
    memDC.FillSolidRect(rc, Surface);
    memDC.FillSolidRect(rc.left, rc.top, 1, rc.Height(), Border);

    // botao de colapsar em formato de pilula, discreto
    CRect rcPill = GetPillRect();
    Gdiplus::Graphics g(memDC.GetSafeHdc());
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    FillRoundRect(g, rcPill, 4.0f, m_bHoverPill ? AccentTint : SurfaceAlt);

    float midX = rcPill.CenterPoint().x + 0.5f;
    float midY = (float)rcPill.CenterPoint().y;
    // painel colapsado: chevron para a esquerda; expandido: para a direita
    DrawChevron(g, midX, midY, m_bCollapsed ? 0 : 1, m_bHoverPill ? Accent : TextMuted, 1.6f, 0.9f);

    dc.BitBlt(0, 0, rc.Width(), rc.Height(), &memDC, 0, 0, SRCCOPY);
    memDC.SelectObject(pOldBmp);
}

void CValueSplitterBar::OnMouseMove(UINT nFlags, CPoint point)
{
    if (!m_bTrackingMouse)
    {
        TRACKMOUSEEVENT tme;
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = m_hWnd;
        _TrackMouseEvent(&tme);
        m_bTrackingMouse = true;
    }

    CRect rcPill = GetPillRect();
    bool hover = (rcPill.PtInRect(point) != FALSE);
    if (hover != m_bHoverPill)
    {
        m_bHoverPill = hover;
        Invalidate();
    }

    if (m_bDragging && (nFlags & MK_LBUTTON))
    {
        CPoint ptScreen = point;
        ClientToScreen(&ptScreen);
        int deltaX = m_dragStartX - ptScreen.x;
        int newWidth = m_dragStartWidth + deltaX;
        m_pGrid->SetValuePanelWidth(newWidth);
    }

    CWnd::OnMouseMove(nFlags, point);
}

void CValueSplitterBar::OnMouseLeave()
{
    m_bTrackingMouse = false;
    if (m_bHoverPill)
    {
        m_bHoverPill = false;
        Invalidate();
    }
}

void CValueSplitterBar::OnLButtonDown(UINT nFlags, CPoint point)
{
    CRect rcPill = GetPillRect();
    if (rcPill.PtInRect(point))
    {
        m_pGrid->ToggleValuePanel();
        return;
    }

    if (!m_bCollapsed)
    {
        m_bDragging = true;
        SetCapture();
        CPoint ptScreen = point;
        ClientToScreen(&ptScreen);
        m_dragStartX = ptScreen.x;
        m_dragStartWidth = m_pGrid->GetValuePanelWidth();
    }

    CWnd::OnLButtonDown(nFlags, point);
}

void CValueSplitterBar::OnLButtonUp(UINT nFlags, CPoint point)
{
    if (m_bDragging)
    {
        m_bDragging = false;
        ReleaseCapture();
    }
    CWnd::OnLButtonUp(nFlags, point);
}

void CValueSplitterBar::OnCaptureChanged(CWnd* pWnd)
{
    m_bDragging = false;
    CWnd::OnCaptureChanged(pWnd);
}

BOOL CValueSplitterBar::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
    CPoint pt;
    GetCursorPos(&pt);
    ScreenToClient(&pt);

    CRect rcPill = GetPillRect();
    if (rcPill.PtInRect(pt))
    {
        ::SetCursor(::LoadCursor(NULL, IDC_HAND));
        return TRUE;
    }

    if (!m_bCollapsed)
    {
        ::SetCursor(::LoadCursor(NULL, IDC_SIZEWE));
        return TRUE;
    }

    return CWnd::OnSetCursor(pWnd, nHitTest, message);
}

/////////////////////////////////////////////////////////////////////////////
// CValueLineGutter
/////////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CValueLineGutter, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

CValueLineGutter::CValueLineGutter()
    : m_pEdit(NULL)
    , m_pFont(NULL)
{
}

CValueLineGutter::~CValueLineGutter()
{
}

BOOL CValueLineGutter::Create(CWnd* pParentWnd, UINT nID, CRichEditCtrl* pEdit)
{
    m_pEdit = pEdit;

    HINSTANCE hInst = AfxGetInstanceHandle();
    WNDCLASS wc;
    if (!::GetClassInfo(hInst, s_szGutterClass, &wc))
    {
        memset(&wc, 0, sizeof(wc));
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = ::DefWindowProc;
        wc.hInstance = hInst;
        wc.hCursor = ::LoadCursor(NULL, IDC_ARROW);
        wc.lpszClassName = s_szGutterClass;
        AfxRegisterClass(&wc);
    }

    return CWnd::Create(s_szGutterClass, _T(""),
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        CRect(0, 0, 0, 0), pParentWnd, nID);
}

void CValueLineGutter::SetFont(CFont* pFont)
{
    m_pFont = pFont;
    Invalidate();
}

BOOL CValueLineGutter::OnEraseBkgnd(CDC*)
{
    return TRUE;
}

void CValueLineGutter::OnPaint()
{
    CPaintDC dc(this);
    CRect rc;
    GetClientRect(rc);

    CDC memDC;
    memDC.CreateCompatibleDC(&dc);
    CBitmap memBmp;
    memBmp.CreateCompatibleBitmap(&dc, rc.Width(), rc.Height());
    CBitmap* pOldBmp = memDC.SelectObject(&memBmp);

    // Fundo do gutter
    memDC.FillSolidRect(rc, ModernTheme::SurfaceAlt);

    // Borda vertical direita do gutter
    CPen borderPen(PS_SOLID, 1, ModernTheme::Border);
    CPen* pOldPen = memDC.SelectObject(&borderPen);
    memDC.MoveTo(rc.right - 1, rc.top);
    memDC.LineTo(rc.right - 1, rc.bottom);

    if (m_pEdit && m_pEdit->GetSafeHwnd())
    {
        CFont* pOldFont = NULL;
        if (m_pFont)
            pOldFont = memDC.SelectObject(m_pFont);

        memDC.SetBkMode(TRANSPARENT);
        memDC.SetTextColor(ModernTheme::TextMuted);

        int firstLine = m_pEdit->GetFirstVisibleLine();
        int lineCount = m_pEdit->GetLineCount();

        for (int line = firstLine; line < lineCount; ++line)
        {
            int charIndex = m_pEdit->LineIndex(line);
            if (charIndex < 0 && line > 0)
                break;

            CPoint pt = m_pEdit->GetCharPos(charIndex);
            if (pt.y > rc.bottom)
                break;

            if (pt.y >= -20)
            {
                CString sLine;
                sLine.Format(_T("%d"), line + 1);
                CRect rcText(rc.left + 2, pt.y, rc.right - 6, pt.y + 18);
                memDC.DrawText(sLine, rcText, DT_RIGHT | DT_SINGLELINE | DT_TOP);
            }
        }

        if (pOldFont)
            memDC.SelectObject(pOldFont);
    }

    memDC.SelectObject(pOldPen);
    dc.BitBlt(0, 0, rc.Width(), rc.Height(), &memDC, 0, 0, SRCCOPY);
    memDC.SelectObject(pOldBmp);
}

/////////////////////////////////////////////////////////////////////////////
// Busca no texto: motor (texto simples ou expressao regular) usado pela barra de busca do painel
/////////////////////////////////////////////////////////////////////////////

namespace
{
    struct FindOptions
    {
        std::wstring query;
        bool matchCase = false, wholeWord = false, regex = false, backslash = false;
    };

    const size_t kMaxMatches = 200000;      // alem disso a contagem para ("200000+")
    const size_t kMaxMarks   = 5000;        // "marcar todos" pinta no maximo estas ocorrencias

    inline bool IsWordChar (wchar_t c)
    {
        return c == L'_' || ::IsCharAlphaNumericW(c);
    }

    COLORREF MarkColor ()
    {
        return ModernTheme::IsDark() ? RGB(112, 92, 0) : RGB(255, 232, 128);
    }

    // texto simples digitado pelo usuario: sequencias com barra (\t, \n, \ddd) se pedidas, e quebras de linha como \n
    std::wstring PlainQuery (const FindOptions& o)
    {
        std::wstring q = o.query;
        if (o.backslash)
        {
            std::string buff;
            Common::to_unprintable_str(Common::str(q).c_str(), buff, false);
            q = Common::wstr(buff);
        }
        std::wstring out;
        for (size_t i = 0; i < q.size(); ++i)
        {
            if (q[i] == L'\r')
            {
                out += L'\n';
                if (i + 1 < q.size() && q[i + 1] == L'\n')
                    ++i;
            }
            else
                out += q[i];
        }
        return out;
    }

    // 'text' usa \n como quebra de linha; as posicoes devolvidas valem para o RichEdit (mesmo tamanho)
    bool FindAllMatches (const std::wstring& text, const FindOptions& o, std::vector<CHARRANGE>& out,
                         bool& capped, CString& error)
    {
        capped = false;
        out.clear();
        if (o.query.empty())
            return true;

        try
        {
            if (o.regex)
            {
                auto flags = std::regex_constants::ECMAScript | std::regex_constants::multiline;
                if (!o.matchCase)
                    flags |= std::regex_constants::icase;
                std::wstring pattern = o.wholeWord ? L"\\b(?:" + o.query + L")\\b" : o.query;
                std::wregex re(pattern, flags);

                for (std::wsregex_iterator it(text.begin(), text.end(), re), end; it != end; ++it)
                {
                    if (it->length() == 0)
                        continue;
                    if (out.size() >= kMaxMatches)
                    {
                        capped = true;
                        break;
                    }
                    CHARRANGE r;
                    r.cpMin = (LONG)it->position();
                    r.cpMax = r.cpMin + (LONG)it->length();
                    out.push_back(r);
                }
            }
            else
            {
                std::wstring needle = PlainQuery(o);
                if (needle.empty())
                    return true;

                std::wstring lowered;
                const std::wstring* hay = &text;
                if (!o.matchCase)
                {
                    lowered = text;
                    ::CharLowerBuffW(&lowered[0], (DWORD)lowered.size());
                    ::CharLowerBuffW(&needle[0], (DWORD)needle.size());
                    hay = &lowered;
                }

                const bool wordL = IsWordChar(needle.front());
                const bool wordR = IsWordChar(needle.back());
                size_t pos = 0;
                while ((pos = hay->find(needle, pos)) != std::wstring::npos)
                {
                    const size_t end = pos + needle.size();
                    bool ok = true;
                    if (o.wholeWord)
                    {
                        if (wordL && pos > 0 && IsWordChar(text[pos - 1]))
                            ok = false;
                        if (wordR && end < text.size() && IsWordChar(text[end]))
                            ok = false;
                    }
                    if (!ok)
                    {
                        ++pos;
                        continue;
                    }
                    if (out.size() >= kMaxMatches)
                    {
                        capped = true;
                        break;
                    }
                    CHARRANGE r;
                    r.cpMin = (LONG)pos;
                    r.cpMax = (LONG)end;
                    out.push_back(r);
                    pos = end;
                }
            }
        }
        catch (const std::regex_error&)
        {
            out.clear();
            error = L"Express\xE3o regular inv\xE1lida";
            return false;
        }
        catch (const std::exception&)
        {
            out.clear();
            error = L"Express\xE3o regular complexa demais";
            return false;
        }
        return true;
    }
}

/////////////////////////////////////////////////////////////////////////////
// CValueFindBar
/////////////////////////////////////////////////////////////////////////////

enum { IDC_VPF_EDIT = 10110 };

static const int kFindRowH   = 24;
static const int kFindBarH   = 64;
static const TCHAR* s_szFindProfile = _T("GridValuePanel");

BEGIN_MESSAGE_MAP(CValueFindBar, CWnd)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_LBUTTONDOWN()
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSELEAVE()
    ON_WM_CTLCOLOR()
    ON_EN_CHANGE(IDC_VPF_EDIT, OnEditChange)
    ON_EN_SETFOCUS(IDC_VPF_EDIT, OnEditFocusChanged)
    ON_EN_KILLFOCUS(IDC_VPF_EDIT, OnEditFocusChanged)
END_MESSAGE_MAP()

CValueFindBar::CValueFindBar(CValuePanelWnd* pPanel)
    : m_matchCase(false), m_wholeWord(false), m_regex(false), m_backslash(false), m_markAll(false)
    , m_pPanel(pPanel)
    , m_brushColor(CLR_INVALID)
    , m_hot(H_NONE)
    , m_bTrackingMouse(false)
    , m_bSettingText(false)
    , m_current(-1)
    , m_total(0)
    , m_capped(false)
{
}

CValueFindBar::~CValueFindBar()
{
}

int CValueFindBar::Height ()
{
    return kFindBarH;
}

BOOL CValueFindBar::Create(CWnd* pParentWnd, UINT nID)
{
    HINSTANCE hInst = AfxGetInstanceHandle();
    WNDCLASS wc;
    if (!::GetClassInfo(hInst, s_szFindClass, &wc))
    {
        memset(&wc, 0, sizeof(wc));
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = ::DefWindowProc;
        wc.hInstance = hInst;
        wc.hCursor = ::LoadCursor(NULL, IDC_ARROW);
        wc.lpszClassName = s_szFindClass;
        AfxRegisterClass(&wc);
    }

    return CWnd::Create(s_szFindClass, _T(""),
        WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        CRect(0, 0, 0, 0), pParentWnd, nID);
}

int CValueFindBar::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CWnd::OnCreate(lpCreateStruct) == -1)
        return -1;

    m_fontUi.CreatePointFont(90, _T("Segoe UI"));
    m_fontMono.CreatePointFont(90, _T("Consolas"));

    m_edit.Create(WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, CRect(0, 0, 0, 0), this, IDC_VPF_EDIT);
    m_edit.SetFont(&m_fontUi);
    m_edit.SendMessage(EM_SETCUEBANNER, TRUE, (LPARAM)L"Localizar");

    CWinApp* app = AfxGetApp();
    m_matchCase = app->GetProfileInt(s_szFindProfile, _T("FindMatchCase"), 0) != 0;
    m_wholeWord = app->GetProfileInt(s_szFindProfile, _T("FindWholeWord"), 0) != 0;
    m_regex     = app->GetProfileInt(s_szFindProfile, _T("FindRegex"), 0) != 0;
    m_backslash = app->GetProfileInt(s_szFindProfile, _T("FindBackslash"), 0) != 0;
    m_markAll   = app->GetProfileInt(s_szFindProfile, _T("FindMarkAll"), 0) != 0;

    m_tip.Create(this, TTS_ALWAYSTIP | TTS_NOPREFIX);
    static const struct { Hit hit; const wchar_t* text; } tips[] = {
        { H_CASE,      L"Diferenciar mai\xFAsculas de min\xFAsculas (Alt+C)" },
        { H_WORD,      L"Palavra inteira (Alt+W)" },
        { H_REGEX,     L"Express\xE3o regular (Alt+R)" },
        { H_BACKSLASH, L"Interpretar sequ\xEAncias com barra invertida: \\t, \\n, \\ddd... (Alt+B)" },
        { H_MARK,      L"Marcar todas as ocorr\xEAncias no texto (Alt+M)" },
        { H_PREV,      L"Ocorr\xEAncia anterior (Shift+Enter)" },
        { H_NEXT,      L"Pr\xF3xima ocorr\xEAncia (Enter)" },
        { H_CLOSE,     L"Fechar (Esc)" },
    };
    for (const auto& t : tips)
        m_tip.AddTool(this, t.text, CRect(0, 0, 0, 0), (UINT_PTR)t.hit);
    m_tip.Activate(TRUE);

    return 0;
}

void CValueFindBar::OnSize(UINT nType, int cx, int cy)
{
    CWnd::OnSize(nType, cx, cy);
    Layout(cx, cy);
}

void CValueFindBar::Layout (int cx, int)
{
    const int pad = 10;
    const int row1 = 7;
    const int row2 = row1 + kFindRowH + 6;
    const int h = kFindRowH;

    m_rcClose   = CRect(cx - pad - 24, row1, cx - pad, row1 + h);
    m_rcNext    = CRect(m_rcClose.left - 24, row1, m_rcClose.left, row1 + h);
    m_rcPrev    = CRect(m_rcNext.left - 24, row1, m_rcNext.left, row1 + h);
    m_rcEditBox = CRect(pad, row1, (std::max)(pad + 60, (int)m_rcPrev.left - 6), row1 + h);

    // altura do campo de texto = altura da fonte, centralizado na caixa
    int editH = 16;
    if (CDC* dc = GetDC())
    {
        CFont* old = dc->SelectObject(&m_fontUi);
        TEXTMETRIC tm;
        if (dc->GetTextMetrics(&tm))
            editH = tm.tmHeight + 1;
        dc->SelectObject(old);
        ReleaseDC(dc);
    }
    m_edit.SetWindowPos(NULL, m_rcEditBox.left + 8, m_rcEditBox.top + (h - editH) / 2,
        (std::max)(10, (int)m_rcEditBox.Width() - 16), editH, SWP_NOZORDER | SWP_NOACTIVATE);

    int x = pad;
    const int bw = 28;
    m_rcCase      = CRect(x, row2, x + bw, row2 + h); x += bw + 2;
    m_rcWord      = CRect(x, row2, x + bw, row2 + h); x += bw + 2;
    m_rcRegex     = CRect(x, row2, x + bw, row2 + h); x += bw + 2;
    m_rcBackslash = CRect(x, row2, x + bw, row2 + h); x += bw + 10;
    m_rcMark      = CRect(x, row2, x + 96, row2 + h);
    m_rcStatus    = CRect(m_rcMark.right + 10, row2, (std::max)((int)m_rcMark.right + 10, cx - pad), row2 + h);

    if (m_tip.m_hWnd)
    {
        for (Hit hit : { H_CASE, H_WORD, H_REGEX, H_BACKSLASH, H_MARK, H_PREV, H_NEXT, H_CLOSE })
            m_tip.SetToolRect(this, (UINT_PTR)hit, RectOf(hit));
    }
    Invalidate();
}

CRect CValueFindBar::RectOf (Hit h) const
{
    switch (h)
    {
    case H_CASE:      return m_rcCase;
    case H_WORD:      return m_rcWord;
    case H_REGEX:     return m_rcRegex;
    case H_BACKSLASH: return m_rcBackslash;
    case H_MARK:      return m_rcMark;
    case H_PREV:      return m_rcPrev;
    case H_NEXT:      return m_rcNext;
    case H_CLOSE:     return m_rcClose;
    default:          return CRect(0, 0, 0, 0);
    }
}

CValueFindBar::Hit CValueFindBar::HitTest (CPoint pt) const
{
    for (Hit h : { H_CASE, H_WORD, H_REGEX, H_BACKSLASH, H_MARK, H_PREV, H_NEXT, H_CLOSE })
        if (RectOf(h).PtInRect(pt))
            return h;
    return H_NONE;
}

std::wstring CValueFindBar::GetQuery () const
{
    CString s;
    if (m_edit.m_hWnd)
        m_edit.GetWindowText(s);
    return std::wstring((LPCTSTR)s);
}

void CValueFindBar::SetQuery (const std::wstring& text)
{
    m_bSettingText = true;
    m_edit.SetWindowText(text.c_str());
    m_bSettingText = false;
}

void CValueFindBar::FocusEdit (bool selectAll)
{
    m_edit.SetFocus();
    if (selectAll)
        m_edit.SetSel(0, -1);
}

bool CValueFindBar::IsEditFocused () const
{
    return m_edit.m_hWnd && ::GetFocus() == m_edit.m_hWnd;
}

void CValueFindBar::SelectAllText ()
{
    m_edit.SetSel(0, -1);
}

void CValueFindBar::CopyText ()
{
    m_edit.Copy();
}

void CValueFindBar::SetStatus (int current, int total, bool capped, const CString& error)
{
    m_current = current;
    m_total = total;
    m_capped = capped;
    m_error = error;
    InvalidateRect(m_rcStatus, FALSE);
    InvalidateRect(m_rcPrev, FALSE);
    InvalidateRect(m_rcNext, FALSE);
}

void CValueFindBar::ThemeChanged ()
{
    Invalidate();
    if (m_edit.m_hWnd)
        m_edit.Invalidate();
}

BOOL CValueFindBar::OnEraseBkgnd(CDC*)
{
    return TRUE;
}

HBRUSH CValueFindBar::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    if (pWnd && pWnd->m_hWnd == m_edit.m_hWnd)
    {
        pDC->SetTextColor(ModernTheme::Text);
        pDC->SetBkColor(ModernTheme::SurfaceAlt);
        return ModernTheme::CachedBrush(m_brushEdit, m_brushColor, ModernTheme::SurfaceAlt);
    }
    return CWnd::OnCtlColor(pDC, pWnd, nCtlColor);
}

void CValueFindBar::OnEditChange()
{
    if (!m_bSettingText && m_pPanel)
        m_pPanel->OnFindChanged(true);
}

void CValueFindBar::OnEditFocusChanged()
{
    InvalidateRect(m_rcEditBox, FALSE);
}

void CValueFindBar::OnPaint()
{
    using namespace ModernTheme;

    CPaintDC dc(this);
    CRect rc;
    GetClientRect(rc);

    CDC mem;
    mem.CreateCompatibleDC(&dc);
    CBitmap bmp;
    bmp.CreateCompatibleBitmap(&dc, rc.Width(), rc.Height());
    CBitmap* pOldBmp = mem.SelectObject(&bmp);

    mem.FillSolidRect(rc, Surface);
    mem.FillSolidRect(rc.left, rc.bottom - 1, rc.Width(), 1, Border);

    Gdiplus::Graphics g(mem.GetSafeHdc());
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

    // caixa do campo de texto (com contorno de destaque quando tem o foco)
    FillRoundRect(g, m_rcEditBox, 5.0f, SurfaceAlt);
    if (IsEditFocused())
    {
        Gdiplus::GraphicsPath path;
        RoundRectPath(path, Gdiplus::RectF((float)m_rcEditBox.left + 0.5f, (float)m_rcEditBox.top + 0.5f,
                                           (float)m_rcEditBox.Width() - 1.0f, (float)m_rcEditBox.Height() - 1.0f), 5.0f);
        Gdiplus::Pen pen(Clr(Accent), 1.0f);
        g.DrawPath(&pen, &path);
    }

    const struct { Hit hit; bool on; } toggles[] = {
        { H_CASE, m_matchCase }, { H_WORD, m_wholeWord }, { H_REGEX, m_regex }, { H_BACKSLASH, m_backslash }, { H_MARK, m_markAll } };
    for (const auto& t : toggles)
    {
        CRect r = RectOf(t.hit);
        if (t.on)
            FillRoundRect(g, r, 5.0f, AccentTint);
        else if (m_hot == t.hit)
            FillRoundRect(g, r, 5.0f, Hover);
    }
    for (Hit hit : { H_PREV, H_NEXT, H_CLOSE })
        if (m_hot == hit && (hit == H_CLOSE || m_total > 0))
            FillRoundRect(g, RectOf(hit), 5.0f, Hover);

    mem.SetBkMode(TRANSPARENT);

    // rotulos dos botoes de opcao
    CFont* pOldFont = mem.SelectObject(&m_fontMono);
    auto label = [&](const CRect& r, const wchar_t* text, bool on)
    {
        mem.SetTextColor(on ? Accent : Text);
        CRect rr = r;
        mem.DrawText(text, -1, rr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    };
    label(m_rcCase,      L"Aa", m_matchCase);
    label(m_rcWord,      L"ab", m_wholeWord);
    label(m_rcRegex,     L".*", m_regex);
    label(m_rcBackslash, L"\\n", m_backslash);

    // palavra inteira: colchete embaixo das letras
    {
        COLORREF c = m_wholeWord ? Accent : Text;
        Gdiplus::Pen pen(Clr(c), 1.0f);
        float cx = (float)m_rcWord.CenterPoint().x, y = (float)m_rcWord.CenterPoint().y + 8.0f;
        g.DrawLine(&pen, cx - 7, y, cx + 7, y);
        g.DrawLine(&pen, cx - 7, y - 2, cx - 7, y);
        g.DrawLine(&pen, cx + 7, y - 2, cx + 7, y);
    }

    // botao "Marcar todos" e a contagem
    mem.SelectObject(&m_fontUi);
    label(m_rcMark, L"Marcar todos", m_markAll);

    CString status;
    bool danger = false;
    if (!m_error.IsEmpty())
    {
        status = m_error;
        danger = true;
    }
    else if (m_edit.m_hWnd && m_edit.GetWindowTextLength() > 0)
    {
        if (m_total == 0)
        {
            status = _T("Sem resultados");
            danger = true;
        }
        else
        {
            CString total;
            total.Format(_T("%d%s"), m_total, m_capped ? _T("+") : _T(""));
            if (m_current >= 0)
                status.Format(_T("%d de %s"), m_current + 1, (LPCTSTR)total);
            else if (m_total == 1 && !m_capped)
                status = _T("1 resultado");
            else
                status.Format(_T("%s resultados"), (LPCTSTR)total);
        }
    }
    mem.SetTextColor(danger ? Danger : TextMuted);
    CRect rcStatus = m_rcStatus;
    mem.DrawText(status, rcStatus, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    mem.SelectObject(pOldFont);

    // navegacao e fechar
    const COLORREF navCol = m_total > 0 ? Text : TextDisabled;
    DrawChevron(g, (float)m_rcPrev.CenterPoint().x, (float)m_rcPrev.CenterPoint().y, 3, navCol, 1.6f);
    DrawChevron(g, (float)m_rcNext.CenterPoint().x, (float)m_rcNext.CenterPoint().y, 2, navCol, 1.6f);
    DrawCloseGlyph(g, (float)m_rcClose.CenterPoint().x, (float)m_rcClose.CenterPoint().y, m_hot == H_CLOSE ? Text : TextMuted);

    dc.BitBlt(0, 0, rc.Width(), rc.Height(), &mem, 0, 0, SRCCOPY);
    mem.SelectObject(pOldBmp);
}

void CValueFindBar::OnMouseMove(UINT nFlags, CPoint point)
{
    if (!m_bTrackingMouse)
    {
        TRACKMOUSEEVENT tme;
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = m_hWnd;
        _TrackMouseEvent(&tme);
        m_bTrackingMouse = true;
    }
    Hit hit = HitTest(point);
    if (hit != m_hot)
    {
        m_hot = hit;
        Invalidate(FALSE);
    }
    CWnd::OnMouseMove(nFlags, point);
}

void CValueFindBar::OnMouseLeave()
{
    m_bTrackingMouse = false;
    if (m_hot != H_NONE)
    {
        m_hot = H_NONE;
        Invalidate(FALSE);
    }
}

void CValueFindBar::OnLButtonDown(UINT nFlags, CPoint point)
{
    Hit hit = HitTest(point);
    if (hit != H_NONE)
    {
        Activate(hit);
        return;
    }
    CWnd::OnLButtonDown(nFlags, point);
}

void CValueFindBar::SaveOptions () const
{
    CWinApp* app = AfxGetApp();
    app->WriteProfileInt(s_szFindProfile, _T("FindMatchCase"), m_matchCase ? 1 : 0);
    app->WriteProfileInt(s_szFindProfile, _T("FindWholeWord"), m_wholeWord ? 1 : 0);
    app->WriteProfileInt(s_szFindProfile, _T("FindRegex"),     m_regex ? 1 : 0);
    app->WriteProfileInt(s_szFindProfile, _T("FindBackslash"), m_backslash ? 1 : 0);
    app->WriteProfileInt(s_szFindProfile, _T("FindMarkAll"),   m_markAll ? 1 : 0);
}

void CValueFindBar::Activate (Hit h)
{
    switch (h)
    {
    case H_CASE:      m_matchCase = !m_matchCase; break;
    case H_WORD:      m_wholeWord = !m_wholeWord; break;
    case H_REGEX:     m_regex     = !m_regex;     break;
    case H_BACKSLASH: m_backslash = !m_backslash; break;
    case H_MARK:      m_markAll   = !m_markAll;   break;
    case H_PREV:      m_pPanel->FindNext(false);  return;
    case H_NEXT:      m_pPanel->FindNext(true);   return;
    case H_CLOSE:     m_pPanel->HideFindBar();    return;
    default:          return;
    }
    SaveOptions();
    Invalidate(FALSE);
    m_pPanel->OnFindChanged(h != H_MARK);
    if (m_edit.m_hWnd)
        m_edit.SetFocus();
}

BOOL CValueFindBar::PreTranslateMessage(MSG* pMsg)
{
    if (m_tip.m_hWnd && pMsg->hwnd == m_hWnd && pMsg->message >= WM_MOUSEFIRST && pMsg->message <= WM_MOUSELAST)
        m_tip.RelayEvent(pMsg);

    if (pMsg->hwnd == m_edit.m_hWnd && (pMsg->message == WM_KEYDOWN || pMsg->message == WM_SYSKEYDOWN))
    {
        const bool ctrl  = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        const bool alt   = (GetKeyState(VK_MENU) & 0x8000) != 0;

        switch (pMsg->wParam)
        {
        case VK_RETURN:
        case VK_F3:
            m_pPanel->FindNext(!shift);
            return TRUE;
        case VK_ESCAPE:
            m_pPanel->HideFindBar();
            return TRUE;
        case 'F':
            if (ctrl && !alt)
            {
                FocusEdit(true);
                return TRUE;
            }
            break;
        }
        if (alt && !ctrl)
        {
            switch (pMsg->wParam)
            {
            case 'C': Activate(H_CASE);      return TRUE;
            case 'W': Activate(H_WORD);      return TRUE;
            case 'R': Activate(H_REGEX);     return TRUE;
            case 'B': Activate(H_BACKSLASH); return TRUE;
            case 'M': Activate(H_MARK);      return TRUE;
            }
        }
    }
    return CWnd::PreTranslateMessage(pMsg);
}

/////////////////////////////////////////////////////////////////////////////
// CValueEditCtrl
/////////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CValueEditCtrl, CRichEditCtrl)
    ON_WM_CONTEXTMENU()
    ON_WM_KEYDOWN()
    ON_WM_GETDLGCODE()
    ON_WM_MOUSEWHEEL()
    ON_MESSAGE(0x020E /*WM_MOUSEHWHEEL*/, OnMouseHWheel)
END_MESSAGE_MAP()

CValueEditCtrl::CValueEditCtrl()
{
}

CValueEditCtrl::~CValueEditCtrl()
{
}

// rolagem horizontal (trackpad e Shift + roda), como no editor e no grid: por colunas de caracteres, com acumulador
static int s_hWheelAccum = 0;

static void ScrollEditHorizontally (CRichEditCtrl& edit, int zDelta)      // positivo = para a direita
{
    UINT chars = 3;
    if (!::SystemParametersInfo(SPI_GETWHEELSCROLLCHARS, 0, &chars, 0) || !chars)
        chars = 3;

    s_hWheelAccum += zDelta;
    int n = ::MulDiv(s_hWheelAccum, chars, WHEEL_DELTA);
    if (n == 0)
        return;
    s_hWheelAccum -= ::MulDiv(n, WHEEL_DELTA, chars);

    const int code = n > 0 ? SB_LINERIGHT : SB_LINELEFT;
    for (int k = abs(n); k > 0; --k)
        edit.SendMessage(WM_HSCROLL, MAKEWPARAM(code, 0), 0);
}

LRESULT CValueEditCtrl::OnMouseHWheel(WPARAM wParam, LPARAM)
{
    ScrollEditHorizontally(*this, (short)HIWORD(wParam));
    return 0;
}

BOOL CValueEditCtrl::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
    if ((nFlags & MK_SHIFT) && !(nFlags & MK_CONTROL))
    {
        ScrollEditHorizontally(*this, -zDelta);                  // roda para cima = para a esquerda
        return TRUE;
    }
    BOOL res = CRichEditCtrl::OnMouseWheel(nFlags, zDelta, pt);
    if (nFlags & MK_CONTROL)
    {
        int num = 0, den = 0;
        SendMessage(EM_GETZOOM, (WPARAM)&num, (LPARAM)&den);
        s_zoomNum = num;
        s_zoomDen = den;
    }
    return res;
}

// o foco costuma estar na grade: a roda sobre o texto do painel Valor e repassada a ele
bool ForwardWheelToValuePanel (UINT msg, UINT nFlags, short zDelta, CPoint pt)
{
    HWND h = ::WindowFromPoint(pt);
    if (!h)
        return false;
    CValueEditCtrl* edit = dynamic_cast<CValueEditCtrl*>(CWnd::FromHandlePermanent(h));
    if (!edit)
        return false;
    edit->SendMessage(msg, MAKEWPARAM(nFlags, zDelta), MAKELPARAM(pt.x, pt.y));
    return true;
}

UINT CValueEditCtrl::OnGetDlgCode()
{
    return DLGC_WANTALLKEYS | DLGC_WANTCHARS | DLGC_WANTARROWS;
}

BOOL CValueEditCtrl::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN)
    {
        bool bCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        if (CValuePanelWnd* pPanel = dynamic_cast<CValuePanelWnd*>(GetParent()))
        {
            if (bCtrl && (pMsg->wParam == 'F' || pMsg->wParam == 'f'))
            {
                pPanel->ShowFindBar();
                return TRUE;
            }
            if (pMsg->wParam == VK_F3)
            {
                pPanel->FindNext((GetKeyState(VK_SHIFT) & 0x8000) == 0);
                return TRUE;
            }
            if (pMsg->wParam == VK_ESCAPE && pPanel->IsFindBarVisible())
            {
                pPanel->HideFindBar();
                return TRUE;
            }
        }
        if (bCtrl)
        {
            if (pMsg->wParam == 'A' || pMsg->wParam == 'a')
            {
                SetSel(0, -1);
                return TRUE;
            }
            if (pMsg->wParam == 'C' || pMsg->wParam == 'c')
            {
                long s = 0, e = 0;
                GetSel(s, e);
                if (s != e)
                {
                    Copy();
                }
                else
                {
                    CValuePanelWnd* pParent = dynamic_cast<CValuePanelWnd*>(GetParent());
                    if (pParent)
                        pParent->CopyToClipboard();
                    else
                        Copy();
                }
                return TRUE;
            }
        }
    }
    return CRichEditCtrl::PreTranslateMessage(pMsg);
}

void CValueEditCtrl::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    bool bCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    if (bCtrl)
    {
        if (nChar == 'A' || nChar == 'a')
        {
            SetSel(0, -1);
            return;
        }
        if (nChar == 'C' || nChar == 'c')
        {
            long s = 0, e = 0;
            GetSel(s, e);
            if (s != e)
            {
                Copy();
            }
            else
            {
                CValuePanelWnd* pParent = dynamic_cast<CValuePanelWnd*>(GetParent());
                if (pParent)
                    pParent->CopyToClipboard();
                else
                    Copy();
            }
            return;
        }
    }
    CRichEditCtrl::OnKeyDown(nChar, nRepCnt, nFlags);
}

void CValueEditCtrl::OnContextMenu(CWnd*, CPoint pos)
{
    if (pos.x == -1 && pos.y == -1)
    {
        CRect rc;
        GetClientRect(rc);
        pos = rc.CenterPoint();
        ClientToScreen(&pos);
    }

    CMenu menu;
    menu.CreatePopupMenu();

    long selStart = 0, selEnd = 0;
    GetSel(selStart, selEnd);

    menu.AppendMenu(MF_STRING, 1, _T("Copiar\tCtrl+C"));
    menu.AppendMenu(MF_STRING, 2, _T("Selecionar Tudo\tCtrl+A"));
    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenu(MF_STRING, 3, _T("Localizar...\tCtrl+F"));

    int cmd = menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD, pos.x, pos.y, this);
    if (cmd == 1)
    {
        long s = 0, e = 0;
        GetSel(s, e);
        if (s != e)
        {
            Copy();
        }
        else
        {
            CValuePanelWnd* pParent = dynamic_cast<CValuePanelWnd*>(GetParent());
            if (pParent)
                pParent->CopyToClipboard();
            else
                Copy();
        }
    }
    else if (cmd == 2)
    {
        SetSel(0, -1);
    }
    else if (cmd == 3)
    {
        if (CValuePanelWnd* pPanel = dynamic_cast<CValuePanelWnd*>(GetParent()))
            pPanel->ShowFindBar();
    }
}

/////////////////////////////////////////////////////////////////////////////
// CValuePanelWnd
/////////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CValuePanelWnd, CWnd)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_LBUTTONDOWN()
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSELEAVE()
    ON_BN_CLICKED(IDC_VP_BTN_FORMAT, OnFormatDropdown)
    ON_BN_CLICKED(IDC_VP_BTN_COPY,   OnBtnCopy)
    ON_BN_CLICKED(IDC_VP_BTN_SAVE,   OnBtnSave)
    ON_EN_VSCROLL(IDC_VP_EDIT,       OnEditVScroll)
    ON_REGISTERED_MESSAGE(ModernTheme::UM_THEME_CHANGED, OnThemeChanged)
END_MESSAGE_MAP()

CValuePanelWnd::CValuePanelWnd(OciGridView* pGrid)
    : m_pGrid(pGrid)
    , m_currentFormat(VF_TEXT)
    , m_bUserSelectedFormat(false)
    , m_bAutoFormat(true)
    , m_bWordWrap(false)
    , m_bCompact(false)
    , m_bFormatCdata(false)
    , m_encoding(VE_UTF8)
    , m_findBar(this)
    , m_bFindVisible(false)
    , m_findTextValid(false)
    , m_findCur(-1)
    , m_findCapped(false)
    , m_marksApplied(false)
    , m_lastMenuCloseTime(0)
    , m_bHoverClose(false)
{
}

CValuePanelWnd::~CValuePanelWnd()
{
}

BOOL CValuePanelWnd::Create(CWnd* pParentWnd, UINT nID)
{
    HINSTANCE hInst = AfxGetInstanceHandle();
    WNDCLASS wc;
    if (!::GetClassInfo(hInst, s_szPanelClass, &wc))
    {
        memset(&wc, 0, sizeof(wc));
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = ::DefWindowProc;
        wc.hInstance = hInst;
        wc.hCursor = ::LoadCursor(NULL, IDC_ARROW);
        wc.lpszClassName = s_szPanelClass;
        AfxRegisterClass(&wc);
    }

    return CWnd::Create(s_szPanelClass, _T("Valor"),
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        CRect(0, 0, 0, 0), pParentWnd, nID);
}

int CValuePanelWnd::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    ::AfxInitRichEdit2();

    if (CWnd::OnCreate(lpCreateStruct) == -1)
        return -1;

    // Fontes
    m_fontEditor.CreatePointFont(95, _T("Consolas"));
    m_fontUi.CreatePointFont(90, _T("Segoe UI"));
    m_fontUiBold.CreateFont(
        -12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, _T("Segoe UI"));

    // Toolbar Botões Modernos com OwnerDraw e Ícones Vetoriais
    m_btnFormat.Create(_T("XML"),
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        CRect(0, 0, 0, 0), this, IDC_VP_BTN_FORMAT);
    m_btnFormat.SetFont(&m_fontUi);
    m_btnFormat.SetIconType(CModernButton::ICON_FORMAT);
    m_btnFormat.SetDropdown(true);

    m_btnCopy.Create(_T("Copiar"),
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        CRect(0, 0, 0, 0), this, IDC_VP_BTN_COPY);
    m_btnCopy.SetFont(&m_fontUi);
    m_btnCopy.SetIconType(CModernButton::ICON_COPY);

    m_btnSave.Create(_T("Salvar..."),
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        CRect(0, 0, 0, 0), this, IDC_VP_BTN_SAVE);
    m_btnSave.SetFont(&m_fontUi);
    m_btnSave.SetIconType(CModernButton::ICON_SAVE);

    // Edit Control (RichEdit 2.0 / MSFTEDIT)
    DWORD editStyle = WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL
        | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY | ES_WANTRETURN;
    m_wndEdit.Create(editStyle, CRect(0, 0, 0, 0), this, IDC_VP_EDIT);
    m_wndEdit.SetFont(&m_fontEditor);
    m_wndEdit.SendMessage(EM_SETOPTIONS, ECOOP_OR, ECO_NOHIDESEL);    // a ocorrencia selecionada continua visivel com o foco na barra de busca
    m_wndEdit.LimitText(0x7FFFFFF);
    m_wndEdit.SetEventMask(m_wndEdit.GetEventMask() | ENM_SCROLL | ENM_CHANGE);
    m_wndEdit.SetBackgroundColor(FALSE, ModernTheme::Surface);
    {
        CHARFORMAT2 cf = {};
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_COLOR;
        cf.crTextColor = ModernTheme::Text;
        m_wndEdit.SetDefaultCharFormat(cf);
    }

    m_bFormatCdata = AfxGetApp()->GetProfileInt(s_szProfileSection, _T("FormatCdata"), 0) != 0;
    ValueFormatters::SetFormatCdata(m_bFormatCdata);

    // Gutter
    m_wndGutter.Create(this, IDC_VP_GUTTER, &m_wndEdit);
    m_wndGutter.SetFont(&m_fontEditor);

    // Barra de busca (Ctrl+F), oculta ate ser pedida
    m_findBar.Create(this, IDC_VP_FIND);

    return 0;
}

CRect CValuePanelWnd::GetTabRect() const
{
    return CRect(0, 0, 76, kHeaderH);
}

CRect CValuePanelWnd::GetCloseButtonRect() const
{
    CRect rc;
    GetClientRect(rc);
    return CRect(rc.right - 30, 4, rc.right - 6, kHeaderH - 4);
}

void CValuePanelWnd::LayoutChildren(int cx, int cy)
{
    if (cx <= 0 || cy <= 0) return;

    int topOffset = kHeaderH + kToolbarH;

    // botoes da toolbar
    int btnY = kHeaderH + 5;
    int btnH = 26;
    const UINT flags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS;
    m_btnFormat.SetWindowPos(NULL, 8, btnY, 88, btnH, flags);
    m_btnCopy.SetWindowPos(NULL, 102, btnY, 80, btnH, flags);
    m_btnSave.SetWindowPos(NULL, 188, btnY, 88, btnH, flags);

    // barra de busca, entre a toolbar e o editor
    if (m_findBar.GetSafeHwnd())
    {
        if (m_bFindVisible)
            m_findBar.SetWindowPos(NULL, 0, topOffset, cx, CValueFindBar::Height(), flags | SWP_SHOWWINDOW);
        else
            m_findBar.ShowWindow(SW_HIDE);
    }
    if (m_bFindVisible)
        topOffset += CValueFindBar::Height();

    // area do editor e gutter
    int gutterW = 40;
    int editW = (std::max)(10, cx - gutterW);
    int contentH = (std::max)(10, cy - topOffset);

    m_wndGutter.SetWindowPos(NULL, 0, topOffset, gutterW, contentH, flags);
    m_wndEdit.SetWindowPos(NULL, gutterW, topOffset, editW, contentH, flags);
}

void CValuePanelWnd::OnSize(UINT nType, int cx, int cy)
{
    CWnd::OnSize(nType, cx, cy);
    LayoutChildren(cx, cy);
}

BOOL CValuePanelWnd::OnEraseBkgnd(CDC*)
{
    return TRUE;
}

void CValuePanelWnd::OnPaint()
{
    using namespace ModernTheme;

    CPaintDC dc(this);
    CRect rc;
    GetClientRect(rc);

    // cabecalho e barra de ferramentas
    CRect rcHeader(rc.left, rc.top, rc.right, rc.top + kHeaderH);
    dc.FillSolidRect(rcHeader, Surface);
    CRect rcToolbar(rc.left, rcHeader.bottom, rc.right, rcHeader.bottom + kToolbarH);
    dc.FillSolidRect(rcToolbar, Surface);
    dc.FillSolidRect(rc.left, rcToolbar.bottom - 1, rc.Width(), 1, Border);
    dc.FillSolidRect(rc.left, rcHeader.bottom - 1, rc.Width(), 1, Border);

    // titulo "Valor" com sublinhado de destaque
    CRect rcTab = GetTabRect();
    CFont* pOldFont = dc.SelectObject(&m_fontUiBold);
    dc.SetBkMode(TRANSPARENT);
    dc.SetTextColor(Text);
    CRect rcTabText = rcTab;
    rcTabText.left += 12;
    dc.DrawText(_T("Valor"), rcTabText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    dc.FillSolidRect(rcTab.left, rcHeader.bottom - 2, rcTab.Width(), 2, Accent);

    // botao fechar
    CRect rcClose = GetCloseButtonRect();
    Gdiplus::Graphics g(dc.GetSafeHdc());
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    if (m_bHoverClose)
        FillRoundRect(g, rcClose, 5.0f, ModernTheme::CaptionClose);
    DrawCloseGlyph(g, (float)rcClose.CenterPoint().x, (float)rcClose.CenterPoint().y, m_bHoverClose ? Text : TextMuted);

    dc.SelectObject(pOldFont);
}

void CValuePanelWnd::OnMouseMove(UINT nFlags, CPoint point)
{
    TRACKMOUSEEVENT tme;
    tme.cbSize = sizeof(tme);
    tme.dwFlags = TME_LEAVE;
    tme.hwndTrack = m_hWnd;
    _TrackMouseEvent(&tme);

    CRect rcClose = GetCloseButtonRect();
    bool bHover = (rcClose.PtInRect(point) != FALSE);
    if (bHover != m_bHoverClose)
    {
        m_bHoverClose = bHover;
        InvalidateRect(rcClose, FALSE);
    }

    CWnd::OnMouseMove(nFlags, point);
}

void CValuePanelWnd::OnMouseLeave()
{
    if (m_bHoverClose)
    {
        m_bHoverClose = false;
        CRect rcClose = GetCloseButtonRect();
        InvalidateRect(rcClose, FALSE);
    }
}

void CValuePanelWnd::OnLButtonDown(UINT nFlags, CPoint point)
{
    CRect rcClose = GetCloseButtonRect();
    if (rcClose.PtInRect(point))
    {
        m_pGrid->ToggleValuePanel();
        return;
    }

    if (m_wndEdit.GetSafeHwnd())
        m_wndEdit.SetFocus();

    CWnd::OnLButtonDown(nFlags, point);
}

void CValuePanelWnd::OnEditVScroll()
{
    m_wndGutter.Invalidate();
}

BOOL CValuePanelWnd::OnCommand(WPARAM wParam, LPARAM lParam)
{
    if (LOWORD(wParam) == IDC_VP_EDIT && HIWORD(wParam) == EN_VSCROLL)
    {
        m_wndGutter.Invalidate();
    }
    return CWnd::OnCommand(wParam, lParam);
}

void CValuePanelWnd::SetCellData(const std::string& rawData)
{
    m_rawInput = rawData;

    // Converte hex se aplicável para detectar formato
    if (ValueFormatters::IsLikelyHex(rawData))
        ValueFormatters::HexToBytes(rawData, m_rawBytes);
    else
        m_rawBytes.assign(rawData.begin(), rawData.end());

    // Se o usuário não travou o formato explicitamente, detecta automaticamente
    if (!m_bUserSelectedFormat)
    {
        m_currentFormat = ValueFormatters::DetectFormat(m_rawBytes, rawData);
    }

    RefreshDisplay();
}

void CValuePanelWnd::ClearData()
{
    m_rawInput.clear();
    m_rawBytes.clear();
    m_wndEdit.SetWindowText(_T(""));
    m_wndGutter.Invalidate();
    m_findTextValid = false;
    m_marksApplied = false;
    if (m_bFindVisible)
        RunFind(false);
}

void CValuePanelWnd::SetFormat(EValueFormat format)
{
    m_currentFormat = format;
    RefreshDisplay();
}

void CValuePanelWnd::SetAutoFormat(bool bAuto)
{
    m_bAutoFormat = bAuto;
    RefreshDisplay();
}

void CValuePanelWnd::SetWordWrap(bool bWrap)
{
    m_bWordWrap = bWrap;
    if (m_wndEdit.GetSafeHwnd())
    {
        m_wndEdit.SetTargetDevice(NULL, bWrap ? 0 : 1);
        m_wndEdit.Invalidate();
        m_wndGutter.Invalidate();
    }
}

void CValuePanelWnd::SetCompact(bool bCompact)
{
    m_bCompact = bCompact;
    RefreshDisplay();
}

void CValuePanelWnd::SetFormatCdata(bool on)
{
    m_bFormatCdata = on;
    ValueFormatters::SetFormatCdata(on);
    AfxGetApp()->WriteProfileInt(s_szProfileSection, _T("FormatCdata"), on ? 1 : 0);
    RefreshDisplay();
}

void CValuePanelWnd::SetEncoding(EValueEncoding enc)
{
    m_encoding = enc;
    RefreshDisplay();
}

struct RtfStreamCookie
{
    const char* pData;
    size_t length;
    size_t offset;
};

static DWORD CALLBACK EditStreamInCallback(DWORD_PTR dwCookie, LPBYTE pbBuff, LONG cb, LONG *pcb)
{
    RtfStreamCookie* pCookie = (RtfStreamCookie*)dwCookie;
    if (!pCookie || pCookie->offset >= pCookie->length)
    {
        *pcb = 0;
        return 0;
    }
    size_t remaining = pCookie->length - pCookie->offset;
    LONG toCopy = (std::min)(remaining, (size_t)cb);
    memcpy(pbBuff, pCookie->pData + pCookie->offset, toCopy);
    pCookie->offset += toCopy;
    *pcb = toCopy;
    return 0;
}

// troca de tema com o programa aberto: fundo e cor do texto do editor e conteudo realcado de novo
LRESULT CValuePanelWnd::OnThemeChanged(WPARAM, LPARAM)
{
    m_wndEdit.SetBackgroundColor(FALSE, ModernTheme::Surface);
    CHARFORMAT2 cf = {};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = ModernTheme::Text;
    m_wndEdit.SetDefaultCharFormat(cf);

    m_findBar.ThemeChanged();
    RefreshDisplay();
    m_wndGutter.Invalidate();
    Invalidate();
    return 0;
}

void CValuePanelWnd::RefreshDisplay()
{
    std::string rtf = ValueFormatters::ProcessValueRtf(
        m_rawInput, m_currentFormat, m_bAutoFormat, m_bCompact, m_encoding, m_rawBytes);

    if (rtf.empty())
    {
        m_wndEdit.SetWindowText(_T(""));
    }
    else
    {
        BOOL bReadOnly = (m_wndEdit.GetStyle() & ES_READONLY) != 0;
        if (bReadOnly)
            m_wndEdit.SetReadOnly(FALSE);

        RtfStreamCookie cookie = { rtf.c_str(), rtf.length(), 0 };
        EDITSTREAM es;
        es.dwCookie = (DWORD_PTR)&cookie;
        es.pfnCallback = EditStreamInCallback;
        m_wndEdit.StreamIn(SF_RTF, es);

        if (bReadOnly)
            m_wndEdit.SetReadOnly(TRUE);

        m_wndEdit.SetSel(0, 0);
    }

    // o RTF zera o zoom: devolve o que o usuario escolheu
    if (s_zoomNum > 0 && s_zoomDen > 0)
        m_wndEdit.SendMessage(EM_SETZOOM, s_zoomNum, s_zoomDen);

    // Atualiza o rótulo do botão Dropdown
    CString btnLabel;
    switch (m_currentFormat)
    {
    case VF_XML:    btnLabel = _T("XML");    break;
    case VF_JSON:   btnLabel = _T("JSON");   break;
    case VF_BINARY: btnLabel = _T("Binary"); break;
    case VF_TEXT:
    default:        btnLabel = _T("Text");   break;
    }
    m_btnFormat.SetWindowText(btnLabel);

    m_wndGutter.Invalidate();

    // o conteudo foi trocado: texto em cache e marcas perdidos, a busca aberta e refeita sem mexer na selecao
    m_findTextValid = false;
    m_marksApplied = false;
    if (m_bFindVisible)
        RunFind(false);
}

/////////////////////////////////////////////////////////////////////////////
// Busca (Ctrl+F)
/////////////////////////////////////////////////////////////////////////////

bool CValuePanelWnd::IsFindEditFocused() const
{
    return m_bFindVisible && m_findBar.IsEditFocused();
}

// texto do editor com \n como quebra de linha (mesmo tamanho: as posicoes valem para o RichEdit)
const std::wstring& CValuePanelWnd::SearchText()
{
    if (!m_findTextValid)
    {
        m_findText.clear();
        GETTEXTLENGTHEX gtl = {};
        gtl.flags = GTL_NUMCHARS;
        gtl.codepage = 1200;
        LRESULT len = m_wndEdit.SendMessage(EM_GETTEXTLENGTHEX, (WPARAM)&gtl, 0);
        if (len > 0 && len != E_INVALIDARG)
        {
            m_findText.resize((size_t)len + 1);
            GETTEXTEX gt = {};
            gt.cb = (DWORD)((len + 1) * sizeof(wchar_t));
            gt.flags = GT_DEFAULT;
            gt.codepage = 1200;
            LRESULT got = m_wndEdit.SendMessage(EM_GETTEXTEX, (WPARAM)&gt, (LPARAM)&m_findText[0]);
            m_findText.resize((size_t)got);
            std::replace(m_findText.begin(), m_findText.end(), L'\r', L'\n');
        }
        m_findTextValid = true;
    }
    return m_findText;
}

void CValuePanelWnd::ShowFindBar()
{
    // o texto selecionado (de uma linha) vira o texto procurado
    std::wstring seed;
    long s = 0, e = 0;
    m_wndEdit.GetSel(s, e);
    if (e > s && e - s <= 200)
    {
        CString sel = m_wndEdit.GetSelText();
        if (sel.FindOneOf(_T("\r\n")) < 0)
            seed = (LPCTSTR)sel;
    }

    m_bFindVisible = true;
    CRect rc;
    GetClientRect(rc);
    LayoutChildren(rc.Width(), rc.Height());

    const bool changed = !seed.empty() && seed != m_findBar.GetQuery();
    if (changed)
        m_findBar.SetQuery(seed);
    m_findBar.FocusEdit(true);
    RunFind(changed);
}

void CValuePanelWnd::HideFindBar()
{
    if (!m_bFindVisible)
        return;
    m_bFindVisible = false;
    ApplyMarks();                       // tira as marcas; a selecao atual permanece
    CRect rc;
    GetClientRect(rc);
    LayoutChildren(rc.Width(), rc.Height());
    m_wndEdit.SetFocus();
    m_wndGutter.Invalidate();
}

void CValuePanelWnd::OnFindChanged(bool jump)
{
    if (m_bFindVisible)
        RunFind(jump);
}

void CValuePanelWnd::RunFind(bool jump)
{
    FindOptions o;
    o.query = m_findBar.GetQuery();
    o.matchCase = m_findBar.m_matchCase;
    o.wholeWord = m_findBar.m_wholeWord;
    o.regex = m_findBar.m_regex;
    o.backslash = m_findBar.m_backslash;

    m_findError.Empty();
    m_findCur = -1;
    if (o.query.empty())
    {
        m_matches.clear();
        m_findCapped = false;
    }
    else
    {
        CWaitCursor wait;
        FindAllMatches(SearchText(), o, m_matches, m_findCapped, m_findError);
    }

    long s = 0, e = 0;
    m_wndEdit.GetSel(s, e);
    if (jump && !m_matches.empty())
    {
        // a partir de onde a selecao comecou, para a busca incremental nao pular a ocorrencia ao digitar
        auto it = std::lower_bound(m_matches.begin(), m_matches.end(), s,
            [](const CHARRANGE& m, long v) { return m.cpMin < v; });
        SelectMatch(it == m_matches.end() ? 0 : (int)(it - m_matches.begin()));
    }
    else
    {
        for (size_t i = 0; i < m_matches.size(); ++i)
            if (m_matches[i].cpMin == s && m_matches[i].cpMax == e)
            {
                m_findCur = (int)i;
                break;
            }
    }

    ApplyMarks();
    UpdateFindStatus();
}

void CValuePanelWnd::UpdateFindStatus()
{
    m_findBar.SetStatus(m_findCur, (int)m_matches.size(), m_findCapped, m_findError);
}

void CValuePanelWnd::SelectMatch(int index)
{
    if (index < 0 || index >= (int)m_matches.size())
        return;
    m_findCur = index;
    const CHARRANGE& m = m_matches[index];
    m_wndEdit.SetSel(m.cpMin, m.cpMax);

    // se a ocorrencia esta fora da vista, rola deixando-a mais ou menos no terco superior
    CRect rcClient;
    m_wndEdit.GetClientRect(rcClient);
    const int first = m_wndEdit.GetFirstVisibleLine();
    int lh = 16;
    const int a = m_wndEdit.LineIndex(first), b = m_wndEdit.LineIndex(first + 1);
    if (a >= 0 && b > a)
        lh = (std::max)(8, (int)(m_wndEdit.GetCharPos(b).y - m_wndEdit.GetCharPos(a).y));
    const CPoint pt = m_wndEdit.GetCharPos(m.cpMin);
    if (pt.y < 0 || pt.y + lh > rcClient.Height())
    {
        const int line = m_wndEdit.LineFromChar(m.cpMin);
        const int visible = (std::max)(1, rcClient.Height() / lh);
        m_wndEdit.LineScroll(line - first - visible / 3);
    }
    m_wndEdit.SendMessage(EM_SCROLLCARET);
    m_wndGutter.Invalidate();
}

void CValuePanelWnd::FindNext(bool forward)
{
    if (!m_bFindVisible)
    {
        ShowFindBar();
        return;
    }
    if (m_matches.empty())
        return;

    long s = 0, e = 0;
    m_wndEdit.GetSel(s, e);
    auto less = [](const CHARRANGE& m, long v) { return m.cpMin < v; };
    int idx;
    if (forward)
    {
        auto it = std::lower_bound(m_matches.begin(), m_matches.end(), e > s ? e : s, less);
        idx = it == m_matches.end() ? 0 : (int)(it - m_matches.begin());
    }
    else
    {
        auto it = std::lower_bound(m_matches.begin(), m_matches.end(), s, less);
        idx = it == m_matches.begin() ? (int)m_matches.size() - 1 : (int)(it - m_matches.begin()) - 1;
    }
    SelectMatch(idx);
    UpdateFindStatus();
}

// "Marcar todos": pinta o fundo das ocorrencias (e desfaz as marcas anteriores); a selecao e a rolagem sao preservadas
void CValuePanelWnd::ApplyMarks()
{
    const bool want = m_bFindVisible && m_findBar.m_markAll && !m_matches.empty();
    if (!want && !m_marksApplied)
        return;

    long s = 0, e = 0;
    m_wndEdit.GetSel(s, e);
    const int firstLine = m_wndEdit.GetFirstVisibleLine();
    const BOOL readOnly = (m_wndEdit.GetStyle() & ES_READONLY) != 0;
    const LRESULT mask = m_wndEdit.SendMessage(EM_SETEVENTMASK, 0, 0);
    m_wndEdit.SetRedraw(FALSE);
    if (readOnly)
        m_wndEdit.SetReadOnly(FALSE);

    CHARFORMAT2 cf = {};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_BACKCOLOR;
    if (m_marksApplied)
    {
        m_wndEdit.SetSel(0, -1);
        cf.dwEffects = CFE_AUTOBACKCOLOR;
        m_wndEdit.SetSelectionCharFormat(cf);
    }
    if (want)
    {
        cf.dwEffects = 0;
        cf.crBackColor = MarkColor();
        const size_t n = (std::min)(m_matches.size(), kMaxMarks);
        for (size_t i = 0; i < n; ++i)
        {
            m_wndEdit.SetSel(m_matches[i].cpMin, m_matches[i].cpMax);
            m_wndEdit.SetSelectionCharFormat(cf);
        }
    }
    m_marksApplied = want;

    m_wndEdit.SetSel(s, e);
    if (readOnly)
        m_wndEdit.SetReadOnly(TRUE);
    const int now = m_wndEdit.GetFirstVisibleLine();
    if (now != firstLine)
        m_wndEdit.LineScroll(firstLine - now);
    m_wndEdit.SendMessage(EM_SETEVENTMASK, 0, mask);
    m_wndEdit.SetRedraw(TRUE);
    m_wndEdit.Invalidate();
    m_wndGutter.Invalidate();
}

void CValuePanelWnd::OnFormatDropdown()
{
    DWORD now = GetTickCount();
    if (now - m_lastMenuCloseTime < 300)
    {
        return;
    }

    CMenu menu;
    menu.CreatePopupMenu();

    menu.AppendMenu(MF_STRING | (m_currentFormat == VF_BINARY ? MF_CHECKED : MF_UNCHECKED), IDM_FORMAT_BINARY, _T("Binary"));
    menu.AppendMenu(MF_STRING | (m_currentFormat == VF_JSON   ? MF_CHECKED : MF_UNCHECKED), IDM_FORMAT_JSON,   _T("JSON"));
    menu.AppendMenu(MF_STRING | (m_currentFormat == VF_TEXT   ? MF_CHECKED : MF_UNCHECKED), IDM_FORMAT_TEXT,   _T("Text"));
    menu.AppendMenu(MF_STRING | (m_currentFormat == VF_XML    ? MF_CHECKED : MF_UNCHECKED), IDM_FORMAT_XML,    _T("XML"));

    menu.AppendMenu(MF_SEPARATOR);

    menu.AppendMenu(MF_STRING | (m_bWordWrap   ? MF_CHECKED : MF_UNCHECKED), IDM_OPT_WORDWRAP,   _T("Quebra de linha"));
    menu.AppendMenu(MF_STRING | (m_bAutoFormat ? MF_CHECKED : MF_UNCHECKED), IDM_OPT_AUTOFORMAT, _T("Formata\xE7\xE3o autom\xE1tica"));
    menu.AppendMenu(MF_STRING | (m_bCompact    ? MF_CHECKED : MF_UNCHECKED), IDM_OPT_COMPACT,    _T("Save compact value (minified)"));

    menu.AppendMenu(MF_STRING | (m_bFormatCdata ? MF_CHECKED : MF_UNCHECKED), IDM_OPT_CDATA, _T("Formatar CDATA"));

    CMenu subEnc;
    subEnc.CreatePopupMenu();
    subEnc.AppendMenu(MF_STRING | (m_encoding == VE_UTF8 ? MF_CHECKED : MF_UNCHECKED), IDM_ENC_UTF8, _T("UTF-8"));
    subEnc.AppendMenu(MF_STRING | (m_encoding == VE_ANSI ? MF_CHECKED : MF_UNCHECKED), IDM_ENC_ANSI, _T("ANSI / Windows-1252"));
    menu.AppendMenu(MF_POPUP, (UINT_PTR)subEnc.m_hMenu, _T("Codifica\xE7\xE3o..."));

    CRect rcBtn;
    m_btnFormat.GetWindowRect(rcBtn);

    int cmd = menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD,
        rcBtn.left, rcBtn.bottom, this);

    m_lastMenuCloseTime = GetTickCount();

    switch (cmd)
    {
    case IDM_FORMAT_BINARY:
        m_bUserSelectedFormat = true;
        SetFormat(VF_BINARY);
        break;
    case IDM_FORMAT_JSON:
        m_bUserSelectedFormat = true;
        SetFormat(VF_JSON);
        break;
    case IDM_FORMAT_TEXT:
        m_bUserSelectedFormat = true;
        SetFormat(VF_TEXT);
        break;
    case IDM_FORMAT_XML:
        m_bUserSelectedFormat = true;
        SetFormat(VF_XML);
        break;
    case IDM_OPT_WORDWRAP:
        SetWordWrap(!m_bWordWrap);
        break;
    case IDM_OPT_AUTOFORMAT:
        SetAutoFormat(!m_bAutoFormat);
        break;
    case IDM_OPT_COMPACT:
        SetCompact(!m_bCompact);
        break;
    case IDM_OPT_CDATA:
        SetFormatCdata(!m_bFormatCdata);
        break;
    case IDM_ENC_UTF8:
        SetEncoding(VE_UTF8);
        break;
    case IDM_ENC_ANSI:
        SetEncoding(VE_ANSI);
        break;
    }
}

void CValuePanelWnd::SelectAll()
{
    if (IsFindEditFocused())
    {
        m_findBar.SelectAllText();
        return;
    }
    if (m_wndEdit.GetSafeHwnd())
    {
        m_wndEdit.SetSel(0, -1);
        m_wndEdit.SetFocus();
    }
}

void CValuePanelWnd::CopySelection()
{
    if (IsFindEditFocused())
    {
        m_findBar.CopyText();
        return;
    }
    if (m_wndEdit.GetSafeHwnd())
    {
        long selStart = 0, selEnd = 0;
        m_wndEdit.GetSel(selStart, selEnd);
        if (selStart != selEnd)
        {
            m_wndEdit.Copy();
        }
        else
        {
            CopyToClipboard();
        }
    }
}

BOOL CValuePanelWnd::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN)
    {
        bool bCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        if (bCtrl)
        {
            if (pMsg->wParam == 'A' || pMsg->wParam == 'a')
            {
                SelectAll();
                return TRUE;
            }
            if (pMsg->wParam == 'C' || pMsg->wParam == 'c')
            {
                CopySelection();
                return TRUE;
            }
            if (pMsg->wParam == 'F' || pMsg->wParam == 'f')
            {
                ShowFindBar();
                return TRUE;
            }
        }
    }
    return CWnd::PreTranslateMessage(pMsg);
}

void CValuePanelWnd::OnBtnCopy()
{
    CopySelection();
}

void CValuePanelWnd::OnBtnSave()
{
    SaveToFile();
}

void CValuePanelWnd::CopyToClipboard()
{
    CString text;
    m_wndEdit.GetWindowText(text);
    if (text.IsEmpty()) return;

    if (OpenClipboard())
    {
        EmptyClipboard();
        size_t bytes = (text.GetLength() + 1) * sizeof(wchar_t);
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (hMem)
        {
            memcpy(GlobalLock(hMem), (LPCTSTR)text, bytes);
            GlobalUnlock(hMem);
            SetClipboardData(CF_UNICODETEXT, hMem);
        }
        CloseClipboard();
    }
}

void CValuePanelWnd::SaveToFile()
{
    CString defaultExt, filter;
    switch (m_currentFormat)
    {
    case VF_XML:
        defaultExt = _T("xml");
        filter = _T("Arquivos XML (*.xml)|*.xml|Todos os Arquivos (*.*)|*.*||");
        break;
    case VF_JSON:
        defaultExt = _T("json");
        filter = _T("Arquivos JSON (*.json)|*.json|Todos os Arquivos (*.*)|*.*||");
        break;
    case VF_BINARY:
        defaultExt = _T("bin");
        filter = _T("Arquivos Bin\xE1rios (*.bin;*.dat)|*.bin;*.dat|Todos os Arquivos (*.*)|*.*||");
        break;
    case VF_TEXT:
    default:
        defaultExt = _T("txt");
        filter = _T("Arquivos de Texto (*.txt)|*.txt|Todos os Arquivos (*.*)|*.*||");
        break;
    }

    CFileDialog dlg(FALSE, defaultExt, _T("valor"),
        OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, filter, this);

    if (dlg.DoModal() == IDOK)
    {
        CString path = dlg.GetPathName();

        if (m_currentFormat == VF_BINARY && !m_rawBytes.empty())
        {
            CFile f;
            if (f.Open(path, CFile::modeCreate | CFile::modeWrite))
            {
                f.Write(m_rawBytes.data(), (UINT)m_rawBytes.size());
                f.Close();
            }
        }
        else
        {
            CString text;
            m_wndEdit.GetWindowText(text);

            CFile f;
            if (f.Open(path, CFile::modeCreate | CFile::modeWrite))
            {
                // Salva em UTF-8 com BOM
                static const unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
                f.Write(bom, 3);

                int utf8Len = WideCharToMultiByte(CP_UTF8, 0, (LPCTSTR)text, text.GetLength(), NULL, 0, NULL, NULL);
                if (utf8Len > 0)
                {
                    std::string utf8Str(utf8Len, '\0');
                    WideCharToMultiByte(CP_UTF8, 0, (LPCTSTR)text, text.GetLength(), &utf8Str[0], utf8Len, NULL, NULL);
                    f.Write(utf8Str.data(), (UINT)utf8Str.size());
                }
                f.Close();
            }
        }
    }
}

} // namespace OG2
