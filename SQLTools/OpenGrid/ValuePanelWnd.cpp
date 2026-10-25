#include "stdafx.h"
#include "ValuePanelWnd.h"
#include "OCIGridView.h"
#include <COMMCTRL.H>
#include <gdiplus.h>
#include "Theme/ModernTheme.h"

namespace OG2
{

// IDs de controles
enum
{
    IDC_VP_BTN_FORMAT = 10101,
    IDC_VP_BTN_COPY   = 10102,
    IDC_VP_BTN_SAVE   = 10103,
    IDC_VP_GUTTER     = 10104,
    IDC_VP_EDIT       = 10105
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
    IDM_ENC_ANSI       = 10209
};

static const TCHAR* s_szSplitterClass = _T("SQLTools_ValueSplitter");
static const TCHAR* s_szGutterClass   = _T("SQLTools_ValueGutter");
static const TCHAR* s_szPanelClass    = _T("SQLTools_ValuePanel");


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
    else               { p[0] = Gdiplus::PointF(cx - 3.5f*s, cy - 1.5f*s); p[1] = Gdiplus::PointF(cx, cy + 2*s); p[2] = Gdiplus::PointF(cx + 3.5f*s, cy - 1.5f*s); }
    g.DrawLines(&pen, p, 3);
}

static void DrawCloseGlyph (Gdiplus::Graphics& g, float cx, float cy, COLORREF color)
{
    Gdiplus::Pen pen(Clr(color), 1.5f); MakeRoundPen(pen);
    g.DrawLine(&pen, cx - 4, cy - 4, cx + 4, cy + 4);
    g.DrawLine(&pen, cx - 4, cy + 4, cx + 4, cy - 4);
}

static void DrawCodeGlyph (Gdiplus::Graphics& g, float cx, float cy, COLORREF color)
{
    Gdiplus::Pen pen(Clr(color), 1.5f); MakeRoundPen(pen);
    Gdiplus::PointF l[3] = { Gdiplus::PointF(cx - 3.5f, cy - 4), Gdiplus::PointF(cx - 7, cy), Gdiplus::PointF(cx - 3.5f, cy + 4) };
    Gdiplus::PointF r[3] = { Gdiplus::PointF(cx + 3.5f, cy - 4), Gdiplus::PointF(cx + 7, cy), Gdiplus::PointF(cx + 3.5f, cy + 4) };
    g.DrawLines(&pen, l, 3);
    g.DrawLines(&pen, r, 3);
    g.DrawLine(&pen, cx + 1.5f, cy - 5, cx - 1.5f, cy + 5);
}

static void DrawCopyGlyph (Gdiplus::Graphics& g, float cx, float cy, COLORREF color)
{
    Gdiplus::Pen pen(Clr(color), 1.4f); MakeRoundPen(pen);
    Gdiplus::GraphicsPath front;
    RoundRectPath(front, Gdiplus::RectF(cx - 2, cy - 2, 9, 9), 1.5f);
    g.DrawPath(&pen, &front);
    Gdiplus::PointF back[5] = {
        Gdiplus::PointF(cx + 3, cy - 2), Gdiplus::PointF(cx + 3, cy - 5.5f), Gdiplus::PointF(cx - 5, cy - 5.5f),
        Gdiplus::PointF(cx - 5, cy + 2), Gdiplus::PointF(cx - 2, cy + 2) };
    g.DrawLines(&pen, back, 5);
}

static void DrawSaveGlyph (Gdiplus::Graphics& g, float cx, float cy, COLORREF color)
{
    Gdiplus::Pen pen(Clr(color), 1.4f); MakeRoundPen(pen);
    Gdiplus::PointF body[6] = {
        Gdiplus::PointF(cx - 6, cy - 6), Gdiplus::PointF(cx + 3.5f, cy - 6), Gdiplus::PointF(cx + 6, cy - 3.5f),
        Gdiplus::PointF(cx + 6, cy + 6), Gdiplus::PointF(cx - 6, cy + 6), Gdiplus::PointF(cx - 6, cy - 6) };
    g.DrawLines(&pen, body, 6);
    Gdiplus::PointF notch[4] = { Gdiplus::PointF(cx - 3, cy - 6), Gdiplus::PointF(cx - 3, cy - 2.5f), Gdiplus::PointF(cx + 2, cy - 2.5f), Gdiplus::PointF(cx + 2, cy - 6) };
    g.DrawLines(&pen, notch, 4);
    Gdiplus::PointF label[4] = { Gdiplus::PointF(cx - 3, cy + 6), Gdiplus::PointF(cx - 3, cy + 1.5f), Gdiplus::PointF(cx + 3, cy + 1.5f), Gdiplus::PointF(cx + 3, cy + 6) };
    g.DrawLines(&pen, label, 4);
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
    COLORREF iconCol = isDisabled ? TextMuted : (isHover || isPressed ? Accent : TextMuted);

    switch (m_iconType)
    {
    case ICON_FORMAT: DrawCodeGlyph(g, 17.0f, centerY, isDisabled ? TextMuted : Accent); textLeft = 31; break;
    case ICON_COPY:   DrawCopyGlyph(g, 16.0f, centerY, iconCol);                          textLeft = 29; break;
    case ICON_SAVE:   DrawSaveGlyph(g, 16.0f, centerY, iconCol);                          textLeft = 29; break;
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
        DrawChevron(g, rcBtn.right - 12.0f, centerY, 2, isHover ? Accent : TextMuted, 1.4f, 0.9f);

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
        memDC.SetTextColor(RGB(138, 146, 156));

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
// CValueEditCtrl
/////////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CValueEditCtrl, CRichEditCtrl)
    ON_WM_CONTEXTMENU()
    ON_WM_KEYDOWN()
    ON_WM_GETDLGCODE()
END_MESSAGE_MAP()

CValueEditCtrl::CValueEditCtrl()
{
}

CValueEditCtrl::~CValueEditCtrl()
{
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
END_MESSAGE_MAP()

CValuePanelWnd::CValuePanelWnd(OciGridView* pGrid)
    : m_pGrid(pGrid)
    , m_currentFormat(VF_TEXT)
    , m_bUserSelectedFormat(false)
    , m_bAutoFormat(true)
    , m_bWordWrap(false)
    , m_bCompact(false)
    , m_encoding(VE_UTF8)
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
    m_wndEdit.LimitText(0x7FFFFFF);
    m_wndEdit.SetEventMask(m_wndEdit.GetEventMask() | ENM_SCROLL | ENM_CHANGE);

    // Gutter
    m_wndGutter.Create(this, IDC_VP_GUTTER, &m_wndEdit);
    m_wndGutter.SetFont(&m_fontEditor);

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
    m_btnFormat.MoveWindow(8, btnY, 88, btnH);
    m_btnCopy.MoveWindow(102, btnY, 80, btnH);
    m_btnSave.MoveWindow(188, btnY, 88, btnH);

    // area do editor e gutter
    int gutterW = 40;
    int editW = (std::max)(10, cx - gutterW);
    int contentH = (std::max)(10, cy - topOffset);

    m_wndGutter.MoveWindow(0, topOffset, gutterW, contentH);
    m_wndEdit.MoveWindow(gutterW, topOffset, editW, contentH);
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
        FillRoundRect(g, rcClose, 5.0f, RGB(236, 238, 242));
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
    if (m_wndEdit.GetSafeHwnd())
    {
        m_wndEdit.SetSel(0, -1);
        m_wndEdit.SetFocus();
    }
}

void CValuePanelWnd::CopySelection()
{
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
