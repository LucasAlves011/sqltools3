#include "stdafx.h"
#include "Update/UpdateManager.h"
#include "Update/UpdateChecker.h"
#include "resource.h"
#include <memory>
#include <time.h>

namespace UpdateManager
{
    namespace
    {
        const wchar_t* const SECTION         = L"Updates";
        const wchar_t* const KEY_AUTO_CHECK  = L"AutoCheck";
        const wchar_t* const KEY_LAST_CHECK  = L"LastCheck";
        const wchar_t* const KEY_SKIPPED     = L"SkippedVersion";
        const wchar_t* const KEY_LAST_RUN    = L"LastRunVersion";
        const int            CHECK_INTERVAL  = 24 * 60 * 60; // segundos

        volatile LONG g_checkRunning = 0;
        bool          g_dialogOpen   = false;

        bool autoCheckEnabled ()
        {
            return AfxGetApp()->GetProfileInt(SECTION, KEY_AUTO_CHECK, 1) != 0;
        }

        bool autoCheckDue ()
        {
            UINT last = AfxGetApp()->GetProfileInt(SECTION, KEY_LAST_CHECK, 0);
            UINT now  = (UINT)time(0);
            return last == 0 || now < last || now - last >= (UINT)CHECK_INTERVAL;
        }

        void openUrl (const std::wstring& url)
        {
            // so abre enderecos https do proprio GitHub vindos da API
            if (url.compare(0, 8, L"https://") != 0)
            {
                AfxMessageBox(L"Invalid download address.", MB_OK | MB_ICONSTOP);
                return;
            }
            HINSTANCE r = ShellExecute(NULL, L"open", url.c_str(), NULL, NULL, SW_SHOW);
            if ((INT_PTR)r <= HINSTANCE_ERROR)
                AfxMessageBox(L"Cannot open a default browser.", MB_OK | MB_ICONSTOP);
        }

        // ---- thread de checagem -----------------------------------------------------------------
        struct WorkerArgs { HWND hwnd; bool manual; };

        UINT AFX_CDECL checkThread (LPVOID p)
        {
            std::unique_ptr<WorkerArgs> args(static_cast<WorkerArgs*>(p));
            Update::CheckResult* result = new Update::CheckResult(Update::CheckForUpdates());
            if (!::IsWindow(args->hwnd) || !::PostMessage(args->hwnd, UM_UPDATE_CHECK_DONE, args->manual ? 1 : 0, (LPARAM)result))
            {
                delete result;
                InterlockedExchange(&g_checkRunning, 0);
            }
            return 0;
        }

        void startCheck (bool manual)
        {
            CWnd* wnd = AfxGetMainWnd();
            if (!wnd || !wnd->GetSafeHwnd())
                return;

            if (InterlockedCompareExchange(&g_checkRunning, 1, 0) != 0)
            {
                if (manual)
                    AfxMessageBox(L"An update check is already in progress.", MB_OK | MB_ICONINFORMATION);
                return;
            }

            WorkerArgs* args = new WorkerArgs;
            args->hwnd   = wnd->GetSafeHwnd();
            args->manual = manual;
            if (!AfxBeginThread(checkThread, args))
            {
                delete args;
                InterlockedExchange(&g_checkRunning, 0);
            }
        }

        std::wstring versionList (const Update::CheckResult& r)
        {
            std::wstring text;
            for (size_t i = 0; i < r.newer.size(); ++i)
            {
                const Update::ReleaseInfo& rel = r.newer[i];
                if (i) text += L"\r\n\r\n";
                text += L"=== " + rel.title + L" ===\r\n";
                text += rel.notes.empty() ? std::wstring(L"(no release notes)") : rel.notes;
            }
            return text;
        }

        // ---- dialogo: nova versao disponivel -------------------------------------------------------
        class CUpdateAvailableDlg : public CDialog
        {
        public:
            CUpdateAvailableDlg (const Update::CheckResult& r, CWnd* parent)
                : CDialog(IDD_UPDATE_AVAILABLE, parent), m_result(r) {}

