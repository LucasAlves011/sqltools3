#include "stdafx.h"
#include "OpenEditor/OEPlsSqlIndenter.h"
#include <algorithm>
#include <cwctype>
#include <map>
#include <set>

/*
    Formatacao de PL/SQL pela estrutura dos blocos.

    O texto e quebrado em tokens (palavras, strings, comentarios, simbolos) e percorrido com uma pilha de blocos
    (DECLARE/IS-AS, BEGIN, EXCEPTION, IF, LOOP, CASE). Cada linha que comeca um comando recebe o recuo do nivel
    do bloco; linhas de continuacao de um comando (SELECT em varias linhas, condicoes de IF, parametros) mantem
    o deslocamento que tinham em relacao a primeira linha do comando, para nao desfazer o formato do SQL.

    Etapas: (1) quebras de linha (THEN, LOOP, parametros, um comando por linha), (2) recuo e caixa das palavras,
    (3) alinhamentos (declaracoes, parametros, atribuicoes), (4) linhas em branco e espacos no fim da linha,
    (5) conferencia: o texto novo tem que ter exatamente os mesmos tokens, senao a formatacao e descartada.
*/

namespace OpenEditor
{
namespace
{
    enum ETokType { T_WORD, T_QUOTED, T_STRING, T_NUMBER, T_COMMENT, T_SYM };

    struct Tok
    {
        ETokType type;
        int line, start;        // inicio (linha, indice)
        int endLine, end;       // fim exclusivo (linha, indice)
        std::wstring text;      // palavras em maiusculas; simbolos como estao; vazio para strings/comentarios
    };

    inline bool isWordStart (wchar_t c) { return iswalpha(c) || c == L'_'; }
    inline bool isWordChar  (wchar_t c) { return iswalnum(c) || c == L'_' || c == L'$' || c == L'#'; }

    std::wstring upper (const std::wstring& s)
    {
        std::wstring r(s);
        for (auto& c : r) c = (wchar_t)towupper(c);
        return r;
    }

    class Lexer
    {
    public:
        Lexer (const std::vector<std::wstring>& lines, int lastLine) : m_lines(lines), m_last(lastLine) {}

        void Run (std::vector<Tok>& out)
        {
            int line = 0, i = 0;
            while (line <= m_last)
            {
                const std::wstring& s = m_lines[line];
                if (i >= (int)s.size()) { ++line; i = 0; continue; }
                const wchar_t c = s[i];
                if (iswspace(c)) { ++i; continue; }

                Tok t;
                t.line = line; t.start = i;

                if (c == L'-' && i + 1 < (int)s.size() && s[i + 1] == L'-')
                {
                    t.type = T_COMMENT; t.endLine = line; t.end = (int)s.size();
                }
                else if (c == L'/' && i + 1 < (int)s.size() && s[i + 1] == L'*')
                {
                    t.type = T_COMMENT;
                    scanUntil(line, i + 2, L"*/", t);
                }
                else if (c == L'\'')
                {
                    t.type = T_STRING;
                    scanString(line, i + 1, t);
                }
                else if ((c == L'q' || c == L'Q') && i + 2 < (int)s.size() && s[i + 1] == L'\'')
                {
                    t.type = T_STRING;
                    scanQString(line, i + 2, t);
                }
                else if ((c == L'n' || c == L'N') && i + 1 < (int)s.size() && s[i + 1] == L'\'')
                {
                    t.type = T_STRING;
                    scanString(line, i + 2, t);
                }
                else if ((c == L'n' || c == L'N') && i + 3 < (int)s.size() && (s[i + 1] == L'q' || s[i + 1] == L'Q') && s[i + 2] == L'\'')
                {
                    t.type = T_STRING;
                    scanQString(line, i + 3, t);
                }
                else if (c == L'"')
                {
                    t.type = T_QUOTED;
                    scanUntil(line, i + 1, L"\"", t);
                }
                else if (isWordStart(c))
                {
                    int j = i + 1;
                    while (j < (int)s.size() && isWordChar(s[j])) ++j;
                    t.type = T_WORD; t.endLine = line; t.end = j;
                    t.text = upper(s.substr(i, j - i));
                }
                else if (iswdigit(c))
                {
                    int j = i + 1;
                    while (j < (int)s.size())
                    {
                        if (iswalnum(s[j])) ++j;
                        else if (s[j] == L'.' && !(j + 1 < (int)s.size() && s[j + 1] == L'.')) ++j;   // 1..10 e um intervalo
                        else break;
                    }
                    t.type = T_NUMBER; t.endLine = line; t.end = j;
                }
                else
                {
                    static const wchar_t* const two[] = { L":=", L"=>", L"<<", L">>", L"||", L"..", L"!=", L"<>", L"<=", L">=", L"**" };
                    int len = 1;
                    for (auto op : two)
                        if (i + 1 < (int)s.size() && s[i] == op[0] && s[i + 1] == op[1]) { len = 2; break; }
                    t.type = T_SYM; t.endLine = line; t.end = i + len;
                    t.text = s.substr(i, len);
                }

                out.push_back(t);
                line = t.endLine; i = t.end;
            }
        }

    private:
        // procura 'term' a partir de (line, i); o token termina logo depois dele (ou no fim do texto analisado)
        void scanUntil (int line, int i, const wchar_t* term, Tok& t)
        {
            const size_t tl = wcslen(term);
            for (; line <= m_last; ++line, i = 0)
            {
                size_t p = m_lines[line].find(term, i);
                if (p != std::wstring::npos) { t.endLine = line; t.end = (int)(p + tl); return; }
            }
            t.endLine = m_last; t.end = (int)m_lines[m_last].size();
        }

        void scanString (int line, int i, Tok& t)
        {
            for (; line <= m_last; ++line, i = 0)
            {
                const std::wstring& s = m_lines[line];
                for (; i < (int)s.size(); ++i)
                    if (s[i] == L'\'')
                    {
                        if (i + 1 < (int)s.size() && s[i + 1] == L'\'') { ++i; continue; }
                        t.endLine = line; t.end = i + 1; return;
                    }
            }
            t.endLine = m_last; t.end = (int)m_lines[m_last].size();
        }

        // q'[...]', q'{...}', q'(...)', q'<...>' ou q'X...X'
        void scanQString (int line, int i, Tok& t)
        {
            const wchar_t open = m_lines[line][i];
            wchar_t close = open;
            switch (open) { case L'[': close = L']'; break; case L'{': close = L'}'; break;
                            case L'(': close = L')'; break; case L'<': close = L'>'; break; }
            const wchar_t term[] = { close, L'\'', 0 };
            scanUntil(line, i + 1, term, t);
        }

        const std::vector<std::wstring>& m_lines;
        const int m_last;
    };

    std::vector<Tok> Lex (const std::vector<std::wstring>& lines, int last)
    {
        std::vector<Tok> toks;
        if (!lines.empty() && last >= 0)
            Lexer(lines, std::min(last, (int)lines.size() - 1)).Run(toks);
        return toks;
    }

