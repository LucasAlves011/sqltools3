/*
	SQLTools is a tool for Oracle database developers and DBAs.
    Copyright (C) 1997-2004 Aleksey Kochetov

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*
    09.02.2004 bug fix, bug-reporting activation for oracle errors on grid fetching
	16.12.2004 (Ken Clubok) Add IsStringField method, to support CSV prefix option
    03.01.2005 (ak) Rename IsStringField to IsTextField
    2017-11-28 implemented range selection
    2017-11-28 added 3 new export formats
*/

#include "stdafx.h"
#include "OCISource.h"
#include "OCIGridView.h"
#include "OCI8\ACursor.h"
#include <COMMON/AppGlobal.h>
#include "COMMON\StrHelpers.h" // for print_time

#include "SQLTools.h"
#include "SQLUtilities.h"
#include "ServerBackgroundThread\TaskQueue.h"
#include "ThreadCommunication/MessageOnlyWindow.h"
#include "FetchWaitDlg.h"

using namespace ServerBackgroundThread;

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

    using namespace OG2;
    using namespace OCI8;

    const std::string OciGridSource::m_emptyString;

OciGridSource::OciGridSource ()
{
    m_bytesAllocated = 0;
    m_fetchPortions = 0;
    m_fetchIsActive = false;
    m_allRowsFetched = true;
    m_headersInLowerCase = true;
    m_fetchTime = 0.0;
}

    struct BackgroundTask_DestroyCursor : Task
    {
        std::shared_ptr<OCI8::AutoCursor> m_cursor;

        BackgroundTask_DestroyCursor ()
            : Task("DestroyCursor", 0)
        {
            SetSilent(true);
        }

        void DoInBackground (OciConnect&)
        {
            m_cursor.reset();
        }
    };

OciGridSource::~OciGridSource ()
{
    // the source owns the cursor but cannot use in the foreground thread
    // it has to send to the thread that owns the connection to destroy it
    TaskPtr task(new BackgroundTask_DestroyCursor);
    static_cast<BackgroundTask_DestroyCursor*>(task.get())->m_cursor.swap(m_cursor);
    FrgdRequestQueue::Get().Push(task);
}

    struct BackgroundTask_SetCursor : Task
    {
        HWND m_hWnd;
        OciGridView& m_view;
        OciGridSource& m_source;
        std::shared_ptr<OCI8::AutoCursor> m_cursor;
        vector<string> m_headers;
        vector<bool> m_isNumber;

        BackgroundTask_SetCursor (OciGridView& view, OciGridSource& source)
            : Task("SetCursor", 0),
            m_view(view),
            m_hWnd(view),
            m_source(source),
            m_cursor(source.m_cursor)
        {
            SetSilent(true);
        }

        void DoInBackground (OciConnect&)
        {
            if (m_cursor.get() && m_cursor->IsOpen())
            {
                int ncols = m_cursor->GetFieldCount();
                m_headers.resize(ncols);
                m_isNumber.resize(ncols);

                for (int i = 0; i < ncols; i++)
                {
                    m_headers.at(i) = m_cursor->GetFieldName(i);
                    m_isNumber.at(i) = m_cursor->IsNumberField(i);
                }
            }
        }

        void ReturnInForeground ()
        {
            if (::IsWindow(m_hWnd) && !m_headers.empty())
            {
                m_source.SetMaxShowLines(GetSQLToolsSettings().GetGridMultilineCount());
                m_source.m_headersInLowerCase = GetSQLToolsSettings().GetGridHeadersInLowerCase();

                int ncols = m_headers.size();
                m_source.SetCols(ncols);
                for (int i = 0; i < ncols; i++)
                {
                    if (m_source.m_headersInLowerCase)
                        m_source.SetColHeader(i, SQLUtilities::GetSafeDatabaseObjectName(m_headers.at(i), true).c_str());
                    else
                        m_source.SetColHeader(i, m_headers.at(i));

                    m_source.SetColAlignment(i, m_isNumber.at(i) ? ecaRight : ecaLeft);
                }

                if (m_source.IsTableOrientation())
                    m_source.Notify_ChangedColsQuantity(0, ncols);
                else
                    m_source.Notify_ChangedRowsQuantity(0, ncols);

                m_view.SetExpandLastCol(!*m_isNumber.rbegin());
            }
        }
    };

