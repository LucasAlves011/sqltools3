#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __OESqlFormatPageTree_h__
#define __OESqlFormatPageTree_h__

#include "resource.h"
#include "OpenEditor/OESettings.h"
#include "Theme/ModernOptionList.h"

	using OpenEditor::SettingsManager;

// pagina "SQL Format" das configuracoes: lista de opcoes no visual do programa (segue o tema claro/escuro)
class COESqlFormatPageTree : public CPropertyPage
{
public:
	COESqlFormatPageTree (SettingsManager& manager);
	virtual ~COESqlFormatPageTree ();

	enum { IDD = IDD_OE_SQL_FORMAT_TREE };

protected:
	virtual BOOL OnInitDialog ();
	virtual BOOL OnApply ();
	virtual BOOL OnCommand (WPARAM wParam, LPARAM lParam);
	virtual void DoDataExchange (CDataExchange* pDX);

	DECLARE_MESSAGE_MAP()

private:
	void LoadValues ();

	SettingsManager&  m_manager;
	CModernOptionList m_list;

	// valores editados (copiados das configuracoes e gravados em OnApply)
	bool m_simpleStmtOn, m_selectListOn, m_fromListOn, m_groupByOn, m_orderByOn, m_selectStmtOn;
	int  m_simpleStmtLen, m_selectListLen, m_fromListLen, m_groupByLen, m_orderByLen, m_selectStmtLen;
	int  m_blockLen, m_caseWhenLen, m_returningLen, m_returningItems;
	bool m_alignAliases, m_setOpBlankLine, m_alignSetAssignments, m_alignCaseInSet;
	int  m_logErrorsLen;
	bool m_brickEnabled, m_brickLinkedInsert, m_brickLinkedSetOp;
	int  m_brickLineLen, m_brickLineItems, m_brickMinItems;

public:
	afx_msg void OnSize(UINT nType, int cx, int cy);
};

#endif//__OESqlFormatPageTree_h__