    const std::set<std::wstring>& Keywords ()
    {
        static const std::set<std::wstring> words = {
            L"AFTER", L"ALL", L"ALTER", L"AND", L"ANY", L"ARRAY", L"AS", L"ASC", L"AT", L"AUTHID", L"AUTONOMOUS_TRANSACTION",
            L"BEFORE", L"BEGIN", L"BETWEEN", L"BODY", L"BULK", L"BULK_EXCEPTIONS", L"BULK_ROWCOUNT", L"BY", L"BYTE", L"CASE",
            L"CHECK", L"CLOSE", L"COLLECT", L"COLUMN", L"COMMENT", L"COMMIT", L"COMPOUND", L"CONNECT", L"CONSTANT",
            L"CONSTRAINT", L"CONSTRUCTOR", L"CONTINUE", L"CREATE", L"CROSS", L"CURRENT", L"CURRENT_USER", L"CURSOR", L"DAY",
            L"DECLARE", L"DEFAULT", L"DEFINER", L"DELETE", L"DESC", L"DETERMINISTIC", L"DISTINCT", L"DROP", L"EACH",
            L"EDITIONABLE", L"ELSE", L"ELSIF", L"END", L"ERRORS", L"ESCAPE", L"EXCEPTION", L"EXCEPTION_INIT", L"EXCEPTIONS",
            L"EXECUTE", L"EXISTS", L"EXIT", L"EXTERNAL", L"FALSE", L"FETCH", L"FINAL", L"FIRST", L"FOLLOWS", L"FOR",
            L"FORALL", L"FOREIGN", L"FOUND", L"FROM", L"FULL", L"FUNCTION", L"GOTO", L"GRANT", L"GROUP", L"HAVING", L"IF",
            L"IMMEDIATE", L"IN", L"INDEX", L"INNER", L"INSERT", L"INSTEAD", L"INTERSECT", L"INTO", L"IS", L"ISOPEN", L"JOIN",
            L"KEY", L"LANGUAGE", L"LAST", L"LEFT", L"LEVEL", L"LIKE", L"LIMIT", L"LOCK", L"LOG", L"LOOP", L"MAP", L"MATCHED",
            L"MEMBER", L"MERGE", L"MINUS", L"MODE", L"NEXT", L"NOCOPY", L"NONEDITIONABLE", L"NOT", L"NOTFOUND", L"NOWAIT",
            L"NULL", L"NULLS", L"OBJECT", L"OF", L"OFF", L"ON", L"OPEN", L"OR", L"ORDER", L"OTHERS", L"OUT", L"OUTER", L"OVER",
            L"OVERRIDING", L"PACKAGE", L"PARALLEL_ENABLE", L"PARTITION", L"PIPE", L"PIPELINED", L"PRAGMA", L"PRIMARY", L"PRIOR",
            L"PROCEDURE", L"RAISE", L"RANGE", L"RECORD", L"REF", L"REFERENCES", L"REFERENCING", L"REPLACE", L"RESULT_CACHE",
            L"RETURN", L"RETURNING", L"REVERSE", L"REVOKE", L"RIGHT", L"ROLLBACK", L"ROW", L"ROWCOUNT", L"ROWNUM", L"ROWS",
            L"ROWTYPE", L"SAVEPOINT", L"SELECT", L"SELF", L"SERIALLY_REUSABLE", L"SET", L"SOME", L"SQL", L"SQLCODE", L"SQLERRM",
            L"START", L"STATIC", L"SUBTYPE", L"SYSDATE", L"SYSTIMESTAMP", L"TABLE", L"THEN", L"TO", L"TRIGGER", L"TRUE",
            L"TYPE", L"UNDER", L"UNION", L"UNIQUE", L"UPDATE", L"USING", L"VALUES", L"VARRAY", L"VIEW", L"WHEN", L"WHERE",
            L"WHILE", L"WITH", L"WORK", L"ZONE",
        };
        return words;
    }

    const std::set<std::wstring>& Types ()
    {
        static const std::set<std::wstring> words = {
            L"BFILE", L"BINARY_DOUBLE", L"BINARY_FLOAT", L"BINARY_INTEGER", L"BLOB", L"BOOLEAN", L"CHAR", L"CHARACTER",
            L"CLOB", L"DATE", L"DEC", L"DECIMAL", L"FLOAT", L"INT", L"INTEGER", L"INTERVAL", L"LONG", L"NATURAL", L"NATURALN",
            L"NCHAR", L"NCLOB", L"NUMBER", L"NUMERIC", L"NVARCHAR2", L"PLS_INTEGER", L"POSITIVE", L"POSITIVEN", L"RAW",
            L"REAL", L"ROWID", L"SIGNTYPE", L"SIMPLE_INTEGER", L"SMALLINT", L"TIMESTAMP", L"UROWID", L"VARCHAR",
            L"VARCHAR2", L"XMLTYPE",
        };
        return words;
    }

    const std::set<std::wstring>& Functions ()
    {
        static const std::set<std::wstring> words = {
            L"ABS", L"ADD_MONTHS", L"ASCII", L"AVG", L"CAST", L"CEIL", L"CHR", L"COALESCE", L"CONCAT", L"COUNT", L"DECODE",
            L"DENSE_RANK", L"EXTRACT", L"FLOOR", L"GREATEST", L"INITCAP", L"INSTR", L"LAG", L"LAST_DAY", L"LEAD", L"LEAST",
            L"LENGTH", L"LISTAGG", L"LOWER", L"LPAD", L"LTRIM", L"MAX", L"MIN", L"MOD", L"MONTHS_BETWEEN", L"NEXT_DAY",
            L"NULLIF", L"NVL", L"NVL2", L"POWER", L"RAISE_APPLICATION_ERROR", L"RANK", L"REGEXP_COUNT", L"REGEXP_INSTR",
            L"REGEXP_LIKE", L"REGEXP_REPLACE", L"REGEXP_SUBSTR", L"ROUND", L"ROW_NUMBER", L"RPAD", L"RTRIM", L"SIGN",
            L"SQRT", L"SUBSTR", L"SUM", L"SYS_CONTEXT", L"SYS_GUID", L"TO_CHAR", L"TO_CLOB", L"TO_DATE", L"TO_NUMBER",
            L"TO_TIMESTAMP", L"TRANSLATE", L"TRIM", L"TRUNC", L"UPPER",
        };
        return words;
    }

    // comandos do SQL*Plus: a linha inteira e um comando (sem ';'), fica na coluna 0 e o texto nao e alterado
    bool IsSqlPlusCommand (const std::wstring& w)
    {
        static const std::set<std::wstring> cmds = {
            L"ACCEPT", L"BREAK", L"BTITLE", L"CLEAR", L"COL", L"COLUMN", L"COMPUTE", L"CONN", L"CONNECT", L"DEF", L"DEFINE",
            L"DESC", L"DESCRIBE", L"EXEC", L"EXECUTE", L"HOST", L"PAUSE", L"PRINT", L"PRO", L"PROMPT", L"REM", L"REMARK",
            L"SET", L"SHO", L"SHOW", L"SPO", L"SPOOL", L"STA", L"START", L"TTITLE", L"UNDEF", L"UNDEFINE", L"VAR",
            L"VARIABLE", L"WHENEVER",
        };
        return cmds.count(w) != 0;
    }