void OciGridSource::SetCursor (OciGridView& view, std::unique_ptr<OCI8::AutoCursor>& cursor)
{
    m_allRowsFetched = false;
    m_bytesAllocated = 0;
    m_fetchPortions = 0;
    Clear();

    {
        TaskPtr task(new BackgroundTask_DestroyCursor);
        static_cast<BackgroundTask_DestroyCursor*>(task.get())->m_cursor.swap(m_cursor);
        FrgdRequestQueue::Get().Push(task);
    }

    m_cursor = std::shared_ptr<OCI8::AutoCursor>(cursor.release());
    m_fetchTime = 0;

    FrgdRequestQueue::Get().Push(TaskPtr(new BackgroundTask_SetCursor(view, *this)));
}

    struct ActivePrimeExecutionNoteI2 : public ThreadCommunication::Note
    {
        bool m_on;

        ActivePrimeExecutionNoteI2 (bool on) : m_on(on) {}

        virtual void Deliver () 
        {
            theApp.SetActivePrimeExecution(m_on);
        }
    };

    struct FetchResultNote : public ThreadCommunication::Note
    {
        HWND m_hWnd;
        OciGridSource& m_source;
        std::vector<std::string> m_result;
        bool m_abort;
        double m_lastFetchTime;

        FetchResultNote (HWND hWnd, OciGridSource& source, double lastFetchTime) 
            : m_hWnd(hWnd),
            m_source(source),
            m_abort(false),
            m_lastFetchTime(lastFetchTime)
        {
        }

        virtual void Deliver () 
        {
            try
            {
                if (::IsWindow(m_hWnd))
                    m_source.AppendFetched(m_result, false, m_lastFetchTime);
                else
                    m_abort = true;
            }
            catch (...)
            {
                m_abort = true;
                throw;
            }
        }
    };

    struct BackgroundTask_GridFetch : Task
    {
        HWND m_hWnd;
        OciGridView& m_view;
        OciGridSource& m_source;
        std::shared_ptr<OCI8::AutoCursor> m_cursor;
        bool m_allRowsFetched;
        int m_rowsToFetch;
        int m_bytesAllocated;
        std::vector<std::string> m_result;
        static const int MAX_FETCH_PORTION = 1000;
        double m_lastFetchTime;

        BackgroundTask_GridFetch (OciGridView& view, OciGridSource& source, int rowsToFetch)
            : Task("Fetch", 0),
            m_hWnd(view),
            m_view(view),
            m_source(source),
            m_cursor(source.m_cursor),
            m_allRowsFetched(false),
            m_rowsToFetch(rowsToFetch),
            m_bytesAllocated(source.m_bytesAllocated),
            m_lastFetchTime(0.0)
        {
            SetSilent(true);
        }

        void setActivePrimeExecution (bool on, bool _nothrow = false)
        {
            try {
                ThreadCommunication::MessageOnlyWindow::GetWindow().Send(ActivePrimeExecutionNoteI2(on));
            } catch (...) {
                if (!_nothrow) throw;
            }
        }

        void DoInBackground (OciConnect& connect)
        {
            try { EXCEPTION_FRAME;

                if (!m_allRowsFetched && connect.IsOpen())
                {
                    setActivePrimeExecution(true);

                    if (m_cursor->IsOpen())
                    {
                        bool stopBySize = false, ignoreSize = false;

                        clock64_t t0 = clock64();

                        int cols = m_cursor->GetFieldCount();

                        m_result.reserve(cols * min(m_rowsToFetch, MAX_FETCH_PORTION));
                    
                        for (int i = 0, ii = 0; i < m_rowsToFetch ; i++, ii++)
                        {
                            if (m_cursor->Fetch())
                            {
                                int from = m_result.size();
                                m_result.resize(from + cols);

                                for (int j = 0; j < cols; j++)
                                {
                                    string& buff = m_result.at(from + j);
                                    m_cursor->GetString(j, buff);
                                    m_bytesAllocated += buff.size() <= buff.capacity() ? sizeof(buff) : sizeof(buff) + buff.size();
                                }

                                if (!ignoreSize && (m_bytesAllocated > (500 * 1024 * 1024)))
                                {
                                    if (AfxMessageBox(L"The size of fetched data reached the safe limit 500 Mb!"
                                        "\n\nPlease choose one of the following options: " 
                                        "\n      press <Yes> to stop fetching and close the cursor, "
                                        "\n      press <No> to continue at your own risk", 
                                        MB_YESNO|MB_ICONEXCLAMATION) == IDYES)
                                    {
                                        m_allRowsFetched = true;
                                        break;
                                    }
                                    else
                                        ignoreSize = true;
                                }

                                clock64_t t1 = clock64();
                                if (stopBySize || ii >= MAX_FETCH_PORTION || double(t1 - t0)/CLOCKS_PER_SEC > 5) // return intermediate result
                                {
                                    m_lastFetchTime = double(t1 - t0)/CLOCKS_PER_SEC;
                                    ii = 0;
                                    t0 = t1;

                                    if (::IsWindow(m_hWnd))
                                    {
                                        {
                                            FetchResultNote note(m_hWnd, m_source, m_lastFetchTime);
                                            note.m_result.swap(m_result);
                                            ThreadCommunication::MessageOnlyWindow::GetWindow().Send(note);
                                            if (note.m_abort)
                                            {
                                                m_allRowsFetched = true;
                                                break;
                                            }
                                        }
                                        m_result.reserve(cols * MAX_FETCH_PORTION);
                                    }
                                    else
                                        break; // the grid window was closed
                                }
                            }
                            else
                            {
                                m_allRowsFetched = true;
                                break;
                            }
                        }

                        m_lastFetchTime = double(clock64() - t0)/CLOCKS_PER_SEC;

                        if (m_allRowsFetched)
                            m_cursor->Close();
                    }
                    else
                        m_allRowsFetched = true;
                }
                else
                    m_allRowsFetched = true;
            }
            catch (const OciException& x)
            {
                m_allRowsFetched = true;
                try { m_cursor->Close(true); } catch (...) {}

                setActivePrimeExecution(false, true);
                if (x == 24374)
                    THROW_APP_EXCEPTION("The result of the query is not shown"
                        "\nbecause all columns types are currently not supported.");
                else if (x == 1013)
                    return;
                else
                    throw;
            }
            catch (const std::exception&)
            {
                m_allRowsFetched = true;
                try { m_cursor->Close(true); } catch (...) {}
                setActivePrimeExecution(false, true);
                throw;
            }
            catch (...)
            {
                m_allRowsFetched = true;
                try { m_cursor->Close(true); } catch (...) {}
                setActivePrimeExecution(false, true);
                throw;
            }

            setActivePrimeExecution(false);
        }

        void ReturnInForeground ()
        {
            if (::IsWindow(m_hWnd))
            {
                m_source.m_fetchIsActive = false;
                int rowsBefore = m_source.GetCount(edVert);
                m_source.AppendFetched(m_result, m_allRowsFetched, m_lastFetchTime);
                if (m_source.IsTableOrientation())
                    m_view.AutofitColumns(-1, rowsBefore);
                m_view.UpdateValuePanel();
            }
        }

        void ReturnErrorInForeground ()
        {
            ReturnInForeground();
        }
    };

