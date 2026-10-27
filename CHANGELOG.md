SQLTools Changelog
==================


3.3.0
-----
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
  * Release used to validate the update flow (no functional changes).


3.1.0
-----
  * Oracle Instant Client 19c is now embedded in SQLTools.exe: no Oracle Client
    installation is required (an OCI.DLL already available on the machine is
    still preferred).
  * New Help > Check for Updates and Help > What's New. SQLTools tells you when a
    new minor version is available. It never updates itself: you decide.
  * DDL options: "Prefix with Schema Name" is checked by default.


3.0.2
-----
  * Cursor peek: redesigned with vector icons, smarter sizing, back navigation
    and a syntax fix; copy the selection or the whole content with a button,
    shortcuts and the context menu.
  * Trackpad gesture support in the editor and a redesigned Value panel toolbar.
  * The window title now shows the SQLTools version.


3.0.1
-----
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
