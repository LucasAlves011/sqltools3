#pragma once

// Cabecalho de lista moderno: pintura propria sobre o controle nativo (com barra de filtros). O controle
// continua cuidando de cliques, ordenacao e da edicao do filtro; so a aparencia muda.

class CModernHeader : public CHeaderCtrl
{
public:
    CModernHeader ();

    void Attach (HWND hHeader);        // assume um cabecalho ja criado (ex.: o de uma lista)

protected:
    DECLARE_MESSAGE_MAP()
    afx_msg void OnPaint ();
    afx_msg BOOL OnEraseBkgnd (CDC*);
    afx_msg void OnMouseMove (UINT nFlags, CPoint point);
    afx_msg void OnMouseLeave ();
    afx_msg HBRUSH OnCtlColor (CDC* pDC, CWnd* pWnd, UINT nCtlColor);

private:
    CBrush m_editBrush;                // fundo da caixa de edicao do filtro
    int  m_hotItem;
    bool m_tracking;
};
