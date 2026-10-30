SQLTools Changelog
==================


3.6.0
-----
Date: 2026-10-30

  New features
    * Value panel: new search bar, opened with Ctrl+F (or "Find..." in the
      right-click menu), just like the one in the code editor. It shows how many
      matches there are ("3 of 12"), jumps to the next (Enter / F3) or previous
      (Shift+Enter / Shift+F3) one and closes with Esc. Options: match case
      (Alt+C), whole word (Alt+W), regular expression (Alt+R), backslash sequences
      such as \n and \t (Alt+B) and "Mark all" (Alt+M), which highlights every
      match. The options are remembered between sessions and follow the dark theme.


3.5.1
-----
Date: 2026-10-30

  Improvements
    * SSH tunnel: the "Test" button warns when the server only accepts older, less
      secure encryption algorithms.

  Bug fixes
    * SSH tunnel: servers with older SSH software no longer fail with "Unable to
      exchange encryption keys". SQLTools tries the modern algorithms first and,
      only if the server accepts none, tries the older ones automatically. The
      server key is still verified.
    * SSH tunnel: fixed a memory corruption when signing in with an RSA private
      key (a bug in the SSH library, now patched).


3.5.0
-----
Date: 2026-10-29

  New features
    * SSH tunnel: connect to databases that are only reachable through an SSH
      server, with no need for PuTTY. In the Connect window, click "SSH Tunnel..."
      and fill in the SSH host, port and user, then choose a password or a private
      key (OpenSSH format, with an optional passphrase). The "Test" button checks
      the SSH data before connecting.
    * The SSH server key is verified: the fingerprint of an unknown server is shown
      for confirmation, and a changed key aborts the connection. Only modern
      algorithms are accepted, the local port is open to SQLTools only, and the SSH
      password is saved encrypted like the database password.
    * If the SSH connection is lost, Reconnect brings the tunnel back
      automatically.

  Improvements
    * The connection bar shows the real database host followed by (SSH) when a
      tunnel is used.
    * When the connection name does not fit the bar, a tooltip with the whole text
      appears after the mouse rests on it.


3.4.0
-----
Date: 2026-10-28
  * View > Dark Theme now switches the theme instantly, without restarting
    SQLTools. Open windows, toolbars, icons, editor, grids and panels follow.
  * Object Viewer: modern look. New icons, theme font and colors, full-row
    selection and a combo box in the dark theme. Behavior is unchanged.
  * What's New: scroll bar follows the dark theme (also in other multi-line text
    boxes).
  * Copy As > SQL Inserts now generates INSERT INTO owner.table when the result
    comes from a single table (joins, sub-queries and WITH queries still use
    &table).
  * Dragging an object from the Objects List into the editor no longer repeats an
    owner that is already typed: "owner." receives only the name, "owner"
    receives ".name" and, if the owner is different, what was typed is kept and
    owner.name is dropped.
  * The "Error happened while running the script" box now follows the theme.
  * No more white flashes when SQLTools starts (main window background, dialog
    animation and the floating Objects List).
  * What's New: English and Portuguese (Brazil) changelogs, chosen with the flags
    at the top of the window.


3.3.2
-----
Date: 2026-10-27
  * Fixed visual glitches (black areas and leftover arrows) when dragging the
    Value panel splitter.
  * The embedded Oracle Instant Client now ships its license (ORACLE_LICENSE.txt)
    next to the extracted DLLs, as required by the Oracle Free Use Terms.
  * Added LICENSE (GPLv3) and THIRD-PARTY-NOTICES.md to the repository, and the
    modernization contributor to the credits.
  * Value panel: the text zoom (Ctrl + wheel) is kept when moving to another cell,
    until SQLTools is closed.
  * Value panel: new "Format CDATA" option. JSON or XML inside a CDATA section is
    formatted and shown in its own colors.
  * Value panel: Shift + wheel and trackpad horizontal gestures scroll the text
    sideways.


3.3.1
-----
Date: 2026-10-27
  * Favorite owners now work at two levels: global favorites (Settings >
    Favorite Owners) appear on every connection, and the star in the owners
    panel saves favorites per connection (owner-favorites.ini in the
    configuration folder). No owner is a favorite by default.
  * Settings opened from the Objects List now has the modern look and dark theme.
  * Objects List: no more color flash while switching owner, filter boxes follow
    the theme, and Ctrl+Backspace works in the filter boxes.


3.3.0
-----
Date: 2026-10-27
  * New modern look across the whole application: flat toolbars, tabs, status
    bar, panels and grid, with a refreshed set of vector icons.
  * New Objects List (Alt+3): type bar, favorite owners, filters, status badges
    and a configuration button.
  * Redesigned Connect dialog, Settings, DDL preferences and all other dialogs
    and message boxes, with modern checkboxes, buttons, combos and spin buttons.
  * Dark theme: View > Dark Theme (takes effect the next time SQLTools starts).
    It covers the editor, grids, panels, dialogs, menus and icons.
  * Cursor peek: modern look, dark theme, syntax colors while typing and more
    SQL keywords and functions.
  * Editor: continuous change bar in the margin, spaced execution arrow, ruler
    off by default. Value panel and Output icons modernized.
  * The row number column of the grid adjusts its width to the number of rows.


3.2.0
-----
Date: 2026-10-24
  * Release used to validate the update flow (no functional changes).


3.1.0
-----
Date: 2026-10-24
  * Oracle Instant Client 19c is now embedded in SQLTools.exe: no Oracle Client
    installation is required (an OCI.DLL already available on the machine is
    still preferred).
  * New Help > Check for Updates and Help > What's New. SQLTools tells you when a
    new minor version is available. It never updates itself: you decide.
  * DDL options: "Prefix with Schema Name" is checked by default.


3.0.2
-----
Date: 2026-09-10
  * Cursor peek: redesigned with vector icons, smarter sizing, back navigation
    and a syntax fix; copy the selection or the whole content with a button,
    shortcuts and the context menu.
  * Trackpad gesture support in the editor and a redesigned Value panel toolbar.
  * The window title now shows the SQLTools version.


3.0.1
-----
Date: 2026-09-09
  * New collapsible Value side panel with automatic BLOB/hex decoding and
    syntax highlighting for XML, JSON, text and hex dump.
  * Isolated Ctrl+A and Ctrl+C in the panel and selective text copy.
  * Floating PL/SQL cursor peek popup with live editing of the original
    declaration.
  * Visual scope guide lines for IF/THEN/END IF and FOR loops.
  * Ctrl + mouse wheel zooms the editor font.
  * Quick owner switcher buttons (DBAMV and MVINTEGRA) in the object list.
  * Horizontal scrolling in the grid with Shift + wheel and trackpad.
  * UTF-8 characters support.
  * The scroll position is kept when sorting columns in the DB Browser.