    int IndentCols (const std::wstring& s, int tabSize, int* chars = nullptr)
    {
        int cols = 0, i = 0;
        for (; i < (int)s.size() && (s[i] == L' ' || s[i] == L'\t'); ++i)
            cols = (s[i] == L'\t') ? (cols / tabSize + 1) * tabSize : cols + 1;
        if (chars) *chars = i;
        return cols;
    }

    std::wstring MakeIndent (int cols, const PlSqlIndentOptions& o)
    {
        if (cols <= 0) return std::wstring();
        if (!o.UseTabs()) return std::wstring(cols, L' ');
        return std::wstring(cols / o.tabSize, L'\t') + std::wstring(cols % o.tabSize, L' ');
    }

    enum EFrame { F_DECL, F_BODY, F_EXC, F_IF, F_LOOP, F_CASE };
    struct Frame { EFrame kind; int close; int when; int content; };

    // tipo do comando em andamento: decide o que THEN / IS / AS / LOOP / BEGIN fazem
    enum EStmt { S_OTHER, S_IF, S_ELSIF, S_WHEN, S_END, S_CREATE, S_HEADER, S_TRIGGER, S_CURSOR, S_LABEL, S_CASE_SELECTOR };

    // marcas por token, usadas pela etapa de quebras de linha
    enum EAnn
    {
        A_STMT   = 0x001,   // comeca um comando
        A_OPEN   = 0x002,   // abre um bloco (THEN, ELSE, LOOP, BEGIN, DECLARE, EXCEPTION, IS/AS): o codigo vem depois
        A_CLOSE  = 0x004,   // fecha ou divide um bloco (END, ELSE, ELSIF, EXCEPTION, WHEN)
        A_THEN   = 0x008,   // THEN de IF / ELSIF
        A_LOOP   = 0x010,   // LOOP que termina um FOR / WHILE
        A_POPEN  = 0x020,   // '(' da lista de parametros de procedure / function
        A_PCOMMA = 0x040,   // ',' entre parametros
        A_PCLOSE = 0x080,   // ')' da lista de parametros
        A_SEMI   = 0x100,   // ';' que termina um comando
    };

    // marcas por linha, usadas pelos alinhamentos
    enum ELineFlag { L_DECL = 1, L_PARAM = 2, L_ASSIGN = 4 };

    class Indenter
    {
    public:
        Indenter (const std::vector<std::wstring>& lines, int last, const PlSqlIndentOptions& o, const std::vector<char>* generated)
            : m_lines(lines), m_last(last), m_o(o), m_generated(generated) {}

        const std::vector<Tok>&            Tokens () const    { return m_toks; }
        const std::vector<unsigned short>& Ann () const       { return m_ann; }
        const std::vector<char>&           LineFlags () const { return m_lineFlags; }

        std::vector<std::wstring> Run (int first)
        {
            m_toks = Lex(m_lines, m_last);
            m_ann.assign(m_toks.size(), 0);
            m_lineFlags.assign(m_last + 1, 0);
            const int indentSize = m_o.IndentSize();

            // linhas que comecam dentro de um token de varias linhas (string ou comentario)
            std::vector<int> insideTok(m_last + 1, -1);
            for (int k = 0; k < (int)m_toks.size(); ++k)
                for (int l = m_toks[k].line + 1; l <= m_toks[k].endLine && l <= m_last; ++l)
                    insideTok[l] = k;

            std::vector<std::wstring> out;
            std::vector<int> lineDelta(m_last + 1, 0);
            size_t k = 0;

            for (int line = 0; line <= m_last; ++line)
            {
                const std::wstring& src = m_lines[line];
                std::wstring text = src;
                int origChars = 0;
                const int orig = IndentCols(src, m_o.tabSize, &origChars);
                const bool generated = m_generated && line < (int)m_generated->size() && (*m_generated)[line];

                // tokens que comecam nesta linha
                const size_t kBegin = k;
                while (k < m_toks.size() && m_toks[k].line == line) ++k;
                const size_t kEnd = k;

                int newCols = -1;          // -1: nao mexer no recuo
                bool processTokens = true;
                int  candidate = 0;        // marcas de linha possiveis (confirmadas depois de ler a linha)

                if (insideTok[line] >= 0)
                {
                    const Tok& t = m_toks[insideTok[line]];
                    if (t.type == T_COMMENT && origChars < (int)src.size())
                        newCols = std::max(0, orig + lineDelta[t.line]);     // acompanha a primeira linha do comentario
                }
                else if (origChars >= (int)src.size())
                {
                    text.clear();                                          // linha em branco
                }
                else
                {
                    size_t firstSig = kBegin;
                    while (firstSig < kEnd && m_toks[firstSig].type == T_COMMENT) ++firstSig;
                    const bool hasSig = firstSig < kEnd;

                    std::wstring trimmed = src.substr(origChars);
                    while (!trimmed.empty() && iswspace(trimmed.back())) trimmed.pop_back();

                    if (trimmed == L"/")
                    {
                        // fim de um bloco no SQL*Plus: zera tudo
                        m_frames.clear(); m_inStmt = false;
                        newCols = 0; processTokens = false;
                    }
                    else if (!m_inStmt && m_frames.empty() && kBegin < kEnd &&
                             ((m_toks[kBegin].type == T_WORD && IsSqlPlusCommand(m_toks[kBegin].text)) ||
                              (m_toks[kBegin].type == T_SYM && m_toks[kBegin].text == L"@")))
                    {
                        newCols = 0; processTokens = false;
                    }
                    else if (hasSig)
                    {
                        const Tok& t = m_toks[firstSig];
                        if (m_inStmt && t.type == T_WORD)
                        {
                            // um cabecalho (procedure sem IS, trigger) ou um SQL sem ';' termina quando o bloco comeca
                            if ((t.text == L"BEGIN" || t.text == L"DECLARE") &&
                                (m_stmt == S_HEADER || m_stmt == S_TRIGGER || m_stmt == S_CREATE || m_frames.empty()))
                                m_inStmt = false;
                            else if (t.text == L"CREATE" && m_frames.empty())
                                m_inStmt = false;
                            else if (t.text == L"WHEN" && m_stmt == S_CASE_SELECTOR)
                                m_inStmt = false;
                        }

                        if (m_inStmt)
                        {
                            newCols = ContinuationCols(t, orig, generated);
                            if (m_stmt == S_HEADER && m_paramState == 1 && m_paren == 1 && (t.type == T_WORD || t.type == T_QUOTED))
                                candidate |= L_PARAM;
                        }
                        else
                        {
                            newCols = LevelFor(firstSig) * indentSize;
                            const bool inDecl = !m_frames.empty() && m_frames.back().kind == F_DECL;
                            if (t.type == T_WORD && !Keywords().count(t.text))
                                candidate |= inDecl ? L_DECL : L_ASSIGN;
                            else if (t.type == T_QUOTED)
                                candidate |= inDecl ? L_DECL : L_ASSIGN;
                        }
                    }
                    else
                    {
                        // so comentario: no nivel do bloco, ou acompanhando o comando em andamento
                        newCols = m_inStmt ? m_stmtCols + std::max(0, orig - m_stmtOrig) : ContentLevel() * indentSize;
                    }
                }

                if (newCols >= 0)
                    lineDelta[line] = newCols - orig;

                m_lineCols = newCols >= 0 ? newCols : orig;
                m_lineOrig = orig;
                m_stmtStartsOnLine = 0;

                bool multiLineToken = insideTok[line] >= 0;
                if (processTokens)
                {
                    for (size_t j = kBegin; j < kEnd; ++j)
                    {
                        if (m_toks[j].endLine > line)
                            multiLineToken = true;
                        if (m_toks[j].type == T_COMMENT)
                            continue;
                        OnToken(j);
                        if (line >= first)
                            ApplyCase(j, text);
                    }
                }

                if (candidate && !multiLineToken && line >= first)
                {
                    if ((candidate & L_PARAM))
                        m_lineFlags[line] |= L_PARAM;
                    if ((candidate & L_DECL) && m_stmtStartsOnLine == 1 && !m_inStmt)
                        m_lineFlags[line] |= L_DECL;
                    if ((candidate & L_ASSIGN) && m_stmtStartsOnLine == 1 && !m_inStmt && m_stmtAssign)
                        m_lineFlags[line] |= L_ASSIGN;
                }

                if (line >= first)
                {
                    if (newCols >= 0 && !text.empty())
                    {
                        int chars = 0;
                        IndentCols(text, m_o.tabSize, &chars);
                        text = MakeIndent(newCols, m_o) + text.substr(chars);
                    }
                    out.push_back(text);
                }
            }
            return out;
        }

