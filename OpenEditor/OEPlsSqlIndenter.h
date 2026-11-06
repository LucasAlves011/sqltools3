#pragma once

#include <string>
#include <vector>

namespace OpenEditor
{
    // Reindenta codigo PL/SQL (procedures, functions, packages, triggers, blocos anonimos) pela estrutura
    // dos blocos. Mantem as quebras de linha do usuario; so troca o recuo do inicio das linhas e, se pedido,
    // a caixa de palavras-chave e identificadores. Strings nunca sao alteradas.
    //
    // Codigo independente do editor (so std) para poder ser testado isoladamente.
    struct PlSqlIndentOptions
    {
        enum ECase { CASE_KEEP = 0, CASE_UPPER = 1, CASE_LOWER = 2 };

        int  indentSize         = 4;        // espacos por nivel
        int  tabSize            = 4;        // largura do tab (para medir recuos com tab e, com useTabs, para gera-los)
        bool useTabs            = false;
        int  keywordCase        = CASE_KEEP;
        int  identifierCase     = CASE_KEEP;
        bool indentDeclarations = true;     // secao de declaracoes depois de IS/AS um nivel para dentro
        bool indentWhen         = true;     // WHEN de EXCEPTION e de CASE um nivel para dentro
    };

    class PlSqlIndenter
    {
    public:
        // 'lines' e o documento (ou pelo menos ate lastLine); a analise comeca na linha 0 para conhecer o contexto,
        // mas so as linhas [firstLine, lastLine] sao devolvidas (na mesma ordem)
        static std::vector<std::wstring> Reindent (const std::vector<std::wstring>& lines,
                                                   int firstLine, int lastLine, const PlSqlIndentOptions& options);
    };

} // namespace OpenEditor
