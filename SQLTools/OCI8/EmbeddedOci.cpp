#include "stdafx.h"
#include "OCI8/EmbeddedOci.h"
#include "resource.h"
// o projeto compila com NTDDI_VERSION antigo; a API de compressao exige Windows 8+
#pragma push_macro("NTDDI_VERSION")
#undef NTDDI_VERSION
#define NTDDI_VERSION 0x06020000
#include <compressapi.h>
#pragma pop_macro("NTDDI_VERSION")
#include <shlobj.h>
#include <string>
#include <vector>

#pragma comment(lib, "Cabinet.lib")

namespace EmbeddedOci
{
    namespace
    {
        struct Entry { std::wstring name; unsigned long long rawSize, compSize; const BYTE* data; };

        class Reader
        {
        public:
            Reader (const BYTE* p, size_t size) : m_p(p), m_end(p + size) {}
            bool read (void* dst, size_t n) { if ((size_t)(m_end - m_p) < n) return false; memcpy(dst, m_p, n); m_p += n; return true; }
            const BYTE* take (unsigned long long n) { if ((unsigned long long)(m_end - m_p) < n) return 0; const BYTE* r = m_p; m_p += (size_t)n; return r; }
        private:
            const BYTE* m_p; const BYTE* m_end;
        };

        bool parse (const BYTE* data, size_t size, std::vector<Entry>& entries)
        {
            Reader r(data, size);
            char magic[4]; unsigned version, count;
            if (!r.read(magic, 4) || memcmp(magic, "OCIB", 4) || !r.read(&version, 4) || version != 1 || !r.read(&count, 4))
                return false;

            for (unsigned i = 0; i < count; ++i)
            {
                Entry e; unsigned short nameLen;
                if (!r.read(&nameLen, 2)) return false;
                const BYTE* name = r.take(nameLen);
                if (!name || !r.read(&e.rawSize, 8) || !r.read(&e.compSize, 8)) return false;
                e.name.assign(name, name + nameLen);
                e.data = r.take(e.compSize);
                if (!e.data) return false;
                // o nome vem do proprio recurso, mas nunca aceita separadores de caminho
                if (e.name.empty() || e.name.find_first_of(L"\\/:") != std::wstring::npos) return false;
                entries.push_back(e);
            }
            return true;
        }

        unsigned long long fnv1a (const BYTE* p, size_t n)
        {
            unsigned long long h = 1469598103934665603ULL;
            for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 1099511628211ULL; }
            return h;
        }

