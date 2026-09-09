#pragma once

#include <string>
#include <vector>
#include "OpenEditor/OEContext.h"

class COEditorView;

namespace OpenEditor
{
    enum PlSqlObjectType
    {
        eObjCursor,
        eObjProcedure,
        eObjFunction
    };

    struct PlSqlObjectInfo
    {
        PlSqlObjectType type = eObjCursor;
        std::wstring name;
        std::wstring params;
        std::wstring title;
        std::wstring selectQuery; // ou código completo da rotina
        int declLineStart = -1;
        int declLineEnd = -1;
        Position selectStart; // Início do trecho editável/substituível no buffer
        Position selectEnd;   // Fim do trecho editável/substituível no buffer
        Position declStart;   // Início da declaração (palavra CURSOR, PROCEDURE ou FUNCTION)
        Position declEnd;     // Fim da declaração (após o ';' final)
    };

    typedef PlSqlObjectInfo PlSqlCursorInfo;

    class PlSqlCursorScanner
    {
    public:
        // Localiza um cursor específico pelo nome (case-insensitive) no editor
        static bool FindCursorByName (COEditorView* pEditor, const std::wstring& cursorName, PlSqlCursorInfo& outInfo);

        // Localiza qualquer objeto local (cursor, procedure local ou function local) pelo nome
        static bool FindLocalObjectByName (COEditorView* pEditor, const std::wstring& objectName, PlSqlObjectInfo& outInfo);

        // Localiza todos os cursores declarados no documento
        static std::vector<PlSqlCursorInfo> FindAllCursors (COEditorView* pEditor);

        // Verifica se uma determinada palavra sob uma posição corresponde a um cursor ou subprograma local declarado
        static bool IsCursorUnderPosition (COEditorView* pEditor, Position pos, PlSqlCursorInfo& outInfo);
        static bool IsObjectUnderPosition (COEditorView* pEditor, Position pos, PlSqlObjectInfo& outInfo);

    private:
        static bool ScanDocument (COEditorView* pEditor, const std::wstring& targetName, PlSqlCursorInfo* pOutSingle, std::vector<PlSqlCursorInfo>* pOutList);
        static bool ScanLocalRoutines (COEditorView* pEditor, const std::wstring& targetName, PlSqlObjectInfo* pOutSingle);
    };
}
