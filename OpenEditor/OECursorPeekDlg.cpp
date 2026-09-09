#include "stdafx.h"
#include "OpenEditor/OECursorPeekDlg.h"
#include "OpenEditor/OEView.h"
#include "Common/ExceptionHelper.h"
#include <algorithm>
#include <cwctype>
#include <unordered_set>

namespace OpenEditor
{

static const UINT IDC_PEEK_EDIT = 10101;
std::vector<COECursorPeekDlg*> COECursorPeekDlg::s_activeDialogs;

COECursorPeekDlg::COECursorPeekDlg()
    : m_pEditor(nullptr)
    , m_bPinned(false)
    , m_bModified(false)
    , m_bSyncing(false)
{
    ::AfxInitRichEdit2();
}

COECursorPeekDlg::~COECursorPeekDlg()
{
}

void COECursorPeekDlg::RegisterDialog(COECursorPeekDlg* pDlg)
{
    if (std::find(s_activeDialogs.begin(), s_activeDialogs.end(), pDlg) == s_activeDialogs.end())
        s_activeDialogs.push_back(pDlg);
}

void COECursorPeekDlg::UnregisterDialog(COECursorPeekDlg* pDlg)
{
    auto it = std::find(s_activeDialogs.begin(), s_activeDialogs.end(), pDlg);
    if (it != s_activeDialogs.end())
        s_activeDialogs.erase(it);
}

void COECursorPeekDlg::CloseAllUnpinned(COEditorView* pEditor)
{
    std::vector<COECursorPeekDlg*> copy = s_activeDialogs;
    for (auto* pDlg : copy)
    {
        if (pDlg && ::IsWindow(pDlg->m_hWnd) && !pDlg->IsPinned())
        {
            if (!pEditor || pDlg->m_pEditor == pEditor)
            {
                pDlg->DestroyWindow();
            }
        }
    }
}

void COECursorPeekDlg::CloseAllForEditor(COEditorView* pEditor)
{
    std::vector<COECursorPeekDlg*> copy = s_activeDialogs;
    for (auto* pDlg : copy)
    {
        if (pDlg && ::IsWindow(pDlg->m_hWnd) && pDlg->m_pEditor == pEditor)
        {
            pDlg->DestroyWindow();
        }
    }
}

bool COECursorPeekDlg::HasActiveDialogs()
{
    return !s_activeDialogs.empty();
}

COECursorPeekDlg* COECursorPeekDlg::OpenPeek(COEditorView* pEditor, const PlSqlObjectInfo& info, CPoint ptScreen)
{
    if (!pEditor || !::IsWindow(pEditor->m_hWnd))
        return nullptr;

    // Se já houver uma janela aberta para este mesmo objeto neste editor, traz para frente
    for (auto* pDlg : s_activeDialogs)
    {
        if (pDlg && ::IsWindow(pDlg->m_hWnd) && pDlg->m_pEditor == pEditor)
        {
            if (_wcsicmp(pDlg->GetObjectName().c_str(), info.name.c_str()) == 0)
            {
                pDlg->BringWindowToTop();
                pDlg->SetForegroundWindow();
                pDlg->m_editCtrl.SetFocus();
                return pDlg;
            }
        }
    }

    // Calcula deslocamento em cascata se já existirem outras janelas abertas
    int activeCount = (int)s_activeDialogs.size();
    int offsetX = (activeCount % 8) * 30;
    int offsetY = (activeCount % 8) * 25;

    CPoint ptFinal(ptScreen.x + offsetX, ptScreen.y + offsetY);

    // Ajusta para manter dentro da área útil do monitor
    HMONITOR hMon = MonitorFromPoint(ptFinal, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi;
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfo(hMon, &mi))
    {
        const int dlgW = 550;
        const int dlgH = 320;
        if (ptFinal.x + dlgW > mi.rcWork.right)
            ptFinal.x = mi.rcWork.right - dlgW - 10;
        if (ptFinal.x < mi.rcWork.left)
            ptFinal.x = mi.rcWork.left + 10;
        if (ptFinal.y + dlgH > mi.rcWork.bottom)
            ptFinal.y = mi.rcWork.bottom - dlgH - 10;
        if (ptFinal.y < mi.rcWork.top)
            ptFinal.y = mi.rcWork.top + 10;
    }

    COECursorPeekDlg* pDlg = new COECursorPeekDlg();
    pDlg->m_pEditor = pEditor;
    pDlg->m_objectInfo = info;

    CString className = AfxRegisterWndClass(
        CS_HREDRAW | CS_VREDRAW,
        ::LoadCursor(NULL, IDC_ARROW),
        (HBRUSH)(COLOR_BTNFACE + 1),
        NULL
    );

    CRect rcInitial(ptFinal.x, ptFinal.y, ptFinal.x + 550, ptFinal.y + 320);

    BOOL ok = pDlg->CreateEx(
        WS_EX_TOOLWINDOW,
        className,
        info.title.c_str(),
        WS_POPUP | WS_THICKFRAME | WS_CLIPCHILDREN | WS_VISIBLE,
        rcInitial,
        AfxGetMainWnd(),
        0
    );

    if (!ok)
    {
        delete pDlg;
        return nullptr;
    }

    RegisterDialog(pDlg);

    pDlg->m_bSyncing = true;
    pDlg->m_editCtrl.SetWindowText(info.selectQuery.c_str());
    pDlg->ApplySyntaxHighlighting();
    pDlg->m_bModified = false;
    pDlg->m_bSyncing = false;

    pDlg->InvalidateRect(CRect(0, 0, 10000, HEADER_HEIGHT), FALSE);
    return pDlg;
}

BEGIN_MESSAGE_MAP(COECursorPeekDlg, CWnd)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_WM_PAINT()
    ON_WM_LBUTTONDOWN()
    ON_WM_NCHITTEST()
    ON_EN_CHANGE(IDC_PEEK_EDIT, OnEditChange)
    ON_WM_KILLFOCUS()
    ON_WM_CLOSE()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

int COECursorPeekDlg::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CWnd::OnCreate(lpCreateStruct) == -1)
        return -1;

    m_font.CreatePointFont(100, _T("Consolas"));
    m_uiFont.CreatePointFont(90, _T("Segoe UI"));

    CRect rcClient;
    GetClientRect(&rcClient);

    CRect rcEdit(0, HEADER_HEIGHT, rcClient.Width(), rcClient.Height());
    m_editCtrl.Create(
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | 
        ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN | ES_NOHIDESEL,
        rcEdit, this, IDC_PEEK_EDIT
    );

    m_editCtrl.SetFont(&m_font);
    m_editCtrl.SetBackgroundColor(FALSE, RGB(255, 255, 255));
    m_editCtrl.SetEventMask(m_editCtrl.GetEventMask() | ENM_CHANGE);

    return 0;
}