        protected:
            virtual BOOL OnInitDialog ()
            {
                CDialog::OnInitDialog();
                const Update::ReleaseInfo& latest = m_result.newer.front();
                std::wstring header = L"SQLTools " + latest.version.ToString() + L" is available.\r\nYou are using " + m_result.current.ToString() + L".";
                SetDlgItemText(IDC_UPD_HEADER, header.c_str());
                SetDlgItemText(IDC_UPD_NOTES, versionList(m_result).c_str());
                ((CEdit*)GetDlgItem(IDC_UPD_NOTES))->SetSel(0, 0);
                GetDlgItem(IDOK)->SetFocus(); // evita o texto das notas iniciar selecionado
                return FALSE;
            }

            afx_msg void OnSkip ()
            {
                AfxGetApp()->WriteProfileString(SECTION, KEY_SKIPPED, CString(m_result.newer.front().tag.c_str()));
                EndDialog(IDC_UPD_SKIP);
            }

            DECLARE_MESSAGE_MAP()
        private:
            const Update::CheckResult& m_result;
        };

        BEGIN_MESSAGE_MAP(CUpdateAvailableDlg, CDialog)
            ON_BN_CLICKED(IDC_UPD_SKIP, OnSkip)
        END_MESSAGE_MAP()

        void showUpdateDialog (const Update::CheckResult& r)
        {
            if (g_dialogOpen || r.newer.empty())
                return;

            g_dialogOpen = true;
            CUpdateAvailableDlg dlg(r, AfxGetMainWnd());
            INT_PTR rc = dlg.DoModal();
            g_dialogOpen = false;

            if (rc == IDOK) // "Download": o usuario decide o que fazer com o arquivo baixado
                openUrl(r.newer.front().url);
        }

        // ---- dialogo: changelog -----------------------------------------------------------------------
        std::wstring embeddedChangelog ()
        {
            HRSRC hRes = FindResourceW(0, MAKEINTRESOURCEW(IDR_CHANGELOG), RT_RCDATA);
            HGLOBAL hMem = hRes ? LoadResource(0, hRes) : 0;
            const char* data = hMem ? (const char*)LockResource(hMem) : 0;
            DWORD size = hRes ? SizeofResource(0, hRes) : 0;
            if (!data || !size)
                return L"Changelog is not available.";

            if (size >= 3 && (unsigned char)data[0] == 0xEF && (unsigned char)data[1] == 0xBB && (unsigned char)data[2] == 0xBF)
                data += 3, size -= 3;

            int n = MultiByteToWideChar(CP_UTF8, 0, data, (int)size, 0, 0);
            std::wstring w(n, L'\0');
            if (n) MultiByteToWideChar(CP_UTF8, 0, data, (int)size, &w[0], n);

            std::wstring r;
            r.reserve(w.size() + w.size() / 20);
            for (size_t i = 0; i < w.size(); ++i)
            {
                if (w[i] == L'\r') continue;
                if (w[i] == L'\n') r += L'\r';
                r += w[i];
            }
            return r;
        }

        class CChangelogDlg : public CDialog
        {
        public:
            CChangelogDlg (CWnd* parent) : CDialog(IDD_CHANGELOG, parent) {}

        protected:
            virtual BOOL OnInitDialog ()
            {
                CDialog::OnInitDialog();
                SetWindowText((L"What's New - SQLTools " + Update::CurrentVersion().ToString()).c_str());
                CEdit* edit = (CEdit*)GetDlgItem(IDC_CHL_TEXT);
                edit->SetLimitText(0);
                SetDlgItemText(IDC_CHL_TEXT, embeddedChangelog().c_str());
                edit->SetSel(0, 0);
                CheckDlgButton(IDC_CHL_AUTOCHECK, autoCheckEnabled() ? BST_CHECKED : BST_UNCHECKED);
                GetDlgItem(IDOK)->SetFocus(); // evita o texto do changelog iniciar selecionado
                return FALSE;
            }

