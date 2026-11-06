#pragma once

#include <string>
#include <vector>

// Lista de opcoes no visual do programa (no lugar de tabelas de propriedades antigas): grupos que abrem e
// fecham, chave liga/desliga, numero com botoes - / + (clicar no numero permite digitar) e a descricao da
// opcao selecionada no rodape. Os valores ficam em variaveis do dono, apontadas por cada opcao; a cada
// mudanca o pai recebe WM_COMMAND com EN_CHANGE.

struct ModernOption
{
    enum EKind
    {
        TOGGLE,         // bool
        NUMBER,         // int
        TOGGLE_NUMBER   // bool que liga a opcao + int (o numero fica apagado com a opcao desligada)
    };

    const wchar_t* group;
    const wchar_t* label;
    const wchar_t* info;
    EKind kind;
    bool* on;
    int*  value;
    int   minValue, maxValue, step;
};

class CModernOptionList : public CWnd
{
public:
    CModernOptionList ();

    BOOL Create (CWnd* parent, UINT id, const CRect& rect);
    void SetOptions (const std::vector<ModernOption>& options);
    void Refresh ()     { Invalidate(FALSE); }

protected:
    DECLARE_MESSAGE_MAP()
    afx_msg void OnPaint ();
    afx_msg BOOL OnEraseBkgnd (CDC*);
    afx_msg void OnSize (UINT, int, int);
    afx_msg void OnLButtonDown (UINT, CPoint);
    afx_msg void OnLButtonDblClk (UINT, CPoint);
    afx_msg void OnMouseMove (UINT, CPoint);
    afx_msg void OnMouseLeave ();
    afx_msg BOOL OnMouseWheel (UINT, short, CPoint);
    afx_msg void OnVScroll (UINT, UINT, CScrollBar*);
    afx_msg void OnKeyDown (UINT, UINT, UINT);
    afx_msg UINT OnGetDlgCode ();
    afx_msg void OnSetFocus (CWnd*);
    afx_msg void OnKillFocus (CWnd*);
    afx_msg BOOL OnSetCursor (CWnd*, UINT, UINT);
    afx_msg HBRUSH OnCtlColor (CDC*, CWnd*, UINT);
    virtual BOOL PreTranslateMessage (MSG* pMsg);
    virtual BOOL OnCommand (WPARAM wParam, LPARAM lParam);

private:
    struct Row { bool group; int option; std::wstring name; };
    enum EPart { P_NONE, P_ROW, P_TOGGLE, P_MINUS, P_VALUE, P_PLUS, P_GROUP };

    void Rebuild ();
    void UpdateScroll ();
    CRect ListRect () const;
    int   RowHeight (int row) const;
    int   RowTop (int row) const;
    CRect ToggleRect (int row) const;
    void  StepperRects (int row, CRect& minus, CRect& value, CRect& plus) const;
    int   HitRow (CPoint pt, EPart& part) const;
    void  Activate (int row, EPart part);
    void  Step (int option, int delta);
    void  BeginEdit (int row);
    void  EndEdit (bool commit);
    void  Changed ();
    void  EnsureVisible (int row);
    bool  NumberEnabled (const ModernOption& o) const;

    std::vector<ModernOption> m_options;
    std::vector<Row> m_rows;
    std::vector<std::wstring> m_collapsed;
    int  m_scroll, m_sel, m_hot;
    EPart m_hotPart;
    bool m_tracking, m_focus;

    CEdit  m_edit;          // edicao do numero
    int    m_editOption;
    CBrush m_editBrush;
    COLORREF m_editBrushColor;
};
