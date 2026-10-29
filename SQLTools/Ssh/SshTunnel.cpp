/*
    SQLTools is a tool for Oracle database developers and DBAs.
    See SshTunnel.h for the security model.
*/
#include "stdafx.h"
#include "SshTunnel.h"
#include <mutex>
#include <vector>

#ifdef SQLTOOLS_SSH

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <shlobj.h>
#include <libssh2.h>
#include <thread>
#include <atomic>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <mutex>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")

namespace Ssh
{
    namespace
    {
        const int    kConnectTimeoutMs = 15000;
        const size_t kMaxPending       = 256 * 1024;
        const size_t kMaxConnections   = 32;

        void Wipe (std::string& s)
        {
            if (!s.empty())
                SecureZeroMemory(&s[0], s.size());
            s.clear();
        }

        // only modern algorithms are offered (no SHA-1 signatures, no CBC, no 3DES/RC4/DSS)
        const char* kKex     = "curve25519-sha256,curve25519-sha256@libssh.org,ecdh-sha2-nistp521,ecdh-sha2-nistp384,ecdh-sha2-nistp256,"
                               "diffie-hellman-group-exchange-sha256,diffie-hellman-group18-sha512,diffie-hellman-group16-sha512,diffie-hellman-group14-sha256";
        const char* kHostKey = "ssh-ed25519,ecdsa-sha2-nistp521,ecdsa-sha2-nistp384,ecdsa-sha2-nistp256,rsa-sha2-512,rsa-sha2-256";
        const char* kCrypt   = "aes256-gcm@openssh.com,aes128-gcm@openssh.com,aes256-ctr,aes192-ctr,aes128-ctr";
        const char* kMac     = "hmac-sha2-512-etm@openssh.com,hmac-sha2-256-etm@openssh.com,hmac-sha2-512,hmac-sha2-256";