    private:
        int ContentLevel () const { return m_frames.empty() ? 0 : m_frames.back().content; }

        // recuo de uma linha que continua o comando em andamento
        int ContinuationCols (const Tok& t, int orig, bool generated) const
        {
            const bool closer = (t.type == T_SYM && t.text == L")") ||
                                (t.type == T_WORD && (t.text == L"THEN" || t.text == L"LOOP" || t.text == L"IS" || t.text == L"AS"));
            if (generated || !m_o.keepContinuation)
                return m_stmtCols + (closer ? 0 : m_o.ContinuationSize());
            return m_stmtCols + std::max(0, orig - m_stmtOrig);
        }

        // proximo token significativo depois de k (ou nullptr)
        const Tok* Next (size_t k) const
        {
            for (++k; k < m_toks.size(); ++k)
                if (m_toks[k].type != T_COMMENT)
                    return &m_toks[k];
            return nullptr;
        }

        bool NextIs (size_t k, const wchar_t* word) const
        {
            const Tok* n = Next(k);
            return n && n->type == T_WORD && n->text == word;
        }

        // indice do bloco que um END fecha (END IF / END LOOP / END CASE procuram o bloco do mesmo tipo)
        int FrameClosedByEnd (size_t k) const
        {
            if (m_frames.empty())
                return -1;
            EFrame want; bool typed = true;
            if (NextIs(k, L"IF")) want = F_IF;
            else if (NextIs(k, L"LOOP")) want = F_LOOP;
            else if (NextIs(k, L"CASE")) want = F_CASE;
            else { typed = false; want = F_BODY; }
            if (typed)
                for (int i = (int)m_frames.size() - 1; i >= 0; --i)
                    if (m_frames[i].kind == want)
                        return i;
            return (int)m_frames.size() - 1;
        }

        // nivel de uma linha que comeca um comando com o token k
        int LevelFor (size_t k) const
        {
            const Tok& t = m_toks[k];
            const int content = ContentLevel();
            if (t.type != T_WORD || m_frames.empty())
                return content;
            const Frame& top = m_frames.back();
            const std::wstring& w = t.text;

            if (w == L"END")
            {
                int f = FrameClosedByEnd(k);
                if (f < 0)
                    return content;
                const Frame& fr = m_frames[f];
                if ((fr.kind == F_IF && m_o.indentEndIf) || (fr.kind == F_LOOP && m_o.indentEndLoop))
                    return fr.content;
                return fr.close;
            }
            if (w == L"ELSE" || w == L"ELSIF")
                return top.kind == F_IF ? top.close : top.kind == F_CASE ? top.when : content;
            if (w == L"WHEN")
                return (top.kind == F_CASE || top.kind == F_EXC) ? top.when : content;
            if (w == L"EXCEPTION")
                return top.kind == F_BODY ? top.close : content;
            if (w == L"BEGIN")
                return top.kind == F_DECL ? top.close : content;
            return content;
        }

        void Push (EFrame kind, int close, int content, int when = 0)
        {
            Frame f = { kind, close, when, content };
            m_frames.push_back(f);
        }

        void StartStatement (size_t k)
        {
            const Tok& t = m_toks[k];
            const int level = LevelFor(k);
            m_inStmt = true; m_stmt = S_OTHER; m_stmtLevel = level; m_paren = 0; m_caseExpr = 0;
            m_stmtCols = m_lineCols; m_stmtOrig = m_lineOrig;
            m_paramState = 0; m_headerIsPackage = false; m_stmtAssign = false;
            ++m_stmtStartsOnLine;
            m_ann[k] |= A_STMT;

            if (t.type == T_SYM)
            {
                if (t.text == L"<<") m_stmt = S_LABEL;
                else OnStatementToken(k);
                return;
            }
            if (t.type != T_WORD)
                return;

            const std::wstring& w = t.text;
            Frame* top = m_frames.empty() ? nullptr : &m_frames.back();

            if (w == L"IF")             m_stmt = S_IF;
            else if (w == L"ELSIF")     { m_stmt = S_ELSIF; m_ann[k] |= A_CLOSE; }
            else if (w == L"WHEN")
            {
                m_stmt = S_WHEN;
                if (top && (top->kind == F_CASE || top->kind == F_EXC)) m_ann[k] |= A_CLOSE;
            }
            else if (w == L"CREATE")    m_stmt = S_CREATE;
            else if (w == L"CURSOR")    m_stmt = S_CURSOR;
            else if (w == L"PROCEDURE" || w == L"FUNCTION") m_stmt = S_HEADER;
            else if (w == L"ELSE")
            {
                m_ann[k] |= A_CLOSE | A_OPEN;
                m_inStmt = false;
            }
            else if (w == L"CASE")
            {
                const int when = level + (m_o.indentCaseWhen ? 1 : 0);
                Push(F_CASE, level, when + 1, when);
                m_stmt = S_CASE_SELECTOR;       // "CASE seletor" vai ate o primeiro WHEN
            }
            else if (w == L"LOOP")
            {
                Push(F_LOOP, level, level + 1);
                m_ann[k] |= A_OPEN;
                m_inStmt = false;
            }
            else if (w == L"BEGIN")
            {
                if (top && top->kind == F_DECL) { top->kind = F_BODY; top->content = top->close + 1; }
                else Push(F_BODY, level, level + 1);
                m_ann[k] |= A_OPEN;
                m_inStmt = false;
            }
            else if (w == L"DECLARE")
            {
                Push(F_DECL, level, level + 1);
                m_ann[k] |= A_OPEN;
                m_inStmt = false;
            }
            else if (w == L"EXCEPTION" && top && top->kind == F_BODY)
            {
                top->kind = F_EXC;
                top->when = top->close + (m_o.indentExceptionWhen ? 1 : 0);
                top->content = top->when + 1;
                m_ann[k] |= A_CLOSE | A_OPEN;
                m_inStmt = false;
            }
            else if (w == L"END")
            {
                int f = FrameClosedByEnd(k);
                if (f >= 0) m_frames.resize(f);
                m_stmt = S_END;
                m_ann[k] |= A_CLOSE;
            }
        }

