#include "stdafx.h"
#include "SqlFormatterHandler.h"
#include "SQLWorksheet/SQLWorksheetDoc.h"
#include "OpenEditor/OEView.h"
#include "SQLTools.h"

bool SqlFormatterHandler::Execute(const std::wstring& payload, std::wstring& out)
{
    out.clear();

    // Parse optional first-up-to-3 parameter lines from the wide payload.
    // Recognised keys (case-sensitive):
    //  .FmtBrickListEnabled=1
    //  .FmtBrickListMaxLineLen=80
    //  .FmtBrickListMaxLineItems=10
    //  .FmtBrickListMinItems=3
    //  .FmtBrickListLinkedInsert=1
    //  .FmtBrickListLinkedSetOp=1
    // Unrecognised or non-'.' starting line stops the param scan.
    int fmtBrickListEnabled      = -1;
    int fmtBrickListMaxLineLen    = -1;
    int fmtBrickListMaxLineItems  = -1;
    int fmtBrickListMinItems      = -1;
    int fmtBrickListLinkedInsert  = -1;
    int fmtBrickListLinkedSetOp   = -1;
    size_t sqlStartPos = 0; // index in payload where SQL text starts
    {
        size_t pos = 0;
        int parsed = 0;
        while (parsed < 3 && pos < payload.size())
        {
            // find end of current line (include only '\n' as delimiter; handle CR if present)
            size_t nl = payload.find(L'\n', pos);
            size_t lineEnd = (nl == std::wstring::npos) ? payload.size() : nl;
            // extract and trim line
            std::wstring line = payload.substr(pos, lineEnd - pos);
            // trim leading spaces
            size_t a = 0;
            while (a < line.size() && iswspace(line[a])) ++a;
            size_t b = line.size();
            while (b > a && iswspace(line[b-1])) --b;
            if (a >= b)
            {
                // empty line — treat as end of param block
                sqlStartPos = pos;
                break;
            }
            std::wstring trimmed = line.substr(a, b - a);

            bool consumed = false;
            if (!trimmed.empty() && trimmed[0] == L'.')
            {
                // parse key=value
                size_t eq = trimmed.find(L'=');
                if (eq != std::wstring::npos && eq+1 < trimmed.size())
                {
                    std::wstring key = trimmed.substr(0, eq);
                    std::wstring val = trimmed.substr(eq+1);
                    // simple integer parse
                    try {
                        int ival = std::stoi(Common::str(val));
                        if (key == L".FmtBrickListEnabled") { fmtBrickListEnabled = ival; consumed = true; }
                        else if (key == L".FmtBrickListMaxLineLen") { fmtBrickListMaxLineLen = ival; consumed = true; }
                        else if (key == L".FmtBrickListMaxLineItems") { fmtBrickListMaxLineItems = ival; consumed = true; }
                        else if (key == L".FmtBrickListMinItems") { fmtBrickListMinItems = ival; consumed = true; }
                        else if (key == L".FmtBrickListLinkedInsert") { fmtBrickListLinkedInsert = ival; consumed = true; }
                        else if (key == L".FmtBrickListLinkedSetOp") { fmtBrickListLinkedSetOp = ival; consumed = true; }
                    }
                    catch (...) { /* ignore parse errors */ }
                }
            }

            if (!consumed)
            {
                // this line is not a parameter — SQL starts here
                sqlStartPos = pos;
                break;
            }

            // consumed this parameter line — move to next
            ++parsed;
            if (nl == std::wstring::npos)
            {
                // no more lines
                sqlStartPos = payload.size();
                break;
            }
            pos = nl + 1;
            // skip a single trailing CR if present at start of next line (handled by trim)
            sqlStartPos = pos;
        }
    }

    // Create a new worksheet document to avoid touching the active one
    CDocTemplate* pDocTemplate = theApp.GetPLSDocTemplate();
    if (!pDocTemplate)
        return false;

    CPLSWorksheetDoc* pDoc = (CPLSWorksheetDoc*)pDocTemplate->OpenDocumentFile(NULL);
    if (!pDoc)
        return false;

    // Use only the SQL part after any parsed header parameters
    std::string text = Common::str(payload.substr(sqlStartPos));
    pDoc->DefaultFileFormat();
    pDoc->SetText(text.c_str(), text.length());

    COEditorView* pEditor = pDoc->GetFirstEditorView();
    if (pEditor)
    {
        pEditor->SelectAll();

        const OpenEditor::GlobalSettingsPtr s = COEDocument::GetSettingsManager().GetGlobalSettings();
        auto orgFmtBrickListEnabled      = s->GetFmtBrickListEnabled();
        auto orgFmtBrickListLinkedInsert = s->GetFmtBrickListLinkedInsert();
        auto orgFmtBrickListLinkedSetOp  = s->GetFmtBrickListLinkedSetOp();
        try
        {
            if (fmtBrickListEnabled != -1)
                s->SetFmtBrickListEnabled(fmtBrickListEnabled ? true : false);
            if (fmtBrickListLinkedInsert != -1)
                s->SetFmtBrickListLinkedInsert(fmtBrickListLinkedInsert ? true : false);
            if (fmtBrickListLinkedSetOp != -1)
                s->SetFmtBrickListLinkedSetOp(fmtBrickListLinkedSetOp ? true : false);

            pEditor->OnEditFormatSql();

            if (fmtBrickListEnabled != -1)
                s->SetFmtBrickListEnabled(orgFmtBrickListEnabled);
            if (fmtBrickListLinkedInsert != -1)
                s->SetFmtBrickListLinkedInsert(orgFmtBrickListLinkedInsert);
            if (fmtBrickListLinkedSetOp != -1)
                s->SetFmtBrickListLinkedSetOp(orgFmtBrickListLinkedSetOp);
        }
        catch (...)
        {
            if (fmtBrickListEnabled != -1)
                s->SetFmtBrickListEnabled(orgFmtBrickListEnabled);
            if (fmtBrickListLinkedInsert != -1)
                s->SetFmtBrickListLinkedInsert(orgFmtBrickListLinkedInsert);
            if (fmtBrickListLinkedSetOp != -1)
                s->SetFmtBrickListLinkedSetOp(orgFmtBrickListLinkedSetOp);
        }

        pEditor->GetBlock(out);
        return true;
    }
    
    return false;
}