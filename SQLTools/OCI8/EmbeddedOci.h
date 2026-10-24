#pragma once

// Oracle Instant Client embutido no SQLTools.exe (somente x64).
// As DLLs ficam comprimidas (LZMS) em um recurso RCDATA e sao extraidas na primeira execucao
// para %LOCALAPPDATA%\SQLTools\oci\<id>. Um OCI.DLL ja acessivel (exe dir, PATH) tem prioridade.

namespace EmbeddedOci
{
    // Retorna true quando o diretorio embutido foi ativado (SetDllDirectory).
    // Chamar antes de qualquer uso de OCI.DLL. Nunca lanca excecao.
    bool Activate ();
}
