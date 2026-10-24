#pragma once

// Verificacao de novas versoes via GitHub Releases (somente consulta; nada e baixado ou instalado).

#include <string>
#include <vector>

namespace Update
{
    // major.minor.patch + sufixo opcional em letra (v3.0.2b -> build 2; sem sufixo -> 0)
    struct Version
    {
        int v[4];
        bool valid;

        Version () : valid(false) { v[0] = v[1] = v[2] = v[3] = 0; }
        std::wstring ToString () const;
    };

    // aceita "v3.0.2", "v.3.0.1d", "3.0.2", "SQLTools 3.0.2"
    Version ParseVersion (const std::string& text);
    Version CurrentVersion ();
    int  Compare (const Version& a, const Version& b);                       // <0, 0, >0
    bool IsMajorOrMinorBump (const Version& current, const Version& latest); // 3.0.x -> 3.1.0 ou 4.0.0

    struct ReleaseInfo
    {
        Version      version;
        std::string  tag;
        std::wstring title;
        std::wstring notes;     // corpo da release, com CRLF
        std::wstring url;       // pagina da release
    };

    struct CheckResult
    {
        bool         ok;
        std::wstring error;
        Version      current;
        std::vector<ReleaseInfo> newer; // somente as mais novas que a instalada, da mais nova para a mais antiga

        CheckResult () : ok(false) {}
        bool HasNewer () const { return ok && !newer.empty(); }
    };

    // Bloqueante (rede): chamar fora da thread de interface. Nunca lanca excecao.
    CheckResult CheckForUpdates ();

    // Exposto para testes
    bool ParseReleasesJson (const std::string& json, const Version& current, std::vector<ReleaseInfo>& newer, std::wstring& error);
}