        bool fileHasSize (const std::wstring& path, unsigned long long size)
        {
            WIN32_FILE_ATTRIBUTE_DATA a;
            if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &a)) return false;
            return (((unsigned long long)a.nFileSizeHigh << 32) | a.nFileSizeLow) == size;
        }

        bool allPresent (const std::vector<Entry>& entries, const std::wstring& dir)
        {
            for (size_t i = 0; i < entries.size(); ++i)
                if (!fileHasSize(dir + L"\\" + entries[i].name, entries[i].rawSize))
                    return false;
            return true;
        }

        bool writeFile (const std::wstring& path, const void* data, size_t size)
        {
            HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
            if (h == INVALID_HANDLE_VALUE) return false;
            const BYTE* p = (const BYTE*)data;
            bool ok = true;
            while (ok && size)
            {
                DWORD chunk = (DWORD)(size > (1u << 24) ? (1u << 24) : size), written = 0;
                ok = WriteFile(h, p, chunk, &written, 0) && written == chunk;
                p += chunk; size -= chunk;
            }
            return CloseHandle(h) && ok;
        }

        bool extractAll (const std::vector<Entry>& entries, const std::wstring& dir)
        {
            DECOMPRESSOR_HANDLE dec = 0;
            if (!CreateDecompressor(COMPRESS_ALGORITHM_LZMS, 0, &dec)) return false;

            bool ok = true;
            for (size_t i = 0; ok && i < entries.size(); ++i)
            {
                const Entry& e = entries[i];
                std::vector<BYTE> raw((size_t)e.rawSize);
                SIZE_T got = 0;
                ok = (e.rawSize == 0 || Decompress(dec, e.data, (SIZE_T)e.compSize, &raw[0], raw.size(), &got))
                  && got == raw.size()
                  && writeFile(dir + L"\\" + e.name, raw.empty() ? 0 : &raw[0], raw.size());
            }
            CloseDecompressor(dec);
            return ok;
        }

        void removeDir (const std::wstring& dir)
        {
            WIN32_FIND_DATAW fd;
            HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
            if (h != INVALID_HANDLE_VALUE)
            {
                do { if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) DeleteFileW((dir + L"\\" + fd.cFileName).c_str()); }
                while (FindNextFileW(h, &fd));
                FindClose(h);
            }
            RemoveDirectoryW(dir.c_str()); // falha (e e ignorado) se alguma DLL ainda estiver em uso
        }

        void removeOldBundles (const std::wstring& root, const std::wstring& keep)
        {
            WIN32_FIND_DATAW fd;
            HANDLE h = FindFirstFileW((root + L"\\bundle-*").c_str(), &fd);
            if (h == INVALID_HANDLE_VALUE) return;
            do {
                if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && keep != fd.cFileName)
                    removeDir(root + L"\\" + fd.cFileName);
            } while (FindNextFileW(h, &fd));
            FindClose(h);
        }

        bool ociAlreadyAvailable ()
        {
            WCHAR buf[MAX_PATH]; LPWSTR file;
            DWORD n = SearchPathW(0, L"OCI.DLL", 0, MAX_PATH, buf, &file);
            return n > 0 && n < MAX_PATH;
        }

        bool activate ()
        {
#ifndef _WIN64
            return false;
#else
            if (ociAlreadyAvailable())
                return false;

            HRSRC hRes = FindResourceW(0, MAKEINTRESOURCEW(IDR_OCI_BUNDLE), RT_RCDATA);
            HGLOBAL hMem = hRes ? LoadResource(0, hRes) : 0;
            const BYTE* data = hMem ? (const BYTE*)LockResource(hMem) : 0;
            size_t size = hRes ? SizeofResource(0, hRes) : 0;
            if (!data || !size)
                return false;

            std::vector<Entry> entries;
            if (!parse(data, size, entries) || entries.empty())
                return false;

            PWSTR local = 0;
            if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, 0, &local)))
                return false;
            std::wstring root = std::wstring(local) + L"\\SQLTools\\oci";
            CoTaskMemFree(local);

            WCHAR idBuf[40];
            swprintf_s(idBuf, L"bundle-%016llx", fnv1a(data, size));
            std::wstring id = idBuf;
            std::wstring dir = root + L"\\" + id;

            if (!allPresent(entries, dir))
            {
                SHCreateDirectoryExW(0, root.c_str(), 0);

                // extrai em diretorio temporario e renomeia: evita DLL pela metade e conflito entre instancias
                WCHAR tmpName[64];
                swprintf_s(tmpName, L"\\tmp-%lu-%lu", GetCurrentProcessId(), GetTickCount());
                std::wstring tmp = root + tmpName;
                CreateDirectoryW(tmp.c_str(), 0);

                bool ok = extractAll(entries, tmp);
                if (ok && !MoveFileExW(tmp.c_str(), dir.c_str(), 0))
                    ok = allPresent(entries, dir); // outra instancia pode ter concluido antes

                removeDir(tmp); // se foi renomeado, ja nao existe
                if (!ok)
                    return false;
            }

            if (!SetDllDirectoryW(dir.c_str()))
                return false;

            removeOldBundles(root, id);
            return true;
#endif
        }
    }

    bool Activate ()
    {
        try { return activate(); }
        catch (...) { return false; }
    }
}
