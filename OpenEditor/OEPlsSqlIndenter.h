#pragma once

#include <string>
#include <vector>

namespace OpenEditor
{
    // Formatacao de codigo PL/SQL (procedures, functions, packages, triggers, blocos anonimos) pela estrutura dos
    // blocos: recuo, maiusculas/minusculas, quebras de linha, alinhamentos e linhas em branco. So espacos, quebras de
    // linha e a caixa das palavras mudam; strings nunca sao alteradas (o resultado e conferido token a token).
    //
    // Codigo independente do editor (so std) para poder ser testado isoladamente.
    struct PlSqlIndentOptions
    {
        // caixa das palavras; tipos e funcoes tambem podem seguir as palavras-chave ou os identificadores
        enum ECase { CASE_KEEP = 0, CASE_UPPER = 1, CASE_LOWER = 2, CASE_AS_KEYWORDS = 3, CASE_AS_IDENTIFIERS = 4 };
        enum ETabs { TABS_EDITOR = 0, TABS_SPACES = 1, TABS_TABS = 2 };
        // posicao de THEN / LOOP / parentese de fechamento
        enum EPlace { PLACE_KEEP = 0, PLACE_SAME_LINE = 1, PLACE_OWN_LINE = 2 };

        // --- Case
        int  keywordCase        = CASE_KEEP;
        int  identifierCase     = CASE_KEEP;
        int  typeCase           = CASE_AS_KEYWORDS;     // NUMBER, VARCHAR2, DATE...
        int  functionCase       = CASE_AS_IDENTIFIERS;  // NVL, TO_CHAR, SUBSTR...

        // --- Tabs and indents
        int  indentSize         = 0;        // 0 = recuo do editor
        int  tabsMode           = TABS_EDITOR;
        int  continuationIndent = 0;        // 0 = igual ao recuo
        bool keepContinuation   = true;     // linhas de continuacao mantem o deslocamento original
        bool indentPackage      = true;     // conteudo de PACKAGE / PACKAGE BODY
        bool indentDeclarations = true;     // declaracoes de procedures e functions (depois de IS / AS)
        bool indentExceptionWhen = true;    // WHEN dentro de EXCEPTION
        bool indentCaseWhen     = true;     // WHEN dentro de CASE
        bool indentEndIf        = false;    // END IF no nivel do codigo de dentro
        bool indentEndLoop      = false;    // END LOOP no nivel do codigo de dentro

        // --- Wrapping
        bool oneStatementPerLine = true;
        bool wrapInnerCode      = true;     // codigo depois de THEN / ELSE / LOOP / BEGIN / IS vai para a linha seguinte
        int  thenPlace          = PLACE_KEEP;
        int  loopPlace          = PLACE_KEEP;
        bool paramsOnePerLine   = false;
        int  paramsMinCount     = 2;        // so quebra listas com pelo menos N parametros
        int  closeParenPlace    = PLACE_KEEP;   // SAME_LINE = junto do ultimo parametro

        // --- Alignment
        bool alignDeclTypes     = true;
        bool alignDeclAssign    = true;
        bool alignParams        = true;
        bool alignAssignments   = false;

        // --- Spaces and blank lines (os espacos so sao acrescentados onde nao ha nenhum; nunca removidos)
        bool spaceAroundOperators = true;   // := => = <> != < > <= >= ||
        bool spaceAfterComma    = true;
        bool spaceAfterParen    = true;     // ")IS" -> ") IS"
        int  maxBlankLines      = 2;        // -1 = manter todas
        bool trimTrailing       = true;

        // valores vindos do editor (nao sao configurados aqui)
        int  editorIndent       = 4;
        int  tabSize            = 4;
        bool editorUsesTabs     = false;

        std::wstring ToString () const;
        void FromString (const std::wstring&);      // chaves desconhecidas sao ignoradas

        int  IndentSize () const        { return indentSize > 0 ? indentSize : editorIndent; }
        int  ContinuationSize () const  { return continuationIndent > 0 ? continuationIndent : IndentSize(); }
        bool UseTabs () const           { return tabsMode == TABS_EDITOR ? editorUsesTabs : tabsMode == TABS_TABS; }
    };

    // trecho de texto classificado, para colorir uma previa (posicoes por linha/indice, fim exclusivo)
    struct PlSqlSpan
    {
        enum EKind { KEYWORD, COMMENT, STRING, NUMBER, QUOTED };
        EKind kind;
        int line, start, endLine, end;
    };

    class PlSqlIndenter
    {
    public:
        static std::vector<PlSqlSpan> Tokenize (const std::vector<std::wstring>& lines);

        // 'lines' e o documento (ou pelo menos ate lastLine); a analise comeca na linha 0 para conhecer o contexto,
        // mas so o trecho [firstLine, lastLine] e formatado e devolvido. Com quebras de linha ligadas o numero de
        // linhas devolvidas pode ser diferente do original.
        static std::vector<std::wstring> Reindent (const std::vector<std::wstring>& lines,
                                                   int firstLine, int lastLine, const PlSqlIndentOptions& options);
    };

} // namespace OpenEditor
