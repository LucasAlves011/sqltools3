/* 
    SQLTools is a tool for Oracle database developers and DBAs.
    Copyright (C) 1997-2025 Aleksey Kochetov

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
#include "SQLToolsSettings.h"
#include "resource1.h"

class CPropAutocompletePage : public CPropertyPage
{
    SQLToolsSettings& m_settings;

public:
    CPropAutocompletePage (SQLToolsSettings&);

    //{{AFX_DATA(CPropAutocompletePage)
    enum { IDD = IDD_PROP_AUTOCOMPLETE };
    //}}AFX_DATA

protected:
    virtual void DoDataExchange (CDataExchange* pDX);

    DECLARE_MESSAGE_MAP()
};