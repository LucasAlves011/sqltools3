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

CPeekEditCtrl::CPeekEditCtrl()
{
}

CPeekEditCtrl::~CPeekEditCtrl()
{
}

BEGIN_MESSAGE_MAP(CPeekEditCtrl, CRichEditCtrl)
    ON_WM_CONTEXTMENU()
    ON_WM_GETDLGCODE()
END_MESSAGE_MAP()

UINT CPeekEditCtrl::OnGetDlgCode()
{
    return DLGC_WANTALLKEYS | DLGC_WANTARROWS | DLGC_WANTCHARS;
}

BOOL CPeekEditCtrl::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN)
    {
        bool bCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        if (bCtrl)
        {
            if (pMsg->wParam == 'C' || pMsg->wParam == VK_INSERT)
            {
                COECursorPeekDlg* pParent = (COECursorPeekDlg*)GetParent();
                if (pParent)
                    pParent->CopyContent();
                return TRUE;
            }
            else if (pMsg->wParam == 'A')
            {
                SetSel(0, -1);
                return TRUE;
            }
            else if (pMsg->wParam == 'X')
            {
                Cut();
                return TRUE;
            }
            else if (pMsg->wParam == 'V')
            {
                Paste();
                return TRUE;
            }
            else if (pMsg->wParam == 'Z')
            {
                Undo();
                return TRUE;
            }
        }
    }
    return CRichEditCtrl::PreTranslateMessage(pMsg);
}

void CPeekEditCtrl::OnContextMenu(CWnd* pWnd, CPoint pos)
{
    if (pos.x == -1 && pos.y == -1)
    {
        CRect rc;
        GetClientRect(&rc);
        pos = rc.CenterPoint();
        ClientToScreen(&pos);
    }

    CMenu menu;
    menu.CreatePopupMenu();

    long selStart = 0, selEnd = 0;
    GetSel(selStart, selEnd);
    bool bHasSelection = (selStart != selEnd);

    enum MenuCommands {
        CMD_UNDO = 101,
        CMD_CUT,
        CMD_COPY,
        CMD_COPY_ALL,
        CMD_PASTE,
        CMD_SELECT_ALL
    };

    menu.AppendMenu(MF_STRING | (CanUndo() ? MF_ENABLED : MF_GRAYED), CMD_UNDO, _T("Desfazer\tCtrl+Z"));
    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenu(MF_STRING | (bHasSelection ? MF_ENABLED : MF_GRAYED), CMD_CUT, _T("Recortar\tCtrl+X"));
    menu.AppendMenu(MF_STRING | (bHasSelection ? MF_ENABLED : MF_GRAYED), CMD_COPY, _T("Copiar\tCtrl+C"));
    menu.AppendMenu(MF_STRING | MF_ENABLED, CMD_COPY_ALL, _T("Copiar Tudo"));
    menu.AppendMenu(MF_STRING | (CanPaste() ? MF_ENABLED : MF_GRAYED), CMD_PASTE, _T("Colar\tCtrl+V"));
    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenu(MF_STRING | MF_ENABLED, CMD_SELECT_ALL, _T("Selecionar Tudo\tCtrl+A"));

    int cmd = menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, pos.x, pos.y, this);
    COECursorPeekDlg* pParent = (COECursorPeekDlg*)GetParent();
    switch (cmd)
    {
    case CMD_UNDO:
        Undo();
        break;
    case CMD_CUT:
        Cut();
        break;
    case CMD_COPY:
        if (pParent)
            pParent->CopyContent();
        else
            Copy();
        break;
    case CMD_COPY_ALL:
        if (pParent)
            pParent->CopyAllToClipboard();
        break;
    case CMD_PASTE:
        Paste();
        break;
    case CMD_SELECT_ALL:
        SetSel(0, -1);
        break;
    }
}

COECursorPeekDlg::COECursorPeekDlg()
    : m_pEditor(nullptr)
    , m_bPinned(false)
    , m_bModified(false)
    , m_bSyncing(false)
    , m_hoverBtn(BTN_NONE)
    , m_hasPrevPos(false)
    , m_prevEditorPos{0, 0}
{
    m_rcBackBtn.SetRectEmpty();
    ::AfxInitRichEdit2();
}

COECursorPeekDlg::~COECursorPeekDlg()
{
}

