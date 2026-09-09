#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __OEPlsSqlOutlinePageTree_h__
#define __OEPlsSqlOutlinePageTree_h__

#include "resource.h"
#include "OpenEditor/OESettings.h"
#include "PropTree\PropTree.h"
#include "PropTree\PropTreeItemEdit.h"
#include "PropTree\PropTreeItemStatic.h"

	using OpenEditor::SettingsManager;

class COEPlsSqlOutlinePageTree : public CPropertyPage
{
public:
	COEPlsSqlOutlinePageTree (SettingsManager& manager);
	virtual ~COEPlsSqlOutlinePageTree ();

	enum { IDD = IDD_OE_PLSQL_OUTLINE_TREE };

protected:
	virtual BOOL OnInitDialog ();
	virtual BOOL OnApply ();
	virtual void DoDataExchange (CDataExchange* pDX);

	afx_msg void OnPropTreeNotify (NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnSize (UINT nType, int cx, int cy);

	DECLARE_MESSAGE_MAP()

private:
	void BuildTree ();

	SettingsManager& m_manager;
	CPropTree        m_tree;

	// Navigation
	CPropTreeItemStatic* m_pKeywordNavigation;

	// Parser limits
	CPropTreeItemEdit*   m_pFileMaxTokens;
	CPropTreeItemEdit*   m_pFileMaxLines;

	// Outline
	CPropTreeItemStatic* m_pOutlineEnabled;
	CPropTreeItemStatic* m_pOutlineSelectEnabled;
	CPropTreeItemStatic* m_pOutlineSelectOnly;
	CPropTreeItemEdit*   m_pOutlineMaxTopItems;
};

#endif//__OEPlsSqlOutlinePageTree_h__
