#include "stdafx.h"
#include "Theme/ModernMessageBox.h"
#include "Theme/ModernTheme.h"
#include <list>
#include <vector>
#include <algorithm>

namespace
{
    // textos dos botoes na lingua do sistema (user32 exporta MB_GetString); em ingles se nao houver
    CString ButtonText (int id)
    {
        static const wchar_t* const fallback[] = { L"OK", L"Cancel", L"Abort", L"Retry", L"Ignore", L"Yes", L"No" };
        int index = -1;
        switch (id)
        {
        case IDOK: index = 0; break;      case IDCANCEL: index = 1; break;
        case IDABORT: index = 2; break;   case IDRETRY: index = 3; break;
        case IDIGNORE: index = 4; break;  case IDYES: index = 5; break;
        case IDNO: index = 6; break;
        }
        if (index < 0)
            return L"";
        typedef LPCWSTR (WINAPI *GetStringFn)(UINT);
        if (HMODULE user32 = ::GetModuleHandleW(L"user32.dll"))
            if (GetStringFn fn = (GetStringFn)::GetProcAddress(user32, "MB_GetString"))
                if (LPCWSTR s = fn(index))
                    return s;
        return fallback[index];
    }

    class CMessageDlg : public CDialog
    {
    public:
        CString m_text, m_caption;
        UINT m_type;
        std::vector<int> m_ids;
        CStatic m_icon, m_label;
        std::list<CButton> m_buttons;

        CMessageDlg (CWnd* parent, LPCTSTR text, LPCTSTR caption, UINT type)
            : CDialog(), m_text(text), m_caption(caption), m_type(type)
        {
            m_pParentWnd = parent;
        }

        INT_PTR Run ()
        {
            // modelo de dialogo na memoria: so a moldura; o conteudo e montado em OnInitDialog
            std::vector<WORD> tpl;
            auto put = [&](WORD w) { tpl.push_back(w); };
            auto putStr = [&](const wchar_t* s) { do { put(*s); } while (*s++); };
            DWORD style = DS_MODALFRAME | DS_SETFONT | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU;
            put(LOWORD(style)); put(HIWORD(style));
            put(0); put(0);                 // estilo estendido
            put(0);                         // sem controles
            put(0); put(0); put(100); put(60);
            put(0);                         // sem menu
            put(0);                         // classe padrao
            putStr(m_caption.IsEmpty() ? AfxGetAppName() : m_caption);
            put(9);                         // fonte
            putStr(L"Segoe UI");
            while (tpl.size() % 2)
                put(0);                     // DWORD alinhado

            InitModalIndirect((LPCDLGTEMPLATE)tpl.data(), m_pParentWnd);
            return DoModal();
        }

    protected:
        DECLARE_MESSAGE_MAP()

