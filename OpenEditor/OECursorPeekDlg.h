#pragma once

#include <afxwin.h>
#include <afxcmn.h>
#include <vector>
#include "OpenEditor/OEPlsSqlCursorScanner.h"

class COEditorView;

namespace OpenEditor
{

class COECursorPeekDlg : public CWnd
{
public:
    COECursorPeekDlg();
    virtual ~COECursorPeekDlg();

    // Cria uma nova janela flutuante ou ativa a existente para o mesmo objeto
    static COECursorPeekDlg* OpenPeek (COEditorView* pEditor, const PlSqlObjectInfo& info, CPoint ptScreen);

    // Fecha pop-ups que não estejam afixados (unpinned)
    static void CloseAllUnpinned (COEditorView* pEditor = nullptr);

    // Fecha todos os pop-ups associados a um editor específico (ex: quando o documento fecha)
    static void CloseAllForEditor (COEditorView* pEditor);

    // Verifica se há alguma janela ativa aberta
    static bool HasActiveDialogs ();

    BOOL IsPinned () const { return m_bPinned; }
    const std::wstring& GetObjectName () const { return m_objectInfo.name; }

    void SyncToEditor ();

protected:
    virtual BOOL PreTranslateMessage(MSG* pMsg);
    virtual void PostNcDestroy();

    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnPaint();
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg LRESULT OnNcHitTest(CPoint point);
    afx_msg void OnEditChange();
    afx_msg void OnKillFocus(CWnd* pNewWnd);
    afx_msg void OnClose();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);

    DECLARE_MESSAGE_MAP()

private:
    void LayoutControls(int cx, int cy);
    void UpdateTitle();
    void ApplySyntaxHighlighting();

    static void RegisterDialog(COECursorPeekDlg* pDlg);
    static void UnregisterDialog(COECursorPeekDlg* pDlg);

    static std::vector<COECursorPeekDlg*> s_activeDialogs;

    COEditorView*    m_pEditor;
    PlSqlObjectInfo  m_objectInfo;
    CRichEditCtrl    m_editCtrl;
    CFont            m_font;
    CFont            m_uiFont;

    bool             m_bPinned;
    bool             m_bModified;
    bool             m_bSyncing;

    CRect            m_rcPinBtn;
    CRect            m_rcGoBtn;
    CRect            m_rcSyncBtn;
    CRect            m_rcCloseBtn;

    static const int HEADER_HEIGHT = 28;
    static const int BTN_WIDTH = 50;
    static const int BTN_HEIGHT = 22;
};

} // namespace OpenEditor
