# Third-party notices

SQLTools itself is licensed under the GNU GPL v3 or later (see `LICENSE`).

## Oracle Instant Client 19c Basic Light (embedded)

The standalone `SQLTools-v<version>-x64.exe` embeds the unmodified Oracle Instant
Client 19.32 Basic Light DLLs (`3dParty/oracle/instantclient19/oci19.bin`).
These DLLs are **not** covered by the GPL; they remain property of Oracle and are
used under the Oracle Free Use Terms and Conditions
(`3dParty/oracle/instantclient19/BASIC_LITE_LICENSE`), which require, among other things:

- redistribute the Programs unmodified, at no additional charge to end users;
- include a copy of that license with every distribution;
- do not remove Oracle's proprietary notices;
- no reverse engineering, and compliance with export-control laws.

Other bundled libraries (Boost, TinyXML, TextEncodingDetect, etc.) keep their own
licenses in their folders under `3dParty/`.

## Flag icons (What's New window)

The US and Brazil flag images in `SQLTools/res/flags/` are used in the What's New window. They come
from the flag-icons project (MIT License, Copyright (c) 2013 Panayiotis Lipiridis); the license text
is in `SQLTools/res/flags/LICENSE-flag-icons.txt`.

## libssh2 and OpenSSL (SSH tunnel, statically linked)

The SSH tunnel feature (connecting to databases through an SSH server) uses:

- **libssh2** 1.11.1 - BSD 3-Clause license, Copyright (c) the libssh2 project and contributors.
  Full text: `3dParty/libssh2/licenses/libssh2-LICENSE.txt`.
- **OpenSSL** 3.x - Apache License 2.0, Copyright (c) The OpenSSL Project Authors.
  Full text: `3dParty/libssh2/licenses/openssl-LICENSE.txt`.

Both are compatible with the GNU GPL v3 used by SQLTools. They are built from the official sources with vcpkg
(triplet `x64-windows-static`) and linked statically into `SQLTools.exe`.
libssh2 carries one local bug-fix patch (a heap buffer bounds check in `userauth.c`), described in `3dParty/libssh2/README.md`.