void OciGridSource::Fetch (OciGridView& view, int rowsToFetch)
{
    if (!m_allRowsFetched && !m_fetchIsActive)
    {
        if (theApp.GetActivePrimeExecution())
        {
            AfxMessageBox(L"The connection is busy.\n\nPlease try again later.", MB_OK|MB_ICONASTERISK);
            AfxThrowUserException();
        }
        FrgdRequestQueue::Get().Push(TaskPtr(new BackgroundTask_GridFetch(view, *this, rowsToFetch)));
        m_fetchIsActive = true;
    }
}

void OciGridSource::FetchAll () // will wait until it is done
{
    if (!m_allRowsFetched)
    {
        OciGridSource* This = const_cast<OciGridSource*>(this);
        if (GridManager* manager = dynamic_cast<GridManager*>(*m_GridManagers.begin()))
            if (OciGridView* view = dynamic_cast<OciGridView*>(manager->m_pClientWnd))
            {   
                This->Fetch(*view, INT_MAX);

                FetchWaitDlg dlg(AfxGetMainWnd(), This->m_allRowsFetched);
                dlg.m_text = "Fetching data, please wait...";

                if (dlg.DoModal() == IDCANCEL)
                {
                    theApp.DoCancel(false/*silent*/);
                    AfxThrowUserException();
                }
            }
    }
}

