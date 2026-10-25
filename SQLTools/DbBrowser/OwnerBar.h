#pragma once

// Seletor de owner (schema) da Lista de objetos: botao do owner atual, atalhos para os owners favoritos
// e um painel com busca, favoritos, recentes e todos. Substitui a lista suspensa e os botoes fixos.

#include <vector>
#include <list>
#include <string>
#include "Theme/ModernPopup.h"

#define OBN_OWNER_PICK 1        // um atalho foi clicado: GetPicked()
#define OBN_OWNER_OPEN 2        // o botao do owner (ou "+") foi clicado: o pai deve chamar ShowPopup()

class COwnerPopup;

class COwnerBar : public CWnd
{
public:
    COwnerBar ();
    virtual ~COwnerBar ();

    BOOL Create (CWnd* pParent, UINT nID);

    void SetCurrent (const CString& owner);
    const CString& GetCurrent () const      { return m_current; }
    const CString& GetPicked () const       { return m_picked; }

    // favoritos (guardados no perfil do usuario)
    const std::vector<CString>& Favorites () const { return m_favorites; }
    bool IsFavorite (const CString& owner) const;
    void ToggleFavorite (const CString& owner);
    void MoveFavorite (int from, int to);

    // painel
    void ShowPopup (const std::vector<CString>& owners, const std::list<std::wstring>& recents, bool loading);
    void UpdatePopup (const std::vector<CString>& owners, bool loading);
    bool IsPopupOpen () const               { return m_popup != NULL; }
    void PickFromPopup (const CString& owner);

    int MinWidth () const;

protected:
    enum HitKind { HIT_NONE, HIT_PILL, HIT_CHIP, HIT_PLUS };
    struct Hit  { HitKind kind; int id; };
    struct Cell { CRect rc; int id; };

    CString m_current, m_picked;
    std::vector<CString> m_favorites;
    CRect m_rcPill, m_rcPlus;
    std::vector<Cell> m_chips;
    Hit   m_hover;
    bool  m_trackingMouse;
    int   m_layoutWidth;
    COwnerPopup* m_popup;
    DWORD m_popupClosedAt;

    friend class COwnerPopup;
    void OnPopupClosed ();
    void Layout (int width);
    Hit  HitTest (CPoint pt) const;
    void LoadFavorites ();
    void SaveFavorites () const;
    void Notify (UINT code);

    DECLARE_MESSAGE_MAP()
    afx_msg void OnPaint ();
    afx_msg BOOL OnEraseBkgnd (CDC*);
    afx_msg void OnSize (UINT nType, int cx, int cy);
    afx_msg void OnMouseMove (UINT nFlags, CPoint point);
    afx_msg void OnMouseLeave ();
    afx_msg void OnLButtonDown (UINT nFlags, CPoint point);
    afx_msg void OnDestroy ();
};