        // token dentro de um comando ja comecado
        void OnStatementToken (size_t k)
        {
            const Tok& t = m_toks[k];
            if (t.type == T_SYM)
            {
                if (t.text == L";") { m_inStmt = false; m_ann[k] |= A_SEMI; }
                else if (t.text == L"(")
                {
                    if (m_stmt == S_HEADER && m_paren == 0 && m_paramState == 0)
                    {
                        m_paramState = 1;
                        m_ann[k] |= A_POPEN;
                    }
                    ++m_paren;
                }
                else if (t.text == L")")
                {
                    if (m_paramState == 1 && m_paren == 1)
                    {
                        m_paramState = 2;
                        m_ann[k] |= A_PCLOSE;
                    }
                    if (m_paren > 0) --m_paren;
                }
                else if (t.text == L",")
                {
                    if (m_paramState == 1 && m_paren == 1)
                        m_ann[k] |= A_PCOMMA;
                }
                else if (t.text == L":=")
                {
                    if (m_paren == 0) m_stmtAssign = true;
                }
                else if (t.text == L">>" && m_stmt == S_LABEL) m_inStmt = false;
                return;
            }
            if (t.type != T_WORD)
                return;

            const std::wstring& w = t.text;
            if (w == L"WHEN" && m_stmt == S_CASE_SELECTOR)
            {
                m_inStmt = false;
                StartStatement(k);
            }
            else if (w == L"CASE")
                ++m_caseExpr;
            else if (w == L"END")
            {
                if (m_caseExpr > 0) --m_caseExpr;
            }
            else if (w == L"THEN")
            {
                if (m_caseExpr == 0)
                {
                    if (m_stmt == S_IF)
                        Push(F_IF, m_stmtLevel, m_stmtLevel + 1);
                    m_ann[k] |= A_OPEN;
                    if (m_stmt == S_IF || m_stmt == S_ELSIF)
                        m_ann[k] |= A_THEN;
                    m_inStmt = false;
                }
            }
            else if (w == L"LOOP")
            {
                if (m_stmt != S_END)
                {
                    Push(F_LOOP, m_stmtLevel, m_stmtLevel + 1);
                    m_ann[k] |= A_OPEN | A_LOOP;
                    m_inStmt = false;
                }
            }
            else if (w == L"IS" || w == L"AS")
            {
                if (m_stmt == S_HEADER && m_paren == 0 &&
                    !NextIs(k, L"LANGUAGE") && !NextIs(k, L"EXTERNAL") && !NextIs(k, L"RESULT"))
                {
                    const bool indent = m_headerIsPackage ? m_o.indentPackage : m_o.indentDeclarations;
                    Push(F_DECL, m_stmtLevel, m_stmtLevel + (indent ? 1 : 0));
                    m_ann[k] |= A_OPEN;
                    m_inStmt = false;
                }
            }
            else if (w == L"PROCEDURE" || w == L"FUNCTION")
            {
                if (m_stmt == S_CREATE || m_stmt == S_OTHER) m_stmt = S_HEADER;   // CREATE FUNCTION, MEMBER FUNCTION
            }
            else if (w == L"PACKAGE")
            {
                if (m_stmt == S_CREATE) { m_stmt = S_HEADER; m_headerIsPackage = true; }
            }
            else if (w == L"TYPE")
            {
                if (m_stmt == S_CREATE && NextIs(k, L"BODY")) { m_stmt = S_HEADER; m_headerIsPackage = true; }
            }
            else if (w == L"TRIGGER")
            {
                if (m_stmt == S_CREATE) m_stmt = S_TRIGGER;
            }
            else if (w == L"BEGIN" || w == L"DECLARE")
            {
                if (m_stmt == S_HEADER || m_stmt == S_TRIGGER || m_stmt == S_CREATE)
                {
                    m_inStmt = false;
                    StartStatement(k);
                }
            }
        }

        void OnToken (size_t k)
        {
            if (!m_inStmt)
                StartStatement(k);
            else
                OnStatementToken(k);
        }

        int CaseMode (int mode) const
        {
            if (mode == PlSqlIndentOptions::CASE_AS_KEYWORDS)    return m_o.keywordCase;
            if (mode == PlSqlIndentOptions::CASE_AS_IDENTIFIERS) return m_o.identifierCase;
            return mode;
        }

        void ApplyCase (size_t k, std::wstring& text) const
        {
            const Tok& t = m_toks[k];
            if (t.type != T_WORD)
                return;
            const wchar_t before = t.start > 0 ? m_lines[t.line][t.start - 1] : 0;
            // variaveis de ligacao e de substituicao ficam como estao
            if (before == L':' || before == L'&')
                return;

            int mode;
            if (before == L'.')                     mode = m_o.identifierCase;   // pacote.rotina, registro.campo
            else if (Keywords().count(t.text))      mode = m_o.keywordCase;
            else if (Types().count(t.text))         mode = CaseMode(m_o.typeCase);
            else if (Functions().count(t.text))     mode = CaseMode(m_o.functionCase);
            else                                    mode = m_o.identifierCase;

            if (mode != PlSqlIndentOptions::CASE_UPPER && mode != PlSqlIndentOptions::CASE_LOWER)
                return;
            for (int i = t.start; i < t.end && i < (int)text.size(); ++i)
                text[i] = (wchar_t)(mode == PlSqlIndentOptions::CASE_UPPER ? towupper(text[i]) : towlower(text[i]));
        }

        const std::vector<std::wstring>& m_lines;
        const int m_last;
        const PlSqlIndentOptions& m_o;
        const std::vector<char>* m_generated;
        std::vector<Tok> m_toks;
        std::vector<unsigned short> m_ann;
        std::vector<char> m_lineFlags;

        std::vector<Frame> m_frames;
        bool  m_inStmt    = false;
        EStmt m_stmt      = S_OTHER;
        int   m_stmtLevel = 0;
        int   m_stmtCols  = 0;     // recuo novo da primeira linha do comando
        int   m_stmtOrig  = 0;     // recuo original da primeira linha do comando
        int   m_lineCols  = 0;     // recuo novo / original da linha em processamento
        int   m_lineOrig  = 0;
        int   m_paren     = 0;
        int   m_caseExpr  = 0;
        int   m_paramState = 0;    // 0 sem lista, 1 dentro da lista de parametros, 2 depois dela
        bool  m_headerIsPackage = false;
        bool  m_stmtAssign = false;
        int   m_stmtStartsOnLine = 0;
    };

    // ---------------------------------------------------------------------------------------------------------
    // Etapa 1: quebras de linha. Trabalha nos intervalos entre tokens: quebrar (por '\n') ou juntar (por ' ').
    // So mexe a partir da linha 'first', entao as linhas anteriores continuam com o mesmo numero.

    std::wstring Slice (const std::vector<std::wstring>& lines, int l1, int i1, int l2, int i2)
    {
        if (l1 == l2)
            return lines[l1].substr(i1, std::max(0, i2 - i1));
        std::wstring r = lines[l1].substr(i1);
        for (int l = l1 + 1; l < l2; ++l)
            r += L'\n' + lines[l];
        r += L'\n' + lines[l2].substr(0, i2);
        return r;
    }

