# Oracle Instant Client 19c (Basic Light) embutido

`oci19.bin` e embutido como recurso no `SQLTools.exe` (x64) e extraido em runtime para
`%LOCALAPPDATA%\SQLTools\oci\bundle-<hash>` por `SQLTools/OCI8/EmbeddedOci.cpp`.
Um `OCI.DLL` ja acessivel (pasta do exe ou PATH) tem prioridade sobre o embutido.

Conteudo: `oci.dll`, `oraociicus19.dll`, `orannzsbb19.dll`, `oraons.dll`
(Instant Client 19.32 Basic Light x64). Licenca: `BASIC_LITE_LICENSE` (Oracle OTN).

## Regerar (ex.: nova versao do Instant Client)

1. Baixar o zip Basic Light x64 em oracle.com e extrair as 4 DLLs acima.
2. Compilar a ferramenta (prompt x64 do Visual Studio):

       cd tools\PackOci
       cl /EHsc /O2 PackOci.cpp /link Cabinet.lib

3. Gerar o pacote:

       PackOci.exe ..\..\3dParty\oracle\instantclient19\oci19.bin oci.dll oraociicus19.dll orannzsbb19.dll oraons.dll

O arquivo e versionado via Git LFS (ver `.gitattributes`).
