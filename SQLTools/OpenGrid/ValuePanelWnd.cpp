#include "stdafx.h"
#include "ValuePanelWnd.h"
#include "OCIGridView.h"
#include <COMMCTRL.H>

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

    // Fundo da toolbar atrás do botão
    memDC.FillSolidRect(rc, RGB(245, 246, 248));

    // Cores modernas estilo Fluent / VS Code
    COLORREF bgCol = RGB(255, 255, 255);
    COLORREF borderCol = RGB(218, 222, 228);

    if (isPressed)
    {
        bgCol = RGB(212, 228, 250);
        borderCol = RGB(0, 120, 215);
    }
    else if (isHover)
    {
        bgCol = RGB(232, 242, 254);
        borderCol = RGB(160, 198, 245);
    }

    CBrush bgBrush(bgCol);
    CPen borderPen(PS_SOLID, 1, borderCol);
    CBrush* pOldBrush = memDC.SelectObject(&bgBrush);
    CPen* pOldPen = memDC.SelectObject(&borderPen);

    memDC.RoundRect(rc, CPoint(4, 4));

    memDC.SelectObject(pOldBrush);
    memDC.SelectObject(pOldPen);

    int textLeft = rc.left + 8;
    int centerY = rc.CenterPoint().y;

    if (m_iconType != ICON_NONE)
    {
        int iconX = rc.left + 9;
        COLORREF iconCol = isHover ? RGB(0, 90, 180) : RGB(70, 75, 85);

        switch (m_iconType)
        {
        case ICON_FORMAT:
            {
                // Ícone de código < / >
                CPen tagPen(PS_SOLID, 1, RGB(0, 102, 204));
                CPen* pOld = memDC.SelectObject(&tagPen);

                // '<'
                memDC.MoveTo(iconX + 3, centerY - 4);
                memDC.LineTo(iconX, centerY);
                memDC.LineTo(iconX + 3, centerY + 4);

                // '/'
                memDC.MoveTo(iconX + 4, centerY + 4);
                memDC.LineTo(iconX + 7, centerY - 4);

                // '>'
                memDC.MoveTo(iconX + 8, centerY - 4);
                memDC.LineTo(iconX + 11, centerY);
                memDC.LineTo(iconX + 8, centerY + 4);

                memDC.SelectObject(pOld);
                textLeft = iconX + 16;
            }
            break;

        case ICON_COPY:
            {
                // Ícone de copiar (duas páginas sobrepostas)
                CPen copyPen(PS_SOLID, 1, iconCol);
                CPen* pOld = memDC.SelectObject(&copyPen);

                // Folha de trás
                memDC.MoveTo(iconX + 2, centerY - 5);
                memDC.LineTo(iconX + 8, centerY - 5);
                memDC.LineTo(iconX + 8, centerY + 2);
                memDC.MoveTo(iconX + 2, centerY - 5);
                memDC.LineTo(iconX - 1, centerY - 5);
                memDC.LineTo(iconX - 1, centerY + 2);

                // Folha da frente
                CRect rcFront(iconX + 1, centerY - 2, iconX + 8, centerY + 6);
                memDC.FillSolidRect(&rcFront, bgCol);
                memDC.Draw3dRect(&rcFront, iconCol, iconCol);

                memDC.SelectObject(pOld);
                textLeft = iconX + 13;
            }
            break;

        case ICON_SAVE:
            {
                // Ícone de salvar (disquete)
                CPen savePen(PS_SOLID, 1, iconCol);
                CPen* pOld = memDC.SelectObject(&savePen);

                memDC.MoveTo(iconX - 1, centerY - 5);
                memDC.LineTo(iconX + 6, centerY - 5);
                memDC.LineTo(iconX + 8, centerY - 3);
                memDC.LineTo(iconX + 8, centerY + 5);
                memDC.LineTo(iconX - 1, centerY + 5);
                memDC.LineTo(iconX - 1, centerY - 5);

                // Obturador
                memDC.MoveTo(iconX + 1, centerY - 5);
                memDC.LineTo(iconX + 1, centerY - 2);
                memDC.LineTo(iconX + 4, centerY - 2);
                memDC.LineTo(iconX + 4, centerY - 5);

                // Etiqueta
                memDC.MoveTo(iconX + 1, centerY + 1);
                memDC.LineTo(iconX + 6, centerY + 1);
                memDC.LineTo(iconX + 6, centerY + 5);
                memDC.LineTo(iconX + 1, centerY + 5);
                memDC.LineTo(iconX + 1, centerY + 1);

                memDC.SelectObject(pOld);
                textLeft = iconX + 14;
            }
            break;

        default:
            break;
        }
    }

    // Texto do botão
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
    memDC.SetTextColor(isDisabled ? RGB(160, 160, 165) : RGB(35, 38, 44));

    int textRight = m_bDropdown ? rc.right - 14 : rc.right - 4;
    CRect rcText(textLeft, rc.top, textRight, rc.bottom);
    memDC.DrawText(text, rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Seta Dropdown discreta à direita
    if (m_bDropdown)
    {
        int arrowX = rc.right - 10;
        CPen arrowPen(PS_SOLID, 1, isHover ? RGB(0, 102, 204) : RGB(100, 105, 115));
        CPen* pOld = memDC.SelectObject(&arrowPen);

        memDC.MoveTo(arrowX - 3, centerY - 1);
        memDC.LineTo(arrowX + 2, centerY - 1);
        memDC.MoveTo(arrowX - 2, centerY);
        memDC.LineTo(arrowX + 1, centerY);
        memDC.MoveTo(arrowX - 1, centerY + 1);
        memDC.LineTo(arrowX, centerY + 1);

        memDC.SelectObject(pOld);
    }

    memDC.SelectObject(pOldF);

    dc.BitBlt(0, 0, rc.Width(), rc.Height(), &memDC, 0, 0, SRCCOPY);
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
    CPaintDC dc(this);
    CRect rc;
    GetClientRect(rc);

    CDC memDC;
    memDC.CreateCompatibleDC(&dc);
    CBitmap memBmp;
    memBmp.CreateCompatibleBitmap(&dc, rc.Width(), rc.Height());
    CBitmap* pOldBmp = memDC.SelectObject(&memBmp);

    // Fundo da barra divisória
    COLORREF bgCol = RGB(245, 246, 248);
    memDC.FillSolidRect(rc, bgCol);

    // Linha divisória lateral esquerda
    CPen linePen(PS_SOLID, 1, RGB(222, 225, 230));
    CPen* pOldPen = memDC.SelectObject(&linePen);
    memDC.MoveTo(rc.left, rc.top);
    memDC.LineTo(rc.left, rc.bottom);

    // Desenha o botão em formato de pílula central moderno
    CRect rcPill = GetPillRect();
    COLORREF pillBg = m_bHoverPill ? RGB(225, 238, 255) : RGB(240, 242, 246);
    COLORREF pillBorder = m_bHoverPill ? RGB(0, 120, 215) : RGB(210, 214, 220);

    CBrush pillBrush(pillBg);
    CPen borderPen(PS_SOLID, 1, pillBorder);
    memDC.SelectObject(&pillBrush);
    memDC.SelectObject(&borderPen);
    memDC.RoundRect(rcPill, CPoint(6, 6));

    // Desenha chevron moderno e nítido
    COLORREF arrowCol = m_bHoverPill ? RGB(0, 102, 204) : RGB(90, 95, 105);
    CPen arrowPen(PS_SOLID, 2, arrowCol);
    memDC.SelectObject(&arrowPen);

    int midX = rcPill.CenterPoint().x;
    int midY = rcPill.CenterPoint().y;

    if (m_bCollapsed)
    {
        // Painel colapsado: chevron aponta para a esquerda (<)
        memDC.MoveTo(midX + 2, midY - 5);
        memDC.LineTo(midX - 2, midY);
        memDC.LineTo(midX + 2, midY + 5);
    }
    else
    {
        // Painel expandido: chevron aponta para a direita (>)
        memDC.MoveTo(midX - 2, midY - 5);
        memDC.LineTo(midX + 2, midY);
        memDC.LineTo(midX - 2, midY + 5);
    }

    // Risquinhos táteis de gripper acima e abaixo da seta
    CPen gripPen(PS_SOLID, 1, m_bHoverPill ? RGB(140, 185, 240) : RGB(190, 195, 205));
    memDC.SelectObject(&gripPen);

    memDC.MoveTo(midX - 2, midY - 12);
    memDC.LineTo(midX + 3, midY - 12);
    memDC.MoveTo(midX - 2, midY - 15);
    memDC.LineTo(midX + 3, midY - 15);

    memDC.MoveTo(midX - 2, midY + 12);
    memDC.LineTo(midX + 3, midY + 12);
    memDC.MoveTo(midX - 2, midY + 15);
    memDC.LineTo(midX + 3, midY + 15);

    memDC.SelectObject(pOldPen);
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
    memDC.FillSolidRect(rc, RGB(245, 245, 247));

    // Borda vertical direita do gutter
    CPen borderPen(PS_SOLID, 1, RGB(225, 225, 230));
    CPen* pOldPen = memDC.SelectObject(&borderPen);
    memDC.MoveTo(rc.right - 1, rc.top);
    memDC.LineTo(rc.right - 1, rc.bottom);

    if (m_pEdit && m_pEdit->GetSafeHwnd())
    {
        CFont* pOldFont = NULL;
        if (m_pFont)
            pOldFont = memDC.SelectObject(m_pFont);

        memDC.SetBkMode(TRANSPARENT);
        memDC.SetTextColor(RGB(145, 148, 155));

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
    return CRect(0, 0, 80, 25);
}

CRect CValuePanelWnd::GetCloseButtonRect() const
{
    CRect rc;
    GetClientRect(rc);
    return CRect(rc.right - 24, 2, rc.right - 2, 23);
}

void CValuePanelWnd::LayoutChildren(int cx, int cy)
{
    if (cx <= 0 || cy <= 0) return;

    int headerH = 25;
    int toolbarH = 30;
    int topOffset = headerH + toolbarH;

    // Posiciona botões da toolbar (altura 24px, moderno)
    int btnY = headerH + 3;
    int btnH = 24;
    m_btnFormat.MoveWindow(6, btnY, 84, btnH);
    m_btnCopy.MoveWindow(94, btnY, 78, btnH);
    m_btnSave.MoveWindow(176, btnY, 84, btnH);

    // Área do editor e gutter
    int gutterW = 38;
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
    CPaintDC dc(this);
    CRect rc;
    GetClientRect(rc);

    int headerH = 25;
    int toolbarH = 30;

    // Header Background
    CRect rcHeader(rc.left, rc.top, rc.right, rc.top + headerH);
    dc.FillSolidRect(rcHeader, RGB(232, 234, 238));

    // Tab "Valor"
    CRect rcTab = GetTabRect();
    dc.FillSolidRect(rcTab, RGB(255, 255, 255));
    CPen tabBorder(PS_SOLID, 1, RGB(200, 202, 208));
    CPen* pOldPen = dc.SelectObject(&tabBorder);
    dc.MoveTo(rcTab.right, rcTab.top);
    dc.LineTo(rcTab.right, rcTab.bottom);

    // Texto da aba
    CFont* pOldFont = dc.SelectObject(&m_fontUiBold);
    dc.SetBkMode(TRANSPARENT);
    dc.SetTextColor(RGB(40, 40, 45));
    CRect rcTabText = rcTab;
    rcTabText.left += 8;
    dc.DrawText(_T("Valor"), rcTabText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Botão Fechar [X] no header
    CRect rcClose = GetCloseButtonRect();
    if (m_bHoverClose)
    {
        dc.FillSolidRect(rcClose, RGB(232, 17, 35));
        dc.SetTextColor(RGB(255, 255, 255));
    }
    else
    {
        dc.SetTextColor(RGB(110, 115, 125));
    }
    dc.SelectObject(&m_fontUi);
    dc.DrawText(_T("\x2715"), rcClose, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Linha divisória abaixo do header
    dc.MoveTo(rc.left, rcHeader.bottom - 1);
    dc.LineTo(rc.right, rcHeader.bottom - 1);

    // Toolbar Background
    CRect rcToolbar(rc.left, rcHeader.bottom, rc.right, rcHeader.bottom + toolbarH);
    dc.FillSolidRect(rcToolbar, RGB(245, 246, 248));

    // Linha divisória abaixo da toolbar
    dc.MoveTo(rc.left, rcToolbar.bottom - 1);
    dc.LineTo(rc.right, rcToolbar.bottom - 1);

    dc.SelectObject(pOldPen);
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