    std::vector<std::wstring> SplitNl (const std::wstring& s)
    {
        std::vector<std::wstring> out;
        size_t p = 0;
        for (;;)
        {
            size_t q = s.find(L'\n', p);
            if (q == std::wstring::npos) { out.push_back(s.substr(p)); break; }
            out.push_back(s.substr(p, q - p));
            p = q + 1;
        }
        return out;
    }

    bool NeedsLineBreaks (const PlSqlIndentOptions& o)
    {
        return o.oneStatementPerLine || o.wrapInnerCode || o.thenPlace != PlSqlIndentOptions::PLACE_KEEP
            || o.loopPlace != PlSqlIndentOptions::PLACE_KEEP || o.paramsOnePerLine
            || o.closeParenPlace != PlSqlIndentOptions::PLACE_KEEP
            || o.spaceAroundOperators || o.spaceAfterComma || o.spaceAfterParen;
    }

    bool IsSpacedOperator (const Tok& t)
    {
        static const std::set<std::wstring> ops = { L":=", L"=>", L"=", L"<>", L"!=", L"<", L">", L"<=", L">=", L"||" };
        return t.type == T_SYM && ops.count(t.text) != 0;
    }

    // dois tokens grudados ("NUMBER:=0", "a,b", ")IS") que devem ganhar um espaco
    bool NeedsSpace (const Tok& prev, const Tok& next, const PlSqlIndentOptions& o)
    {
        if (o.spaceAroundOperators && (IsSpacedOperator(prev) || IsSpacedOperator(next)))
            return true;
        if (o.spaceAfterComma && prev.type == T_SYM && prev.text == L",")
            return true;
        if (o.spaceAfterParen && prev.type == T_SYM && prev.text == L")" && (next.type == T_WORD || next.type == T_QUOTED))
            return true;
        return false;
    }

    void BreakLines (const std::vector<std::wstring>& lines, int last, int first, const PlSqlIndentOptions& o,
                     std::vector<std::wstring>& outLines, std::vector<char>& generated)
    {
        Indenter analysis(lines, last, o, nullptr);
        analysis.Run(last + 1);     // so a analise; nenhuma linha devolvida
        const std::vector<Tok>& toks = analysis.Tokens();
        const std::vector<unsigned short>& ann = analysis.Ann();

        // listas de parametros que devem ficar uma por linha (indice do '(' -> quantidade de parametros)
        std::vector<char> qualifies(toks.size(), 0);
        for (size_t i = 0; i < toks.size(); ++i)
            if (ann[i] & A_POPEN)
            {
                int commas = 0; size_t j = i + 1;
                for (; j < toks.size() && !(ann[j] & A_PCLOSE); ++j)
                    if (ann[j] & A_PCOMMA) ++commas;
                const bool empty = (j == i + 1);
                const bool ok = !empty && j < toks.size() && commas + 1 >= std::max(1, o.paramsMinCount);
                if (ok)
                    for (size_t x = i; x <= j; ++x)
                        if (ann[x] & (A_POPEN | A_PCOMMA | A_PCLOSE)) qualifies[x] = 1;
            }

        enum { KEEP, BREAK, JOIN };
        std::wstring text;
        std::vector<int> genLines;          // linhas criadas por uma quebra
        int curLine = 0;

        auto append = [&](const std::wstring& s) {
            for (wchar_t c : s) if (c == L'\n') ++curLine;
            text += s;
        };

        if (toks.empty())
        {
            outLines.assign(lines.begin(), lines.begin() + last + 1);
            generated.assign(outLines.size(), 0);
            return;
        }

        append(Slice(lines, 0, 0, toks[0].line, toks[0].start));
        for (size_t i = 0; i < toks.size(); ++i)
        {
            if (i > 0)
            {
                const Tok& prev = toks[i - 1];
                const Tok& t = toks[i];
                std::wstring gap = Slice(lines, prev.endLine, prev.end, t.line, t.start);
                const bool hasNl = gap.find(L'\n') != std::wstring::npos;
                int d = KEEP;

                if (prev.endLine >= first && t.line >= first)
                {
                    const bool prevComment = prev.type == T_COMMENT;
                    const unsigned short a = ann[i], pa = ann[i - 1];

                    if (a & A_STMT)
                    {
                        if (o.oneStatementPerLine && (pa & A_SEMI)) d = BREAK;
                        if (o.wrapInnerCode && ((pa & A_OPEN) || (a & A_CLOSE))) d = BREAK;
                    }
                    if (o.paramsOnePerLine && !prevComment && ((pa & A_POPEN) || (pa & A_PCOMMA)) && qualifies[i - 1])
                        d = BREAK;
                    if ((a & A_THEN) && o.thenPlace != PlSqlIndentOptions::PLACE_KEEP)
                        d = o.thenPlace == PlSqlIndentOptions::PLACE_OWN_LINE ? BREAK : (prevComment ? KEEP : JOIN);
                    if ((a & A_LOOP) && o.loopPlace != PlSqlIndentOptions::PLACE_KEEP)
                        d = o.loopPlace == PlSqlIndentOptions::PLACE_OWN_LINE ? BREAK : (prevComment ? KEEP : JOIN);
                    if ((a & A_PCLOSE) && o.closeParenPlace != PlSqlIndentOptions::PLACE_KEEP && (qualifies[i] || !o.paramsOnePerLine))
                        d = o.closeParenPlace == PlSqlIndentOptions::PLACE_OWN_LINE ? BREAK : (prevComment ? KEEP : JOIN);
                    // um comentario de linha nunca pode receber o codigo seguinte
                    if (d == JOIN && prevComment)
                        d = KEEP;
                }

                if (d == BREAK && !hasNl)
                {
                    append(L"\n");
                    genLines.push_back(curLine);
                }
                else if (d == JOIN && hasNl)
                    append(L" ");
                else if (d == KEEP && gap.empty() && prev.endLine >= first && NeedsSpace(prev, t, o))
                    append(L" ");
                else
                    append(gap);
            }
            const Tok& t = toks[i];
            append(Slice(lines, t.line, t.start, t.endLine, t.end));
        }
        const Tok& lastTok = toks.back();
        append(Slice(lines, lastTok.endLine, lastTok.end, last, (int)lines[last].size()));

        outLines = SplitNl(text);
        generated.assign(outLines.size(), 0);
        for (int l : genLines)
            if (l < (int)generated.size()) generated[l] = 1;
    }

    // ---------------------------------------------------------------------------------------------------------
    // Etapa 3: alinhamentos (dentro de grupos de linhas seguidas com o mesmo recuo)

    std::vector<Tok> LexLine (const std::wstring& line)
    {
        std::vector<std::wstring> one(1, line);
        return Lex(one, 0);
    }

    int LeadingChars (const std::wstring& s)
    {
        size_t p = s.find_first_not_of(L" \t");
        return p == std::wstring::npos ? (int)s.size() : (int)p;
    }

