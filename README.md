# SQLTools 3.0.1 — by Lucas Matheus

A modernized, tuned-up build of **SQLTools**, the lightweight, high-performance client for Oracle databases. This version is focused on making day-to-day PL/SQL work faster: writing Packages, Procedures, Functions, and digging through binary or complex data without fighting the tool.

---

## What's new in 3.0.1

### 1. The "Value" side panel (DBeaver-style)
A dedicated viewer for whatever cell you've got selected in the results grid — no more squinting at truncated text or freezing the UI to see the full content.

- **Smart BLOB/RAW decoding** — hex-encoded data is detected and converted to its real format automatically.
- **Live syntax highlighting**:
  - *XML* — tags, element names, attributes, quoted values, comments, and `<![CDATA[` blocks are all colored.
  - *JSON* — keys, strings, numbers, booleans (`true`/`false`/`null`), and structural delimiters.
  - *Binary (hex dump)* — the classic layout: offsets, hex bytes, and the readable ASCII column.
  - *Text* — normalized line breaks with adjustable encoding (UTF-8 / ANSI).
- **Selection and copy that actually behave**:
  - `Ctrl+C` (or right-click → Copy) copies exactly what you've selected — not the raw binary from the grid.
  - `Ctrl+A` inside the panel selects only the panel's text, not the whole grid.
  - The format toggle button is a simple click-to-open, click-to-close.
  - Export straight to a file (`.xml`, `.json`, `.txt`, `.bin`) with the *Save...* button.

### 2. Better PL/SQL editing
- **Cursor inspection popup** — click a cursor name anywhere in your code and a floating window pops up with its query. There's a pin button to keep it open while you work elsewhere, and edits made in the popup go straight back into the cursor's original declaration.
- **Block scope guide lines** — a visual line connects `IF ... THEN` and `FOR ... LOOP` to their matching `END IF;` / `END LOOP;`, so you can actually see where a block ends without hunting for it.

### 3. General editor and browser improvements
- **Quick zoom** — `Ctrl + Scroll` resizes the editor font on the fly.
- **One-click schema/owner switching** — dedicated buttons in the toolbar and owner selector jump straight to your most-used schemas (e.g. `DBAMV`, `MVINTEGRA`).
- **No more scroll jumping in the DB Browser** — sorting a column in the object list keeps your scroll position steady instead of snapping back to the top.

---

## Download

Grab a ready-to-run build from the **[Releases](../../releases)** page:

- **`SQLTools-v3.0.1-x64.zip`** — full package with the executable and its dependencies.
- **`SQLTools.exe`** — standalone 64-bit executable, no install needed.

---

## Building from source

### Requirements
- Windows 10 / 11 (64-bit)
- Visual Studio 2022 (with C++ Desktop and MFC support), or the Visual Studio Build Tools
- Oracle Client 11g, 12c, 19c, or 21c (64-bit)

### Build via PowerShell
```powershell
& "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe" "SQLTools\SQLTools.sln" /p:Configuration=Release /p:Platform=x64 /m
```

The compiled binary lands here:
```
sqlt.20\_output_\Release.x64\SQLTools.exe
```

---

## License
Licensed under the GNU General Public License v2 (GPL-2.0). See `COPYING` for the full text.