void OciGridSource::AppendFetched (std::vector<std::string>& result, bool allRowsFetched, double lastFetchTime)
{
    m_fetchTime += lastFetchTime;

    if (allRowsFetched)
        m_allRowsFetched = allRowsFetched;
    
    if (result.size() > 0)
    {
        CWaitCursor wait;

        OciGridSource::NotificationDisabler disabler(this);

        int nrows = GetCount(IsTableOrientation() ? edVert : edHorz);
        int ncols = GetCount(IsTableOrientation() ? edHorz : edVert);

        int size = 0;

        ReserveMore(result.size()/ncols);

        std::vector<std::string>::iterator it = result.begin();
        int i = 0;
        for (; it != result.end(); ++i)
        {
            int row = Append();
            for (int col = 0; col < ncols; col++, ++it)
            {
                size += it->size() <= it->capacity() ? sizeof(*it) : sizeof(*it) + it->size();
                GetString(row, col).swap(*it);
            }
        }

        m_bytesAllocated += size;

        disabler.Enable();

        if (IsTableOrientation())
            Notify_ChangedRowsQuantity(nrows, GetCount(edVert)); // indirectly calls autofit
        else
            Notify_ChangedColsQuantity(nrows, GetCount(edHorz));
                
        std::ostringstream o;

        o << "Fetch #" << ++m_fetchPortions << ", packet " << i << " row(s)";
    
        if (m_fetchPortions > 1)
        {
            o << ", size = ";

            if (size < 1024)
                o << size << "bytes";
            else if (size / 1024 < 1024)
                o << size / 1024 << "Kb";
            else 
                o << size / 1024 / 1024 << "Mb";
    
            o << ", total size = ";

            if (m_bytesAllocated < 1024)
                o << m_bytesAllocated << "bytes";
            else if (m_bytesAllocated / 1024 < 1024)
                o << m_bytesAllocated / 1024 << "Kb.";
            else 
                o << m_bytesAllocated / 1024 / 1024 << "Mb.";
        }

        o << ", exec " << Common::print_time(m_cursor->GetExecutionTime());
        o << ", fetch " << Common::print_time(m_fetchTime);
        o << ", total " << Common::print_time(m_cursor->GetExecutionTime() + m_fetchTime);

        Global::SetStatusText(o.str().c_str());
    }
}

