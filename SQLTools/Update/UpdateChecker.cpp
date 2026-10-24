#include "stdafx.h"
#include "Update/UpdateChecker.h"
#include "VersionInfo.rh"
#include <winhttp.h>
#include <algorithm>
#include <memory>

#pragma comment(lib, "winhttp.lib")

#ifndef WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY
#define WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY 4
#endif

namespace Update
{
    namespace
    {
        const wchar_t* const RELEASES_HOST = L"api.github.com";
        const wchar_t* const RELEASES_PATH = L"/repos/LucasAlves011/sqltools3/releases?per_page=30";
        const size_t MAX_RESPONSE = 4u * 1024 * 1024;

        std::wstring utf8ToWide (const std::string& s)
        {
            if (s.empty()) return std::wstring();
            int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), 0, 0);
            std::wstring w(n, L'\0');
            if (n) MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
            return w;
        }

        std::wstring withCrLf (const std::wstring& s)
        {
            std::wstring r;
            r.reserve(s.size() + s.size() / 20);
            for (size_t i = 0; i < s.size(); ++i)
            {
                if (s[i] == L'\r') continue;
                if (s[i] == L'\n') r += L'\r';
                r += s[i];
            }
            return r;
        }

        // ---- JSON minimo (suficiente para a resposta da API de releases) ----------------------
        struct JVal
        {
            enum Type { Null, Bool, Num, Str, Arr, Obj } type;
            bool b;
            std::string s;
            std::vector<JVal> a;
            std::vector<std::pair<std::string, JVal> > o;

            JVal () : type(Null), b(false) {}

            const JVal* get (const char* key) const
            {
                for (size_t i = 0; i < o.size(); ++i)
                    if (o[i].first == key) return &o[i].second;
                return 0;
            }
            std::string str (const char* key) const { const JVal* v = get(key); return v && v->type == Str ? v->s : std::string(); }
            bool flag (const char* key) const       { const JVal* v = get(key); return v && v->type == Bool && v->b; }
        };

        class JsonParser
        {
        public:
            JsonParser (const std::string& t) : m_t(t), m_p(0) {}

            bool parse (JVal& out) { return value(out, 0) && (ws(), m_p == m_t.size()); }

        private:
            const std::string& m_t;
            size_t m_p;

            void ws () { while (m_p < m_t.size() && (m_t[m_p] == ' ' || m_t[m_p] == '\t' || m_t[m_p] == '\r' || m_t[m_p] == '\n')) ++m_p; }
            bool lit (const char* w) { size_t n = strlen(w); if (m_t.compare(m_p, n, w) != 0) return false; m_p += n; return true; }

            static void putUtf8 (std::string& s, unsigned cp)
            {
                if (cp < 0x80)         s += (char)cp;
                else if (cp < 0x800)   { s += (char)(0xC0 | (cp >> 6));  s += (char)(0x80 | (cp & 0x3F)); }
                else if (cp < 0x10000) { s += (char)(0xE0 | (cp >> 12)); s += (char)(0x80 | ((cp >> 6) & 0x3F)); s += (char)(0x80 | (cp & 0x3F)); }
                else                   { s += (char)(0xF0 | (cp >> 18)); s += (char)(0x80 | ((cp >> 12) & 0x3F)); s += (char)(0x80 | ((cp >> 6) & 0x3F)); s += (char)(0x80 | (cp & 0x3F)); }
            }

            bool hex4 (unsigned& v)
            {
                if (m_p + 4 > m_t.size()) return false;
                v = 0;
                for (int i = 0; i < 4; ++i)
                {
                    char c = m_t[m_p++]; v <<= 4;
                    if (c >= '0' && c <= '9') v |= c - '0';
                    else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
                    else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
                    else return false;
                }
                return true;
            }

            bool string (std::string& out)
            {
                if (m_p >= m_t.size() || m_t[m_p] != '"') return false;
                ++m_p;
                while (m_p < m_t.size())
                {
                    char c = m_t[m_p++];
                    if (c == '"') return true;
                    if (c != '\\') { out += c; continue; }
                    if (m_p >= m_t.size()) return false;
                    c = m_t[m_p++];
                    switch (c)
                    {
                    case '"': case '\\': case '/': out += c; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'n': out += '\n'; break;
                    case 'r': out += '\r'; break;
                    case 't': out += '\t'; break;
                    case 'u':
                    {
                        unsigned cp;
                        if (!hex4(cp)) return false;
                        if (cp >= 0xD800 && cp < 0xDC00 && m_p + 1 < m_t.size() && m_t[m_p] == '\\' && m_t[m_p + 1] == 'u')
                        {
                            m_p += 2;
                            unsigned lo;
                            if (!hex4(lo)) return false;
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        }
                        putUtf8(out, cp);
                        break;
                    }
                    default: return false;
                    }
                }
                return false;
            }

            bool value (JVal& v, int depth)
            {
                if (depth > 32) return false;
                ws();
                if (m_p >= m_t.size()) return false;
                char c = m_t[m_p];
                if (c == '{')
                {
                    ++m_p; v.type = JVal::Obj; ws();
                    if (m_p < m_t.size() && m_t[m_p] == '}') { ++m_p; return true; }
                    for (;;)
                    {
                        ws();
                        std::string key;
                        if (!string(key)) return false;
                        ws();
                        if (m_p >= m_t.size() || m_t[m_p++] != ':') return false;
                        JVal child;
                        if (!value(child, depth + 1)) return false;
                        v.o.push_back(std::make_pair(key, child));
                        ws();
                        if (m_p >= m_t.size()) return false;
                        c = m_t[m_p++];
                        if (c == '}') return true;
                        if (c != ',') return false;
                    }
                }
                if (c == '[')
                {
                    ++m_p; v.type = JVal::Arr; ws();
                    if (m_p < m_t.size() && m_t[m_p] == ']') { ++m_p; return true; }
                    for (;;)
                    {
                        JVal child;
                        if (!value(child, depth + 1)) return false;
                        v.a.push_back(child);
                        ws();
                        if (m_p >= m_t.size()) return false;
                        c = m_t[m_p++];
                        if (c == ']') return true;
                        if (c != ',') return false;
                    }
                }
                if (c == '"') { v.type = JVal::Str; return string(v.s); }
                if (lit("true"))  { v.type = JVal::Bool; v.b = true;  return true; }
                if (lit("false")) { v.type = JVal::Bool; v.b = false; return true; }
                if (lit("null"))  { v.type = JVal::Null; return true; }
                // numero (nao usado, apenas consumido)
                size_t start = m_p;
                while (m_p < m_t.size() && strchr("+-0123456789.eE", m_t[m_p])) ++m_p;
                if (m_p == start) return false;
                v.type = JVal::Num;
                return true;
            }
        };

        // ---- HTTP ------------------------------------------------------------------------------
        class HInternet
        {
        public:
            explicit HInternet (HINTERNET h = 0) : m_h(h) {}
            ~HInternet () { if (m_h) WinHttpCloseHandle(m_h); }
            operator HINTERNET () const { return m_h; }
        private:
            HInternet (const HInternet&);
            HInternet& operator= (const HInternet&);
            HINTERNET m_h;
        };

        std::wstring lastErrorText (const wchar_t* what, DWORD err)
        {
            wchar_t buf[64];
            swprintf_s(buf, L" (error %lu)", err);
            return std::wstring(what) + buf;
        }

        bool httpGet (const wchar_t* host, const wchar_t* path, std::string& body, std::wstring& error)
        {
            std::wstring agent = L"SQLTools/" + CurrentVersion().ToString();

            HINTERNET hSession = WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
            if (!hSession) // Windows anterior ao 8.1 nao conhece o proxy automatico
                hSession = WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
            HInternet session(hSession);
            if (!session) { error = lastErrorText(L"Cannot initialize the network session", GetLastError()); return false; }

            WinHttpSetTimeouts(session, 10000, 10000, 10000, 15000);

            HInternet connect(WinHttpConnect(session, host, INTERNET_DEFAULT_HTTPS_PORT, 0));
            if (!connect) { error = lastErrorText(L"Cannot connect to the update server", GetLastError()); return false; }

            HInternet request(WinHttpOpenRequest(connect, L"GET", path, 0, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
            if (!request) { error = lastErrorText(L"Cannot create the request", GetLastError()); return false; }

            const wchar_t* headers = L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\n";
            if (!WinHttpSendRequest(request, headers, (DWORD)-1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
             || !WinHttpReceiveResponse(request, 0))
            {
                error = lastErrorText(L"Cannot reach the update server", GetLastError());
                return false;
            }

            DWORD status = 0, size = sizeof(status);
            WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
            if (status != 200)
            {
                wchar_t buf[96];
                swprintf_s(buf, L"The update server answered with HTTP status %lu.", status);
                error = buf;
                return false;
            }

            for (;;)
            {
                DWORD avail = 0;
                if (!WinHttpQueryDataAvailable(request, &avail)) { error = lastErrorText(L"Network error while reading the response", GetLastError()); return false; }
                if (avail == 0) break;
                if (body.size() + avail > MAX_RESPONSE) { error = L"The update server response is too large."; return false; }
                size_t old = body.size();
                body.resize(old + avail);
                DWORD got = 0;
                if (!WinHttpReadData(request, &body[old], avail, &got)) { error = lastErrorText(L"Network error while reading the response", GetLastError()); return false; }
                body.resize(old + got);
            }
            return true;
        }
    }

    // ---- Versao ---------------------------------------------------------------------------------
    std::wstring Version::ToString () const
    {
        wchar_t buf[48];
        swprintf_s(buf, L"%d.%d.%d", v[0], v[1], v[2]);
        std::wstring s = buf;
        if (v[3] > 0 && v[3] <= 26)
            s += (wchar_t)(L'a' + v[3] - 1);
        return s;
    }

    Version ParseVersion (const std::string& text)
    {
        Version r;
        size_t p = 0;
        while (p < text.size() && !isdigit((unsigned char)text[p])) ++p;   // "v", "v.", "SQLTools "
        if (p >= text.size()) return r;

        for (int i = 0; i < 3; ++i)
        {
            if (p >= text.size() || !isdigit((unsigned char)text[p])) return r;
            long n = 0;
            while (p < text.size() && isdigit((unsigned char)text[p]))
            {
                n = n * 10 + (text[p] - '0');
                if (n > 100000) return r;
                ++p;
            }
            r.v[i] = (int)n;
            if (i < 2)
            {
                if (p >= text.size() || text[p] != '.') return r;
                ++p;
            }
        }
        if (p < text.size() && text[p] >= 'a' && text[p] <= 'z')
            r.v[3] = text[p] - 'a' + 1;

        r.valid = true;
        return r;
    }

    Version CurrentVersion ()
    {
        return ParseVersion(SQLTOOLS_RELEASE_STR);
    }

    int Compare (const Version& a, const Version& b)
    {
        for (int i = 0; i < 4; ++i)
            if (a.v[i] != b.v[i])
                return a.v[i] < b.v[i] ? -1 : 1;
        return 0;
    }

    bool IsMajorOrMinorBump (const Version& current, const Version& latest)
    {
        return latest.v[0] > current.v[0]
            || (latest.v[0] == current.v[0] && latest.v[1] > current.v[1]);
    }

    // ---- Releases -------------------------------------------------------------------------------
    bool ParseReleasesJson (const std::string& json, const Version& current, std::vector<ReleaseInfo>& newer, std::wstring& error)
    {
        JVal root;
        if (!JsonParser(json).parse(root) || root.type != JVal::Arr)
        {
            error = L"Unexpected response from the update server.";
            return false;
        }

        newer.clear();
        for (size_t i = 0; i < root.a.size(); ++i)
        {
            const JVal& r = root.a[i];
            if (r.type != JVal::Obj || r.flag("draft") || r.flag("prerelease"))
                continue;

            ReleaseInfo info;
            info.tag     = r.str("tag_name");
            info.version = ParseVersion(info.tag);
            if (!info.version.valid || Compare(info.version, current) <= 0)
                continue;

            std::string name = r.str("name");
            info.title = utf8ToWide(name.empty() ? info.tag : name);
            info.notes = withCrLf(utf8ToWide(r.str("body")));
            info.url   = utf8ToWide(r.str("html_url"));
            newer.push_back(info);
        }

        std::sort(newer.begin(), newer.end(),
            [] (const ReleaseInfo& a, const ReleaseInfo& b) { return Compare(a.version, b.version) > 0; });
        return true;
    }

    CheckResult CheckForUpdates ()
    {
        CheckResult result;
        try
        {
            result.current = CurrentVersion();
            if (!result.current.valid)
            {
                result.error = L"Cannot determine the installed version.";
                return result;
            }

            std::string body;
            if (httpGet(RELEASES_HOST, RELEASES_PATH, body, result.error)
             && ParseReleasesJson(body, result.current, result.newer, result.error))
                result.ok = true;
        }
        catch (...)
        {
            result.ok = false;
            result.error = L"Unexpected error while checking for updates.";
        }
        return result;
    }
}
