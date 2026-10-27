#pragma once

// Caixa de mensagem no visual do programa (substitui a do sistema, que nao segue o tema claro/escuro).
// Mesmo contrato de MessageBox: mesmos tipos (MB_*) e mesmos valores de retorno (ID*).

namespace ModernMessageBox
{
    int Show (CWnd* pParent, LPCTSTR text, LPCTSTR caption, UINT type);
}