CSize COECursorPeekDlg::CalculateIdealSize(const std::wstring& text, const CRect& rcWork)
{
    int lineCount = 1;
    int maxLineLen = 0;
    int curLineLen = 0;

    for (wchar_t ch : text)
    {
        if (ch == L'\n')
        {
            lineCount++;
            if (curLineLen > maxLineLen)
                maxLineLen = curLineLen;
            curLineLen = 0;
        }
        else if (ch != L'\r')
        {
            if (ch == L'\t')
                curLineLen += 4;
            else
                curLineLen++;
        }
    }
    if (curLineLen > maxLineLen)
        maxLineLen = curLineLen;

    // Métricas aproximadas da fonte Consolas 10pt (8px largura, 17px altura por linha)
    const int charWidth = 8;
    const int lineHeight = 17;

    // Largura baseada no tamanho da maior linha + margens laterais + espaço do scroll
    int idealW = maxLineLen * charWidth + 60;
    // Mínimo de 450px para garantir espaço folgado ao título e todos os botões no cabeçalho
    int minW = 450;
    int maxW = std::min(880, (int)(rcWork.Width() * 0.85));
    idealW = std::max(minW, std::min(maxW, idealW));

    // Altura baseada nas linhas do cursor + cabeçalho + margens
    int idealH = HEADER_HEIGHT + lineCount * lineHeight + 26;
    // Mínimo de 150px (evita caixas achatadas) e máximo de 560px (ou 75% da altura útil)
    int minH = 150;
    int maxH = std::min(560, (int)(rcWork.Height() * 0.75));
    idealH = std::max(minH, std::min(maxH, idealH));

    return CSize(idealW, idealH);
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

    // Ajusta para manter dentro da área útil do monitor e calcula tamanho ideal do cursor
    HMONITOR hMon = MonitorFromPoint(ptFinal, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi;
    mi.cbSize = sizeof(mi);
    CRect rcWork(0, 0, 1920, 1080);
    if (GetMonitorInfo(hMon, &mi))
    {
        rcWork = mi.rcWork;
    }

    CSize idealSize = CalculateIdealSize(info.selectQuery, rcWork);
    const int dlgW = idealSize.cx;
    const int dlgH = idealSize.cy;

    if (ptFinal.x + dlgW > rcWork.right)
        ptFinal.x = rcWork.right - dlgW - 10;
    if (ptFinal.x < rcWork.left)
        ptFinal.x = rcWork.left + 10;
    if (ptFinal.y + dlgH > rcWork.bottom)
        ptFinal.y = rcWork.bottom - dlgH - 10;
    if (ptFinal.y < rcWork.top)
        ptFinal.y = rcWork.top + 10;

    COECursorPeekDlg* pDlg = new COECursorPeekDlg();
    pDlg->m_pEditor = pEditor;
    pDlg->m_objectInfo = info;

    CString className = AfxRegisterWndClass(
        CS_HREDRAW | CS_VREDRAW,
        ::LoadCursor(NULL, IDC_ARROW),
        (HBRUSH)(COLOR_BTNFACE + 1),
        NULL
    );

    CRect rcInitial(ptFinal.x, ptFinal.y, ptFinal.x + dlgW, ptFinal.y + dlgH);

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
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSELEAVE()
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

    // Inicializa tooltips para os botões do cabeçalho
    m_toolTip.Create(this, TTS_ALWAYSTIP | TTS_NOPREFIX);
    m_toolTip.SetMaxTipWidth(300);
    m_toolTip.AddTool(this, _T("Ir para a declaração no editor"), &m_rcGoBtn, 1);
    m_toolTip.AddTool(this, _T("Copiar seleção ou consulta completa (Ctrl+C)"), &m_rcCopyBtn, 2);
    m_toolTip.AddTool(this, _T("Salvar alterações de volta no editor (Ctrl+S)"), &m_rcSyncBtn, 3);
    m_toolTip.AddTool(this, _T("Fixar janela (manter sempre visível)"), &m_rcPinBtn, 4);
    m_toolTip.AddTool(this, _T("Fechar (Esc)"), &m_rcCloseBtn, 5);
    m_toolTip.AddTool(this, _T("Voltar para onde estava no editor"), &m_rcBackBtn, 6);
    m_toolTip.Activate(TRUE);

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

    int btnTop = (HEADER_HEIGHT - BTN_SIZE) / 2;
    int r = cx - 5;

    m_rcCloseBtn = CRect(r - BTN_SIZE, btnTop, r, btnTop + BTN_SIZE);
    r -= (BTN_SIZE + 2);

    m_rcPinBtn = CRect(r - BTN_SIZE, btnTop, r, btnTop + BTN_SIZE);
    r -= (BTN_SIZE + 2);

    m_rcSyncBtn = CRect(r - BTN_SIZE, btnTop, r, btnTop + BTN_SIZE);
    r -= (BTN_SIZE + 2);

    m_rcCopyBtn = CRect(r - BTN_SIZE, btnTop, r, btnTop + BTN_SIZE);
    r -= (BTN_SIZE + 2);

    m_rcGoBtn = CRect(r - BTN_SIZE, btnTop, r, btnTop + BTN_SIZE);

    if (m_hasPrevPos)
    {
        r -= (BTN_SIZE + 2);
        m_rcBackBtn = CRect(r - BTN_SIZE, btnTop, r, btnTop + BTN_SIZE);
    }
    else
    {
        m_rcBackBtn.SetRectEmpty();
    }

    if (m_toolTip.m_hWnd && ::IsWindow(m_toolTip.m_hWnd))
    {
        m_toolTip.SetToolRect(this, 1, &m_rcGoBtn);
        m_toolTip.SetToolRect(this, 2, &m_rcCopyBtn);
        m_toolTip.SetToolRect(this, 3, &m_rcSyncBtn);
        m_toolTip.SetToolRect(this, 4, &m_rcPinBtn);
        m_toolTip.SetToolRect(this, 5, &m_rcCloseBtn);
        m_toolTip.SetToolRect(this, 6, &m_rcBackBtn);

        if (m_hasPrevPos)
        {
            CString tip;
            tip.Format(_T("Voltar para onde estava no editor (Linha %d)"), m_prevEditorPos.line + 1);
            m_toolTip.UpdateTipText(tip, this, 6);
        }
    }
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

void COECursorPeekDlg::OnMouseMove(UINT nFlags, CPoint point)
{
    int newHover = BTN_NONE;
    if (m_hasPrevPos && m_rcBackBtn.PtInRect(point)) newHover = BTN_BACK;
    else if (m_rcGoBtn.PtInRect(point))        newHover = BTN_GO;
    else if (m_rcCopyBtn.PtInRect(point)) newHover = BTN_COPY;
    else if (m_rcSyncBtn.PtInRect(point)) newHover = BTN_SYNC;
    else if (m_rcPinBtn.PtInRect(point))  newHover = BTN_PIN;
    else if (m_rcCloseBtn.PtInRect(point)) newHover = BTN_CLOSE;

    if (newHover != m_hoverBtn)
    {
        m_hoverBtn = newHover;
        InvalidateRect(CRect(0, 0, 10000, HEADER_HEIGHT), FALSE);
    }

    TRACKMOUSEEVENT tme;
    tme.cbSize = sizeof(tme);
    tme.dwFlags = TME_LEAVE;
    tme.hwndTrack = m_hWnd;
    _TrackMouseEvent(&tme);

    CWnd::OnMouseMove(nFlags, point);
}

void COECursorPeekDlg::OnMouseLeave()
{
    if (m_hoverBtn != BTN_NONE)
    {
        m_hoverBtn = BTN_NONE;
        InvalidateRect(CRect(0, 0, 10000, HEADER_HEIGHT), FALSE);
    }
    CWnd::OnMouseLeave();
}

void COECursorPeekDlg::DrawDbIcon(CDC* pDC, int x, int y)
{
    // Desenha ícone de banco de dados moderno (cilindro em 3 camadas)
    COLORREF borderCol = RGB(0, 102, 204);
    COLORREF fillCol = RGB(224, 238, 255);

    CPen pen(PS_SOLID, 1, borderCol);
    CBrush brush(fillCol);
    CPen* pOldPen = pDC->SelectObject(&pen);
    CBrush* pOldBrush = pDC->SelectObject(&brush);

    // Camada inferior
    pDC->Ellipse(x, y + 9, x + 14, y + 14);
    pDC->Rectangle(x, y + 7, x + 14, y + 12);

    // Camada média
    pDC->Ellipse(x, y + 5, x + 14, y + 10);
    pDC->Rectangle(x, y + 3, x + 14, y + 8);

    // Camada superior
    pDC->Ellipse(x, y, x + 14, y + 5);

    pDC->SelectObject(pOldBrush);
    pDC->SelectObject(pOldPen);
}

void COECursorPeekDlg::DrawHeaderButton(CDC* pDC, const CRect& rc, int btnId)
{
    bool isHover = (m_hoverBtn == btnId);

    // Desenha fundo de hover
    if (btnId == BTN_CLOSE && isHover)
    {
        pDC->FillSolidRect(&rc, RGB(232, 17, 35));
    }
    else if (btnId == BTN_PIN && m_bPinned)
    {
        pDC->FillSolidRect(&rc, isHover ? RGB(205, 225, 250) : RGB(220, 235, 252));
        pDC->Draw3dRect(&rc, RGB(160, 195, 235), RGB(160, 195, 235));
    }
    else if (isHover)
    {
        pDC->FillSolidRect(&rc, RGB(225, 230, 238));
        pDC->Draw3dRect(&rc, RGB(205, 212, 222), RGB(205, 212, 222));
    }

    CPoint c = rc.CenterPoint();
    COLORREF iconCol = (btnId == BTN_CLOSE && isHover) ? RGB(255, 255, 255) : RGB(65, 72, 82);

    switch (btnId)
    {
    case BTN_BACK:
        {
            // Ícone 'Voltar': folha de código à direita + seta à esquerda (retornar)
            CPen pen(PS_SOLID, 1, iconCol);
            CPen* pOldPen = pDC->SelectObject(&pen);

            // Retângulo do arquivo/editor à direita
            pDC->MoveTo(c.x + 1, c.y - 6);
            pDC->LineTo(c.x + 7, c.y - 6);
            pDC->LineTo(c.x + 7, c.y + 6);
            pDC->LineTo(c.x + 1, c.y + 6);
            pDC->LineTo(c.x + 1, c.y - 6);

            // Linhas internas de texto no arquivo
            pDC->MoveTo(c.x + 3, c.y - 3);
            pDC->LineTo(c.x + 5, c.y - 3);
            pDC->MoveTo(c.x + 3, c.y);
            pDC->LineTo(c.x + 5, c.y);

            // Seta em azul para a esquerda
            COLORREF arrowCol = isHover ? RGB(0, 90, 180) : RGB(0, 120, 215);
            CPen penArrow(PS_SOLID, 2, arrowCol);
            pDC->SelectObject(&penArrow);

            pDC->MoveTo(c.x + 1, c.y);
            pDC->LineTo(c.x - 4, c.y);
            pDC->MoveTo(c.x - 2, c.y - 3);
            pDC->LineTo(c.x - 5, c.y);
            pDC->LineTo(c.x - 2, c.y + 3);

            pDC->SelectObject(pOldPen);
        }
        break;

    case BTN_GO:
        {
            // Ícone 'Ir': folha de código + seta à direita
            CPen pen(PS_SOLID, 1, iconCol);
            CPen* pOldPen = pDC->SelectObject(&pen);

            // Retângulo do arquivo/editor à esquerda
            pDC->MoveTo(c.x - 7, c.y - 6);
            pDC->LineTo(c.x - 1, c.y - 6);
            pDC->LineTo(c.x - 1, c.y + 6);
            pDC->LineTo(c.x - 7, c.y + 6);
            pDC->LineTo(c.x - 7, c.y - 6);

            // Linhas internas de texto no arquivo
            pDC->MoveTo(c.x - 5, c.y - 3);
            pDC->LineTo(c.x - 2, c.y - 3);
            pDC->MoveTo(c.x - 5, c.y);
            pDC->LineTo(c.x - 2, c.y);

            // Seta em azul para a direita
            COLORREF arrowCol = isHover ? RGB(0, 90, 180) : RGB(0, 120, 215);
            CPen penArrow(PS_SOLID, 2, arrowCol);
            pDC->SelectObject(&penArrow);

            pDC->MoveTo(c.x + 1, c.y);
            pDC->LineTo(c.x + 6, c.y);
            pDC->MoveTo(c.x + 4, c.y - 3);
            pDC->LineTo(c.x + 7, c.y);
            pDC->LineTo(c.x + 4, c.y + 3);

            pDC->SelectObject(pOldPen);
        }
        break;

    case BTN_COPY:
        {
            // Ícone 'Copiar': duas folhas sobrepostas
            CPen pen(PS_SOLID, 1, iconCol);
            CPen* pOldPen = pDC->SelectObject(&pen);

            // Folha de trás
            pDC->MoveTo(c.x - 2, c.y - 5);
            pDC->LineTo(c.x + 5, c.y - 5);
            pDC->LineTo(c.x + 5, c.y + 2);
            pDC->MoveTo(c.x - 2, c.y - 5);
            pDC->LineTo(c.x - 5, c.y - 5);
            pDC->LineTo(c.x - 5, c.y + 2);

            // Folha da frente (com fundo preenchido)
            CRect rcFront(c.x - 3, c.y - 2, c.x + 4, c.y + 6);
            COLORREF bgFront = (isHover ? RGB(225, 230, 238) : RGB(243, 245, 248));
            pDC->FillSolidRect(&rcFront, bgFront);
            pDC->Draw3dRect(&rcFront, iconCol, iconCol);

            pDC->SelectObject(pOldPen);
        }
        break;

    case BTN_SYNC:
        {
            // Ícone 'Salvar': disquete clássico
            CPen pen(PS_SOLID, 1, iconCol);
            CPen* pOldPen = pDC->SelectObject(&pen);

            // Corpo do disquete
            pDC->MoveTo(c.x - 5, c.y - 5);
            pDC->LineTo(c.x + 3, c.y - 5);
            pDC->LineTo(c.x + 5, c.y - 3);
            pDC->LineTo(c.x + 5, c.y + 5);
            pDC->LineTo(c.x - 5, c.y + 5);
            pDC->LineTo(c.x - 5, c.y - 5);

            // Janela do obturador em cima
            pDC->MoveTo(c.x - 3, c.y - 5);
            pDC->LineTo(c.x - 3, c.y - 2);
            pDC->LineTo(c.x + 1, c.y - 2);
            pDC->LineTo(c.x + 1, c.y - 5);

            // Etiqueta embaixo
            pDC->MoveTo(c.x - 3, c.y + 1);
            pDC->LineTo(c.x + 3, c.y + 1);
            pDC->LineTo(c.x + 3, c.y + 5);
            pDC->LineTo(c.x - 3, c.y + 5);
            pDC->LineTo(c.x - 3, c.y + 1);

            pDC->SelectObject(pOldPen);

            // Se modificado, desenha ponto âmbar/alaranjado no canto do botão
            if (m_bModified)
            {
                CBrush brushMod(RGB(245, 130, 10));
                CPen penMod(PS_SOLID, 1, RGB(200, 100, 0));
                CBrush* pOldB = pDC->SelectObject(&brushMod);
                CPen* pOldP = pDC->SelectObject(&penMod);
                pDC->Ellipse(c.x + 3, c.y - 7, c.x + 8, c.y - 2);
                pDC->SelectObject(pOldB);
                pDC->SelectObject(pOldP);
            }
        }
        break;

    case BTN_PIN:
        {
            // Ícone 'Fixar': alfinete/pushpin
            COLORREF pinCol = m_bPinned ? RGB(0, 102, 204) : iconCol;
            CPen pen(PS_SOLID, 1, pinCol);
            CPen* pOldPen = pDC->SelectObject(&pen);

            // Cabeça superior
            pDC->MoveTo(c.x - 3, c.y - 5);
            pDC->LineTo(c.x + 3, c.y - 5);

            // Corpo do alfinete
            pDC->MoveTo(c.x, c.y - 5);
            pDC->LineTo(c.x, c.y - 2);
            pDC->MoveTo(c.x - 4, c.y - 2);
            pDC->LineTo(c.x + 4, c.y - 2);
            pDC->LineTo(c.x + 2, c.y + 1);
            pDC->LineTo(c.x - 2, c.y + 1);
            pDC->LineTo(c.x - 4, c.y - 2);

            // Ponta da agulha
            pDC->MoveTo(c.x, c.y + 1);
            pDC->LineTo(c.x, c.y + 6);

            pDC->SelectObject(pOldPen);
        }
        break;

    case BTN_CLOSE:
        {
            // Ícone 'Fechar': 'X' minimalista
            CPen pen(PS_SOLID, 1, iconCol);
            CPen* pOldPen = pDC->SelectObject(&pen);

            pDC->MoveTo(c.x - 4, c.y - 4);
            pDC->LineTo(c.x + 4, c.y + 4);
            pDC->MoveTo(c.x - 3, c.y - 4);
            pDC->LineTo(c.x + 5, c.y + 4);

            pDC->MoveTo(c.x + 3, c.y - 4);
            pDC->LineTo(c.x - 5, c.y + 4);
            pDC->MoveTo(c.x + 4, c.y - 4);
            pDC->LineTo(c.x - 4, c.y + 4);

            pDC->SelectObject(pOldPen);
        }
        break;
    }
}

void COECursorPeekDlg::OnPaint()
{
    CPaintDC dc(this);
    CRect rcClient;
    GetClientRect(&rcClient);

    CRect rcHeader(0, 0, rcClient.Width(), HEADER_HEIGHT);

    COLORREF headerBg   = RGB(243, 245, 248);
    COLORREF headerLine = RGB(218, 224, 233);
    COLORREF textColor  = RGB(33, 37, 41);

    // Preenche cabeçalho claro e moderno
    dc.FillSolidRect(&rcHeader, headerBg);
    dc.FillSolidRect(0, HEADER_HEIGHT - 1, rcClient.Width(), 1, headerLine);

    // Ícone de banco de dados
    DrawDbIcon(&dc, 8, (HEADER_HEIGHT - 14) / 2);

    // Título do objeto
    CFont* pOldFont = dc.SelectObject(&m_uiFont);
    dc.SetBkMode(TRANSPARENT);
    dc.SetTextColor(textColor);

    CString strTitle;
    strTitle.Format(_T("%s  (Linha %d)%s"), 
        m_objectInfo.title.c_str(), 
        m_objectInfo.declLineStart + 1,
        m_bModified ? _T(" *") : _T(""));

    int titleRight = (m_hasPrevPos && !m_rcBackBtn.IsRectEmpty()) ? m_rcBackBtn.left - 6 : m_rcGoBtn.left - 6;
    CRect rcTitle(28, 0, titleRight, HEADER_HEIGHT);
    dc.DrawText(strTitle, &rcTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    dc.SelectObject(pOldFont);

    // Botões com ícones vetoriais modernos
    if (m_hasPrevPos && !m_rcBackBtn.IsRectEmpty())
    {
        DrawHeaderButton(&dc, m_rcBackBtn, BTN_BACK);
    }
    DrawHeaderButton(&dc, m_rcGoBtn, BTN_GO);
    DrawHeaderButton(&dc, m_rcCopyBtn, BTN_COPY);
    DrawHeaderButton(&dc, m_rcSyncBtn, BTN_SYNC);
    DrawHeaderButton(&dc, m_rcPinBtn, BTN_PIN);
    DrawHeaderButton(&dc, m_rcCloseBtn, BTN_CLOSE);
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
            !m_rcCopyBtn.PtInRect(ptClient) &&
            !m_rcGoBtn.PtInRect(ptClient) &&
            (!m_hasPrevPos || !m_rcBackBtn.PtInRect(ptClient)))
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
    else if (m_hasPrevPos && m_rcBackBtn.PtInRect(point))
    {
        if (m_pEditor && ::IsWindow(m_pEditor->m_hWnd))
        {
            EXCEPTION_FRAME;
            try
            {
                Position curPos = m_pEditor->GetPosition();

                m_pEditor->ScrollTo(m_prevEditorPos.line);
                m_pEditor->MoveTo(m_prevEditorPos);
                m_pEditor->SetFocus();

                // Permite alternar facilmente de volta entre a declaração e o uso
                m_prevEditorPos = curPos;

                CRect rcClient;
                GetClientRect(&rcClient);
                LayoutControls(rcClient.Width(), rcClient.Height());
                InvalidateRect(CRect(0, 0, 10000, HEADER_HEIGHT), FALSE);
            }
            catch (...)
            {
            }
        }
        return;
    }
    else if (m_rcGoBtn.PtInRect(point))
    {
        if (m_pEditor && ::IsWindow(m_pEditor->m_hWnd))
        {
            EXCEPTION_FRAME;
            try
            {
                // Salva a posição onde o usuário estava antes de ir para a declaração
                m_prevEditorPos = m_pEditor->GetPosition();
                m_hasPrevPos = true;

                m_pEditor->ScrollTo(m_objectInfo.declLineStart);
                Position pos;
                pos.line = m_objectInfo.declLineStart;
                pos.column = m_objectInfo.declStart.column;
                m_pEditor->MoveTo(pos);
                m_pEditor->SetFocus();

                CRect rcClient;
                GetClientRect(&rcClient);
                LayoutControls(rcClient.Width(), rcClient.Height());
                InvalidateRect(CRect(0, 0, 10000, HEADER_HEIGHT), FALSE);
            }
            catch (...)
            {
            }
        }
        return;
    }
    else if (m_rcCopyBtn.PtInRect(point))
    {
        CopyContent();
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
    if (m_toolTip.m_hWnd && ::IsWindow(m_toolTip.m_hWnd))
    {
        m_toolTip.RelayEvent(pMsg);
    }

    if (pMsg->message == WM_KEYDOWN)
    {
        bool bCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
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
        else if (pMsg->wParam == 'A' && bCtrl)
        {
            m_editCtrl.SetFocus();
            m_editCtrl.SetSel(0, -1);
            return TRUE;
        }
        else if ((pMsg->wParam == 'C' || pMsg->wParam == VK_INSERT) && bCtrl)
        {
            CopyContent();
            return TRUE;
        }
        else if (pMsg->wParam == 'S' && bCtrl)
        {
            SyncToEditor();
            InvalidateRect(CRect(0, 0, 10000, HEADER_HEIGHT), FALSE);
            return TRUE;
        }
    }

    return CWnd::PreTranslateMessage(pMsg);
}

void COECursorPeekDlg::CopyContent()
{
    long selStart = 0, selEnd = 0;
    m_editCtrl.GetSel(selStart, selEnd);
    if (selStart != selEnd)
    {
        CString selText = m_editCtrl.GetSelText();
        if (!selText.IsEmpty())
        {
            if (OpenClipboard())
            {
                EmptyClipboard();
                size_t size = (selText.GetLength() + 1) * sizeof(wchar_t);
                HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, size);
                if (hGlob)
                {
                    memcpy(GlobalLock(hGlob), selText.GetString(), size);
                    GlobalUnlock(hGlob);
                    SetClipboardData(CF_UNICODETEXT, hGlob);
                }
                CloseClipboard();
            }
            return;
        }
        m_editCtrl.Copy();
    }
    else
    {
        CopyAllToClipboard();
    }
}

void COECursorPeekDlg::CopyAllToClipboard()
{
    CString allText;
    m_editCtrl.GetWindowText(allText);
    if (allText.IsEmpty())
        return;

    if (OpenClipboard())
    {
        EmptyClipboard();
        size_t size = (allText.GetLength() + 1) * sizeof(wchar_t);
        HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, size);
        if (hGlob)
        {
            memcpy(GlobalLock(hGlob), allText.GetString(), size);
            GlobalUnlock(hGlob);
            SetClipboardData(CF_UNICODETEXT, hGlob);
        }
        CloseClipboard();
    }
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

    // 1. Reseta a formatação de todo o texto para cor padrão preta e peso normal
    CHARFORMAT2W cfDefault;
    ZeroMemory(&cfDefault, sizeof(cfDefault));
    cfDefault.cbSize = sizeof(cfDefault);
    cfDefault.dwMask = CFM_COLOR | CFM_BOLD | CFM_ITALIC;
    cfDefault.crTextColor = RGB(0, 0, 0);
    cfDefault.dwEffects = 0;

    m_editCtrl.SetSel(0, -1);
    m_editCtrl.SendMessage(EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cfDefault);

    // Palavras-chave oficiais de SQL e PL/SQL
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

    COLORREF colKeyword = RGB(0, 70, 215);       // Azul elegante
    COLORREF colString  = RGB(163, 21, 21);     // Marrom/vermelho escuro
    COLORREF colComment = RGB(0, 128, 0);       // Verde escuro
    COLORREF colNumber  = RGB(9, 134, 88);      // Verde petróleo

    auto applyFormat = [&](int start, int end, COLORREF color, bool bold) {
        if (start < end && start >= 0)
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

    // 2. Parser linha por linha baseado em EM_GETLINE e LineIndex para garantir
    // alinhamento 100% exato com o offset interno do RichEdit (eliminando o bug de \r\n vs \r)
    int lineCount = m_editCtrl.GetLineCount();
    bool inBlockComment = false;

    for (int line = 0; line < lineCount; ++line)
    {
        int lineStart = m_editCtrl.LineIndex(line);
        if (lineStart < 0)
            continue;

        int lineLen = m_editCtrl.LineLength(lineStart);
        if (lineLen <= 0)
            continue;

        std::vector<wchar_t> lineBuf(lineLen + 4, 0);
        *(WORD*)lineBuf.data() = (WORD)(lineLen + 2);
        int fetched = (int)m_editCtrl.SendMessage(EM_GETLINE, (WPARAM)line, (LPARAM)lineBuf.data());
        if (fetched <= 0)
            continue;
        lineBuf[fetched] = L'\0';

        const wchar_t* pLine = lineBuf.data();
        int i = 0;

        // Se já estávamos dentro de comentário de bloco /* ... */ de linhas anteriores
        if (inBlockComment)
        {
            int blockEnd = -1;
            for (int k = 0; k + 1 < fetched; ++k)
            {
                if (pLine[k] == L'*' && pLine[k + 1] == L'/')
                {
                    blockEnd = k + 2;
                    break;
                }
            }

            if (blockEnd != -1)
            {
                applyFormat(lineStart, lineStart + blockEnd, colComment, false);
                i = blockEnd;
                inBlockComment = false;
            }
            else
            {
                applyFormat(lineStart, lineStart + fetched, colComment, false);
                continue;
            }
        }

        while (i < fetched)
        {
            // Comentário de bloco /* ... */
            if (i + 1 < fetched && pLine[i] == L'/' && pLine[i + 1] == L'*')
            {
                int start = i;
                i += 2;
                int blockEnd = -1;
                while (i + 1 < fetched)
                {
                    if (pLine[i] == L'*' && pLine[i + 1] == L'/')
                    {
                        blockEnd = i + 2;
                        break;
                    }
                    ++i;
                }

                if (blockEnd != -1)
                {
                    applyFormat(lineStart + start, lineStart + blockEnd, colComment, false);
                    i = blockEnd;
                }
                else
                {
                    applyFormat(lineStart + start, lineStart + fetched, colComment, false);
                    inBlockComment = true;
                    break;
                }
                continue;
            }

            // Comentário de linha -- ...
            if (i + 1 < fetched && pLine[i] == L'-' && pLine[i + 1] == L'-')
            {
                applyFormat(lineStart + i, lineStart + fetched, colComment, false);
                break;
            }

            // String literal '...'
            if (pLine[i] == L'\'')
            {
                int start = i;
                ++i;
                while (i < fetched)
                {
                    if (pLine[i] == L'\'')
                    {
                        if (i + 1 < fetched && pLine[i + 1] == L'\'')
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
                applyFormat(lineStart + start, lineStart + i, colString, false);
                continue;
            }

            // Identificador ou palavra-chave
            if (iswalpha(pLine[i]) || pLine[i] == L'_')
            {
                int start = i;
                std::wstring word;
                while (i < fetched && (iswalnum(pLine[i]) || pLine[i] == L'_' || pLine[i] == L'$' || pLine[i] == L'#'))
                {
                    word += towupper(pLine[i]);
                    ++i;
                }

                if (s_keywords.find(word) != s_keywords.end())
                {
                    applyFormat(lineStart + start, lineStart + i, colKeyword, true);
                }
                continue;
            }
            // Número
            else if (iswdigit(pLine[i]))
            {
                int start = i;
                while (i < fetched && (iswdigit(pLine[i]) || pLine[i] == L'.' || towupper(pLine[i]) == L'E'))
                    ++i;
                applyFormat(lineStart + start, lineStart + i, colNumber, false);
                continue;
            }

            ++i;
        }
    }

    m_editCtrl.SetSel(crOrg);
    m_editCtrl.SendMessage(EM_SETSCROLLPOS, 0, (LPARAM)&scrollPos);
    m_editCtrl.SetRedraw(TRUE);
    m_editCtrl.Invalidate();
}

} // namespace OpenEditor