    // posicao do primeiro ":=" ou DEFAULT fora de parenteses (depois do primeiro token), ou -1
    int AssignToken (const std::vector<Tok>& toks)
    {
        int paren = 0;
        for (size_t i = 1; i < toks.size(); ++i)
        {
            const Tok& t = toks[i];
            if (t.type == T_SYM && t.text == L"(") ++paren;
            else if (t.type == T_SYM && t.text == L")") --paren;
            else if (paren == 0 && ((t.type == T_SYM && t.text == L":=") || (t.type == T_WORD && t.text == L"DEFAULT")))
                return (int)i;
        }
        return -1;
    }

    // coloca o ":=" / DEFAULT de todas as linhas do grupo na mesma coluna
    void AlignAssign (std::vector<std::wstring>& out, const std::vector<int>& group)
    {
        int target = -1;
        std::map<int, std::pair<int, int>> where;      // linha -> (fim do token anterior, inicio do ":=")
        for (int l : group)
        {
            std::vector<Tok> toks = LexLine(out[l]);
            int a = AssignToken(toks);
            if (a <= 0) continue;
            where[l] = std::make_pair(toks[a - 1].end, toks[a].start);
            target = std::max(target, toks[a - 1].end + 1);
        }
        if (where.size() < 2)
            return;
        for (auto& w : where)
        {
            std::wstring& s = out[w.first];
            s = s.substr(0, w.second.first) + std::wstring(target - w.second.first, L' ') + s.substr(w.second.second);
        }
    }

    void AlignDeclarations (std::vector<std::wstring>& out, const std::vector<int>& group, const PlSqlIndentOptions& o)
    {
        if (o.alignDeclTypes)
        {
            int nameW = 0;
            for (int l : group)
            {
                std::vector<Tok> toks = LexLine(out[l]);
                if (toks.size() >= 2) nameW = std::max(nameW, toks[0].end - toks[0].start);
            }
            for (int l : group)
            {
                std::wstring& s = out[l];
                std::vector<Tok> toks = LexLine(s);
                if (toks.size() < 2 || toks[1].line != 0) continue;
                const std::wstring name = s.substr(toks[0].start, toks[0].end - toks[0].start);
                s = s.substr(0, toks[0].start) + name + std::wstring(nameW - (int)name.size() + 1, L' ') + s.substr(toks[1].start);
            }
        }
        if (o.alignDeclAssign)
            AlignAssign(out, group);
    }

    void AlignParameters (std::vector<std::wstring>& out, const std::vector<int>& group)
    {
        struct P { std::wstring name, mode, rest; int indentChars; bool ok; };
        std::vector<P> ps;
        size_t nameW = 0, modeW = 0;
        for (int l : group)
        {
            const std::wstring& s = out[l];
            std::vector<Tok> toks = LexLine(s);
            P p; p.ok = toks.size() >= 2; p.indentChars = toks.empty() ? 0 : toks[0].start;
            if (p.ok)
            {
                p.name = s.substr(toks[0].start, toks[0].end - toks[0].start);
                size_t i = 1;
                for (; i < toks.size() && toks[i].type == T_WORD &&
                       (toks[i].text == L"IN" || toks[i].text == L"OUT" || toks[i].text == L"NOCOPY"); ++i)
                {
                    if (!p.mode.empty()) p.mode += L' ';
                    p.mode += s.substr(toks[i].start, toks[i].end - toks[i].start);
                }
                if (i < toks.size())
                    p.rest = s.substr(toks[i].start);
                else
                    p.ok = false;
                if (p.ok) { nameW = std::max(nameW, p.name.size()); modeW = std::max(modeW, p.mode.size()); }
            }
            ps.push_back(p);
        }
        for (size_t g = 0; g < group.size(); ++g)
        {
            const P& p = ps[g];
            if (!p.ok) continue;
            std::wstring s = out[group[g]].substr(0, p.indentChars) + p.name + std::wstring(nameW - p.name.size() + 1, L' ');
            if (modeW)
                s += p.mode + std::wstring(modeW - p.mode.size() + 1, L' ');
            s += p.rest;
            out[group[g]] = s;
        }
        AlignAssign(out, group);
    }

    // grupos de linhas seguidas com a marca 'flag' e o mesmo recuo
    std::vector<std::vector<int>> Groups (const std::vector<std::wstring>& out, const std::vector<char>& flags, int first, int flag)
    {
        std::vector<std::vector<int>> groups;
        std::vector<int> cur;
        std::wstring curIndent;
        for (int l = first; l < (int)out.size(); ++l)
        {
            const bool on = l < (int)flags.size() && (flags[l] & flag);
            const std::wstring indent = on ? out[l].substr(0, LeadingChars(out[l])) : std::wstring();
            if (on && !cur.empty() && indent == curIndent && cur.back() == l - 1)
                cur.push_back(l);
            else
            {
                if (cur.size() >= 2) groups.push_back(cur);
                cur.clear();
                if (on) { cur.push_back(l); curIndent = indent; }
            }
        }
        if (cur.size() >= 2) groups.push_back(cur);
        return groups;
    }

    // ---------------------------------------------------------------------------------------------------------
    // Etapa 5: conferencia token a token (so espaco, quebra de linha e caixa de palavras podem mudar)

    std::wstring TokText (const std::vector<std::wstring>& lines, const Tok& t)
    {
        std::wstring s = Slice(lines, t.line, t.start, t.endLine, t.end);
        if (t.type == T_COMMENT)
        {
            // as linhas de dentro de um comentario de bloco podem mudar de recuo
            std::wstring r;
            for (wchar_t c : s) if (!iswspace(c)) r += c;
            return r;
        }
        if (t.type == T_WORD)
            return t.text;
        return s;
    }

    bool SameTokens (const std::vector<std::wstring>& a, const std::vector<std::wstring>& b)
    {
        std::vector<Tok> ta = Lex(a, (int)a.size() - 1), tb = Lex(b, (int)b.size() - 1);
        if (ta.size() != tb.size())
            return false;
        for (size_t i = 0; i < ta.size(); ++i)
            if (ta[i].type != tb[i].type || TokText(a, ta[i]) != TokText(b, tb[i]))
                return false;
        return true;
    }

