#pragma once
#ifndef __VALUEPANELWND_H__
#define __VALUEPANELWND_H__

#include <afxwin.h>
#include <afxcmn.h>
#include <richedit.h>
#include <vector>
#include <string>
#include "ValueFormatters.h"

class OciGridView;

namespace OG2
{

class CValuePanelWnd;

/////////////////////////////////////////////////////////////////////////////
// CModernButton - Botão moderno com suporte a ícones vetoriais e hover suave
/////////////////////////////////////////////////////////////////////////////
class CModernButton : public CButton
{
public:
    enum ButtonIcon { ICON_NONE = 0, ICON_FORMAT, ICON_COPY, ICON_SAVE };

    CModernButton();
    virtual ~CModernButton();

    void SetIconType(ButtonIcon icon) { m_iconType = icon; if (GetSafeHwnd()) Invalidate(); }
    void SetDropdown(bool bDropdown) { m_bDropdown = bDropdown; if (GetSafeHwnd()) Invalidate(); }

    virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);

protected:
    DECLARE_MESSAGE_MAP()
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnMouseLeave();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);

private:
    ButtonIcon m_iconType;
    bool       m_bDropdown;
    bool       m_bHover;
    bool       m_bTrackingMouse;
};

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
// CValueFindBar - barra de busca (Ctrl+F) do painel Valor
/////////////////////////////////////////////////////////////////////////////
class CValueFindBar : public CWnd
{
public:
    CValueFindBar(CValuePanelWnd* pPanel);
    virtual ~CValueFindBar();

    BOOL Create(CWnd* pParentWnd, UINT nID);

    static int Height();

    std::wstring GetQuery() const;
    void SetQuery(const std::wstring& text);        // sem disparar a busca
    void FocusEdit(bool selectAll);
    bool IsEditFocused() const;
    void SelectAllText();
    void CopyText();
    void SetStatus(int current, int total, bool capped, const CString& error);
    void ThemeChanged();

    bool m_matchCase;
    bool m_wholeWord;
    bool m_regex;
    bool m_backslash;
    bool m_markAll;

    virtual BOOL PreTranslateMessage(MSG* pMsg);

protected:
    DECLARE_MESSAGE_MAP()
    afx_msg int  OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnMouseLeave();
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    afx_msg void OnEditChange();
    afx_msg void OnEditFocusChanged();

private:
    enum Hit { H_NONE, H_CASE, H_WORD, H_REGEX, H_BACKSLASH, H_MARK, H_PREV, H_NEXT, H_CLOSE };

    void Layout(int cx, int cy);
    CRect RectOf(Hit h) const;
    Hit   HitTest(CPoint pt) const;
    void  Activate(Hit h);
    void  SaveOptions() const;
    void  ToggleOption(Hit h);

    CValuePanelWnd* m_pPanel;
    CEdit           m_edit;
    CToolTipCtrl    m_tip;
    CFont           m_fontUi;
    CFont           m_fontMono;
    CBrush          m_brushEdit;
    COLORREF        m_brushColor;

    CRect m_rcEditBox, m_rcPrev, m_rcNext, m_rcClose;
    CRect m_rcCase, m_rcWord, m_rcRegex, m_rcBackslash, m_rcMark, m_rcStatus;

    Hit   m_hot;
    bool  m_bTrackingMouse;
    bool  m_bSettingText;

    int     m_current, m_total;
    bool    m_capped;
    CString m_error;
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
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg LRESULT OnMouseHWheel(WPARAM wParam, LPARAM lParam);
};

/////////////////////////////////////////////////////////////////////////////
// CValuePanelWnd - Painel de exibição e formatação de valores da célula
/////////////////////////////////////////////////////////////////////////////
// repassa WM_MOUSEWHEEL / WM_MOUSEHWHEEL ao texto do painel Valor se o ponteiro estiver sobre ele
bool ForwardWheelToValuePanel (UINT msg, UINT nFlags, short zDelta, CPoint pt);

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

    void SetFormatCdata(bool on);
    bool GetFormatCdata() const { return m_bFormatCdata; }

    void SetEncoding(EValueEncoding enc);
    EValueEncoding GetEncoding() const { return m_encoding; }

    void SelectAll();
    void CopySelection();
    void CopyToClipboard();
    void SaveToFile();

    void RefreshDisplay();

    // busca (Ctrl+F)
    void ShowFindBar();
    void HideFindBar();
    bool IsFindBarVisible() const { return m_bFindVisible; }
    bool IsFindEditFocused() const;
    void FindNext(bool forward);
    void OnFindChanged(bool jump);      // texto ou opcoes da barra mudaram

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
    afx_msg LRESULT OnThemeChanged(WPARAM, LPARAM);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnMouseLeave();
    virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);

private:
    void LayoutChildren(int cx, int cy);
    CRect GetCloseButtonRect() const;
    CRect GetTabRect() const;

    void RunFind(bool jump);
    void SelectMatch(int index);
    void ApplyMarks();
    void UpdateFindStatus();
    const std::wstring& SearchText();

    OciGridView*      m_pGrid;
    CFont             m_fontEditor;
    CFont             m_fontUi;
    CFont             m_fontUiBold;

    // Controles filhos
    CModernButton     m_btnFormat;
    CModernButton     m_btnCopy;
    CModernButton     m_btnSave;
    CValueLineGutter  m_wndGutter;
    CValueEditCtrl    m_wndEdit;
    CValueFindBar     m_findBar;

    // estado da busca
    bool                  m_bFindVisible;
    std::wstring          m_findText;          // texto do editor (quebras de linha como \n), em cache
    bool                  m_findTextValid;
    std::vector<CHARRANGE> m_matches;
    int                   m_findCur;
    bool                  m_findCapped;
    CString               m_findError;
    bool                  m_marksApplied;

    DWORD             m_lastMenuCloseTime;
    bool              m_bHoverClose;

    // Estado do conteúdo
    std::string          m_rawInput;
    std::vector<uint8_t> m_rawBytes;
    EValueFormat         m_currentFormat;
    bool                 m_bUserSelectedFormat;
    bool                 m_bAutoFormat;
    bool                 m_bWordWrap;
    bool                 m_bCompact;
    bool                 m_bFormatCdata;
    EValueEncoding       m_encoding;
};

} // namespace OG2

#endif // __VALUEPANELWND_H__
