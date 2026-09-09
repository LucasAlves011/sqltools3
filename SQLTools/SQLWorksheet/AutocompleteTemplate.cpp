/*
	SQLTools is a tool for Oracle database developers and DBAs.
    Copyright (C) 1997-2015 Aleksey Kochetov

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

#include "stdafx.h"
#include "SQLTools.h"
#include "AutocompleteTemplate.h"
#include "DbBrowser\ObjectTree_Builder.h"
#include "DbBrowser\FindObjectsTask.h"
#include <ActivePrimeExecutionNote.h>
#include "SQLUtilities.h"
#include "SessionCache.h"

using namespace OpenEditor;
using namespace ObjectTree;
using namespace ServerBackgroundThread;

AutocompleteTemplate::AutocompleteTemplate (TreeNodePoolPtr poolPtr)
    : m_poolPtr(poolPtr)
{ 
    SetKeyColumnHeader("Name");
    SetNameColumnHeader("Type");
}

    struct BackgroundTask_FindAndLoadProc : ObjectTree::FindObjectsTask
    {
        CEvent& m_event;
        TreeNodePoolPtr m_poolPtr;
        TreeNode* m_object;
        bool m_descriptorFromCache;

        BackgroundTask_FindAndLoadProc (CEvent& event, TreeNodePoolPtr poolPtr, const std::string& input)
            : FindObjectsTask(input, 0),
            m_event(event),
            m_poolPtr(poolPtr),
            m_object(0),
            m_descriptorFromCache(false)
        {
            m_silent = true;
            ObjectTree::ObjectDescriptor cachedDesc;
            if (SessionCache::FindCachedDescriptor(m_input, cachedDesc))
            {
                m_result.push_back(cachedDesc);
                m_descriptorFromCache = true;
            }
        }

        void DoInBackground (OciConnect& connect)
        {
            if (!m_descriptorFromCache)
                FindObjectsTask::DoInBackground(connect);

            if (m_result.size() > 0)
            {
                TreeNode* object = m_poolPtr->CreateObject(m_result.at(0));

                if (object && !object->IsComplete())
                {
                    ActivePrimeExecutionOnOff onOff;
                    object->Load(connect);
                }

                m_object = object;
            }

            m_event.SetEvent();
        }

        void ReturnInForeground ()
        {
            if (!m_descriptorFromCache && m_result.size() > 0)
                SessionCache::StoreCachedDescriptor(m_input, m_result.at(0));
        }
    };

    static bool pump_messages_and_wait_for_event (CEvent& event, int timeout)
    {
        clock64_t startTime = clock64();
        bool ready = false;
        MSG Msg;
        while (GetMessage(&Msg, NULL, 0, 0) > 0)
        {
            TranslateMessage(&Msg);
            DispatchMessage(&Msg);

            if (WaitForSingleObject(event, 0) == WAIT_OBJECT_0)
            {
                ready = true;
                break;
            }

            if ((clock64() - startTime) > timeout)
                break;            
        }

        return ready;
    }

bool AutocompleteTemplate::AfterExpand (const Entry& entry, string& text, Position& pos)
{
    if (entry.name == "PROCEDURE" || entry.name == "FUNCTION")
    {
        CWaitCursor wait;

        m_bkgrSyncEvent.ResetEvent();
        BackgroundTask_FindAndLoadProc* task = new BackgroundTask_FindAndLoadProc(m_bkgrSyncEvent, m_poolPtr, text);
        TaskPtr taskPtr(task);

        BkgdRequestQueue::Get().Push(taskPtr);

        if (pump_messages_and_wait_for_event(m_bkgrSyncEvent, 5000))
        {
            if (task->m_object)
            {
                TextCtx ctx;
                //ctx.m_form = etfCOLUMN;
                ctx.m_form = etfLINE;
                task->m_object->GetTextToCopy(ctx);

                text = ctx.m_text;
                string::size_type inx = text.find_first_of("=>");

                if (inx != string::npos)
                {
                    // taking in consideration multi-line formats
                    bool multiline = false;
                    int col = 0, line = 0;
                    for (string::size_type i = 0; i < inx && text.at(i); i++)
                    {
                        if (text.at(i) == '\n') {
                            multiline = true;
                            col = 0;
                            line++;
                        } else
                            col++;
                    }
                    pos.column = col + 3; 
                    pos.line = line; 
                    //pos.column = inx + 3;
                }
                else
                {
                    pos.column = text.length();
                    if (ctx.m_form == etfCOLUMN)
                        --pos.column; // because the text ends with \n 
                }
            }
        }
        else
            AfxMessageBox(L"Timeout error while loading object(s).\nPlease try again.", MB_ICONERROR | MB_OK); 
    }

    return true;
}

    struct BackgroundTask_FindAndLoad : ObjectTree::FindObjectsTask
    {
        CEvent& m_event;
        TreeNodePoolPtr m_poolPtr;
        vector<TreeNode*> m_children;
        bool m_descriptorFromCache;

        BackgroundTask_FindAndLoad (CEvent& event, TreeNodePoolPtr poolPtr, const std::string& input)
            : FindObjectsTask(input, 0),
            m_event(event),
            m_poolPtr(poolPtr),
            m_descriptorFromCache(false)
        {
            m_silent = true;
            ObjectTree::ObjectDescriptor cachedDesc;
            if (SessionCache::FindCachedDescriptor(m_input, cachedDesc))
            {
                m_result.push_back(cachedDesc);
                m_descriptorFromCache = true;
            }
        }

        void DoInBackground (OciConnect& connect)
        {
            if (!m_descriptorFromCache)
                FindObjectsTask::DoInBackground(connect);

            if (m_result.size() > 0)
            {
                ActivePrimeExecutionOnOff onOff;

                if (Object* object = m_poolPtr->CreateObject(m_result.at(0)))
                {
                    if (!object->IsComplete())
                        object->Load(connect);

                    string type = object->GetType();

                    if (Synonym* syn = dynamic_cast<Synonym*>(object))
                    {
                        ObjectDescriptor desc;
                        desc.owner = syn->GetRefOwner();
                        desc.name  = syn->GetRefName();
                        desc.type  = syn->GetRefType();

                        if (syn->GetRefDBLink().empty())
                        {
                            if (Object* refObject = m_poolPtr->CreateObject(desc))
                            {
                                if (!refObject->IsComplete())
                                    refObject->Load(connect);
                                object = refObject;
                                type = object->GetType();
                            }
                        }
                    }

                    if (type == "PACKAGE" || type == "TYPE" || type == "TABLE" || type == "VIEW")
                    {
                        vector<TreeNode*> children;

                        unsigned int ncount = object->GetChildrenCount();
                        for (unsigned int i = 0; i < ncount; ++i)
                        {
                            if (TreeNode* child = object->GetChild(i))
                                if (dynamic_cast<Column*>(child) || dynamic_cast<PackageProcedure*>(child))
                                    children.push_back(child);
                        }

                        swap(m_children, children);
                    }
                }
            }

            m_event.SetEvent();
        }

        void ReturnInForeground ()
        {
            if (!m_descriptorFromCache && m_result.size() > 0)
                SessionCache::StoreCachedDescriptor(m_input, m_result.at(0));
        }
    };


AutocompleteColBaseTemplate::AutocompleteColBaseTemplate (const string& alias, const AutocompleteTemplateParams& params)
    : m_alias(alias), m_params(params)
{
    SetMultiSelectionSupported(m_params.enableMultiColumnSelection);
    SetSortedList(m_params.sortSubobjects);
    if (m_params.useLowercaseForAliases)
        std::transform(m_alias.begin(), m_alias.end(), m_alias.begin(),
            [](unsigned char c) { return std::tolower(c); });
}

bool AutocompleteColBaseTemplate::ExpandKeyword (int index, string& text, OpenEditor::Position& pos)
{
    if (m_selection.size() == 0)
        return Template::ExpandKeyword(index, text, pos);

    if (m_params.insertColumnsOnSeparateLines)
    {

        vector<string> lines;
        for (int& index : m_selection)
        {
            string buff;
            OpenEditor::Position dummy;
            if (Template::ExpandKeyword(index, buff, dummy))
                lines.push_back(buff);
        }

        if (m_params.useLeadingComma)
        {
            string result;
            int last_length = 0;
            int inx = 0;
            for (const string& line : lines)
            {
                if (inx > 0)
                {
                    result += ", ";
                    result += m_alias;
                    result += '.';
                }
                result += line;
                result += "\n";
                inx++;
            }
            result += ", ";
            last_length = 2;


            text = result;
            pos.line = inx;
            pos.column = last_length;

            m_selection.clear();

            return result.size() > 0;
        }
        else
        {
            string result;
            int last_length = 0;
            int inx = 0;
            for (const string& line : lines)
            {
                last_length = 0;

                if (inx > 0)
                {
                    result += m_alias;
                    result += '.';
                    last_length = m_alias.size() + 1;
                }
                result += line;
                last_length += line.size();

                if (inx < lines.size() - 1)
                {
                    result += ",\n";
                    last_length += 2;
                }
                else
                {
                    result += ",";
                    last_length += 1;
                }

                inx++;
            }


            text = result;
            pos.line = inx - 1;
            pos.column = last_length;

            m_selection.clear();

            return result.size() > 0;
        }
    }
    else // single line
    {
        vector<string> lines;
        for (int& index : m_selection)
        {
            string buff;
            OpenEditor::Position dummy;
            if (Template::ExpandKeyword(index, buff, dummy))
                lines.push_back(buff);
        }

        string result;
        int inx = 0;
        for (const string& line : lines)
        {
            if (inx > 0)
            {
                result += m_alias;
                result += '.';
            }
            result += line;

            if (inx < lines.size() - 1)
            {
                result += ", ";
            }
            inx++;
        }


        text = result;
        pos.line = 0;
        pos.column = result.length();

        m_selection.clear();

        return result.length() > 0;
    }

    return false;
}


AutocompleteSubobjectTemplate::AutocompleteSubobjectTemplate (TreeNodePoolPtr poolPtr, const string& object, const string& alias, const AutocompleteTemplateParams& params)
    : m_poolPtr(poolPtr), AutocompleteColBaseTemplate(alias, params)
{
	CWaitCursor wait;

    SetKeyColumnHeader("Name");
    SetNameColumnHeader("Type");
    SetImageListRes(IDB_SQL_GENERAL_LIST);

    m_bkgrSyncEvent.ResetEvent();
    BackgroundTask_FindAndLoad* task = new BackgroundTask_FindAndLoad(m_bkgrSyncEvent, m_poolPtr, object);
    TaskPtr taskPtr(task);

    BkgdRequestQueue::Get().Push(taskPtr);

    if (pump_messages_and_wait_for_event(m_bkgrSyncEvent, 7500))
    {
        vector<TreeNode*>::const_iterator it = task->m_children.begin();
        
        if (it != task->m_children.end() && typeid(**it) != typeid(Column)) // Multi-selection is supported for columns only
            SetMultiSelectionSupported(false);

        for (; it != task->m_children.end(); ++it)
        {
            Entry entry;
            TextCtx ctx;
            ctx.m_form = typeid(**it) == typeid(Column) ? etfSHORT : etfLINE; // 2017-12-14 bug fix, table columns are expanded with datatype
            ctx.m_with_package_name = false;
            (*it)->GetTextToCopy(ctx);
            entry.keyword = 
            entry.text    = ctx.m_text;
            entry.name    = (*it)->GetType();
            entry.image   = (*it)->GetImage();

            // TODO: take in consideration multi-line formats
            string::size_type inx = entry.text.find_first_of("=>");
            if (inx != string::npos)
                entry.curPos = inx + 3;
            else
                entry.curPos = entry.text.length();

            entry.minLength = ctx.m_text.length() + 1;

            AppendEntry(entry);
        }
    }
    else
        AfxMessageBox(L"Timeout error while loading object(s).\nPlease try again.", MB_ICONERROR | MB_OK); 
}

AutocompleteColumnsTemplate::AutocompleteColumnsTemplate (const std::vector<std::string>& columns, const string& alias, const AutocompleteTemplateParams& params)
    : AutocompleteColBaseTemplate(alias, params)
{
    SetKeyColumnHeader("Column");
    SetNameColumnHeader("Type");
    for (const auto& col : columns)
    {
        Entry e;
        e.keyword = 
        e.text = SQLUtilities::GetSafeDatabaseObjectName(col);
        e.name = "COLUMN";
        e.image = -1; // no image
        e.curPos = (int)e.text.size();
        e.minLength = (int)e.keyword.size();
        AppendEntry(e);
    }
}