    std::vector<std::wstring> Format (const std::vector<std::wstring>& lines, int first, int last, const PlSqlIndentOptions& o, bool full)
    {
        // 1. quebras de linha
        std::vector<std::wstring> src(lines.begin(), lines.begin() + last + 1);
        std::vector<char> generated;
        if (full && NeedsLineBreaks(o))
        {
            std::vector<std::wstring> broken;
            BreakLines(src, last, first, o, broken, generated);
            src.swap(broken);
        }
        const int newLast = (int)src.size() - 1;

        // 2. recuo e caixa
        Indenter indenter(src, newLast, o, generated.empty() ? nullptr : &generated);
        std::vector<std::wstring> range = indenter.Run(first);
        std::vector<std::wstring> out(src.begin(), src.begin() + first);
        out.insert(out.end(), range.begin(), range.end());

        // 3. alinhamentos
        if (full)
        {
            const std::vector<char>& flags = indenter.LineFlags();
            if (o.alignDeclTypes || o.alignDeclAssign)
                for (auto& g : Groups(out, flags, first, L_DECL))
                    AlignDeclarations(out, g, o);
            if (o.alignParams)
                for (auto& g : Groups(out, flags, first, L_PARAM))
                    AlignParameters(out, g);
            if (o.alignAssignments)
                for (auto& g : Groups(out, flags, first, L_ASSIGN))
                    AlignAssign(out, g);
        }

        // 4. linhas em branco e espacos no fim (nunca dentro de uma string de varias linhas)
        std::vector<char> inString(out.size(), 0);
        for (const Tok& t : Lex(out, (int)out.size() - 1))
            if ((t.type == T_STRING || t.type == T_QUOTED) && t.endLine > t.line)
                for (int l = t.line; l < t.endLine && l < (int)out.size(); ++l)
                    inString[l] = 1, inString[l + 1] |= 2;      // 1: termina dentro da string, 2: comeca dentro

        std::vector<std::wstring> result;
        int blanks = 0;
        for (int l = first; l < (int)out.size(); ++l)
        {
            std::wstring s = out[l];
            if (o.trimTrailing && !(inString[l] & 1))
                while (!s.empty() && (s.back() == L' ' || s.back() == L'\t')) s.pop_back();
            const bool blank = s.find_first_not_of(L" \t") == std::wstring::npos && !inString[l];
            if (blank)
            {
                if (o.maxBlankLines >= 0 && ++blanks > o.maxBlankLines)
                    continue;
            }
            else
                blanks = 0;
            result.push_back(s);
        }
        return result;
    }
}

// ---------------------------------------------------------------------------------------------------------------

std::wstring PlSqlIndentOptions::ToString () const
{
    const std::pair<const wchar_t*, int> values[] = {
        { L"kw", keywordCase }, { L"id", identifierCase }, { L"ty", typeCase }, { L"fn", functionCase },
        { L"size", indentSize }, { L"tabs", tabsMode }, { L"cont", continuationIndent }, { L"keepcont", keepContinuation },
        { L"pkg", indentPackage }, { L"decl", indentDeclarations }, { L"excw", indentExceptionWhen },
        { L"casew", indentCaseWhen }, { L"endif", indentEndIf }, { L"endloop", indentEndLoop },
        { L"stmt", oneStatementPerLine }, { L"inner", wrapInnerCode }, { L"then", thenPlace }, { L"loop", loopPlace },
        { L"params", paramsOnePerLine }, { L"minparams", paramsMinCount }, { L"rparen", closeParenPlace },
        { L"aldecl", alignDeclTypes }, { L"aldeclassign", alignDeclAssign }, { L"alparams", alignParams },
        { L"alassign", alignAssignments }, { L"blank", maxBlankLines }, { L"trail", trimTrailing },
        { L"spop", spaceAroundOperators }, { L"spcomma", spaceAfterComma }, { L"spparen", spaceAfterParen },
    };
    std::wstring s;
    for (auto& v : values)
        s += std::wstring(v.first) + L'=' + std::to_wstring(v.second) + L';';
    return s;
}

void PlSqlIndentOptions::FromString (const std::wstring& s)
{
    std::map<std::wstring, int> m;
    size_t p = 0;
    while (p < s.size())
    {
        size_t semi = s.find(L';', p);
        if (semi == std::wstring::npos) semi = s.size();
        const std::wstring item = s.substr(p, semi - p);
        const size_t eq = item.find(L'=');
        if (eq != std::wstring::npos)
            m[item.substr(0, eq)] = _wtoi(item.substr(eq + 1).c_str());
        p = semi + 1;
    }
    auto get = [&](const wchar_t* key, int& v) { auto it = m.find(key); if (it != m.end()) v = it->second; };
    auto getb = [&](const wchar_t* key, bool& v) { auto it = m.find(key); if (it != m.end()) v = it->second != 0; };
    get(L"kw", keywordCase); get(L"id", identifierCase); get(L"ty", typeCase); get(L"fn", functionCase);
    get(L"size", indentSize); get(L"tabs", tabsMode); get(L"cont", continuationIndent); getb(L"keepcont", keepContinuation);
    getb(L"pkg", indentPackage); getb(L"decl", indentDeclarations); getb(L"excw", indentExceptionWhen);
    getb(L"casew", indentCaseWhen); getb(L"endif", indentEndIf); getb(L"endloop", indentEndLoop);
    getb(L"stmt", oneStatementPerLine); getb(L"inner", wrapInnerCode); get(L"then", thenPlace); get(L"loop", loopPlace);
    getb(L"params", paramsOnePerLine); get(L"minparams", paramsMinCount); get(L"rparen", closeParenPlace);
    getb(L"aldecl", alignDeclTypes); getb(L"aldeclassign", alignDeclAssign); getb(L"alparams", alignParams);
    getb(L"alassign", alignAssignments); get(L"blank", maxBlankLines); getb(L"trail", trimTrailing);
    getb(L"spop", spaceAroundOperators); getb(L"spcomma", spaceAfterComma); getb(L"spparen", spaceAfterParen);
}

std::vector<PlSqlSpan> PlSqlIndenter::Tokenize (const std::vector<std::wstring>& lines)
{
    std::vector<PlSqlSpan> spans;
    for (const Tok& t : Lex(lines, (int)lines.size() - 1))
    {
        PlSqlSpan s;
        switch (t.type)
        {
        case T_WORD:    if (!Keywords().count(t.text) && !Types().count(t.text)) continue; s.kind = PlSqlSpan::KEYWORD; break;
        case T_COMMENT: s.kind = PlSqlSpan::COMMENT; break;
        case T_STRING:  s.kind = PlSqlSpan::STRING;  break;
        case T_NUMBER:  s.kind = PlSqlSpan::NUMBER;  break;
        case T_QUOTED:  s.kind = PlSqlSpan::QUOTED;  break;
        default:        continue;
        }
        s.line = t.line; s.start = t.start; s.endLine = t.endLine; s.end = t.end;
        spans.push_back(s);
    }
    return spans;
}

std::vector<std::wstring> PlSqlIndenter::Reindent (const std::vector<std::wstring>& lines,
                                                   int firstLine, int lastLine, const PlSqlIndentOptions& options)
{
    if (lines.empty())
        return std::vector<std::wstring>();
    lastLine  = std::min(lastLine, (int)lines.size() - 1);
    firstLine = std::max(0, std::min(firstLine, lastLine));

    PlSqlIndentOptions o = options;
    if (o.editorIndent < 1) o.editorIndent = 4;
    if (o.indentSize < 0) o.indentSize = 0;
    if (o.continuationIndent < 0) o.continuationIndent = 0;
    if (o.tabSize < 1) o.tabSize = 1;

    std::vector<std::wstring> before(lines.begin(), lines.begin() + lastLine + 1);
    std::vector<std::wstring> prefix(lines.begin(), lines.begin() + firstLine);

    // formatacao completa; se a conferencia falhar, so recuo e caixa; se ainda falhar, nada muda
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        std::vector<std::wstring> range = Format(lines, firstLine, lastLine, o, attempt == 0);
        std::vector<std::wstring> after(prefix);
        after.insert(after.end(), range.begin(), range.end());
        if (SameTokens(before, after))
            return range;
    }
    return std::vector<std::wstring>(lines.begin() + firstLine, lines.begin() + lastLine + 1);
}

} // namespace OpenEditor
