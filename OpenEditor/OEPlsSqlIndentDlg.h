#pragma once

#include <string>
#include <vector>
#include "OpenEditor/OEPlsSqlIndenter.h"

class COEDocument;
class COEditorView;

// uma opcao da janela de estilo: aba, grupo, texto, descricao e o campo de PlSqlIndentOptions que ela altera
struct PlSqlStyleChoice { const wchar_t* text; int value; };

struct PlSqlStyleOption
{
    enum EKind { BOOL, INT, CHOICE };

    int tab;
    const wchar_t* group;
    const wchar_t* label;
    const wchar_t* info;
    EKind kind;
    bool OpenEditor::PlSqlIndentOptions::* b;
    int  OpenEditor::PlSqlIndentOptions::* i;
    const PlSqlStyleChoice* choices;
    int choiceCount;
    int minValue, maxValue;             // INT
    int special;                        // INT: valor mostrado com o texto abaixo (ex.: 0 = recuo do editor)
    const wchar_t* specialText;
};

// abas de texto com sublinhado na cor de destaque (no lugar do controle de abas do Windows)
class CPlSqlStyleTabs : public CWnd
{
public:
    CPlSqlStyleTabs ();
    BOOL Create (CWnd* parent, UINT id, const wchar_t* const* names, int count);
    int  GetCurSel () const { return m_sel; }

protected:
    DECLARE_MESSAGE_MAP()
    afx_msg void OnPaint ();
    afx_msg BOOL OnEraseBkgnd (CDC*);
    afx_msg void OnLButtonDown (UINT, CPoint);
    afx_msg void OnMouseMove (UINT, CPoint);
    afx_msg void OnMouseLeave ();
    afx_msg BOOL OnSetCursor (CWnd*, UINT, UINT);

private:
    int HitTest (CPoint pt) const;
    std::vector<std::wstring> m_names;
    std::vector<CRect> m_rects;
    int  m_sel, m_hot;
    bool m_tracking;
};

// lista de opcoes no visual do programa: grupos que abrem e fecham, chave liga/desliga, botoes - / + para
// numeros e lista suspensa em painel flutuante; a descricao da opcao selecionada aparece no rodape
class CPlSqlStyleOptionList : public CWnd
{
public:
    CPlSqlStyleOptionList ();
    BOOL Create (CWnd* parent, UINT id, const PlSqlStyleOption* options, int count, OpenEditor::PlSqlIndentOptions* values);
    void SetTab (int tab);
    void Refresh ()                 { Invalidate(); }
    void SetChoice (int option, int value);     // chamado pelo painel da lista suspensa

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

private:
    struct Row { bool group; int option; std::wstring groupName; };
    enum EPart { P_NONE, P_ROW, P_TOGGLE, P_CHOICE, P_MINUS, P_PLUS, P_GROUP };

    void Rebuild ();
    void UpdateScroll ();
    int  RowTop (int row) const;
    int  RowHeight (int row) const;
    CRect ListRect () const;
    CRect ControlRect (int row) const;
    void PartRects (int row, CRect& minus, CRect& value, CRect& plus) const;
    int  HitRow (CPoint pt, EPart& part) const;
    void Activate (int row, EPart part);
    void Step (int option, int delta);
    void Changed ();
    void EnsureVisible (int row);
    std::wstring ValueText (int option) const;

    const PlSqlStyleOption* m_options;
    int m_count;
    OpenEditor::PlSqlIndentOptions* m_values;
    int m_tab;
    std::vector<Row> m_rows;
    std::vector<std::wstring> m_collapsed;      // grupos fechados (aba + nome)
    int  m_scroll;
    int  m_sel, m_hot;
    EPart m_hotPart;
    bool m_tracking;
    bool m_focus;
};

// Configuracao do Indent PL/SQL no estilo "Code Style" das IDEs: abas com grupos de opcoes a esquerda e, a direita,
// uma previa que e o proprio editor de codigo (mesmas cores e fonte), reformatada na hora a cada mudanca de opcao.
// Na previa o usuario pode digitar ou colar o proprio codigo.
class COEPlsSqlIndentDlg : public CDialog
{
public:
    // 'options' traz as regras atuais e o que vem do editor (recuo, tab, tabs ou espacos);
    // 'initialText' e a selecao do editor (vazio = codigo de exemplo)
    COEPlsSqlIndentDlg (CWnd* parent, const OpenEditor::PlSqlIndentOptions& options, const std::wstring& initialText);
    ~COEPlsSqlIndentDlg ();

    INT_PTR DoModal () override;

    const OpenEditor::PlSqlIndentOptions& GetOptions () const { return m_options; }

protected:
    BOOL OnInitDialog () override;
    void OnOK () override;
    BOOL OnCommand (WPARAM wParam, LPARAM lParam) override;
    BOOL PreTranslateMessage (MSG* pMsg) override;

    afx_msg void OnSize (UINT nType, int cx, int cy);
    afx_msg void OnGetMinMaxInfo (MINMAXINFO* lpMMI);
    afx_msg void OnDestroy ();
    afx_msg void OnPaint ();
    DECLARE_MESSAGE_MAP()

private:
    void SetPreviewText (const std::wstring& text);
    std::vector<std::wstring> GetPreviewLines () const;
    void Reformat ();
    void Layout (int cx, int cy);

    OpenEditor::PlSqlIndentOptions m_options;
    std::wstring                   m_initialText;

    CPlSqlStyleTabs       m_tabs;
    CPlSqlStyleOptionList m_list;
    CRect     m_previewCard;
    CButton   m_btnSample, m_btnDefaults, m_btnOk, m_btnCancel;

    COEDocument*  m_previewDoc;
    COEditorView* m_preview;

    // a previa e sempre formatada a partir do texto original (e nao do ultimo resultado), para que ligar e
    // desligar uma opcao volte exatamente ao que era; o que o usuario digita passa a ser o novo original
    std::vector<std::wstring> m_source;
    std::vector<std::wstring> m_shown;      // o que a previa mostrou depois da ultima formatacao

    bool m_ready;
    std::vector<unsigned char> m_template;
};
