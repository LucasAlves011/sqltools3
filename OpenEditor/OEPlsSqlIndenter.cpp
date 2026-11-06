#include "stdafx.h"
#include "OpenEditor/OEPlsSqlIndenter.h"
#include <algorithm>
#include <cwctype>
#include <set>

/*
    Reindentacao de PL/SQL pela estrutura dos blocos.

    O texto e quebrado em tokens (palavras, strings, comentarios, simbolos) e percorrido com uma pilha de blocos
    (DECLARE/IS-AS, BEGIN, EXCEPTION, IF, LOOP, CASE). Cada linha que comeca um comando recebe o recuo do nivel
    do bloco; linhas de continuacao de um comando (SELECT em varias linhas, condicoes de IF, parametros) mantem
    o deslocamento que tinham em relacao a primeira linha do comando, para nao desfazer o formato do SQL.
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

    const std::set<std::wstring>& Keywords ()
    {
        static const std::set<std::wstring> words = {
            L"AFTER", L"ALL", L"ALTER", L"AND", L"ANY", L"ARRAY", L"AS", L"ASC", L"AT", L"AUTHID", L"AUTONOMOUS_TRANSACTION",
            L"BEFORE", L"BEGIN", L"BETWEEN", L"BINARY_DOUBLE", L"BINARY_FLOAT", L"BINARY_INTEGER", L"BLOB", L"BODY", L"BOOLEAN",
            L"BULK", L"BULK_EXCEPTIONS", L"BULK_ROWCOUNT", L"BY", L"BYTE", L"CASE", L"CHAR", L"CHARACTER", L"CHECK", L"CLOB",
            L"CLOSE", L"COLLECT", L"COLUMN", L"COMMENT", L"COMMIT", L"COMPOUND", L"CONNECT", L"CONSTANT", L"CONSTRAINT",
            L"CONSTRUCTOR", L"CONTINUE", L"CREATE", L"CROSS", L"CURRENT", L"CURRENT_USER", L"CURSOR", L"DATE", L"DAY", L"DEC",
            L"DECIMAL", L"DECLARE", L"DEFAULT", L"DEFINER", L"DELETE", L"DESC", L"DETERMINISTIC", L"DISTINCT", L"DROP", L"EACH",
            L"EDITIONABLE", L"ELSE", L"ELSIF", L"END", L"ERRORS", L"ESCAPE", L"EXCEPTION", L"EXCEPTION_INIT", L"EXCEPTIONS",
            L"EXECUTE", L"EXISTS", L"EXIT", L"EXTERNAL", L"FALSE", L"FETCH", L"FINAL", L"FIRST", L"FLOAT", L"FOLLOWS", L"FOR",
            L"FORALL", L"FOREIGN", L"FOUND", L"FROM", L"FULL", L"FUNCTION", L"GOTO", L"GRANT", L"GROUP", L"HAVING", L"IF",
            L"IMMEDIATE", L"IN", L"INDEX", L"INNER", L"INSERT", L"INSTEAD", L"INTEGER", L"INTERSECT", L"INTERVAL", L"INTO",
            L"IS", L"ISOPEN", L"JOIN", L"KEY", L"LANGUAGE", L"LAST", L"LEFT", L"LEVEL", L"LIKE", L"LIMIT", L"LOCK", L"LOG",
            L"LONG", L"LOOP", L"MAP", L"MATCHED", L"MEMBER", L"MERGE", L"MINUS", L"MODE", L"NATURAL", L"NATURALN", L"NCHAR",
            L"NCLOB", L"NEXT", L"NOCOPY", L"NONEDITIONABLE", L"NOT", L"NOTFOUND", L"NOWAIT", L"NULL", L"NULLS", L"NUMBER",
            L"NUMERIC", L"NVARCHAR2", L"OBJECT", L"OF", L"OFF", L"ON", L"OPEN", L"OR", L"ORDER", L"OTHERS", L"OUT", L"OUTER",
            L"OVER", L"OVERRIDING", L"PACKAGE", L"PARALLEL_ENABLE", L"PARTITION", L"PIPE", L"PIPELINED", L"PLS_INTEGER",
            L"POSITIVE", L"POSITIVEN", L"PRAGMA", L"PRIMARY", L"PRIOR", L"PROCEDURE", L"RAISE", L"RANGE", L"RAW", L"REAL",
            L"RECORD", L"REF", L"REFERENCES", L"REFERENCING", L"REPLACE", L"RESULT_CACHE", L"RETURN", L"RETURNING", L"REVERSE",
            L"REVOKE", L"RIGHT", L"ROLLBACK", L"ROW", L"ROWCOUNT", L"ROWID", L"ROWNUM", L"ROWS", L"ROWTYPE", L"SAVEPOINT",
            L"SELECT", L"SELF", L"SERIALLY_REUSABLE", L"SET", L"SIGNTYPE", L"SMALLINT", L"SOME", L"SQL", L"SQLCODE", L"SQLERRM",
            L"START", L"STATIC", L"SUBTYPE", L"SYSDATE", L"SYSTIMESTAMP", L"TABLE", L"THEN", L"TIMESTAMP", L"TO", L"TRIGGER",
            L"TRUE", L"TYPE", L"UNDER", L"UNION", L"UNIQUE", L"UPDATE", L"USING", L"VALUES", L"VARCHAR", L"VARCHAR2", L"VARRAY",
            L"VIEW", L"WHEN", L"WHERE", L"WHILE", L"WITH", L"WORK", L"ZONE",
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
        if (!o.useTabs) return std::wstring(cols, L' ');
        return std::wstring(cols / o.tabSize, L'\t') + std::wstring(cols % o.tabSize, L' ');
    }

    enum EFrame { F_DECL, F_BODY, F_EXC, F_IF, F_LOOP, F_CASE };
    struct Frame { EFrame kind; int close; int when; int content; };

    // tipo do comando em andamento: decide o que THEN / IS / AS / LOOP / BEGIN fazem
    enum EStmt { S_OTHER, S_IF, S_ELSIF, S_WHEN, S_END, S_CREATE, S_HEADER, S_TRIGGER, S_CURSOR, S_LABEL, S_CASE_SELECTOR };

    class Indenter
    {
    public:
        Indenter (const std::vector<std::wstring>& lines, int last, const PlSqlIndentOptions& o)
            : m_lines(lines), m_last(last), m_o(o) {}

        std::vector<std::wstring> Run (int first)
        {
            Lexer(m_lines, m_last).Run(m_toks);

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

                // tokens que comecam nesta linha
                const size_t kBegin = k;
                while (k < m_toks.size() && m_toks[k].line == line) ++k;
                const size_t kEnd = k;

                int newCols = -1;          // -1: nao mexer no recuo
                bool processTokens = true;

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
                    const bool hasSig = firstSig < kEnd && !(m_toks[firstSig].type == T_COMMENT);

                    std::wstring trimmed = src.substr(origChars);
                    while (!trimmed.empty() && iswspace(trimmed.back())) trimmed.pop_back();

                    if (trimmed == L"/")
                    {
                        // fim de um bloco no SQL*Plus: zera tudo
                        m_frames.clear(); m_inStmt = false;
                        newCols = 0; processTokens = false;
                    }
                    else if (!m_inStmt && m_frames.empty() && kBegin < kEnd && m_toks[kBegin].line == line &&
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
                            newCols = m_stmtCols + std::max(0, orig - m_stmtOrig);
                        else
                            newCols = LevelFor(firstSig) * m_o.indentSize;
                    }
                    else
                    {
                        // so comentario: no nivel do bloco, ou acompanhando o comando em andamento
                        newCols = m_inStmt ? m_stmtCols + std::max(0, orig - m_stmtOrig) : ContentLevel() * m_o.indentSize;
                    }
                }

                if (newCols >= 0)
                    lineDelta[line] = newCols - orig;

                m_lineCols = newCols >= 0 ? newCols : orig;
                m_lineOrig = orig;

                if (processTokens)
                {
                    for (size_t j = kBegin; j < kEnd; ++j)
                    {
                        if (m_toks[j].type == T_COMMENT)
                            continue;
                        OnToken(j);
                        if (line >= first)
                            ApplyCase(j, text);
                    }
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
                return f >= 0 ? m_frames[f].close : content;
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
            else if (w == L"ELSIF")     m_stmt = S_ELSIF;
            else if (w == L"WHEN")      m_stmt = S_WHEN;
            else if (w == L"CREATE")    m_stmt = S_CREATE;
            else if (w == L"CURSOR")    m_stmt = S_CURSOR;
            else if (w == L"PROCEDURE" || w == L"FUNCTION") m_stmt = S_HEADER;
            else if (w == L"ELSE")
                m_inStmt = false;
            else if (w == L"CASE")
            {
                const int when = level + (m_o.indentWhen ? 1 : 0);
                Push(F_CASE, level, when + 1, when);
                m_stmt = S_CASE_SELECTOR;       // "CASE seletor" vai ate o primeiro WHEN
            }
            else if (w == L"LOOP")
            {
                Push(F_LOOP, level, level + 1);
                m_inStmt = false;
            }
            else if (w == L"BEGIN")
            {
                if (top && top->kind == F_DECL) { top->kind = F_BODY; top->content = top->close + 1; }
                else Push(F_BODY, level, level + 1);
                m_inStmt = false;
            }
            else if (w == L"DECLARE")
            {
                Push(F_DECL, level, level + 1);
                m_inStmt = false;
            }
            else if (w == L"EXCEPTION" && top && top->kind == F_BODY)
            {
                top->kind = F_EXC;
                top->when = top->close + (m_o.indentWhen ? 1 : 0);
                top->content = top->when + 1;
                m_inStmt = false;
            }
            else if (w == L"END")
            {
                int f = FrameClosedByEnd(k);
                if (f >= 0) m_frames.resize(f);
                m_stmt = S_END;
            }
        }

        // token dentro de um comando ja comecado
        void OnStatementToken (size_t k)
        {
            const Tok& t = m_toks[k];
            if (t.type == T_SYM)
            {
                if (t.text == L";") { m_inStmt = false; }
                else if (t.text == L"(") ++m_paren;
                else if (t.text == L")") { if (m_paren > 0) --m_paren; }
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
                    m_inStmt = false;
                }
            }
            else if (w == L"LOOP")
            {
                if (m_stmt != S_END)
                {
                    Push(F_LOOP, m_stmtLevel, m_stmtLevel + 1);
                    m_inStmt = false;
                }
            }
            else if (w == L"IS" || w == L"AS")
            {
                if (m_stmt == S_HEADER && m_paren == 0 &&
                    !NextIs(k, L"LANGUAGE") && !NextIs(k, L"EXTERNAL") && !NextIs(k, L"RESULT"))
                {
                    Push(F_DECL, m_stmtLevel, m_stmtLevel + (m_o.indentDeclarations ? 1 : 0));
                    m_inStmt = false;
                }
            }
            else if (w == L"PROCEDURE" || w == L"FUNCTION")
            {
                if (m_stmt == S_CREATE || m_stmt == S_OTHER) m_stmt = S_HEADER;   // CREATE FUNCTION, MEMBER FUNCTION
            }
            else if (w == L"PACKAGE")
            {
                if (m_stmt == S_CREATE) m_stmt = S_HEADER;
            }
            else if (w == L"TYPE")
            {
                if (m_stmt == S_CREATE && NextIs(k, L"BODY")) m_stmt = S_HEADER;
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

        void ApplyCase (size_t k, std::wstring& text) const
        {
            const Tok& t = m_toks[k];
            if (t.type != T_WORD)
                return;
            // variaveis de ligacao e de substituicao ficam como estao
            if (t.start > 0 && (m_lines[t.line][t.start - 1] == L':' || m_lines[t.line][t.start - 1] == L'&'))
                return;
            const int mode = Keywords().count(t.text) ? m_o.keywordCase : m_o.identifierCase;
            if (mode == PlSqlIndentOptions::CASE_KEEP)
                return;
            for (int i = t.start; i < t.end && i < (int)text.size(); ++i)
                text[i] = (wchar_t)(mode == PlSqlIndentOptions::CASE_UPPER ? towupper(text[i]) : towlower(text[i]));
        }

        const std::vector<std::wstring>& m_lines;
        const int m_last;
        const PlSqlIndentOptions& m_o;
        std::vector<Tok> m_toks;

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
    };
}

std::vector<std::wstring> PlSqlIndenter::Reindent (const std::vector<std::wstring>& lines,
                                                   int firstLine, int lastLine, const PlSqlIndentOptions& options)
{
    if (lines.empty())
        return std::vector<std::wstring>();
    lastLine  = std::min(lastLine, (int)lines.size() - 1);
    firstLine = std::max(0, std::min(firstLine, lastLine));

    PlSqlIndentOptions o = options;
    if (o.indentSize < 0) o.indentSize = 0;
    if (o.tabSize < 1) o.tabSize = 1;

    return Indenter(lines, lastLine, o).Run(firstLine);
}

} // namespace OpenEditor
