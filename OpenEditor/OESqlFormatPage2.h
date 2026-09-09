#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __OESqlFormatPage2_h__
#define __OESqlFormatPage2_h__

#include "resource.h"
#include "OpenEditor/OESettings.h"

	using OpenEditor::SettingsManager;

class COESqlFormatPage2 : public CPropertyPage
{
public:
	COESqlFormatPage2 (SettingsManager& manager);
	virtual ~COESqlFormatPage2 ();

	enum { IDD = IDD_OE_SQL_FORMAT2 };

protected:
	virtual void DoDataExchange (CDataExchange* pDX);

	DECLARE_MESSAGE_MAP()

public:
	SettingsManager& m_manager;

	// miscellaneous
	BOOL m_AlignAliases;
	BOOL m_SetOpBlankLine;

	virtual BOOL OnApply ();
};

#endif//__OESqlFormatPage2_h__