//TODO#2: re-implement this w/o a modal dialog
    namespace
    {
        bool isIdentChar (char c)
        {
            return isalnum((unsigned char)c) || c == '_' || c == '$' || c == '#';
        }

        // reads [owner.]name (each part bare or "quoted") starting at pos; bare parts are upper-cased
        bool readQualifiedName (const string& s, size_t& pos, string& owner, string& name)
        {
            string parts[2];
            int n = 0;
            for (;;)
            {
                string part;
                if (pos < s.size() && s[pos] == '"')
                {
                    size_t end = s.find('"', pos + 1);
                    if (end == string::npos) return false;
                    part = s.substr(pos, end - pos + 1);
                    pos = end + 1;
                }
                else
                {
                    size_t start = pos;
                    while (pos < s.size() && isIdentChar(s[pos])) pos++;
                    if (pos == start) return false;
                    part = s.substr(start, pos - start);
                    for (size_t i = 0; i < part.size(); i++) part[i] = (char)toupper((unsigned char)part[i]);
                }
                parts[n++] = part;
                if (n < 2 && pos < s.size() && s[pos] == '.') { pos++; continue; }
                break;
            }
            if (pos < s.size() && s[pos] == '@') return false; // database link
            if (n == 2) { owner = parts[0]; name = parts[1]; }
            else        { owner.clear();    name = parts[0]; }
            return true;
        }

        void skipSpaces (const string& s, size_t& pos)
        {
            while (pos < s.size() && isspace((unsigned char)s[pos])) pos++;
        }

        // reads a bare word (upper-cased); pos is left after it
        string readWord (const string& s, size_t& pos)
        {
            size_t start = pos;
            while (pos < s.size() && isIdentChar(s[pos])) pos++;
            string w = s.substr(start, pos - start);
            for (size_t i = 0; i < w.size(); i++) w[i] = (char)toupper((unsigned char)w[i]);
            return w;
        }

        bool isJoinOrSetWord (const string& w)
        {
            return w == "JOIN" || w == "INNER" || w == "LEFT" || w == "RIGHT" || w == "FULL" || w == "CROSS"
                || w == "NATURAL" || w == "UNION" || w == "MINUS" || w == "INTERSECT";
        }

        bool isClauseWord (const string& w)
        {
            return w == "WHERE" || w == "GROUP" || w == "ORDER" || w == "HAVING" || w == "CONNECT" || w == "START"
                || w == "FETCH" || w == "OFFSET" || w == "FOR" || w == "MODEL" || isJoinOrSetWord(w);
        }

        // blanks out comments and string literals, keeping the offsets
        string stripCommentsAndStrings (const string& sql)
        {
            string s = sql;
            size_t i = 0;
            while (i < s.size())
            {
                if (s[i] == '-' && i + 1 < s.size() && s[i+1] == '-')
                {
                    while (i < s.size() && s[i] != '\n') s[i++] = ' ';
                }
                else if (s[i] == '/' && i + 1 < s.size() && s[i+1] == '*')
                {
                    size_t end = s.find("*/", i + 2);
                    end = (end == string::npos) ? s.size() : end + 2;
                    while (i < end) s[i++] = ' ';
                }
                else if (s[i] == '\'')
                {
                    s[i++] = ' ';
                    while (i < s.size())
                    {
                        if (s[i] == '\'')
                        {
                            if (i + 1 < s.size() && s[i+1] == '\'') { s[i++] = ' '; s[i++] = ' '; continue; }
                            s[i++] = ' ';
                            break;
                        }
                        s[i++] = ' ';
                    }
                }
                else
                    i++;
            }
            return s;
        }

        // finds the single table of a plain "SELECT ... FROM [owner.]table [alias] ..." query;
        // returns false for joins, sub-queries, set operations and WITH queries
        bool findSingleTable (const string& sql, string& owner, string& name)
        {
            const string s = stripCommentsAndStrings(sql);

            size_t pos = 0;
            skipSpaces(s, pos);
            if (readWord(s, pos) != "SELECT")
                return false;

            int depth = 0;
            while (pos < s.size())
            {
                char c = s[pos];
                if (c == '"')
                {
                    size_t end = s.find('"', pos + 1);
                    if (end == string::npos) return false;
                    pos = end + 1;
                }
                else if (c == '(') { depth++; pos++; }
                else if (c == ')') { depth--; pos++; }
                else if (isIdentChar(c))
                {
                    string w = readWord(s, pos);
                    if (depth != 0) continue;
                    if (w == "UNION" || w == "MINUS" || w == "INTERSECT")
                        return false;
                    if (w != "FROM") continue;

                    skipSpaces(s, pos);
                    if (!readQualifiedName(s, pos, owner, name)) // also rejects "FROM (sub-query)"
                        return false;

                    // optional alias, then nothing that turns it into a join
                    skipSpaces(s, pos);
                    size_t after = pos;
                    string next = readWord(s, after);
                    if (next == "AS") { skipSpaces(s, after); next = readWord(s, after); }
                    if (!next.empty() && !isClauseWord(next))
                    {
                        skipSpaces(s, after); // alias consumed
                        size_t p2 = after;
                        next = readWord(s, p2);
                        after = p2;
                    }
                    if (isJoinOrSetWord(next))
                        return false;
                    skipSpaces(s, after);
                    return !(next.empty() && after < s.size() && s[after] == ',');
                }
                else
                    pos++;
            }
            return false;
        }
    }

