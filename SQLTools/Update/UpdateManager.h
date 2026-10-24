#pragma once

// Interface com o usuario para atualizacoes: tudo depende de uma acao explicita do usuario.
// O programa apenas consulta se ha versao nova e, se o usuario quiser, abre a pagina de download.

namespace UpdateManager
{
    // mensagens postadas para a janela principal (tratadas em CMDIMainFrame)
    enum
    {
        UM_UPDATE_CHECK_DONE = WM_USER + 3,   // wParam = manual?, lParam = Update::CheckResult* (dono: receptor)
        UM_UPDATE_STARTUP    = WM_USER + 4
    };

    // Chamar quando a janela principal estiver pronta: agenda changelog pos-atualizacao e checagem automatica.
    void ScheduleStartup (CWnd* mainWnd);

    LRESULT OnStartup ();
    LRESULT OnCheckDone (WPARAM manual, LPARAM result);

    void CheckNow ();                   // Help > Check for Updates (checagem manual)
    void ShowChangelog (CWnd* parent);  // Help > What's New
}
