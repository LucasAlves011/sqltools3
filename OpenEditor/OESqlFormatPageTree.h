#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __OESqlFormatPageTree_h__
#define __OESqlFormatPageTree_h__

#include "resource.h"
#include "OpenEditor/OESettings.h"
#include "PropTree\PropTree.h"
#include "PropTree\PropTreeItemEdit.h"
#include "PropTree\PropTreeItemStatic.h"

	using OpenEditor::SettingsManager;

class COESqlFormatPageTree : public CPropertyPage
{
public:
	COESqlFormatPageTree (SettingsManager& manager);
	virtual ~COESqlFormatPageTree ();

	enum { IDD = IDD_OE_SQL_FORMAT_TREE };

	//void SetApplyRecipient (CWnd* recipient, UINT command);

protected:
	virtual BOOL OnInitDialog ();
	virtual BOOL OnApply ();
	virtual void DoDataExchange (CDataExchange* pDX);

	afx_msg void OnPropTreeNotify (NMHDR* pNMHDR, LRESULT* pResult);

	DECLARE_MESSAGE_MAP()

private:
	void BuildTree ();

	//CWnd* m_recipient;
	//UINT m_recipientCommand;

	SettingsManager& m_manager;
	CPropTree        m_tree;

	// Short-line heuristics
	CPropTreeItemEdit*   m_pSimpleStmt;
	CPropTreeItemEdit*   m_pSelectList;
	CPropTreeItemEdit*   m_pFromList;
	CPropTreeItemEdit*   m_pGroupByList;
	CPropTreeItemEdit*   m_pOrderByList;
	CPropTreeItemEdit*   m_pSelectStmt;
	CPropTreeItemEdit*   m_pBlock;
	CPropTreeItemEdit*   m_pCaseWhen;
	CPropTreeItemEdit*   m_pReturningMaxLen;
	CPropTreeItemEdit*   m_pReturningMaxItems;

	// Miscellaneous
	CPropTreeItemStatic* m_pAlignAliases;
	CPropTreeItemStatic* m_pSetOpBlankLine;
	CPropTreeItemStatic* m_pAlignSetAssignments;
	CPropTreeItemStatic* m_pAlignCaseInSet;
	CPropTreeItemEdit*   m_pLogErrorsMaxLen;

	// Brick list
	CPropTreeItemStatic* m_pBrickListEnabled;
	CPropTreeItemEdit*   m_pBrickListMaxLineLen;
	CPropTreeItemEdit*   m_pBrickListMaxLineItems;
	CPropTreeItemEdit*   m_pBrickListMinItems;
	CPropTreeItemStatic* m_pBrickListLinkedInsert;
	CPropTreeItemStatic* m_pBrickListLinkedSetOp;
public:
	afx_msg void OnSize(UINT nType, int cx, int cy);
};

#endif//__OESqlFormatPageTree_h__
