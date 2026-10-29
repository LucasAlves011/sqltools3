/*
    SQLTools is a tool for Oracle database developers and DBAs.

    SSH tunnel (local port forwarding) used to reach databases that are only
    visible from an SSH/bastion host, in place of an external tool like PuTTY.
    Built on libssh2 (see 3dParty/libssh2).

    Security model:
      - the server host key is verified against %LOCALAPPDATA%\SQLTools\ssh_known_hosts;
        an unknown key needs an explicit user confirmation (SHA256 fingerprint is shown),
        a changed key aborts the connection;
      - only modern key exchange / cipher / MAC / host key algorithms are offered;
      - the local listener is bound to 127.0.0.1 on a random port and only accepts
        connections coming from this very process;
      - the forwarding destination is fixed (it can not be changed by a local client);
      - secrets are kept only in memory and wiped after use.
*/
#pragma once

#include <string>
#include <functional>
#include <memory>
#include <stdexcept>

namespace Ssh
{
    struct Error : std::runtime_error
    {
        explicit Error (const std::string& what) : std::runtime_error(what) {}
    };

    struct Config
    {
        std::string host;
        int         port = 22;
        std::string user;
        bool        useKey = false;     // private key authentication instead of password
        std::string keyFile;
        std::string secret;             // password, or the passphrase of the private key
        std::string targetHost;         // database host as seen from the SSH server
        int         targetPort = 1521;
        unsigned short localPort = 0;   // preferred local port (0 - any), used to bring a dead tunnel back on the same port

        Config () = default;
        Config (const Config&) = default;
        Config& operator = (const Config&) = default;
        ~Config () { Wipe(); }
        void Wipe ();
    };

    // called (from the thread that calls Tunnel::Start) when the server key is not known yet;
    // return true to trust and remember it
    typedef std::function<bool (const std::string& endpoint, const std::string& keyType, const std::string& fingerprint)> ConfirmHostKeyFn;

    class Tunnel
    {
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    public:
        Tunnel ();
        ~Tunnel ();

        static bool IsAvailable ();

        // connects, authenticates and starts forwarding, throws Ssh::Error
        void Start (const Config&, const ConfirmHostKeyFn&);
        void Stop ();
        bool IsRunning () const;
        unsigned short LocalPort () const;
    };

    // The tunnel of the main (interactive) connection. A new tunnel does not close the previous one at once,
    // because the old connection may still be open; the previous ones are released when the new connect task is done.
    unsigned short StartMainTunnel (const Config&, const ConfirmHostKeyFn&);
    // brings the main tunnel back (same local port) if the SSH connection was lost; no-op if it is alive or there is none.
    // Only already trusted host keys are accepted, throws Ssh::Error.
    void EnsureMainTunnel ();
    // if host:port is the local end of the main tunnel, replaces host by the real database host (for display)
    bool MapDisplayHost (std::string& host, const std::string& port);
    void ReleaseRetiredTunnels ();
    void StopAllTunnels ();
}
