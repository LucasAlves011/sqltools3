#include "stdafx.h"
#include "Theme/ModernPopup.h"

using namespace ModernTheme;

static const wchar_t* const POPUP_CLASS = L"SQLTools_ModernPopup";

BEGIN_MESSAGE_MAP(CModernPopup, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_ACTIVATE()
    ON_WM_KEYDOWN()
    ON_WM_GETDLGCODE()
    ON_WM_MOUSEACTIVATE()
END_MESSAGE_MAP()

CModernPopup::CModernPopup ()
{
}

CModernPopup::~CModernPopup ()
{
}

BOOL CModernPopup::CreatePopup (CWnd* owner, CPoint screenPos, CSize size)
{
    HINSTANCE hInst = AfxGetInstanceHandle();
    WNDCLASS wc;
    if (!::GetClassInfo(hInst, POPUP_CLASS, &wc))
    {
        memset(&wc, 0, sizeof(wc));
        wc.style = CS_DROPSHADOW | CS_DBLCLKS;
        wc.lpfnWndProc = ::DefWindowProc;
        wc.hInstance = hInst;
        wc.hCursor = ::LoadCursor(NULL, IDC_ARROW);
        wc.lpszClassName = POPUP_CLASS;
        AfxRegisterClass(&wc);
    }

    // mantem o painel dentro da area de trabalho do monitor
    CRect want(screenPos, size);
    HMONITOR hMon = ::MonitorFromPoint(screenPos, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(mi) };
    if (::GetMonitorInfo(hMon, &mi))
    {
        if (want.right > mi.rcWork.right)   want.OffsetRect(mi.rcWork.right - want.right, 0);
        if (want.bottom > mi.rcWork.bottom) want.OffsetRect(0, mi.rcWork.bottom - want.bottom);
        if (want.left < mi.rcWork.left)     want.OffsetRect(mi.rcWork.left - want.left, 0);
        if (want.top < mi.rcWork.top)       want.OffsetRect(0, mi.rcWork.top - want.top);
    }

    CWnd* top = owner ? owner->GetTopLevelParent() : NULL;
    if (!CreateEx(WS_EX_TOOLWINDOW, POPUP_CLASS, L"", WS_POPUP | WS_CLIPCHILDREN, want, top, 0))
        return FALSE;

    // cantos arredondados nativos (Windows 11); em versoes anteriores ficam retos
    typedef HRESULT (WINAPI *DwmSetAttr)(HWND, DWORD, LPCVOID, DWORD);
    if (HMODULE hDwm = ::LoadLibraryW(L"dwmapi.dll"))
    {
        if (DwmSetAttr fn = (DwmSetAttr)::GetProcAddress(hDwm, "DwmSetWindowAttribute"))
        {
            DWORD pref = 2; // DWMWCP_ROUND
            fn(m_hWnd, 33 /*DWMWA_WINDOW_CORNER_PREFERENCE*/, &pref, sizeof(pref));
        }
        ::FreeLibrary(hDwm);
    }

    OnPopupCreated();
    ShowWindow(SW_SHOW);
    OnPopupShown();
    return TRUE;
}

void CModernPopup::Close ()
{
    if (m_hWnd)
        DestroyWindow();
}

void CModernPopup::PostNcDestroy ()
{
    OnPopupClosed();
    delete this;
}

BOOL CModernPopup::OnEraseBkgnd (CDC*)
{
    return TRUE;
}

void CModernPopup::OnPaint ()
{
    CPaintDC dc(this);
    CRect rc;
    GetClientRect(rc);

    CDC mem;
    mem.CreateCompatibleDC(&dc);
    CBitmap bmp;
    bmp.CreateCompatibleBitmap(&dc, rc.Width(), rc.Height());
    CBitmap* pOld = mem.SelectObject(&bmp);

    mem.FillSolidRect(rc, Surface);
    {
        Gdiplus::Graphics g(mem.GetSafeHdc());
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        Paint(mem, g, rc);
        // borda fina por cima de tudo
        Gdiplus::Pen pen(ModernDraw::Clr(BorderStrong), 1.0f);
        g.DrawRectangle(&pen, 0, 0, rc.Width() - 1, rc.Height() - 1);
    }

    dc.BitBlt(0, 0, rc.Width(), rc.Height(), &mem, 0, 0, SRCCOPY);
    mem.SelectObject(pOld);
}

void CModernPopup::OnActivate (UINT nState, CWnd* pWndOther, BOOL bMinimized)
{
    CWnd::OnActivate(nState, pWndOther, bMinimized);
    if (nState == WA_INACTIVE)
        PostMessage(WM_CLOSE);
}

void CModernPopup::OnKeyDown (UINT nChar, UINT nRepCnt, UINT nFlags)
{
    if (nChar == VK_ESCAPE)
    {
        Close();
        return;
    }
    CWnd::OnKeyDown(nChar, nRepCnt, nFlags);
}

UINT CModernPopup::OnGetDlgCode ()
{
    return DLGC_WANTALLKEYS;
}

int CModernPopup::OnMouseActivate (CWnd*, UINT, UINT)
{
    return MA_ACTIVATE;
}
