#pragma once

#include <string>
#include "OpenEditor/OEContext.h"

class COEditorView;

namespace OpenEditor
{

class PlSqlSmartIndent
{
public:
    // Retorna a coluna de indentação recomendada para a nova linha após 'prevLine'
    static int CalculateNewLineColumn (COEditorView* pEditor, int prevLine, int baseColumn, int indentSpacing);

    // Ajusta a indentação da linha se ela iniciar com palavra-chave de fechamento (END IF, END LOOP, END, ELSE, ELSIF)
    static bool AdjustLineIndentIfClosing (COEditorView* pEditor, int line, int indentSpacing);

    // Reindenta um intervalo de linhas selecionadas com base na estrutura de blocos
    static void ReindentRange (COEditorView* pEditor, int startLine, int endLine, int indentSpacing);

    // Obtém a coluna do primeiro caractere não-espaço de uma linha
    static int GetLineIndentColumn (COEditorView* pEditor, int line);

    // Altera a indentação inicial de uma linha para a coluna desejada
    static void SetLineIndentColumn (COEditorView* pEditor, int line, int targetCol);
};

} // namespace OpenEditor