        BOOL OnInitDialog () override
        {
            CDialog::OnInitDialog();

            switch (m_type & MB_TYPEMASK)
            {
            case MB_OKCANCEL:          m_ids = { IDOK, IDCANCEL }; break;
            case MB_ABORTRETRYIGNORE:  m_ids = { IDABORT, IDRETRY, IDIGNORE }; break;
            case MB_YESNOCANCEL:       m_ids = { IDYES, IDNO, IDCANCEL }; break;
            case MB_YESNO:             m_ids = { IDYES, IDNO }; break;
            case MB_RETRYCANCEL:       m_ids = { IDRETRY, IDCANCEL }; break;
            default:                   m_ids = { IDOK }; break;
            }

            // icone e som
            LPCWSTR iconId = NULL;
            UINT beep = MB_OK;
            switch (m_type & MB_ICONMASK)
            {
            case MB_ICONERROR:       iconId = IDI_ERROR;       beep = MB_ICONERROR; break;
            case MB_ICONQUESTION:    iconId = IDI_QUESTION;    beep = MB_ICONQUESTION; break;
            case MB_ICONWARNING:     iconId = IDI_WARNING;     beep = MB_ICONWARNING; break;
            case MB_ICONINFORMATION: iconId = IDI_INFORMATION; beep = MB_ICONINFORMATION; break;
            }

            CFont* font = GetFont();
            CClientDC dc(this);
            CFont* oldFont = dc.SelectObject(font);

            const int margin = 22, gap = 16, iconSize = iconId ? 32 : 0;
            const int btnW = 92, btnH = 30, btnGap = 10;
            const int maxText = 380;

            CRect measure(0, 0, maxText, 0);
            dc.DrawText(m_text, measure, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX | DT_EXPANDTABS);
            const int textW = (std::max)(measure.Width(), 120);
            const int textH = (std::max)(measure.Height(), 16);
            dc.SelectObject(oldFont);

            const int buttonsW = (int)m_ids.size() * btnW + ((int)m_ids.size() - 1) * btnGap;
            const int bodyW = (iconSize ? iconSize + gap : 0) + textW;
            const int clientW = (std::max)(bodyW, buttonsW) + 2 * margin;
            const int bodyH = (std::max)(iconSize, textH);
            const int clientH = margin + bodyH + 26 + btnH + 18;

            CRect wr(0, 0, clientW, clientH);
            ::AdjustWindowRectEx(wr, GetStyle(), FALSE, GetExStyle());
            SetWindowPos(NULL, 0, 0, wr.Width(), wr.Height(), SWP_NOMOVE | SWP_NOZORDER);

            int x = margin;
            if (iconId)
            {
                m_icon.Create(L"", WS_CHILD | WS_VISIBLE | SS_ICON, CRect(x, margin, x + 32, margin + 32), this, 0xFFFF);
                m_icon.SetIcon(::LoadIcon(NULL, iconId));
                x += iconSize + gap;
            }
            m_label.Create(m_text, WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, CRect(x, margin + (bodyH - textH) / 2, x + textW, margin + (bodyH - textH) / 2 + textH), this, 0xFFFE);
            m_label.SetFont(font);

            // botoes alinhados a direita
            int defIndex = 0;
            switch (m_type & MB_DEFMASK) { case MB_DEFBUTTON2: defIndex = 1; break; case MB_DEFBUTTON3: defIndex = 2; break; }
            if (defIndex >= (int)m_ids.size())
                defIndex = 0;

            int bx = clientW - margin - buttonsW;
            const int by = margin + bodyH + 26;
            for (size_t i = 0; i < m_ids.size(); ++i)
            {
                m_buttons.emplace_back();
                CButton& b = m_buttons.back();
                b.Create(ButtonText(m_ids[i]), WS_CHILD | WS_VISIBLE | WS_TABSTOP | ((int)i == defIndex ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON),
                         CRect(bx, by, bx + btnW, by + btnH), this, m_ids[i]);
                b.SetFont(font);
                bx += btnW + btnGap;
            }
            GetDlgItem(m_ids[defIndex])->SetFocus();

            if (!(m_ids.size() == 1 || std::find(m_ids.begin(), m_ids.end(), IDCANCEL) != m_ids.end()))
                if (CMenu* sys = GetSystemMenu(FALSE))
                    sys->EnableMenuItem(SC_CLOSE, MF_BYCOMMAND | MF_GRAYED);   // como a caixa do sistema: sim/nao nao fecha pelo X

            CenterWindow(m_pParentWnd);
            ::MessageBeep(beep);
            return FALSE;       // o foco ja foi definido
        }

        void OnCancel () override
        {
            // Esc / X: Cancelar, quando existe; unico OK fecha; sim/nao nao fecha
            if (std::find(m_ids.begin(), m_ids.end(), IDCANCEL) != m_ids.end())
                EndDialog(IDCANCEL);
            else if (m_ids.size() == 1)
                EndDialog(m_ids[0]);
        }

        void OnOK () override
        {
            // Enter aciona o botao padrao com foco
            CWnd* focus = GetFocus();
            if (focus && focus->GetParent() == this && focus->GetDlgCtrlID() > 0 && focus->GetDlgCtrlID() < 0xFFFE)
                EndDialog(focus->GetDlgCtrlID());
            else
                EndDialog(m_ids[0]);
        }

        BOOL OnCommand (WPARAM wp, LPARAM lp) override
        {
            const int id = LOWORD(wp);
            if (HIWORD(wp) == BN_CLICKED && std::find(m_ids.begin(), m_ids.end(), id) != m_ids.end())
            {
                EndDialog(id);
                return TRUE;
            }
            return CDialog::OnCommand(wp, lp);
        }
    };

    BEGIN_MESSAGE_MAP(CMessageDlg, CDialog)
    END_MESSAGE_MAP()
}

namespace ModernMessageBox
{
    int Show (CWnd* pParent, LPCTSTR text, LPCTSTR caption, UINT type)
    {
        if (!pParent)
            pParent = AfxGetMainWnd();
        CMessageDlg dlg(pParent, text, caption, type);
        return (int)dlg.Run();
    }
}
