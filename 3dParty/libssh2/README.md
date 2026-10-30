libssh2 1.11.1 + OpenSSL 3.6.5 (static, x64, /MT), built with vcpkg triplet x64-windows-static
(`vcpkg install libssh2[openssl]:x64-windows-static`). Used by SQLTools\Ssh\SshTunnel.cpp.
Licenses: licenses\libssh2-LICENSE.txt (BSD-3-Clause) and licenses\openssl-LICENSE.txt (Apache-2.0).

Local patch to libssh2 1.11.1 (src/userauth.c, `_libssh2_key_sign_algorithm`): bounds check when building the filtered signature algorithm list.
Without it, an RSA key login against a server whose `server-sig-algs` list has repeated entries writes past the end of a heap buffer.
