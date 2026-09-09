#include "stdafx.h"
#include "ColumnAutocompleteHandler.h"
#include "SQLWorksheet/SQLWorksheetDoc.h"
#include "OpenEditor/OEDocument.h"
#include "OpenEditor/OEView.h"
#include <atlbase.h>
#include <SessionCache.h>
#include "SQLTools.h"
#include <SQLUtilities.h>
#include <SQLWorksheet/AutocompleteTemplate.h>


// For now, stub: echo CSV or trigger minimal logic.
bool ColumnAutocompleteHandler::Execute(const std::wstring& payload, std::wstring& out)
{
    out.clear();

    // Create a new worksheet document to avoid touching the active one
    CDocTemplate* pDocTemplate = theApp.GetPLSDocTemplate();
    if (!pDocTemplate)
        return false;

    CPLSWorksheetDoc* pDoc = (CPLSWorksheetDoc*)pDocTemplate->OpenDocumentFile(NULL);
    if (!pDoc)
        return false;

    std::string text = Common::str(payload);
    pDoc->DefaultFileFormat();
    pDoc->SetText(text.c_str(), text.length());

    // Ensure the database connection is open before proceeding
    if (!(theApp.GetConnectOpen() && theApp.GetDatabaseOpen()))
    {
        std::wstring base = L"Database connection is not open.";
        // Log error into this document's output and return
        pDoc->PutError(Common::str(base), 0, 0);
        pDoc->ShowOutput(true);
        out += base + L"\n";
        return true;
    }

    COEditorView* pEditor = pDoc->GetFirstEditorView();
    if (pEditor)
    {
        OpenEditor::Position pos = { 0, 0 };
        OpenEditor::Storage& storage = pDoc->GetStorage();
        OpenEditor::Searcher& searcher = storage.GetSearcher();
        searcher.SetOption(false, false, true, true, false);
        searcher.SetText(L"\\.([a-zA-Z_$][a-zA-Z0-9_$]*)");
        OpenEditor::FindCtx ctx;
        ctx.m_storage = &storage;
        ctx.m_line = 0;
        ctx.m_start = 0;
        ctx.m_end = 0;
        ctx.m_thruEof = false;

        while (searcher.Find(ctx) && !ctx.m_eofPassed)
        {
            // Convert inx -> pos for selection and caret
            const int startInx = ctx.m_start + 1; // skip the dot
            const int endInx   = ctx.m_end;
            const int startPos = pEditor->InxToPos(ctx.m_line, startInx);
            const int endPos   = pEditor->InxToPos(ctx.m_line, endInx);

            OpenEditor::Square sel;
            sel.start.line = ctx.m_line;
            sel.start.column = startPos;
            sel.end.line = ctx.m_line;
            sel.end.column = endPos;
            pEditor->SetSelection(sel);
            std::wstring buffer;
            pEditor->GetBlock(buffer, &sel);

            pos.line = ctx.m_line;
            pos.column = startPos;
            pEditor->MoveTo(pos);

            // Prepare location prefix (1-based line:column)
            std::wstring loc = L"[" + std::to_wstring(pos.line + 1) + L":" + std::to_wstring(startPos + 1) + L"] ";

            // use table alias before the dot
            // Provide index where caret is (after dot)
            auto [alias, table, columns, col] = ResolveSqlAlias(pEditor, pos.line, startInx);

            bool found = false;
            std::string columnList, column = Common::str(buffer);

            if (!table.empty() || !columns.empty())
            {
                if (!columns.empty())
                {
                    for (auto& col : columns)
                    {
                        col = SQLUtilities::GetSafeDatabaseObjectName(col);
                        if (col == column)
                            found = true;
                        if (!columnList.empty())
                            columnList += ",";
                        columnList += col;
					}
                }
                else
                {
                    AutocompleteTemplateParams params;

                    if (OpenEditor::TemplatePtr tmpl = SessionCache::GetAutocompleteSubobjectTemplate(table, alias, params))
                    {
                        for (const auto& entry : tmpl->GetEntries())
                        {
                            if (entry.text == column)
                                found = true;
                            if (!columnList.empty())
                                columnList += ",";
                            columnList += entry.text;
                        }
                    }
                }
                // Build a single base message (no loc/no tabs) to use in Output view
                std::wstring head = !table.empty()
                    ? (L"Table alias '" + Common::wstr(alias) + L"' resolved to table '" + Common::wstr(table) + L"'.")
                    : (L"Table alias '" + Common::wstr(alias) + L"' NOT resolved to table.");
                std::wstring tail = found
                    ? (L"Column '" + buffer + L"' found in (" + Common::wstr(columnList) + L")")
                    : (L"Column '" + buffer + L"' NOT found in (" + Common::wstr(columnList) + L")");
                std::wstring base = head + L" " + tail;

                // Return value with location and tabbed column line
                out += loc + head + L"\n";
                out += L"\t" + tail + L"\n";

                if (found)
                {
                    pDoc->PutMessage(Common::str(base), ctx.m_line, startInx);
                }
                else
                {
                    pEditor->SetQueueBookmark(pos.line, true);
                    pDoc->PutError(Common::str(base), ctx.m_line, startInx);
                }
            }
            else
            {
                // Try schema autocomplete: alias may be a schema name rather than a table alias
                OpenEditor::TemplatePtr schemaTmpl = SessionCache::GetAutocompleteTemplateForSchema(alias);
                if (schemaTmpl.get() && schemaTmpl->GetCount() > 0)
                {
                    // alias is a schema; buffer is an object name in that schema
                    for (const auto& entry : schemaTmpl->GetEntries())
                    {
                        if (entry.text == column)
                            found = true;
                        if (!columnList.empty())
                            columnList += ",";
                        columnList += entry.text;
                    }

                    std::wstring head = L"Alias '" + Common::wstr(alias) + L"' resolved to schema.";
                    std::wstring tail = found
                        ? (L"Object '" + buffer + L"' found in schema (" + Common::wstr(columnList) + L")")
                        : (L"Object '" + buffer + L"' NOT found in schema (" + Common::wstr(columnList) + L")");
                    std::wstring base = head + L" " + tail;

                    out += loc + head + L"\n";
                    out += L"\t" + tail + L"\n";

                    if (found)
                    {
                        pDoc->PutMessage(Common::str(base), ctx.m_line, startInx);
                    }
                    else
                    {
                        pEditor->SetQueueBookmark(pos.line, true);
                        pDoc->PutError(Common::str(base), ctx.m_line, startInx);
                    }
                }
                else
                {
                    // Build single base message (no loc/no tabs)
                    std::wstring base = L"Table alias '" + Common::wstr(alias) + L"' NOT resolved to any table.";
                    out += loc + base + L"\n";
                    // Mark this line with a bookmark for quick navigation
                    pEditor->SetQueueBookmark(pos.line, true);
                    // Log as error in Output view
                    pDoc->PutError(Common::str(base), ctx.m_line, startInx);
                }
            }

            ctx.m_start = ctx.m_end;
        }
        pDoc->ShowOutput(true);
    }
    return true;
}
