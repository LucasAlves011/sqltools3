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

#pragma once
#include "OpenEditor/OETemplates.h"

    using std::vector;

    namespace ObjectTree 
    {
        class TreeNodePool;
    };
    typedef std::shared_ptr<ObjectTree::TreeNodePool> TreeNodePoolPtr;

    struct AutocompleteTemplateParams
    {
        bool sortSubobjects               = false;
        bool enableMultiColumnSelection   = false;
        bool insertColumnsOnSeparateLines = false;
        bool useLeadingComma              = false;
        bool useLowercaseForAliases       = false;
    };


struct AutocompleteTemplate : OpenEditor::Template
{    
    AutocompleteTemplate (TreeNodePoolPtr);

    virtual bool AfterExpand (const Entry&, string& text, OpenEditor::Position& pos);

private:
    TreeNodePoolPtr m_poolPtr;
    CEvent          m_bkgrSyncEvent;
};

struct AutocompleteColBaseTemplate : OpenEditor::Template
{
    AutocompleteColBaseTemplate (const string& alias, const AutocompleteTemplateParams&);

    virtual void SetMultiSelection(const vector<int>& selection) { m_selection = selection; };
    virtual bool ExpandKeyword(int index, string& text, OpenEditor::Position& pos);

protected:
    string m_alias;
    vector<int> m_selection;
    AutocompleteTemplateParams m_params;
};


struct AutocompleteSubobjectTemplate : AutocompleteColBaseTemplate
{    
    AutocompleteSubobjectTemplate (TreeNodePoolPtr, const string& object, const string& alias, const AutocompleteTemplateParams&);

private:
    TreeNodePoolPtr m_poolPtr;
    CEvent          m_bkgrSyncEvent;
};

// Shows a list of columns for CTEs/subqueries
struct AutocompleteColumnsTemplate : AutocompleteColBaseTemplate
{
    AutocompleteColumnsTemplate (const std::vector<std::string>& columns, const string& alias, const AutocompleteTemplateParams&);
};