            void saveOptions ()
            {
                AfxGetApp()->WriteProfileInt(SECTION, KEY_AUTO_CHECK, IsDlgButtonChecked(IDC_CHL_AUTOCHECK) == BST_CHECKED ? 1 : 0);
            }

            virtual void OnOK ()     { saveOptions(); CDialog::OnOK(); }
            virtual void OnCancel () { saveOptions(); CDialog::OnCancel(); }

            afx_msg void OnCheck ()  { saveOptions(); EndDialog(IDC_CHL_CHECK); }

            DECLARE_MESSAGE_MAP()
        };

        BEGIN_MESSAGE_MAP(CChangelogDlg, CDialog)
            ON_BN_CLICKED(IDC_CHL_CHECK, OnCheck)
        END_MESSAGE_MAP()
    }

    void ScheduleStartup (CWnd* mainWnd)
    {
        if (mainWnd && mainWnd->GetSafeHwnd())
            mainWnd->PostMessage(UM_UPDATE_STARTUP, 0, 0);
    }

    LRESULT OnStartup ()
    {
        try
        {
            CWinApp* app = AfxGetApp();

            // primeira execucao apos uma atualizacao: mostra o que mudou (uma unica vez)
            CString current = Update::CurrentVersion().ToString().c_str();
            CString lastRun = app->GetProfileString(SECTION, KEY_LAST_RUN, L"");
            if (lastRun != current)
            {
                app->WriteProfileString(SECTION, KEY_LAST_RUN, current);
                if (!lastRun.IsEmpty())
                    ShowChangelog(AfxGetMainWnd());
            }

            if (autoCheckEnabled() && autoCheckDue())
                startCheck(false);
        }
        catch (...) {}
        return 0;
    }

    LRESULT OnCheckDone (WPARAM manual, LPARAM param)
    {
        std::unique_ptr<Update::CheckResult> result(reinterpret_cast<Update::CheckResult*>(param));
        InterlockedExchange(&g_checkRunning, 0);

        try
        {
            if (!result.get())
                return 0;

            CWinApp* app = AfxGetApp();

            if (!result->ok)
            {
                if (manual)
                    AfxMessageBox((L"Cannot check for updates.\n\n" + result->error).c_str(), MB_OK | MB_ICONWARNING);
                return 0;
            }

            // outra janela de atualizacao aberta: nao registra a checagem, para tentar de novo na proxima abertura
            if (!manual && !g_dialogOpen)
                app->WriteProfileInt(SECTION, KEY_LAST_CHECK, (int)time(0));

            if (manual)
            {
                // manual: sugere qualquer versao mais nova, inclusive correcao de bugs
                if (result->HasNewer())
                    showUpdateDialog(*result);
                else
                    AfxMessageBox((L"You are using the latest version (" + result->current.ToString() + L").").c_str(), MB_OK | MB_ICONINFORMATION);
                return 0;
            }

            // automatica: so avisa versao minor/major e nao insiste em versao ja ignorada
            if (result->HasNewer())
            {
                const Update::ReleaseInfo& latest = result->newer.front();
                CString skipped = app->GetProfileString(SECTION, KEY_SKIPPED, L"");
                if (Update::IsMajorOrMinorBump(result->current, latest.version)
                 && skipped != CString(latest.tag.c_str()))
                    showUpdateDialog(*result);
            }
        }
        catch (...) {}
        return 0;
    }

    void CheckNow ()
    {
        startCheck(true);
    }

    void ShowChangelog (CWnd* parent)
    {
        if (g_dialogOpen)
            return;

        g_dialogOpen = true;
        CChangelogDlg dlg(parent);
        INT_PTR rc = dlg.DoModal();
        g_dialogOpen = false;

        if (rc == IDC_CHL_CHECK)
            CheckNow();
    }
}
