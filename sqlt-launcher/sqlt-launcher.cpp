// sqlt-launcher.cpp : Defines the entry point for the application.
//
#include "stdafx.h"

#pragma comment(lib, "comctl32.lib") // For TaskDialog

#if defined _M_IX86
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

#include <Commctrl.h>
#include <Commdlg.h>
#include <memory>
#include <string>
#include <vector>
#include <shlobj.h>
#include "shlwapi.h"
#include "resource.h"
#include "tinyxml\tinyxml.h"

using namespace std;

// Helper structure to hold program data
struct ProgramData 
{
    string id;
    string name;
    string executable;
    string oracle_bin_path;
    string oracle_home;
    string tns_admin;
    string nls_lang;
    TiXmlElement* xmlElement;  // Pointer to the XML element
};

// Global data for editor
struct EditorData 
{
    TiXmlDocument* doc;
    char* config_path;
    vector<ProgramData> programs;
    bool modified;
    string default_id;
};

// Forward declarations
INT_PTR CALLBACK EditorDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK EditProgramDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

// Helper: Load XML and build program list
bool LoadConfigurationFile(TiXmlDocument& doc, TiXmlNode*& root, TiXmlElement*& rootE, 
                          TiXmlNode*& first, TiXmlElement*& program,
                          int& default_idx, const char*& default_id, bool& detailed_labels,
                          vector<basic_string<wchar_t>>& program_list)
{
    root = doc.FirstChild("programs");
    if (!root)
    {
        MessageBox(NULL, "Cannot find root node 'programs'", "sql-launcher", MB_OK|MB_ICONERROR);
        return false;
    }

    rootE = root->ToElement();

    default_idx = -1;
    default_id = rootE->Attribute("default");
    detailed_labels = false;
    if (const char* detailed_labels_str = rootE->Attribute("detailed_labels"))
        detailed_labels = !strcmp(detailed_labels_str, "1") ? true : false;

    first = root->FirstChild("program");
    program = first ? first->ToElement() : 0;
    if (!first || !program)
    {
        MessageBox(NULL, "Cannot find node 'programs\\program'", "sql-launcher", MB_OK|MB_ICONERROR);
        return false;
    }

    typedef basic_string<wchar_t> string_w;
    program_list.clear();
    for (int i = 0; program; ++i, program = program->NextSiblingElement("program"))
    {
        string buffer = "Unnamed";

        if (const char* name = program->Attribute("name"))
            buffer = name;

        if (const char* id = program->Attribute("id"))
            if (default_idx == -1 && !strcmp(default_id, id))
                default_idx = i;

        if (detailed_labels)
        {
            if (const char* oracle_home = program->Attribute("oracle_home"))
            {
                buffer += "\nORACLE_HOME=";
                buffer += oracle_home;
            }

            if (const char* tns_admin = program->Attribute("tns_admin"))
            {
                buffer += "\nTNS_ADMIN=";
                buffer += tns_admin;
            }

            if (const char* oracle_bin_path = program->Attribute("oracle_bin_path"))
            {
                buffer += "\nORACLE_BIN_PATH=";
                buffer += oracle_bin_path;
            }
        }

        WCHAR buff[128];
        size_t buff_size = sizeof(buff)/sizeof(buff[0]);
        mbstowcs_s(&buff_size, buff, buffer.c_str(), _TRUNCATE);
        program_list.push_back(buff);
    }

    if (program_list.empty())
    {
        MessageBox(NULL, "Configuration list is empty!", "sql-launcher", MB_OK|MB_ICONERROR);
        return false;
    }

    return true;
}

