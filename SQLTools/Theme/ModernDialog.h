#pragma once

// Visual moderno para janelas de configuracao (folhas de propriedades): fundo, caixas de selecao,
// botoes de opcao, botoes e campos de texto redesenhados sobre os controles nativos. Somente aparencia:
// os controles continuam os mesmos (estado, DDX, atalhos e foco nao mudam).

#include <CTreePropSheet\PropPageFrameDefault.h>

namespace ModernDialog
{
    // pagina de configuracao (fundo branco)
    void SkinPage (HWND hPage);

    // icone e cor de um botao de comando, pelo id do controle (icone da tira DialogIcons.bmp)
    struct ButtonIcon
    {
        UINT id;
        int icon;           // ModernDraw::DI_*
        COLORREF tint;      // cor do contorno e do realce
    };

    // dialogo comum (nao folha de propriedades): fundo cinza claro, controles redesenhados e,
    // opcionalmente, icones e cores nos botoes de comando
    void SkinDialog (HWND hDlg, const ButtonIcon* icons = NULL, int iconCount = 0);

    // troca de tema com o programa aberto: reaplica o tema a todas as janelas e controles existentes
    void ReThemeAll ();

    // aplica o visual moderno a todo dialogo e folha de propriedades criados pela aplicacao (gancho da thread)
    void InstallGlobalSkin ();

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