        std::string Utf8Path (const std::wstring& w)
        {
            if (w.empty()) return std::string();
            int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), 0, 0, 0, 0);
            std::string s(n, '\0');
            WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, 0, 0);
            return s;
        }

        std::wstring Widen (const std::string& s)
        {
            if (s.empty()) return std::wstring();
            int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), 0, 0);
            std::wstring w(n, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
            return w;
        }

        std::string Base64NoPad (const unsigned char* d, size_t n)
        {
            static const char t[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            std::string r;
            for (size_t i = 0; i < n; i += 3)
            {
                unsigned v = d[i] << 16;
                if (i + 1 < n) v |= d[i + 1] << 8;
                if (i + 2 < n) v |= d[i + 2];
                r += t[(v >> 18) & 63];
                r += t[(v >> 12) & 63];
                if (i + 1 < n) r += t[(v >> 6) & 63];
                if (i + 2 < n) r += t[v & 63];
            }
            return r;
        }

        std::wstring KnownHostsPath ()
        {
            PWSTR p = 0;
            std::wstring dir;
            if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, 0, &p)) && p)
                dir = p;
            CoTaskMemFree(p);
            dir += L"\\SQLTools";
            CreateDirectoryW(dir.c_str(), 0);
            return dir + L"\\ssh_known_hosts";
        }

        const char* KeyTypeName (int type)
        {
            switch (type)
            {
            case LIBSSH2_HOSTKEY_TYPE_RSA:        return "ssh-rsa";
            case LIBSSH2_HOSTKEY_TYPE_DSS:        return "ssh-dss";
            case LIBSSH2_HOSTKEY_TYPE_ECDSA_256:  return "ecdsa-sha2-nistp256";
            case LIBSSH2_HOSTKEY_TYPE_ECDSA_384:  return "ecdsa-sha2-nistp384";
            case LIBSSH2_HOSTKEY_TYPE_ECDSA_521:  return "ecdsa-sha2-nistp521";
            case LIBSSH2_HOSTKEY_TYPE_ED25519:    return "ssh-ed25519";
            }
            return "unknown";
        }

        std::string LastError (LIBSSH2_SESSION* s, const char* what)
        {
            char* msg = 0; int len = 0;
            int rc = libssh2_session_last_error(s, &msg, &len, 0);
            std::string r = std::string(what) + " (" + (msg ? std::string(msg, len) : std::string("error ") + std::to_string(rc)) + ")";
            return r;
        }

        // the loopback client must be a socket of this process
        bool IsOwnProcessPeer (const sockaddr_in& peer, unsigned short listenPort)
        {
            for (int attempt = 0; attempt < 5; ++attempt)
            {
                ULONG size = 0;
                GetExtendedTcpTable(0, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
                std::vector<BYTE> buf(size + 1024);
                if (GetExtendedTcpTable(buf.data(), &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR)
                {
                    const MIB_TCPTABLE_OWNER_PID* t = (const MIB_TCPTABLE_OWNER_PID*)buf.data();
                    for (DWORD i = 0; i < t->dwNumEntries; ++i)
                    {
                        const MIB_TCPROW_OWNER_PID& r = t->table[i];
                        if (r.dwLocalAddr == peer.sin_addr.s_addr
                        && (unsigned short)r.dwLocalPort == peer.sin_port
                        && (unsigned short)r.dwRemotePort == htons(listenPort))
                            return r.dwOwningPid == GetCurrentProcessId();
                    }
                }
                Sleep(10);
            }
            return false;
        }

        void CloseSock (SOCKET& s)
        {
            if (s != INVALID_SOCKET) { closesocket(s); s = INVALID_SOCKET; }
        }

        // ---- known hosts ----
        enum HostKeyState { Known, Unknown, Changed };

        HostKeyState CheckKnownHost (const std::string& endpoint, const std::string& type, const std::string& fp)
        {
            std::ifstream f(KnownHostsPath());
            std::string line;
            while (std::getline(f, line))
            {
                std::istringstream is(line);
                std::string e, t, h;
                if (!(is >> e >> t >> h) || e != endpoint || t != type)
                    continue;
                return h == fp ? Known : Changed;
            }
            return Unknown;
        }

        void RememberHost (const std::string& endpoint, const std::string& type, const std::string& fp)
        {
            std::ofstream f(KnownHostsPath(), std::ios::app);
            f << endpoint << ' ' << type << ' ' << fp << "\n";
        }

        void KbdIntCallback (const char*, int, const char*, int, int numPrompts,
            const LIBSSH2_USERAUTH_KBDINT_PROMPT*, LIBSSH2_USERAUTH_KBDINT_RESPONSE* responses, void** abstract)
        {
            const std::string* secret = (const std::string*)*abstract;
            for (int i = 0; i < numPrompts; ++i)
            {
                responses[i].text   = _strdup(secret->c_str());
                responses[i].length = (unsigned int)secret->size();
            }
        }

        struct WinSock
        {
            bool ok;
            WinSock () { WSADATA d; ok = WSAStartup(MAKEWORD(2, 2), &d) == 0; }
            ~WinSock () { if (ok) WSACleanup(); }
        };
    }

    void Config::Wipe ()
    {
        ::Ssh::Wipe(secret);
    }

    struct Tunnel::Impl
    {
        struct Conn
        {
            SOCKET s = INVALID_SOCKET;
            LIBSSH2_CHANNEL* ch = 0;
            std::string toChan, toSock;
            bool sockClosed = false, eofSent = false, chanEof = false, dead = false;
        };

        std::unique_ptr<WinSock> ws;
        SOCKET sock = INVALID_SOCKET, listener = INVALID_SOCKET;
        LIBSSH2_SESSION* session = 0;
        unsigned short port = 0;
        std::string targetHost;
        int targetPort = 0;
        std::thread worker;
        std::atomic<bool> stop{false}, running{false};
        std::vector<Conn> conns;
        std::string secretCopy;     // for keyboard-interactive callback only during Start

        ~Impl () { Shutdown(); }

        void Shutdown ()
        {
            stop = true;
            if (worker.joinable()) worker.join();
            running = false;
            for (Conn& c : conns) CloseConn(c);
            conns.clear();
            if (session)
            {
                libssh2_session_set_blocking(session, 1);
                libssh2_session_set_timeout(session, 2000);
                libssh2_session_disconnect(session, "SQLTools closed the tunnel");
                libssh2_session_free(session);
                session = 0;
                libssh2_exit();
            }
            CloseSock(listener);
            CloseSock(sock);
            ::Ssh::Wipe(secretCopy);
            ws.reset();
        }

        void CloseConn (Conn& c)
        {
            if (c.ch) { libssh2_channel_free(c.ch); c.ch = 0; }
            CloseSock(c.s);
        }

        void WaitSocket (int ms)
        {
            fd_set rd, wr;
            FD_ZERO(&rd); FD_ZERO(&wr);
            int dir = libssh2_session_block_directions(session);
            if (dir & LIBSSH2_SESSION_BLOCK_INBOUND)  FD_SET(sock, &rd);
            if (dir & LIBSSH2_SESSION_BLOCK_OUTBOUND) FD_SET(sock, &wr);
            timeval tv = { ms / 1000, (ms % 1000) * 1000 };
            select(0, &rd, &wr, 0, &tv);
        }

        void ConnectSocket (const Config& cfg)
        {
            addrinfo hints = {}, *res = 0;
            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;
            if (getaddrinfo(cfg.host.c_str(), std::to_string(cfg.port).c_str(), &hints, &res) != 0 || !res)
                throw Error("Cannot resolve the SSH host \"" + cfg.host + "\".");

            std::string lastErr = "no address";
            for (addrinfo* a = res; a; a = a->ai_next)
            {
                SOCKET s = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
                if (s == INVALID_SOCKET) continue;
                u_long nb = 1; ioctlsocket(s, FIONBIO, &nb);
                int rc = connect(s, a->ai_addr, (int)a->ai_addrlen);
                if (rc != 0 && WSAGetLastError() == WSAEWOULDBLOCK)
                {
                    fd_set wr, ex; FD_ZERO(&wr); FD_ZERO(&ex); FD_SET(s, &wr); FD_SET(s, &ex);
                    timeval tv = { kConnectTimeoutMs / 1000, 0 };
                    if (select(0, 0, &wr, &ex, &tv) > 0 && FD_ISSET(s, &wr))
                        rc = 0;
                    else
                        lastErr = "connection timed out or refused";
                }
                if (rc == 0)
                {
                    nb = 0; ioctlsocket(s, FIONBIO, &nb);
                    BOOL nd = TRUE; setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char*)&nd, sizeof nd);
                    sock = s;
                    freeaddrinfo(res);
                    return;
                }
                closesocket(s);
            }
            freeaddrinfo(res);
            throw Error("Cannot connect to the SSH server " + cfg.host + ":" + std::to_string(cfg.port) + " (" + lastErr + ").");
        }

        void VerifyHostKey (const Config& cfg, const ConfirmHostKeyFn& confirm)
        {
            size_t len = 0; int type = 0;
            if (!libssh2_session_hostkey(session, &len, &type))
                throw Error("The SSH server did not present a host key.");
            const char* h = libssh2_hostkey_hash(session, LIBSSH2_HOSTKEY_HASH_SHA256);
            if (!h)
                throw Error("Cannot compute the SSH host key fingerprint.");

            std::string fp = "SHA256:" + Base64NoPad((const unsigned char*)h, 32);
            std::string typeName = KeyTypeName(type);
            std::string host = cfg.host;
            std::transform(host.begin(), host.end(), host.begin(), [](unsigned char c) { return (char)tolower(c); });
            std::string endpoint = host + ":" + std::to_string(cfg.port);

            switch (CheckKnownHost(endpoint, typeName, fp))
            {
            case Known:
                return;
            case Changed:
                throw Error("WARNING: the SSH host key of " + endpoint + " has CHANGED!\n\n"
                    "Someone may be intercepting the connection (man-in-the-middle), or the server was reinstalled.\n"
                    "Received " + typeName + " " + fp + ".\n\n"
                    "The connection was aborted. If the change is legitimate, remove the line of this host from\n" +
                    Utf8Path(KnownHostsPath()) + "\nand connect again.");
            default:
                if (!confirm || !confirm(endpoint, typeName, fp))
                    throw Error("The SSH host key of " + endpoint + " was not accepted.");
                RememberHost(endpoint, typeName, fp);
            }
        }

        void Authenticate (const Config& cfg)
        {
            const char* user = cfg.user.c_str();
            char* methods = libssh2_userauth_list(session, user, (unsigned)cfg.user.size());
            if (libssh2_userauth_authenticated(session))
                return;
            std::string list = methods ? methods : "";

            if (cfg.useKey)
            {
                if (cfg.keyFile.empty())
                    throw Error("No SSH private key file was selected.");
                int rc = libssh2_userauth_publickey_fromfile_ex(session, user, (unsigned)cfg.user.size(), 0,
                    cfg.keyFile.c_str(), cfg.secret.empty() ? 0 : cfg.secret.c_str());
                if (rc)
                    throw Error(LastError(session, "SSH key authentication failed (wrong key, passphrase or user?)"));
                return;
            }

            if (list.find("password") != std::string::npos)
            {
                if (!libssh2_userauth_password_ex(session, user, (unsigned)cfg.user.size(), cfg.secret.c_str(), (unsigned)cfg.secret.size(), 0))
                    return;
            }
            if (list.find("keyboard-interactive") != std::string::npos)
            {
                secretCopy = cfg.secret;
                *libssh2_session_abstract(session) = &secretCopy;
                int rc = libssh2_userauth_keyboard_interactive_ex(session, user, (unsigned)cfg.user.size(), &KbdIntCallback);
                ::Ssh::Wipe(secretCopy);
                if (!rc)
                    return;
            }
            throw Error("SSH authentication failed: wrong user or password, or the server does not accept this method"
                + (list.empty() ? std::string() : " (server accepts: " + list + ")") + ".");
        }

        void Start (const Config& cfg, const ConfirmHostKeyFn& confirm)
        {
            if (cfg.host.empty() || cfg.user.empty())
                throw Error("The SSH host and user are required.");
            if (cfg.targetHost.empty() || cfg.targetPort <= 0 || cfg.targetPort > 65535)
                throw Error("The database host and port are required to open the SSH tunnel.");

            ws.reset(new WinSock);
            if (!ws->ok) throw Error("Cannot initialize Winsock.");
            if (libssh2_init(0)) throw Error("Cannot initialize libssh2.");

            targetHost = cfg.targetHost;
            targetPort = cfg.targetPort;

            ConnectSocket(cfg);

            session = libssh2_session_init();
            if (!session) throw Error("Cannot create the SSH session.");
            libssh2_session_set_blocking(session, 1);
            libssh2_session_set_timeout(session, kConnectTimeoutMs);

            libssh2_session_method_pref(session, LIBSSH2_METHOD_KEX,      kKex);
            libssh2_session_method_pref(session, LIBSSH2_METHOD_HOSTKEY,  kHostKey);
            libssh2_session_method_pref(session, LIBSSH2_METHOD_CRYPT_CS, kCrypt);
            libssh2_session_method_pref(session, LIBSSH2_METHOD_CRYPT_SC, kCrypt);
            libssh2_session_method_pref(session, LIBSSH2_METHOD_MAC_CS,   kMac);
            libssh2_session_method_pref(session, LIBSSH2_METHOD_MAC_SC,   kMac);

            if (libssh2_session_handshake(session, sock))
                throw Error(LastError(session, "SSH handshake failed (the server may only support outdated algorithms)"));

            VerifyHostKey(cfg, confirm);
            Authenticate(cfg);

            // local listener: loopback only, random port, nobody else can take the port over
            listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (listener == INVALID_SOCKET) throw Error("Cannot create the local tunnel socket.");
            BOOL excl = TRUE;
            setsockopt(listener, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*)&excl, sizeof excl);
            sockaddr_in a = {};
            a.sin_family = AF_INET;
            a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            a.sin_port = htons(cfg.localPort);
            bool bound = false;
            for (int attempt = 0; attempt < 30 && !bound; ++attempt)
            {
                bound = bind(listener, (sockaddr*)&a, sizeof a) == 0;
                // the previous tunnel of the same connection may still be releasing its port
                if (!bound && (!cfg.localPort || WSAGetLastError() != WSAEADDRINUSE))
                    break;
                if (!bound)
                    Sleep(100);
            }
            if (!bound || listen(listener, 8) != 0)
                throw Error("Cannot open the local tunnel port.");
            int alen = sizeof a;
            getsockname(listener, (sockaddr*)&a, &alen);
            port = ntohs(a.sin_port);
            u_long nb = 1; ioctlsocket(listener, FIONBIO, &nb);

            libssh2_keepalive_config(session, 1, 30);
            libssh2_session_set_blocking(session, 0);

            running = true;
            worker = std::thread([this] { Run(); });
        }

        bool OpenChannel (Conn& c, const sockaddr_in& peer)
        {
            DWORD t0 = GetTickCount();
            for (;;)
            {
                c.ch = libssh2_channel_direct_tcpip_ex(session, targetHost.c_str(), targetPort, "127.0.0.1", ntohs(peer.sin_port));
                if (c.ch) return true;
                if (libssh2_session_last_errno(session) != LIBSSH2_ERROR_EAGAIN) return false;
                if (GetTickCount() - t0 > (DWORD)kConnectTimeoutMs) return false;
                WaitSocket(100);
            }
        }

        void AcceptLocal ()
        {
            sockaddr_in peer = {};
            int plen = sizeof peer;
            SOCKET s = accept(listener, (sockaddr*)&peer, &plen);
            if (s == INVALID_SOCKET) return;

            if (conns.size() >= kMaxConnections || peer.sin_addr.s_addr != htonl(INADDR_LOOPBACK) || !IsOwnProcessPeer(peer, port))
            {
                closesocket(s);
                return;
            }
            u_long nb = 1; ioctlsocket(s, FIONBIO, &nb);
            BOOL nd = TRUE; setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char*)&nd, sizeof nd);

            Conn c;
            c.s = s;
            if (!OpenChannel(c, peer))
            {
                CloseConn(c);
                return;
            }
            conns.push_back(std::move(c));
        }

        // one forwarding step for a connection
        void Pump (Conn& c, bool sockReadable, bool sockWritable)
        {
            char buf[16384];

            if (sockReadable && !c.sockClosed && c.toChan.size() < kMaxPending)
            {
                int n = recv(c.s, buf, sizeof buf, 0);
                if (n > 0) c.toChan.append(buf, n);
                else if (n == 0 || WSAGetLastError() != WSAEWOULDBLOCK) c.sockClosed = true;
            }

            while (!c.toChan.empty())
            {
                ssize_t rc = libssh2_channel_write(c.ch, c.toChan.data(), c.toChan.size());
                if (rc == LIBSSH2_ERROR_EAGAIN) break;
                if (rc < 0) { c.dead = true; return; }
                c.toChan.erase(0, (size_t)rc);
            }

            if (c.sockClosed && c.toChan.empty() && !c.eofSent)
            {
                int rc = libssh2_channel_send_eof(c.ch);
                if (rc == 0) c.eofSent = true;
                else if (rc != LIBSSH2_ERROR_EAGAIN) { c.dead = true; return; }
            }

            if (!c.chanEof && c.toSock.size() < kMaxPending)
            {
                ssize_t rc = libssh2_channel_read(c.ch, buf, sizeof buf);
                if (rc > 0) c.toSock.append(buf, (size_t)rc);
                else if (rc == 0 || libssh2_channel_eof(c.ch)) c.chanEof = true;
                else if (rc != LIBSSH2_ERROR_EAGAIN) { c.dead = true; return; }
            }

            while (!c.toSock.empty())
            {
                int n = send(c.s, c.toSock.data(), (int)c.toSock.size(), 0);
                if (n > 0) c.toSock.erase(0, n);
                else if (WSAGetLastError() == WSAEWOULDBLOCK) break;
                else { c.dead = true; return; }
            }

            if (c.chanEof && c.toSock.empty())
                c.dead = true;
            if (c.sockClosed && c.eofSent && c.chanEof)
                c.dead = true;
        }

        void Run ()
        {
            while (!stop)
            {
                fd_set rd, wr;
                FD_ZERO(&rd); FD_ZERO(&wr);
                FD_SET(listener, &rd);
                FD_SET(sock, &rd);
                for (Conn& c : conns)
                {
                    if (!c.sockClosed && c.toChan.size() < kMaxPending) FD_SET(c.s, &rd);
                    if (!c.toSock.empty()) FD_SET(c.s, &wr);
                }
                timeval tv = { 0, 30000 };
                select(0, &rd, &wr, 0, &tv);

                if (stop) break;

                if (FD_ISSET(listener, &rd))
                    AcceptLocal();

                // the server closed the SSH connection
                if (FD_ISSET(sock, &rd))
                {
                    char peek;
                    int n = recv(sock, &peek, 1, MSG_PEEK);
                    if (n == 0 || (n < 0 && WSAGetLastError() != WSAEWOULDBLOCK))
                        break;
                }

                for (Conn& c : conns)
                    Pump(c, FD_ISSET(c.s, &rd) != 0, FD_ISSET(c.s, &wr) != 0);

                for (size_t i = conns.size(); i-- > 0; )
                    if (conns[i].dead)
                    {
                        CloseConn(conns[i]);
                        conns.erase(conns.begin() + i);
                    }

                int next = 0;
                if (libssh2_keepalive_send(session, &next) < 0 && libssh2_session_last_errno(session) != LIBSSH2_ERROR_EAGAIN)
                    break;  // the SSH connection is gone
            }
            running = false;
            // wake up everything that depends on the tunnel
            for (Conn& c : conns) CloseConn(c);
            conns.clear();
            CloseSock(listener);
        }
    };

    Tunnel::Tunnel () {}
    Tunnel::~Tunnel () { Stop(); }

    bool Tunnel::IsAvailable () { return true; }

    void Tunnel::Start (const Config& cfg, const ConfirmHostKeyFn& confirm)
    {
        Stop();
        std::unique_ptr<Impl> impl(new Impl);
        try
        {
            impl->Start(cfg, confirm);
        }
        catch (...)
        {
            impl->Shutdown();
            throw;
        }
        m_impl = std::move(impl);
    }

    void Tunnel::Stop ()
    {
        m_impl.reset();
    }

    bool Tunnel::IsRunning () const { return m_impl && m_impl->running; }
    unsigned short Tunnel::LocalPort () const { return m_impl ? m_impl->port : 0; }
}