// Helper: Browse for folder using IFileDialog (Vista+) or fallback to SHBrowseForFolder
bool BrowseForFolder(HWND hwnd, char* path, int path_size)
{
    bool result = false;
    char initialPath[MAX_PATH];
    strncpy_s(initialPath, path, MAX_PATH);

    // Trim trailing backslash if present (except for root paths like "C:\")
    if (initialPath[0] != '\0')
    {
        PathRemoveBackslash(initialPath);
    }

    // Try modern IFileDialog first (Vista+)
    CoInitialize(NULL);

    IFileDialog *pfd = NULL;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, 
                                   IID_PPV_ARGS(&pfd));

    if (SUCCEEDED(hr))
    {
        DWORD dwOptions;
        hr = pfd->GetOptions(&dwOptions);
        if (SUCCEEDED(hr))
        {
            pfd->SetOptions(dwOptions | FOS_PICKFOLDERS);

            // Set initial folder if path exists
            if (initialPath[0] != '\0' && PathFileExists(initialPath))
            {
                WCHAR wInitialPath[MAX_PATH];
                MultiByteToWideChar(CP_ACP, 0, initialPath, -1, wInitialPath, MAX_PATH);

                IShellItem *psiFolder = NULL;
                hr = SHCreateItemFromParsingName(wInitialPath, NULL, IID_PPV_ARGS(&psiFolder));
                if (SUCCEEDED(hr))
                {
                    pfd->SetFolder(psiFolder);
                    psiFolder->Release();
                }
            }

            hr = pfd->Show(hwnd);

            if (SUCCEEDED(hr))
            {
                IShellItem *psi;
                hr = pfd->GetResult(&psi);
                if (SUCCEEDED(hr))
                {
                    PWSTR pszPath = NULL;
                    hr = psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
                    if (SUCCEEDED(hr))
                    {
                        WideCharToMultiByte(CP_ACP, 0, pszPath, -1, path, path_size, NULL, NULL);
                        CoTaskMemFree(pszPath);
                        result = true;
                    }
                    psi->Release();
                }
            }
        }
        pfd->Release();
    }
    else
    {
        // Fallback to old SHBrowseForFolder
        BROWSEINFO bi = { 0 };
        bi.hwndOwner = hwnd;
        bi.lpszTitle = "Select Folder";
        bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

        LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
        if (pidl != NULL)
        {
            if (SHGetPathFromIDList(pidl, path))
                result = true;
            CoTaskMemFree(pidl);
        }
    }

    CoUninitialize();
    return result;
}

// Helper: Browse for executable file
bool BrowseForExecutable(HWND hwnd, char* path, int path_size)
{
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(ofn));

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = path;
    ofn.nMaxFile = path_size;
    ofn.lpstrFilter = "Executables\0*.exe\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = "Select Executable";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    // Set initial directory from the path if it exists
    char initialDir[MAX_PATH] = {0};
    if (path[0] != '\0')
    {
        strncpy_s(initialDir, path, MAX_PATH);
        PathRemoveFileSpec(initialDir);
        if (initialDir[0] != '\0' && PathFileExists(initialDir))
            ofn.lpstrInitialDir = initialDir;
    }

    return GetOpenFileName(&ofn) == TRUE;
}

// Load programs from XML into editor data
void LoadProgramsFromXML(EditorData* pData)
{
    pData->programs.clear();

    TiXmlNode* root = pData->doc->FirstChild("programs");
    if (!root) return;

    TiXmlElement* rootE = root->ToElement();
    const char* default_id = rootE->Attribute("default");
    if (default_id)
        pData->default_id = default_id;

    TiXmlElement* program = root->FirstChild("program") ? root->FirstChild("program")->ToElement() : NULL;

    while (program)
    {
        ProgramData pd;
        pd.xmlElement = program;

        if (const char* id = program->Attribute("id")) pd.id = id;
        if (const char* name = program->Attribute("name")) pd.name = name;
        if (const char* executable = program->Attribute("executable")) pd.executable = executable;
        if (const char* oracle_bin_path = program->Attribute("oracle_bin_path")) pd.oracle_bin_path = oracle_bin_path;
        if (const char* oracle_home = program->Attribute("oracle_home")) pd.oracle_home = oracle_home;
        if (const char* tns_admin = program->Attribute("tns_admin")) pd.tns_admin = tns_admin;
        if (const char* nls_lang = program->Attribute("nls_lang")) pd.nls_lang = nls_lang;

        pData->programs.push_back(pd);
        program = program->NextSiblingElement("program");
    }
}

