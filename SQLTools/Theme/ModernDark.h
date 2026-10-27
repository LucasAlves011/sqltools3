#pragma once

// Suporte ao tema escuro para o que o MFC e o Windows nao pintam com a paleta: barra de titulo, barra de menus,
// menus de contexto, barras de rolagem e os icones (que sao desenhados em tons de cinza escuro).

namespace ModernDark
{
    // chamar uma vez, na partida, quando o tema escuro esta ativo
    void InitApp ();

    // nome do tema de controles do sistema para listas, arvores e barras de rolagem
    const wchar_t* ControlTheme ();

    // barra de titulo escura (e cor da legenda igual a paleta) para uma janela de nivel superior
    void StyleTitleBar (HWND hWnd);

    // menu principal escuro (a barra de menus e desenhada pelo programa)
    void StyleMenuBar (HWND hFrame);

    // troca os tons escuros dos icones por tons claros; so tem efeito no tema escuro
    // premultiplied: a imagem guarda as cores multiplicadas pelo alfa (imagens das barras de ferramentas do MFC)
    void RecolorBitmap (HBITMAP hBitmap, bool premultiplied = false);

    // barra de ferramentas nativa (CToolBarCtrl): cores para o tema escuro; usar a partir de NM_CUSTOMDRAW
    LRESULT CustomDrawToolbar (NMHDR* pNMHDR);
}