#else // !SQLTOOLS_SSH

namespace Ssh
{
    void Config::Wipe () { if (!secret.empty()) SecureZeroMemory(&secret[0], secret.size()); secret.clear(); }

    struct Tunnel::Impl {};
    Tunnel::Tunnel () {}
    Tunnel::~Tunnel () {}
    bool Tunnel::IsAvailable () { return false; }
    void Tunnel::Start (const Config&, const ConfirmHostKeyFn&) { throw Error("SSH tunnels are not available in this build."); }
    void Tunnel::Stop () {}
    bool Tunnel::IsRunning () const { return false; }
    unsigned short Tunnel::LocalPort () const { return 0; }
}

#endif

namespace Ssh
{
    namespace
    {
        struct Holder
        {
            std::mutex mutex;
            std::unique_ptr<Tunnel> current;
            std::unique_ptr<Config> config;     // how the current tunnel was started (to bring it back)
            std::vector<std::unique_ptr<Tunnel>> retired;
        };

        Holder& H ()
        {
            static Holder h;
            return h;
        }
    }

    unsigned short StartMainTunnel (const Config& cfg, const ConfirmHostKeyFn& confirm)
    {
        std::unique_ptr<Tunnel> t(new Tunnel);
        t->Start(cfg, confirm);
        unsigned short port = t->LocalPort();

        Holder& h = H();
        std::lock_guard<std::mutex> lock(h.mutex);
        if (h.current)
            h.retired.push_back(std::move(h.current));
        h.current = std::move(t);
        h.config.reset(new Config(cfg));
        h.config->localPort = port;
        return port;
    }