void COECursorPeekDlg::PostNcDestroy()
{
    UnregisterDialog(this);
    delete this;
}

void COECursorPeekDlg::LayoutControls(int cx, int cy)
{
    if (m_editCtrl.m_hWnd && ::IsWindow(m_editCtrl.m_hWnd))
    {
        m_editCtrl.MoveWindow(0, HEADER_HEIGHT, cx, std::max(0, cy - HEADER_HEIGHT));
    }

    int r = cx - 4;
    m_rcCloseBtn = CRect(r - 28, 3, r, 3 + BTN_HEIGHT);
    r -= (28 + 4);

    m_rcPinBtn = CRect(r - 54, 3, r, 3 + BTN_HEIGHT);
    r -= (54 + 4);

    m_rcSyncBtn = CRect(r - 54, 3, r, 3 + BTN_HEIGHT);
    r -= (54 + 4);

    m_rcGoBtn = CRect(r - 34, 3, r, 3 + BTN_HEIGHT);
}

void COECursorPeekDlg::OnSize(UINT nType, int cx, int cy)
{
    CWnd::OnSize(nType, cx, cy);
    LayoutControls(cx, cy);
    InvalidateRect(CRect(0, 0, cx, HEADER_HEIGHT), FALSE);
}

BOOL COECursorPeekDlg::OnEraseBkgnd(CDC* pDC)
{
    return TRUE;
}

