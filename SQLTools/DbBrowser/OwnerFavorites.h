#pragma once

// Owners favoritos da Lista de objetos, em dois niveis:
//   - globais: valem para todas as conexoes, editados em Settings (perfil do usuario no registro);
//   - por conexao: favoritados pela estrela do painel de owners, em <pasta de configuracao>\owner-favorites.ini
//     (uma secao por conexao, ex.: [SCOTT@ORCL]).

#include <vector>

namespace OwnerFavorites
{
    std::vector<CString> LoadGlobal ();
    void                 SaveGlobal (const std::vector<CString>& owners);
    // muda a cada SaveGlobal; permite a barra de owners saber que precisa recarregar
    unsigned             GlobalVersion ();

    // false se a conexao ainda nao tem lista propria
    bool LoadConnection (const CString& connection, std::vector<CString>& owners);
    void SaveConnection (const CString& connection, const std::vector<CString>& owners);

    // lista antiga (uma so, no registro): entregue uma unica vez a primeira conexao que abrir
    bool TakeLegacy (std::vector<CString>& owners);
}