    void EnsureMainTunnel ()
    {
        Holder& h = H();
        std::lock_guard<std::mutex> lock(h.mutex);
        if (!h.config || (h.current && h.current->IsRunning()))
            return;

        if (h.current)
        {
            h.current->Stop();
            h.current.reset();
        }
        std::unique_ptr<Tunnel> t(new Tunnel);
        // no confirmation: a host key that is not trusted yet (or has changed) is an error here
        t->Start(*h.config, ConfirmHostKeyFn());
        h.current = std::move(t);
    }

    bool MapDisplayHost (std::string& host, const std::string& port)
    {
        Holder& h = H();
        std::lock_guard<std::mutex> lock(h.mutex);
        if (!h.config || host != "127.0.0.1" || port != std::to_string(h.config->localPort))
            return false;
        host = h.config->targetHost;
        return true;
    }

    void ReleaseRetiredTunnels ()
    {
        Holder& h = H();
        std::vector<std::unique_ptr<Tunnel>> old;
        {
            std::lock_guard<std::mutex> lock(h.mutex);
            old.swap(h.retired);
        }
        old.clear();
    }

    void StopAllTunnels ()
    {
        Holder& h = H();
        std::vector<std::unique_ptr<Tunnel>> old;
        {
            std::lock_guard<std::mutex> lock(h.mutex);
            old.swap(h.retired);
            if (h.current)
                old.push_back(std::move(h.current));
            h.config.reset();
        }
        old.clear();
    }
}