// Save programs from editor data back to XML
void SaveProgramsToXML(EditorData* pData)
{
    TiXmlNode* root = pData->doc->FirstChild("programs");
    if (!root) return;

    TiXmlElement* rootE = root->ToElement();
    rootE->SetAttribute("default", pData->default_id.c_str());

    // Create backup once before first modification
    static bool backupCreated = false;
    if (!backupCreated)
    {
        char backup_path[MAX_PATH];
        _snprintf_s(backup_path, sizeof(backup_path), "%s.bak", pData->config_path);
        CopyFile(pData->config_path, backup_path, FALSE);
        backupCreated = true;
    }

    if (pData->doc->SaveFile(pData->config_path))
        pData->modified = false;
}

// Edit Program Dialog Procedure
INT_PTR CALLBACK EditProgramDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    static ProgramData* pProgram = NULL;

    switch (message)
    {
    case WM_INITDIALOG:
        {
            pProgram = (ProgramData*)lParam;
            SetDlgItemText(hDlg, IDC_EDIT_ID, pProgram->id.c_str());
            SetDlgItemText(hDlg, IDC_EDIT_NAME, pProgram->name.c_str());
            SetDlgItemText(hDlg, IDC_EDIT_EXECUTABLE, pProgram->executable.c_str());
            SetDlgItemText(hDlg, IDC_EDIT_ORACLE_BIN, pProgram->oracle_bin_path.c_str());
            SetDlgItemText(hDlg, IDC_EDIT_ORACLE_HOME, pProgram->oracle_home.c_str());
            SetDlgItemText(hDlg, IDC_EDIT_TNS_ADMIN, pProgram->tns_admin.c_str());
            SetDlgItemText(hDlg, IDC_EDIT_NLS_LANG, pProgram->nls_lang.c_str());

            // Set focus on Name field
            SetFocus(GetDlgItem(hDlg, IDC_EDIT_NAME));
            return FALSE; // We set focus manually
        }

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_BTN_BROWSE_EXEC:
            {
                char path[MAX_PATH] = {0};
                GetDlgItemText(hDlg, IDC_EDIT_EXECUTABLE, path, MAX_PATH);
                if (BrowseForExecutable(hDlg, path, MAX_PATH))
                    SetDlgItemText(hDlg, IDC_EDIT_EXECUTABLE, path);
                return TRUE;
            }

        case IDC_BTN_BROWSE_BIN:
            {
                char path[MAX_PATH] = {0};
                GetDlgItemText(hDlg, IDC_EDIT_ORACLE_BIN, path, MAX_PATH);
                if (BrowseForFolder(hDlg, path, MAX_PATH))
                    SetDlgItemText(hDlg, IDC_EDIT_ORACLE_BIN, path);
                return TRUE;
            }

        case IDC_BTN_BROWSE_HOME:
            {
                char path[MAX_PATH] = {0};
                GetDlgItemText(hDlg, IDC_EDIT_ORACLE_HOME, path, MAX_PATH);
                if (BrowseForFolder(hDlg, path, MAX_PATH))
                    SetDlgItemText(hDlg, IDC_EDIT_ORACLE_HOME, path);
                return TRUE;
            }

        case IDC_BTN_BROWSE_TNS:
            {
                char path[MAX_PATH] = {0};
                GetDlgItemText(hDlg, IDC_EDIT_TNS_ADMIN, path, MAX_PATH);
                if (BrowseForFolder(hDlg, path, MAX_PATH))
                    SetDlgItemText(hDlg, IDC_EDIT_TNS_ADMIN, path);
                return TRUE;
            }

        case IDOK:
            {
                char buffer[1024];
                GetDlgItemText(hDlg, IDC_EDIT_ID, buffer, sizeof(buffer));
                pProgram->id = buffer;
                GetDlgItemText(hDlg, IDC_EDIT_NAME, buffer, sizeof(buffer));
                pProgram->name = buffer;
                GetDlgItemText(hDlg, IDC_EDIT_EXECUTABLE, buffer, sizeof(buffer));
                pProgram->executable = buffer;
                GetDlgItemText(hDlg, IDC_EDIT_ORACLE_BIN, buffer, sizeof(buffer));
                pProgram->oracle_bin_path = buffer;
                GetDlgItemText(hDlg, IDC_EDIT_ORACLE_HOME, buffer, sizeof(buffer));
                pProgram->oracle_home = buffer;
                GetDlgItemText(hDlg, IDC_EDIT_TNS_ADMIN, buffer, sizeof(buffer));
                pProgram->tns_admin = buffer;
                GetDlgItemText(hDlg, IDC_EDIT_NLS_LANG, buffer, sizeof(buffer));
                pProgram->nls_lang = buffer;

                // Update XML element - remove attribute if empty
                pProgram->xmlElement->SetAttribute("id", pProgram->id.c_str());
                pProgram->xmlElement->SetAttribute("name", pProgram->name.c_str());
                pProgram->xmlElement->SetAttribute("executable", pProgram->executable.c_str());

                if (!pProgram->oracle_bin_path.empty())
                    pProgram->xmlElement->SetAttribute("oracle_bin_path", pProgram->oracle_bin_path.c_str());
                else
                    pProgram->xmlElement->RemoveAttribute("oracle_bin_path");

                if (!pProgram->oracle_home.empty())
                    pProgram->xmlElement->SetAttribute("oracle_home", pProgram->oracle_home.c_str());
                else
                    pProgram->xmlElement->RemoveAttribute("oracle_home");

                if (!pProgram->tns_admin.empty())
                    pProgram->xmlElement->SetAttribute("tns_admin", pProgram->tns_admin.c_str());
                else
                    pProgram->xmlElement->RemoveAttribute("tns_admin");

                if (!pProgram->nls_lang.empty())
                    pProgram->xmlElement->SetAttribute("nls_lang", pProgram->nls_lang.c_str());
                else
                    pProgram->xmlElement->RemoveAttribute("nls_lang");

                EndDialog(hDlg, IDOK);
                return TRUE;
            }

        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }

    return FALSE;
}