std::string OciGridSource::GetInsertTableName () const
{
    if (!m_cursor.get())
        return std::string();

    string owner, name;
    if (!findSingleTable(m_cursor->GetText(), owner, name))
        return std::string();

    if (owner.empty())
        owner = theApp.GetCurrentSchema();
    if (owner.empty())
        owner = theApp.GetUser();

    return owner.empty() ? name : owner + "." + name;
}

int OciGridSource::ExportText (std::ostream& of, int row, int nrows, int col, int ncols, int format) const
{
    try
    {
        if (!m_allRowsFetched 
        && ((row == -1 && IsTableOrientation()) || (col == -1 && !IsTableOrientation())))
        {
            OciGridSource* This = const_cast<OciGridSource*>(this);
            //if (GridManager* manager = dynamic_cast<GridManager*>(*m_GridManagers.begin()))
            //    if (OciGridView* view = dynamic_cast<OciGridView*>(manager->m_pClientWnd))
            //    {   
            //        This->Fetch(*view, INT_MAX);
            //
            //        FetchWaitDlg dlg(AfxGetMainWnd(), This->m_allRowsFetched);
            //        dlg.m_text = "Fetching data, please wait...";
            //
            //        if (dlg.DoModal() == IDCANCEL)
            //        {
            //            theApp.DoCancel(false/*silent*/);
            //            AfxThrowUserException();
            //        }
            //    }
            This->FetchAll();
        }
        return GridStringSource::ExportText(of, row, nrows, col, ncols, format);
    }
    catch (const OciException& x)
    {
        MessageBeep(MB_OK|MB_ICONHAND);
        AfxMessageBox(x);
    }

    return etfPlainText;
}

    static string s_null = "NULL";

void OciGridSource::GetCellStrForExport (string& str,  int line, int col, int subformat) const
{
    GridStringSource::GetCellStrForExport(str, line, col, subformat);
    
    if (subformat > 0 && m_cursor.get())
    {
        if (m_cursor->GetStringNull() == str)
        {
            str = s_null;
            return;
        }

        int pos = IsTableOrientation() ? col : line;

        if(m_cursor->IsNumberField(pos))
            return;

        if (m_cursor->IsDatetimeField(pos))
        {
            str = m_cursor->BackcastField(pos, str);
            return;
        }

        if (str.find('\'') != string::npos)
        {
            string buffer = "'";
            for (string::const_iterator it = str.begin(); it != str.end(); ++it)
            {
                buffer += *it;
                if (*it == '\'') buffer += *it;
            }
            buffer += "'";
            str = buffer;
        }
        else
            str = "'" + str + "'";

        return;
    }
    
    if (m_cursor->GetStringNull() == str)
        str = m_emptyString;
}

bool OciGridSource::IsTextField (int col) const
{
	return !(m_cursor->IsNumberField(col) || m_cursor->IsDatetimeField(col));
}

void OciGridSource::CopyColumnNames (std::string& buff, int from, int to) const
{
    // TODO: remove that in the next major release - return whatever you see in the data grid

    if (!m_headersInLowerCase)
    {
        int nCols = m_ColumnInfo.size();
        for (int i = from; i < nCols && i <= to; i++)
        {
            buff += SQLUtilities::GetSafeDatabaseObjectName(GetColHeader(i));
            if (i != nCols - 1 && i != to)
                buff += ", ";
        }
    }
    else 
        __super::CopyColumnNames(buff, from, to);
}
