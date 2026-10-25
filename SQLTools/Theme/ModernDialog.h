#pragma once

// Visual moderno para janelas de configuracao (folhas de propriedades): fundo, caixas de selecao,
// botoes de opcao, botoes e campos de texto redesenhados sobre os controles nativos. Somente aparencia:
// os controles continuam os mesmos (estado, DDX, atalhos e foco nao mudam).

#include <CTreePropSheet\PropPageFrameDefault.h>

namespace ModernDialog
{
    // pagina de configuracao (fundo branco)
    void SkinPage (HWND hPage);

    // moldura da janela de configuracao (fundo cinza claro, botoes OK/Cancelar e contorno da arvore)
    void SkinSheet (HWND hSheet);

    // moldura da area das paginas: cartao branco com borda fina no lugar do painel de aba do sistema
    class CPageFrame : public TreePropSheet::CPropPageFrameDefault
    {
    protected:
        DECLARE_MESSAGE_MAP()
        afx_msg BOOL OnEraseBkgnd (CDC* pDC);
    };
}
