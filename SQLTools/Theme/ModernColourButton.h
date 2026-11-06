#pragma once

// Botao de escolha de cor no visual do programa: caixa arredondada nas cores do tema com a amostra da cor e
// a seta da lista. O comportamento (paleta, cor atual, mensagens) continua o do CColourPickerXP.

#include "ColourPickerXP\ColourPickerXP.h"

class CModernColourButton : public CColourPickerXP
{
public:
    virtual void DrawItem (LPDRAWITEMSTRUCT lpDrawItemStruct);

protected:
    virtual void DrawHotArrow (BOOL bHot);
};
