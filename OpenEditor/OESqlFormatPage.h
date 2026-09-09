#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __OESqlFormatPage_h__
#define __OESqlFormatPage_h__

#include "resource.h"
#include "OpenEditor/OESettings.h"

	using OpenEditor::SettingsManager;

class COESqlFormatPage : public CPropertyPage
{
public:
	COESqlFormatPage (SettingsManager& manager);
	virtual ~COESqlFormatPage ();

	enum { IDD = IDD_OE_SQL_FORMAT };

protected:
	virtual void DoDataExchange (CDataExchange* pDX);

	DECLARE_MESSAGE_MAP()

public:
	SettingsManager& m_manager;

	// simple statement
	BOOL m_SimpleStmtEnabled;
	int  m_SimpleStmtMaxLen;

	// SELECT column list
	BOOL m_SelectListEnabled;
	int  m_SelectListMaxLen;

	// FROM table list
	BOOL m_FromListEnabled;
	int  m_FromListMaxLen;

	// GROUP BY list
	BOOL m_GroupByListEnabled;
	int  m_GroupByListMaxLen;

	// ORDER BY list
	BOOL m_OrderByListEnabled;
	int  m_OrderByListMaxLen;

	// subquery / parenthesised block
	BOOL m_SelectStmtEnabled;
	int  m_SelectStmtMaxLen;
	int  m_BlockMaxLen;

	// CASE expression
	int  m_CaseWhenMaxLen;

	virtual BOOL OnApply ();
};

#endif//__OESqlFormatPage_h__
