#pragma once

#include <string>
#include "OpenEditor/OEContext.h"
#include "OpenEditor/OELanguageSupport.h"

class COEditorView;

namespace OpenEditor
{

class PlSqlBlockMatcher
{
public:
    // Localiza o par correspondente de um bloco PL/SQL caso o cursor esteja sobre uma palavra-chave
    static bool FindMatchingBlock (COEditorView* pEditor, Position curPos, LanguageSupport::Match& outMatch);

    // Constrói a descrição do escopo do bloco que envolve a linha atual (ex: "Bloco: IF (L.25) > FOR LOOP (L.32)")
    static bool GetEnclosingBlockInfo (COEditorView* pEditor, Position curPos, std::wstring& outScopeDesc);
};

} // namespace OpenEditor
