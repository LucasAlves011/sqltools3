#include "stdafx.h"
#include "DbBrowser/OwnerFavorites.h"
#include "Common/ConfigFilesLocator.h"
#include <shlobj.h>

namespace OwnerFavorites
{
    static const wchar_t* const PROFILE_SECTION = L"ObjectsList";
    static const wchar_t* const GLOBAL_KEY      = L"GlobalFavoriteOwners";
    static const wchar_t* const LEGACY_KEY      = L"FavoriteOwners";
    static const wchar_t* const INI_KEY         = L"Owners";

    static unsigned s_globalVersion = 1;

    static CString Join (const std::vector<CString>& owners)
    {
        CString s;
        for (size_t i = 0; i < owners.size(); ++i)
        {
            if (i) s += L"|";
            s += owners[i];
        }
        return s;
    }

    static void Split (const CString& s, std::vector<CString>& owners)
    {
        owners.clear();
        int pos = 0;
        CString tok = s.Tokenize(L"|", pos);
        while (!tok.IsEmpty())
        {
            tok.Trim();
            bool dup = false;
            for (size_t i = 0; i < owners.size(); ++i)
                if (owners[i].CompareNoCase(tok) == 0) dup = true;
            if (!tok.IsEmpty() && !dup)
                owners.push_back(tok);
            tok = s.Tokenize(L"|", pos);
        }
    }

    static CString IniPath ()
    {
        CString folder = Common::ConfigFilesLocator::GetBaseFolder().c_str();
        if (folder.IsEmpty())
        {
            wchar_t buff[MAX_PATH] = L"";
            if (SUCCEEDED(::SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, buff)))
            {
                folder = buff;
                folder += L"\\SQLTools";
                ::CreateDirectory(folder, NULL);
            }
        }
        if (folder.IsEmpty())
            return CString();
        return folder + L"\\owner-favorites.ini";
    }

    std::vector<CString> LoadGlobal ()
    {
        std::vector<CString> owners;
        Split(AfxGetApp()->GetProfileString(PROFILE_SECTION, GLOBAL_KEY, L""), owners);
        return owners;
    }

    void SaveGlobal (const std::vector<CString>& owners)
    {
        AfxGetApp()->WriteProfileString(PROFILE_SECTION, GLOBAL_KEY, Join(owners));
        ++s_globalVersion;
    }

    unsigned GlobalVersion ()
    {
        return s_globalVersion;
    }

    bool LoadConnection (const CString& connection, std::vector<CString>& owners)
    {
        owners.clear();
        CString path = IniPath();
        if (path.IsEmpty() || connection.IsEmpty())
            return false;

        const CString none(L"\x01");
        wchar_t buff[4096] = L"";
        ::GetPrivateProfileString(connection, INI_KEY, none, buff, 4096, path);
        if (none == buff)
            return false;
        Split(buff, owners);
        return true;
    }

    void SaveConnection (const CString& connection, const std::vector<CString>& owners)
    {
        CString path = IniPath();
        if (path.IsEmpty() || connection.IsEmpty())
            return;
        // lista vazia continua gravada (valor em branco) para nao receber a lista antiga de novo
        ::WritePrivateProfileString(connection, INI_KEY, Join(owners), path);
    }

    bool TakeLegacy (std::vector<CString>& owners)
    {
        owners.clear();
        CString s = AfxGetApp()->GetProfileString(PROFILE_SECTION, LEGACY_KEY, L"");
        if (s.IsEmpty())
            return false;
        AfxGetApp()->WriteProfileString(PROFILE_SECTION, LEGACY_KEY, NULL);    // apaga o valor
        Split(s, owners);
        return !owners.empty();
    }
}