// Editor Dialog Procedure
INT_PTR CALLBACK EditorDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    static EditorData* pData = NULL;
    static HWND hListView = NULL;

    switch (message)
    {
    case WM_INITDIALOG:
        {
            pData = (EditorData*)lParam;
            hListView = GetDlgItem(hDlg, IDC_PROGRAM_LIST);

            // Set up ListView
            DWORD exStyle = LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES;
            SendMessage(hListView, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, exStyle);

            // Add columns
            LVCOLUMN lvc;
            lvc.mask = LVCF_TEXT | LVCF_WIDTH;
            lvc.pszText = "ID";
            lvc.cx = 30;
            SendMessage(hListView, LVM_INSERTCOLUMN, 0, (LPARAM)&lvc);

            lvc.pszText = "Name";
            lvc.cx = 150;
            SendMessage(hListView, LVM_INSERTCOLUMN, 1, (LPARAM)&lvc);

            lvc.pszText = "Executable";
            lvc.cx = 200;
            SendMessage(hListView, LVM_INSERTCOLUMN, 2, (LPARAM)&lvc);

            lvc.pszText = "Oracle BIN Path";
            lvc.cx = 200;
            SendMessage(hListView, LVM_INSERTCOLUMN, 3, (LPARAM)&lvc);

            lvc.pszText = "Oracle HOME";
            lvc.cx = 200;
            SendMessage(hListView, LVM_INSERTCOLUMN, 4, (LPARAM)&lvc);

            // Populate list
            for (size_t i = 0; i < pData->programs.size(); ++i)
            {
                LVITEM lvi;
                lvi.mask = LVIF_TEXT | LVIF_PARAM;
                lvi.iItem = i;
                lvi.iSubItem = 0;
                lvi.lParam = (LPARAM)i;
                lvi.pszText = (char*)pData->programs[i].id.c_str();
                int idx = SendMessage(hListView, LVM_INSERTITEM, 0, (LPARAM)&lvi);

                ListView_SetItemText(hListView, idx, 1, (char*)pData->programs[i].name.c_str());
                ListView_SetItemText(hListView, idx, 2, (char*)pData->programs[i].executable.c_str());
                ListView_SetItemText(hListView, idx, 3, (char*)pData->programs[i].oracle_bin_path.c_str());
                ListView_SetItemText(hListView, idx, 4, (char*)pData->programs[i].oracle_home.c_str());
            }

            return TRUE;
        }

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_BTN_ADD:
            {
                ProgramData newProg;

                // Auto-generate ID: max existing + 1, or 1 if empty
                int maxId = 0;
                for (size_t i = 0; i < pData->programs.size(); ++i)
                {
                    int currentId = atoi(pData->programs[i].id.c_str());
                    if (currentId > maxId)
                        maxId = currentId;
                }

                char idBuffer[32];
                _snprintf_s(idBuffer, sizeof(idBuffer), "%d", maxId + 1);
                newProg.id = idBuffer;
                newProg.name = "New Program";

                // Create new XML element
                TiXmlNode* root = pData->doc->FirstChild("programs");
                if (root)
                {
                    TiXmlElement newElem("program");
                    newProg.xmlElement = (TiXmlElement*)root->InsertEndChild(newElem);

                    if (DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_EDIT_PROGRAM), 
                                       hDlg, EditProgramDlgProc, (LPARAM)&newProg) == IDOK)
                    {
                        pData->programs.push_back(newProg);
                        pData->modified = true;

                        // Add to list view
                        LVITEM lvi;
                        lvi.mask = LVIF_TEXT | LVIF_PARAM;
                        lvi.iItem = pData->programs.size() - 1;
                        lvi.iSubItem = 0;
                        lvi.lParam = (LPARAM)(pData->programs.size() - 1);
                        lvi.pszText = (char*)newProg.id.c_str();
                        int idx = SendMessage(hListView, LVM_INSERTITEM, 0, (LPARAM)&lvi);

                        ListView_SetItemText(hListView, idx, 1, (char*)newProg.name.c_str());
                        ListView_SetItemText(hListView, idx, 2, (char*)newProg.executable.c_str());
                        ListView_SetItemText(hListView, idx, 3, (char*)newProg.oracle_bin_path.c_str());
                        ListView_SetItemText(hListView, idx, 4, (char*)newProg.oracle_home.c_str());
                    }
                    else
                    {
                        // User cancelled, remove the XML element
                        root->RemoveChild(newProg.xmlElement);
                    }
                }
                return TRUE;
            }

        case IDC_BTN_EDIT:
            {
                int sel = SendMessage(hListView, LVM_GETNEXTITEM, -1, LVNI_SELECTED);
                if (sel >= 0)
                {
                    LVITEM lvi;
                    lvi.mask = LVIF_PARAM;
                    lvi.iItem = sel;
                    SendMessage(hListView, LVM_GETITEM, 0, (LPARAM)&lvi);
                    int idx = (int)lvi.lParam;

                    if (DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_EDIT_PROGRAM), 
                                       hDlg, EditProgramDlgProc, (LPARAM)&pData->programs[idx]) == IDOK)
                    {
                        pData->modified = true;

                        // Update list view
                        ListView_SetItemText(hListView, sel, 0, (char*)pData->programs[idx].id.c_str());
                        ListView_SetItemText(hListView, sel, 1, (char*)pData->programs[idx].name.c_str());
                        ListView_SetItemText(hListView, sel, 2, (char*)pData->programs[idx].executable.c_str());
                        ListView_SetItemText(hListView, sel, 3, (char*)pData->programs[idx].oracle_bin_path.c_str());
                        ListView_SetItemText(hListView, sel, 4, (char*)pData->programs[idx].oracle_home.c_str());
                    }
                }
                return TRUE;
            }

        case IDC_BTN_DELETE:
            {
                int sel = SendMessage(hListView, LVM_GETNEXTITEM, -1, LVNI_SELECTED);
                if (sel >= 0)
                {
                    if (MessageBox(hDlg, "Are you sure you want to delete this program?", 
                                   "Confirm Delete", MB_YESNO | MB_ICONQUESTION) == IDYES)
                    {
                        LVITEM lvi;
                        lvi.mask = LVIF_PARAM;
                        lvi.iItem = sel;
                        SendMessage(hListView, LVM_GETITEM, 0, (LPARAM)&lvi);
                        int idx = (int)lvi.lParam;

                        // Remove from XML
                        TiXmlNode* root = pData->doc->FirstChild("programs");
                        if (root)
                        {
                            root->RemoveChild(pData->programs[idx].xmlElement);
                            pData->programs.erase(pData->programs.begin() + idx);
                            pData->modified = true;

                            // Remove from list view
                            SendMessage(hListView, LVM_DELETEITEM, sel, 0);
                        }
                    }
                }
                return TRUE;
            }

        case IDC_BTN_SET_DEFAULT:
            {
                int sel = SendMessage(hListView, LVM_GETNEXTITEM, -1, LVNI_SELECTED);
                if (sel >= 0)
                {
                    LVITEM lvi;
                    lvi.mask = LVIF_PARAM;
                    lvi.iItem = sel;
                    SendMessage(hListView, LVM_GETITEM, 0, (LPARAM)&lvi);
                    int idx = (int)lvi.lParam;

                    // Build confirmation message with program name
                    char message[512];
                    _snprintf_s(message, sizeof(message), 
                        "Set '%s' as default?\n\n"
                        "The launcher will skip the selection UI and automatically run this program.\n\n"
                        "To reset: edit sqlt-launcher.xml and set the 'default' attribute to '0'.",
                        pData->programs[idx].name.c_str());

                    if (MessageBox(hDlg, message, "Confirm Set Default", MB_YESNO | MB_ICONWARNING) == IDYES)
                    {
                        pData->default_id = pData->programs[idx].id;
                        pData->modified = true;
                        MessageBox(hDlg, "Default configuration set successfully.", "Success", MB_OK | MB_ICONINFORMATION);
                    }
                }
                return TRUE;
            }

        case IDOK:
        case IDCANCEL:
            if (pData->modified)
            {
                if (MessageBox(hDlg, "Save changes to configuration file?", 
                               "Save Changes", MB_YESNO | MB_ICONQUESTION) == IDYES)
                {
                    SaveProgramsToXML(pData);
                }
            }
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        break;

    case WM_NOTIFY:
        {
            LPNMHDR pnmh = (LPNMHDR)lParam;
            if (pnmh->idFrom == IDC_PROGRAM_LIST && pnmh->code == NM_DBLCLK)
            {
                // Double-click to edit
                SendMessage(hDlg, WM_COMMAND, MAKEWPARAM(IDC_BTN_EDIT, 0), 0);
                return TRUE;
            }
        }
        break;
    }

    return FALSE;
}

