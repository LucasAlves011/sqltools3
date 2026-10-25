#pragma once

// Barra de tipos de objeto da Lista de objetos (Alt+3): abas de categoria e botoes de tipo lado a lado,
// com um painel para escolher quais tipos aparecem. Substitui a lista suspensa (modo "combo").

#include <vector>
#include "Theme/ModernPopup.h"

// codigo de notificacao enviado ao pai via WM_COMMAND quando o tipo selecionado muda
#define TBN_TYPE_SELCHANGE 1

class CTypeBar : public CWnd
{
public:
    struct Item
    {
        CString title;
        int     imageIndex;     // IDB_DB_OBJ_IMAGE_INDEX
        int     category;       // 0 Tabelas, 1 Codigo, 2 Objetos, 3 Administracao
    };

    static const int CATEGORY_COUNT = 4;
    static const wchar_t* CategoryName (int category);
    static int CategoryOfType (int typeIndex);

    CTypeBar ();
    virtual ~CTypeBar ();

    BOOL Create (CWnd* pParent, UINT nID);

    void SetItems (const std::vector<Item>& items);
    void SetSel (int index);
    int  GetSel () const                    { return m_sel; }
    int  CalcHeight (int width);

    // personalizacao (guardada no perfil do usuario)
    bool IsTypeVisible (int index) const;
    bool SetTypeVisible (int index, bool visible);      // false se for o ultimo tipo visivel
    int  VisibleCount () const;
    bool GroupByCategory () const           { return m_grouped; }
    void SetGroupByCategory (bool grouped);
    void ResetConfig ();
    const std::vector<Item>& Items () const { return m_items; }

protected:
    enum HitKind { HIT_NONE, HIT_CATEGORY, HIT_TYPE, HIT_GEAR };
    struct Hit { HitKind kind; int id; };

    struct Cell { CRect rc; int id; };

    std::vector<Item> m_items;
    std::vector<bool> m_hidden;
    std::vector<Cell> m_tabs, m_chips;
    CRect m_rcGear;
    int   m_sel, m_cat, m_height, m_layoutWidth;
    bool  m_grouped, m_trackingMouse;
    Hit   m_hover;
    class CTypeBarPopup* m_popup;
    DWORD m_popupClosedAt;

    bool ShowTabs () const;
    void Layout (int width);
    Hit  HitTest (CPoint pt) const;
    void LoadConfig ();
    void SaveConfig () const;
    void Notify ();
    void OpenConfigPopup ();
    void Reflow ();
    void MoveSel (int delta);

    friend class CTypeBarPopup;
    void OnPopupClosed ();

    DECLARE_MESSAGE_MAP()
    afx_msg void OnPaint ();
    afx_msg BOOL OnEraseBkgnd (CDC*);
    afx_msg void OnSize (UINT nType, int cx, int cy);
    afx_msg void OnMouseMove (UINT nFlags, CPoint point);
    afx_msg void OnMouseLeave ();
    afx_msg void OnLButtonDown (UINT nFlags, CPoint point);
    afx_msg void OnKeyDown (UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg UINT OnGetDlgCode ();
    afx_msg void OnSetFocus (CWnd*);
    afx_msg void OnKillFocus (CWnd*);
    afx_msg void OnDestroy ();
};
