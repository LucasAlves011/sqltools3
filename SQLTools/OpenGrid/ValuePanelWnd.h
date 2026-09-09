#pragma once
#ifndef __VALUEPANELWND_H__
#define __VALUEPANELWND_H__

#include <afxwin.h>
#include <afxcmn.h>
#include <vector>
#include <string>
#include "ValueFormatters.h"

class OciGridView;

namespace OG2
{

class CValuePanelWnd;

/////////////////////////////////////////////////////////////////////////////
// CValueSplitterBar - Separador vertical arrastável com botão colapsar/expandir
/////////////////////////////////////////////////////////////////////////////
class CValueSplitterBar : public CWnd
{
public:
    CValueSplitterBar(OciGridView* pGrid);
    virtual ~CValueSplitterBar();

    BOOL Create(CWnd* pParentWnd, UINT nID);

    void SetCollapsed(bool bCollapsed);
    bool IsCollapsed() const { return m_bCollapsed; }

protected:
    DECLARE_MESSAGE_MAP()
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnCaptureChanged(CWnd* pWnd);
    afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
    afx_msg void OnMouseLeave();

private:
    CRect GetPillRect() const;

    OciGridView* m_pGrid;
    bool         m_bCollapsed;
    bool         m_bDragging;
    bool         m_bHoverPill;
    bool         m_bTrackingMouse;
    int          m_dragStartX;
    int          m_dragStartWidth;
};

/////////////////////////////////////////////////////////////////////////////
// CValueLineGutter - Gutter com numeração de linhas sincronizada
/////////////////////////////////////////////////////////////////////////////
class CValueLineGutter : public CWnd
{
public:
    CValueLineGutter();
    virtual ~CValueLineGutter();

    BOOL Create(CWnd* pParentWnd, UINT nID, CRichEditCtrl* pEdit);

    void SetFont(CFont* pFont);

protected:
    DECLARE_MESSAGE_MAP()
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);

private:
    CRichEditCtrl* m_pEdit;
    CFont*         m_pFont;
};

/////////////////////////////////////////////////////////////////////////////
// CValueEditCtrl - RichEdit especializado para captura de Ctrl+A / Ctrl+C e context menu
/////////////////////////////////////////////////////////////////////////////
class CValueEditCtrl : public CRichEditCtrl
{
public:
    CValueEditCtrl();
    virtual ~CValueEditCtrl();

    virtual BOOL PreTranslateMessage(MSG* pMsg);

protected:
    DECLARE_MESSAGE_MAP()
    afx_msg void OnContextMenu(CWnd* pWnd, CPoint pos);
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg UINT OnGetDlgCode();
};

/////////////////////////////////////////////////////////////////////////////
// CValuePanelWnd - Painel de exibição e formatação de valores da célula
/////////////////////////////////////////////////////////////////////////////
class CValuePanelWnd : public CWnd
{
public:
    CValuePanelWnd(OciGridView* pGrid);
    virtual ~CValuePanelWnd();

    BOOL Create(CWnd* pParentWnd, UINT nID);

    void SetCellData(const std::string& rawData);
    void ClearData();

    void SetFormat(EValueFormat format);
    EValueFormat GetFormat() const { return m_currentFormat; }

    void SetAutoFormat(bool bAuto);
    bool GetAutoFormat() const { return m_bAutoFormat; }

    void SetWordWrap(bool bWrap);
    bool GetWordWrap() const { return m_bWordWrap; }

    void SetCompact(bool bCompact);
    bool GetCompact() const { return m_bCompact; }

    void SetEncoding(EValueEncoding enc);
    EValueEncoding GetEncoding() const { return m_encoding; }

    void SelectAll();
    void CopySelection();
    void CopyToClipboard();
    void SaveToFile();

    void RefreshDisplay();

    virtual BOOL PreTranslateMessage(MSG* pMsg);

protected:
    DECLARE_MESSAGE_MAP()
    afx_msg int  OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnFormatDropdown();
    afx_msg void OnBtnCopy();
    afx_msg void OnBtnSave();
    afx_msg void OnEditVScroll();
    virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);

private:
    void LayoutChildren(int cx, int cy);
    CRect GetCloseButtonRect() const;
    CRect GetTabRect() const;

    OciGridView*      m_pGrid;
    CFont             m_fontEditor;
    CFont             m_fontUi;
    CFont             m_fontUiBold;

    // Controles filhos
    CButton           m_btnFormat;
    CButton           m_btnCopy;
    CButton           m_btnSave;
    CValueLineGutter  m_wndGutter;
    CValueEditCtrl    m_wndEdit;

    DWORD             m_lastMenuCloseTime;

    // Estado do conteúdo
    std::string          m_rawInput;
    std::vector<uint8_t> m_rawBytes;
    EValueFormat         m_currentFormat;
    bool                 m_bUserSelectedFormat;
    bool                 m_bAutoFormat;
    bool                 m_bWordWrap;
    bool                 m_bCompact;
    EValueEncoding       m_encoding;
};

} // namespace OG2

#endif // __VALUEPANELWND_H__