void COECursorPeekDlg::OnPaint()
{
    CPaintDC dc(this);
    CRect rcClient;
    GetClientRect(&rcClient);

    CRect rcHeader(0, 0, rcClient.Width(), HEADER_HEIGHT);

    COLORREF headerBg = RGB(36, 41, 46);
    COLORREF textColor = RGB(240, 242, 245);
    COLORREF btnBg = RGB(50, 56, 64);
    COLORREF btnBorder = RGB(70, 78, 88);

    dc.FillSolidRect(&rcHeader, headerBg);
    dc.FillSolidRect(0, HEADER_HEIGHT - 1, rcClient.Width(), 1, RGB(60, 68, 77));

    CFont* pOldFont = dc.SelectObject(&m_uiFont);
    dc.SetBkMode(TRANSPARENT);
    dc.SetTextColor(textColor);

    CString strTitle;
    strTitle.Format(_T("  %s (Linha %d)"), m_objectInfo.title.c_str(), m_objectInfo.declLineStart + 1);
    CRect rcTitle(4, 0, m_rcGoBtn.left - 8, HEADER_HEIGHT);
    dc.DrawText(strTitle, &rcTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    auto drawButton = [&](const CRect& rc, const TCHAR* label, bool active) {
        dc.FillSolidRect(&rc, active ? RGB(0, 120, 215) : btnBg);
        dc.Draw3dRect(&rc, btnBorder, btnBorder);
        dc.SetTextColor(active ? RGB(255, 255, 255) : textColor);
        CRect rcText = rc;
        dc.DrawText(label, -1, &rcText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    };

    drawButton(m_rcGoBtn, _T("Ir"), false);
    drawButton(m_rcSyncBtn, m_bModified ? _T("Salvar*") : _T("Salvar"), m_bModified);
    drawButton(m_rcPinBtn, m_bPinned ? _T("Fixado") : _T("Fixar"), m_bPinned);
    drawButton(m_rcCloseBtn, _T("X"), false);

    dc.SelectObject(pOldFont);
}

LRESULT COECursorPeekDlg::OnNcHitTest(CPoint point)
{
    CPoint ptClient = point;
    ScreenToClient(&ptClient);

    if (ptClient.y >= 0 && ptClient.y < HEADER_HEIGHT)
    {
        if (!m_rcCloseBtn.PtInRect(ptClient) &&
            !m_rcPinBtn.PtInRect(ptClient) &&
            !m_rcSyncBtn.PtInRect(ptClient) &&
            !m_rcGoBtn.PtInRect(ptClient))
        {
            return HTCAPTION;
        }
    }

    return CWnd::OnNcHitTest(point);
}

void COECursorPeekDlg::OnLButtonDown(UINT nFlags, CPoint point)
{
    if (m_rcCloseBtn.PtInRect(point))
    {
        if (m_bModified)
            SyncToEditor();
        DestroyWindow();
        return;
    }
    else if (m_rcPinBtn.PtInRect(point))
    {
        m_bPinned = !m_bPinned;
        SetWindowPos(m_bPinned ? &wndTopMost : &wndNoTopMost, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        InvalidateRect(CRect(0, 0, 10000, HEADER_HEIGHT), FALSE);
        return;
    }
    else if (m_rcGoBtn.PtInRect(point))
    {
        if (m_pEditor && ::IsWindow(m_pEditor->m_hWnd))
        {
            EXCEPTION_FRAME;
            try
            {
                m_pEditor->ScrollTo(m_objectInfo.declLineStart);
                Position pos;
                pos.line = m_objectInfo.declLineStart;
                pos.column = m_objectInfo.declStart.column;
                m_pEditor->MoveTo(pos);
                m_pEditor->SetFocus();
            }
            catch (...)
            {
            }
        }
        return;
    }
    else if (m_rcSyncBtn.PtInRect(point))
    {
        SyncToEditor();
        InvalidateRect(CRect(0, 0, 10000, HEADER_HEIGHT), FALSE);
        return;
    }

    CWnd::OnLButtonDown(nFlags, point);
}

void COECursorPeekDlg::OnEditChange()
{
    if (m_bSyncing)
        return;

    m_bModified = true;
    InvalidateRect(CRect(0, 0, 10000, HEADER_HEIGHT), FALSE);
}

void COECursorPeekDlg::SyncToEditor()
{
    if (!m_pEditor || !::IsWindow(m_pEditor->m_hWnd) || !m_bModified || m_bSyncing)
        return;

    m_bSyncing = true;

    CString newText;
    m_editCtrl.GetWindowText(newText);

    EXCEPTION_FRAME;
    try
    {
        COEditorView::UndoGroup undoGroup(*m_pEditor);

        Position savedPos = m_pEditor->GetPosition();

        Square selSquare;
        selSquare.start = m_objectInfo.selectStart;
        selSquare.end   = m_objectInfo.selectEnd;

        m_pEditor->SetBlockMode(ebtStream);
        m_pEditor->MoveTo(selSquare.start);
        m_pEditor->SetSelection(selSquare);
        m_pEditor->DeleteBlock(false);
        m_pEditor->InsertBlock(newText.GetString(), true, false);

        m_pEditor->ClearSelection();
        m_pEditor->MoveTo(savedPos);

        PlSqlObjectInfo updatedInfo;
        if (PlSqlCursorScanner::FindLocalObjectByName(m_pEditor, m_objectInfo.name, updatedInfo))
        {
            m_objectInfo = updatedInfo;
        }
    }
    catch (...)
    {
    }

    m_bModified = false;
    m_bSyncing = false;
}

void COECursorPeekDlg::OnKillFocus(CWnd* pNewWnd)
{
    CWnd::OnKillFocus(pNewWnd);

    if (!m_bPinned && pNewWnd != &m_editCtrl && pNewWnd != this)
    {
        if (m_bModified)
            SyncToEditor();
    }
}

void COECursorPeekDlg::OnClose()
{
    if (m_bModified)
        SyncToEditor();
    DestroyWindow();
}

BOOL COECursorPeekDlg::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN)
    {
        if (pMsg->wParam == VK_ESCAPE)
        {
            if (!m_bPinned)
            {
                if (m_bModified)
                    SyncToEditor();
                DestroyWindow();
                return TRUE;
            }
        }
        else if (pMsg->wParam == 'A' && (GetKeyState(VK_CONTROL) & 0x8000))
        {
            if (::GetFocus() == m_editCtrl.m_hWnd)
            {
                m_editCtrl.SetSel(0, -1);
                return TRUE;
            }
        }
        else if (pMsg->wParam == 'S' && (GetKeyState(VK_CONTROL) & 0x8000))
        {
            SyncToEditor();
            InvalidateRect(CRect(0, 0, 10000, HEADER_HEIGHT), FALSE);
            return TRUE;
        }
    }

    return CWnd::PreTranslateMessage(pMsg);
}

void COECursorPeekDlg::ApplySyntaxHighlighting()
{
    if (!m_editCtrl.m_hWnd || !::IsWindow(m_editCtrl.m_hWnd))
        return;

    m_editCtrl.SetRedraw(FALSE);

    POINT scrollPos;
    m_editCtrl.SendMessage(EM_GETSCROLLPOS, 0, (LPARAM)&scrollPos);
    CHARRANGE crOrg;
    m_editCtrl.GetSel(crOrg);

    CString text;
    m_editCtrl.GetWindowText(text);
    int len = text.GetLength();
    if (len == 0)
    {
        m_editCtrl.SetRedraw(TRUE);
        return;
    }

    // Define cor base padrão (preto)
    CHARFORMAT2W cf;
    ZeroMemory(&cf, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = RGB(0, 0, 0);

    m_editCtrl.SetSel(0, len);
    m_editCtrl.SendMessage(EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);

    static const std::unordered_set<std::wstring> s_keywords = {
        L"SELECT", L"FROM", L"WHERE", L"AND", L"OR", L"NOT", L"IN", L"IS", L"AS",
        L"INSERT", L"INTO", L"VALUES", L"UPDATE", L"SET", L"DELETE", L"JOIN", L"LEFT",
        L"RIGHT", L"INNER", L"OUTER", L"FULL", L"CROSS", L"ON", L"GROUP", L"BY",
        L"ORDER", L"HAVING", L"UNION", L"ALL", L"INTERSECT", L"MINUS", L"DISTINCT",
        L"CURSOR", L"PROCEDURE", L"FUNCTION", L"BEGIN", L"END", L"DECLARE", L"RETURN",
        L"IF", L"THEN", L"ELSE", L"ELSIF", L"LOOP", L"FOR", L"WHILE", L"EXIT", L"WHEN",
        L"CASE", L"EXCEPTION", L"RAISE", L"OPEN", L"FETCH", L"CLOSE", L"NULL", L"LIKE",
        L"BETWEEN", L"EXISTS", L"COUNT", L"SUM", L"AVG", L"MIN", L"MAX", L"NVL",
        L"DECODE", L"SUBSTR", L"INSTR", L"TO_CHAR", L"TO_DATE", L"TO_NUMBER", L"UPPER",
        L"LOWER", L"TRIM", L"RTRIM", L"LTRIM", L"SYSDATE", L"ROWNUM", L"ROWID",
        L"VARCHAR2", L"NUMBER", L"DATE", L"BOOLEAN", L"CHAR", L"CLOB", L"BLOB",
        L"INTEGER", L"TIMESTAMP", L"TYPE", L"ROWTYPE", L"RECORD", L"TABLE", L"OF",
        L"INDEX", L"BINARY_INTEGER", L"PLS_INTEGER", L"TRUE", L"FALSE", L"DEFAULT"
    };

    COLORREF colKeyword = RGB(0, 0, 255);       // Azul
    COLORREF colString  = RGB(163, 21, 21);     // Vermelho/marrom escuro
    COLORREF colComment = RGB(0, 128, 0);       // Verde escuro
    COLORREF colNumber  = RGB(9, 134, 88);      // Verde petróleo

    auto applyColor = [&](int start, int end, COLORREF color, bool bold = false) {
        if (start < end && start >= 0 && end <= len)
        {
            CHARFORMAT2W cfTok;
            ZeroMemory(&cfTok, sizeof(cfTok));
            cfTok.cbSize = sizeof(cfTok);
            cfTok.dwMask = CFM_COLOR | (bold ? CFM_BOLD : 0);
            cfTok.crTextColor = color;
            if (bold) cfTok.dwEffects = CFE_BOLD;

            m_editCtrl.SetSel(start, end);
            m_editCtrl.SendMessage(EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cfTok);
        }
    };

    const wchar_t* pStr = text.GetString();
    int i = 0;
    while (i < len)
    {
        // Comentário de bloco /* ... */
        if (i + 1 < len && pStr[i] == L'/' && pStr[i + 1] == L'*')
        {
            int start = i;
            i += 2;
            while (i + 1 < len && !(pStr[i] == L'*' && pStr[i + 1] == L'/'))
                ++i;
            if (i + 1 < len) i += 2;
            else i = len;
            applyColor(start, i, colComment);
            continue;
        }

        // Comentário de linha -- ...
        if (i + 1 < len && pStr[i] == L'-' && pStr[i + 1] == L'-')
        {
            int start = i;
            while (i < len && pStr[i] != L'\r' && pStr[i] != L'\n')
                ++i;
            applyColor(start, i, colComment);
            continue;
        }

        // String literal '...'
        if (pStr[i] == L'\'')
        {
            int start = i;
            ++i;
            while (i < len)
            {
                if (pStr[i] == L'\'')
                {
                    if (i + 1 < len && pStr[i + 1] == L'\'')
                        i += 2;
                    else
                    {
                        ++i;
                        break;
                    }
                }
                else
                    ++i;
            }
            applyColor(start, i, colString);
            continue;
        }

        // Palavra ou número
        if (iswalpha(pStr[i]) || pStr[i] == L'_')
        {
            int start = i;
            std::wstring word;
            while (i < len && (iswalnum(pStr[i]) || pStr[i] == L'_' || pStr[i] == L'$' || pStr[i] == L'#'))
            {
                word += towupper(pStr[i]);
                ++i;
            }

            if (s_keywords.find(word) != s_keywords.end())
            {
                applyColor(start, i, colKeyword, true);
            }
            continue;
        }
        else if (iswdigit(pStr[i]))
        {
            int start = i;
            while (i < len && (iswdigit(pStr[i]) || pStr[i] == L'.' || towupper(pStr[i]) == L'E'))
                ++i;
            applyColor(start, i, colNumber);
            continue;
        }

        ++i;
    }

    m_editCtrl.SetSel(crOrg);
    m_editCtrl.SendMessage(EM_SETSCROLLPOS, 0, (LPARAM)&scrollPos);
    m_editCtrl.SetRedraw(TRUE);
    m_editCtrl.Invalidate();
}

} // namespace OpenEditor