int APIENTRY WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
	const char* config = "sqlt-launcher.xml";

	bool found = false;
	char full_path[MAX_PATH];
	{
		char curr_dir[MAX_PATH];

		if (GetCurrentDirectory(sizeof(curr_dir)-1, curr_dir))
		{

			::PathCombine(full_path, curr_dir, config);

			if (::PathFileExists(full_path))
				found = true;
		}
	}

	if (!found)
	{
		char program_folder[_MAX_PATH];
		if (::GetModuleFileName(hInstance, program_folder, _MAX_PATH))
		{
			::PathRemoveFileSpec(program_folder);

			::PathCombine(full_path, program_folder, config);

			if (::PathFileExists(full_path))
				found = true;
		}
	}

	if (!found)
	{
		MessageBox(NULL, "Config file not found!", "sql-launcher", MB_OK|MB_ICONERROR);
		return -1;
	}

	TiXmlDocument doc(full_path);

	if (!doc.LoadFile())
	{
		char message[1024];
		_snprintf_s(message, sizeof(message), "Could not config file. Error='%s'.", doc.ErrorDesc());
		MessageBox(NULL, message, "sql-launcher", MB_OK|MB_ICONERROR);
		return -1;
	}

	TiXmlNode* root = NULL;
	TiXmlElement* rootE = NULL;
	TiXmlNode* first = NULL;
	TiXmlElement* program = NULL;
	int default_idx = -1;
	const char* default_id = NULL;
	bool detailed_labels = false;
	typedef basic_string<wchar_t> string_w;
	vector<string_w> program_list;

	// Load configuration using helper function
	if (!LoadConfigurationFile(doc, root, rootE, first, program, 
							   default_idx, default_id, detailed_labels, program_list))
		return -1;

    // Main loop: show TaskDialog until user selects a program or cancels
    while (default_idx == -1)
    {
        // Build button array with Edit button at the end
        std::unique_ptr<TASKDIALOG_BUTTON> buttons(new TASKDIALOG_BUTTON[program_list.size() + 1]);

        vector<string_w>::const_iterator it = program_list.begin();
        for (int i = 0; it != program_list.end(); ++it, ++i)
        {
            buttons.get()[i].nButtonID = 1000 + i;
            buttons.get()[i].pszButtonText = it->c_str();
        }

        // Add "Edit Configurations..." button at the end
        buttons.get()[program_list.size()].nButtonID = 2000;
        buttons.get()[program_list.size()].pszButtonText = L"Edit Configurations...";

        HRESULT hr;
        TASKDIALOGCONFIG tdc = { sizeof(TASKDIALOGCONFIG) };
        int clicked_btn = -1;

        tdc.hwndParent = NULL;
        tdc.dwFlags = TDF_USE_COMMAND_LINKS|TDF_USE_HICON_MAIN|TDF_ALLOW_DIALOG_CANCELLATION;
        tdc.pButtons = buttons.get();
        tdc.cButtons = program_list.size() + 1;
        tdc.pszWindowTitle = L"SQLTools Launcher";
        tdc.hMainIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MAIN));
        tdc.pszMainInstruction = L"Select the configuration to execute:";
        tdc.pszFooter = L"Configurations are stored in sqlt-launcher.xml";

        hr = TaskDialogIndirect(&tdc, &clicked_btn, NULL, NULL);

        if (clicked_btn == 2000)
        {
            // Open editor
            EditorData edData;
            edData.doc = &doc;
            edData.config_path = full_path;
            edData.modified = false;
            LoadProgramsFromXML(&edData);

            DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_EDITOR), NULL, EditorDlgProc, (LPARAM)&edData);

            // Reload configuration
            if (!doc.LoadFile())
            {
                MessageBox(NULL, "Failed to reload configuration!", "sql-launcher", MB_OK|MB_ICONERROR);
                return -1;
            }

            // Reload using helper function
            if (!LoadConfigurationFile(doc, root, rootE, first, program, 
                                       default_idx, default_id, detailed_labels, program_list))
                return -1;

            // Continue loop to show dialog again
        }
        else if (clicked_btn >= 1000 && clicked_btn < 1000 + (int)program_list.size())
        {
            default_idx = clicked_btn - 1000;
        }
        else
        {
            // User cancelled
            return 0;
        }
    }
    
    program = first ? first->ToElement() : 0;
    for (int i = 0; program; ++i, program = program->NextSiblingElement("program"))
    {
        if (i == default_idx)
        {
            const char* name         = program->Attribute("name");
            const char* executable   = program->Attribute("executable");
            const char* oracle_bin   = program->Attribute("oracle_bin_path");
            const char* oracle_home  = program->Attribute("oracle_home");
            const char* tns_admin    = program->Attribute("tns_admin");
            const char* nls_lang     = program->Attribute("nls_lang");

            if (!executable)
            {
                char message[1024];
		        _snprintf_s(message, sizeof(message), "programs\\program with name='%s' does not have the required 'path' attribute!", (name ? name : ""));
                MessageBox(NULL, message, "sql-launcher", MB_OK|MB_ICONERROR);
                return -1;
            }

            LPTCH env = 0;

            if (oracle_home || oracle_bin || tns_admin || nls_lang)
            {
                if (oracle_home)
                    SetEnvironmentVariable("ORACLE_HOME", oracle_home);
  
                if (oracle_bin)
                {
                    char org_path[32767]; 
                    
                    string new_path(oracle_bin);
                    new_path += ';';
                    if (GetEnvironmentVariable("PATH", org_path, sizeof(org_path))) 
                        new_path += org_path;

                    SetEnvironmentVariable("PATH", new_path.c_str());
                }

                if (tns_admin)
                    SetEnvironmentVariable("TNS_ADMIN", tns_admin);

                if (nls_lang)
                    SetEnvironmentVariable("NLS_LANG", nls_lang);

                env = GetEnvironmentStrings();
            }

            STARTUPINFO startInfo;
            memset(&startInfo, 0, sizeof startInfo);
            startInfo.cb = sizeof(startInfo);
            startInfo.dwFlags = STARTF_USESHOWWINDOW;
            startInfo.wShowWindow = SW_SHOWNORMAL;

            PROCESS_INFORMATION procInfo;
            memset(&procInfo, 0, sizeof procInfo);

            char _executable[32768];
            strncpy_s(_executable, executable, sizeof(_executable));

            BOOL status = CreateProcess(
                0,                      //_In_opt_     LPCTSTR lpApplicationName,
                _executable,            //_Inout_opt_  LPTSTR lpCommandLine,
                0,                      //_In_opt_     LPSECURITY_ATTRIBUTES lpProcessAttributes,
                0,                      //_In_opt_     LPSECURITY_ATTRIBUTES lpThreadAttributes,
                FALSE,                  //_In_         BOOL bInheritHandles,
                0,                      //_In_         DWORD dwCreationFlags,
                env,                    //_In_opt_     LPVOID lpEnvironment,
                0,                      //_In_opt_     LPCTSTR lpCurrentDirectory,
                &startInfo,             //_In_         LPSTARTUPINFO lpStartupInfo,
                &procInfo               //_Out_        LPPROCESS_INFORMATION lpProcessInformation
            );

            if (!status)
            {
                MessageBox(NULL, "Failed to launch!", "sql-launcher", MB_OK|MB_ICONERROR);
                return -1;
            }

            // Allow the new process to set foreground window
            AllowSetForegroundWindow(procInfo.dwProcessId);

            // Wait briefly for the process to create its window
            WaitForInputIdle(procInfo.hProcess, 2000);

            // Find and bring the main window to foreground
            DWORD processId = procInfo.dwProcessId;
            HWND hwnd = NULL;

            // Try to find the main window
            for (int i = 0; i < 20 && hwnd == NULL; i++)
            {
                EnumWindows([](HWND h, LPARAM lParam) -> BOOL {
                    DWORD pid = 0;
                    GetWindowThreadProcessId(h, &pid);
                    if (pid == (DWORD)lParam && IsWindowVisible(h))
                    {
                        HWND* pHwnd = (HWND*)((LPARAM*)lParam + 1);
                        *pHwnd = h;
                        return FALSE; // Stop enumeration
                    }
                    return TRUE; // Continue enumeration
                }, (LPARAM)processId);

                if (hwnd == NULL)
                    Sleep(50);
            }

            if (hwnd != NULL)
            {
                SetForegroundWindow(hwnd);
                BringWindowToTop(hwnd);
                SetFocus(hwnd);
            }

            // Close process and thread handles
            CloseHandle(procInfo.hThread);
            CloseHandle(procInfo.hProcess);

            return 1;
        }
    }

    MessageBox(NULL, "Failed to find the correct config entry!", "sql-launcher", MB_OK|MB_ICONERROR);
	return -1;
}
