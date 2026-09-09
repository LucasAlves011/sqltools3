/* 
	SQLTools is a tool for Oracle database developers and DBAs.
	Copyright (C) 1997-2026 Aleksey Kochetov

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
*/

#include "stdafx.h"
#include <map>
#include "OpenEditor/OEHelpers.h"
#include "Common/PlSqlParserEx.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

namespace Common
{
namespace PlsSql
{
	using std::wstring;

	// Same delimiter set as PlSqlParser
	extern OpenEditor::DelimitersMap m_Delimiters;

// ---------------------------------------------------------------------------
// construction / reset
// ---------------------------------------------------------------------------

PlSqlParserEx::PlSqlParserEx (SyntaxAnalyser* analyzer,
							   TokenMapPtr     tokenMap,
							   ReservedPtr     reserved)
	: m_analyzer(analyzer)
	, m_tokenMap(tokenMap)
	, m_reserved(reserved)
	, m_strState(esNone)
	, m_qClose(0)
{
}

void PlSqlParserEx::Clear ()
{
	m_strState = esNone;
	m_qClose   = 0;
	m_strToken = Token();
}

void PlSqlParserEx::PutEOF (int line)
{
	Token tk;
	tk = etEOF;
	tk.line   = line;
	tk.offset = 0;
	tk.length = 0;
	m_analyzer->PutToken(tk);
}

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

void PlSqlParserEx::emit (EToken type, int line, int offset, int length,
						   const wstring& value)
{
	Token tk;
	tk.token    = type;
	tk.line     = line;
	tk.offset   = static_cast<Token::size_pos>(offset);
	tk.length   = static_cast<Token::size_pos>(length);
	tk.reserved = 0;
	tk.value    = value;
	m_analyzer->PutToken(tk);
}

// Scan digits [. digits] [E [+-] digits] starting at digitStart.
// Returns true and sets tokenEnd past the last character on success.
bool PlSqlParserEx::scanNumberBody (const wchar_t* str, int length,
									 int digitStart, int& tokenEnd) const
{
	int i = digitStart;
	if (i >= length || !iswdigit(str[i])) return false;
	while (i < length && iswdigit(str[i])) ++i;
	if (i < length && str[i] == L'.')
	{
		++i;
		while (i < length && iswdigit(str[i])) ++i;
	}
	if (i < length && (str[i] == L'E' || str[i] == L'e'))
	{
		int save = i++;
		if (i < length && (str[i] == L'+' || str[i] == L'-')) ++i;
		if (i >= length || !iswdigit(str[i]))
			i = save; // not a valid exponent — backtrack
		else
			while (i < length && iswdigit(str[i])) ++i;
	}
	tokenEnd = i;
	return true;
}

// ---------------------------------------------------------------------------
// emit an EOL token
// ---------------------------------------------------------------------------
static void emitEol(SyntaxAnalyser* analyzer, int line, int length)
{
	Token eol;
	eol.token  = etEOL;
	eol.line   = line;
	eol.offset = static_cast<Token::size_pos>(length);
	eol.length = 0;
	eol.value  = L"\n";
	analyzer->PutToken(eol);
}

// ---------------------------------------------------------------------------
// PutLine — main entry point
// ---------------------------------------------------------------------------
bool PlSqlParserEx::PutLine (int line, const wchar_t* str, int length)
{
	// ------------------------------------------------------------------
	// Continuation: we are inside a multi-line token from a previous line.
	// ------------------------------------------------------------------
	if (m_strState == esSQuote)
	{
		m_strToken.value += L'\n';
		for (int i = 0; i < length; ++i)
		{
			m_strToken.value += str[i];
			if (str[i] == L'\'')
			{
				// closing quote
				m_strToken.length = static_cast<Token::size_pos>(m_strToken.value.size());
				m_analyzer->PutToken(m_strToken);
				m_strState = esNone;
				// recurse on whatever follows the closing quote on this line
				if (i + 1 < length)
					PutLine(line, str + i + 1, length - i - 1);
				else
					emitEol(m_analyzer, line, length);
				return true;
			}
		}
		// not closed on this line
		emitEol(m_analyzer, line, length);
		return true;
	}

	if (m_strState == esQQuote)
	{
		m_strToken.value += L'\n';
		for (int i = 0; i < length - 1; ++i)
		{
			m_strToken.value += str[i];
			if (str[i] == m_qClose && str[i+1] == L'\'')
			{
				m_strToken.value += str[i+1]; // closing '
				m_strToken.length = static_cast<Token::size_pos>(m_strToken.value.size());
				m_analyzer->PutToken(m_strToken);
				m_strState = esNone;
				int rest = i + 2;
				if (rest < length)
					PutLine(line, str + rest, length - rest);
				else
					emitEol(m_analyzer, line, length);
				return true;
			}
		}
		// append last char (can't be the two-char close sequence)
		if (length > 0)
			m_strToken.value += str[length - 1];
		emitEol(m_analyzer, line, length);
		return true;
	}

	if (m_strState == esComment)
	{
		m_strToken.value += L'\n';
		for (int i = 0; i < length; ++i)
		{
			m_strToken.value += str[i];
			if (i > 0 && str[i-1] == L'*' && str[i] == L'/')
			{
				m_strToken.length = static_cast<Token::size_pos>(m_strToken.value.size());
				m_analyzer->PutToken(m_strToken);
				m_strState = esNone;
				if (i + 1 < length)
					PutLine(line, str + i + 1, length - i - 1);
				else
					emitEol(m_analyzer, line, length);
				return true;
			}
		}
		emitEol(m_analyzer, line, length);
		return true;
	}

	// ------------------------------------------------------------------
	// Normal line scan
	// ------------------------------------------------------------------
	int i = 0;

	while (i < length)
	{
		// skip whitespace — same as original PlSqlParser: emit nothing
		if (iswspace(str[i]))
		{
			while (i < length && iswspace(str[i])) ++i;
			continue;
		}

		wchar_t ch = str[i];

		// ---- q-quoted string: q'X...X'  (X is a bracket or any char) ----
		if ((ch == L'q' || ch == L'Q') && i + 2 < length && str[i+1] == L'\'')
		{
			wchar_t open  = str[i+2];
			wchar_t close;
			switch (open) {
				case L'[': close = L']'; break;
				case L'{': close = L'}'; break;
				case L'(': close = L')'; break;
				case L'<': close = L'>'; break;
				default:   close = open; break;
			}
			// search for close+' on this line
			int start = i;
			wstring val(str + i, 3); // q'X
			i += 3;
			bool found = false;
			while (i < length)
			{
				val += str[i];
				if (str[i] == close && i + 1 < length && str[i+1] == L'\'')
				{
					val   += str[i+1];
					i     += 2;
					found = true;
					break;
				}
				++i;
			}
			if (found)
			{
				emit(etQUOTED_STRING, line, start, i - start, val);
			}
			else
			{
				// multi-line q-literal
				m_strState        = esQQuote;
				m_qClose          = close;
				m_strToken        = Token();
				m_strToken.token  = etQUOTED_STRING;
				m_strToken.line   = line;
				m_strToken.offset = static_cast<Token::size_pos>(start);
				m_strToken.length = 0;
				m_strToken.value  = val;
				emitEol(m_analyzer, line, length);
				return true;
			}
			continue;
		}

		// ---- regular single-quoted string: handles '' escapes and multi-line ----
		if (ch == L'\'')
		{
			int start = i;
			wstring val;
			val += ch;
			++i;
			bool closed = false;
			while (i < length)
			{
				val += str[i];
				if (str[i] == L'\'')
				{
					if (i + 1 < length && str[i+1] == L'\'')
					{
						// escaped '' — consume both
						++i;
						val += str[i];
						++i;
					}
					else
					{
						++i;
						closed = true;
						break;
					}
				}
				else
					++i;
			}
			if (closed)
			{
				emit(etQUOTED_STRING, line, start, i - start, val);
			}
			else
			{
				// multi-line string
				m_strState        = esSQuote;
				m_strToken        = Token();
				m_strToken.token  = etQUOTED_STRING;
				m_strToken.line   = line;
				m_strToken.offset = static_cast<Token::size_pos>(start);
				m_strToken.length = 0;
				m_strToken.value  = val;
				// BugFix: do not see any reason to emitEol because it is internal to a multi-line string
				// emitEol(m_analyzer, line, length);
				return true;
			}
			continue;
		}

		// ---- double-quoted identifier ----
		if (ch == L'"')
		{
			int start = i;
			wstring val;
			val += ch;
			++i;
			bool closed = false;
			while (i < length)
			{
				val += str[i];
				if (str[i] == L'"') { ++i; closed = true; break; }
				++i;
			}
			if (closed)
				emit(etDOUBLE_QUOTED_STRING, line, start, i - start, val);
			// if not closed (unusual) just drop — identifier can't span lines
			continue;
		}

		// ---- end-line comment: -- ----
		if (ch == L'-' && i + 1 < length && str[i+1] == L'-')
		{
			wstring val(str + i, length - i);
			emit(etCOMMENT, line, i, length - i, val);
			emitEol(m_analyzer, line, length);
			return true;
		}

		// ---- block comment: /* ... */ ----
		if (ch == L'/' && i + 1 < length && str[i+1] == L'*')
		{
			int start = i;
			wstring val;
			val += ch; val += str[i+1];
			i += 2;
			bool closed = false;
			while (i < length)
			{
				val += str[i];
				if (str[i] == L'/' && i > 0 && str[i-1] == L'*')
				{
					++i; closed = true; break;
				}
				++i;
			}
			if (closed)
				emit(etCOMMENT, line, start, i - start, val);
			else
			{
				m_strState        = esComment;
				m_strToken        = Token();
				m_strToken.token  = etCOMMENT;
				m_strToken.line   = line;
				m_strToken.offset = static_cast<Token::size_pos>(start);
				m_strToken.length = 0;
				m_strToken.value  = val;
				emitEol(m_analyzer, line, length);
				return true;
			}
			continue;
		}

		// ---- signed numeric literal: [+-][ws]digits[.digits][E[+-]digits] ----
		if (ch == L'+' || ch == L'-')
		{
			int digitStart = i + 1;
			while (digitStart < length && iswspace(str[digitStart])) ++digitStart;

			if (digitStart < length && iswdigit(str[digitStart]))
			{
				// Unary context: sign is a prefix to a literal, not a binary operator.
				// True when the nearest non-space char before the sign is a keyword
				// letter, comma, opening paren, comparison operator, another sign, or
				// there is no such character (sign is the first token on the line).
				bool unary = false;
				int k = i - 1;
				while (k >= 0 && iswspace(str[k])) --k;
				if (k < 0)
				{
					// Only whitespace (or nothing) before the sign — start of line
					unary = true;
				}
				else
				{
					wchar_t p = str[k];
					unary = (iswalpha(p) || p==L','||p==L'('||p==L'='
										 ||p==L'<'||p==L'>'||p==L'+'||p==L'-');
				}
				if (unary)
				{
					int tokenEnd = 0;
					if (scanNumberBody(str, length, digitStart, tokenEnd))
					{
						// value: sign + number digits (no internal whitespace)
						wstring val;
						val += ch;
						val += wstring(str + digitStart, tokenEnd - digitStart);
						emit(etUNKNOWN, line, i, tokenEnd - i, val);
						i = tokenEnd;
						continue;
					}
				}
			}
		}

		// ---- SQL*Plus / on its own line (etEOS) ----
		if (ch == L'/')
		{
			// check if entire line (before and after) is blank
			bool blankBefore = true, blankAfter = true;
			for (int k = 0; k < i; ++k)
				if (!iswspace(str[k])) { blankBefore = false; break; }
			for (int k = i + 1; k < length; ++k)
				if (!iswspace(str[k])) { blankAfter = false; break; }

			if (blankBefore && blankAfter)
			{
				emit(etEOS, line, i, 1, wstring(1, ch));
				++i;
				continue;
			}
			// ordinary slash — fall through to keyword/delimiter scan
		}

		// ---- two-character operators (must be checked before single-char delimiters) ----
		// Pairs: >= <= <> != := => || **
		if (m_Delimiters[ch] && i + 1 < length)
		{
			wchar_t ch2 = str[i + 1];
			bool isTwoChar = false;
			EToken twoType = etUNKNOWN;

			switch (ch)
			{
			case L'>': if (ch2 == L'=') { twoType = etUNKNOWN; isTwoChar = true; } break; // >=
			case L'<': if (ch2 == L'=' || ch2 == L'>') { twoType = etUNKNOWN; isTwoChar = true; } break; // <= <>
			case L'!': if (ch2 == L'=') { twoType = etUNKNOWN; isTwoChar = true; } break; // !=
			case L':': if (ch2 == L'=') { twoType = etUNKNOWN; isTwoChar = true; } break; // :=
			case L'=': if (ch2 == L'>') { twoType = etUNKNOWN; isTwoChar = true; } break; // =>
			case L'|': if (ch2 == L'|') { twoType = etUNKNOWN; isTwoChar = true; } break; // ||
			case L'*': if (ch2 == L'*') { twoType = etUNKNOWN; isTwoChar = true; } break; // **
			case L'.': if (ch2 == L'.') { twoType = etUNKNOWN; isTwoChar = true; } break; // ..
			}

			if (isTwoChar)
			{
				wstring val(str + i, 2);
				emit(twoType, line, i, 2, val);
				i += 2;
				continue;
			}
		}

		// ---- keyword or plain delimiter ----
		if (m_Delimiters[ch])
		{
			// single-character delimiter
			wstring val(1, ch);
			wstring upper(1, towupper(ch));

			// look up in token map
			auto it = m_tokenMap->find(upper);
			if (it != m_tokenMap->end())
			{
				EToken type = static_cast<EToken>(it->second);
				emit(type, line, i, 1, val);
			}
			else
			{
				// Map common punctuation to their enum values directly
				EToken type = etUNKNOWN;
				switch (ch) {
					case L',': type = etCOMMA;               break;
					case L'.': type = etDOT;                 break;
					case L';': type = etSEMICOLON;           break;
					case L'(': type = etLEFT_ROUND_BRACKET;  break;
					case L')': type = etRIGHT_ROUND_BRACKET; break;
					case L'=': type = etEQUAL;               break;
					case L'<': type = etLESS;                break;
					case L'>': type = etGREATER;             break;
					case L'+': type = etUNKNOWN;             break;
					case L'-': type = etMINUS;               break;
					case L'*': type = etSTAR;                break;
					case L'/': type = etSLASH;               break;
					case L'%': type = etPERCENT_SIGN;        break;
					case L'@': type = etAT_SIGN;             break;
					case L':': type = etCOLON;               break;
					default:   type = etUNKNOWN;             break;
				}
				emit(type, line, i, 1, val);
			}
			++i;
			continue;
		}

		// ---- word token (keyword or identifier) ----
		{
			int start = i;
			wstring val, upper;
			while (i < length && !m_Delimiters[str[i]] && !iswspace(str[i]))
			{
				val   += str[i];
				upper += towupper(str[i]);
				++i;
			}

			// look up reserved words
			int reserved = 0;
			auto resIt = m_reserved->find(upper);
			if (resIt != m_reserved->end())
				reserved = resIt->second;

			auto it = m_tokenMap->find(upper);
			if (it != m_tokenMap->end())
			{
				Token tk;
				tk.token    = static_cast<EToken>(it->second);
				tk.line     = line;
				tk.offset   = static_cast<Token::size_pos>(start);
				tk.length   = static_cast<Token::size_pos>(i - start);
				tk.reserved = reserved;
				tk.value    = val;
				m_analyzer->PutToken(tk);
			}
			else
			{
				Token tk;
				tk.token    = etUNKNOWN;
				tk.line     = line;
				tk.offset   = static_cast<Token::size_pos>(start);
				tk.length   = static_cast<Token::size_pos>(i - start);
				tk.reserved = reserved;
				tk.value    = val;
				m_analyzer->PutToken(tk);
			}
			continue;
		}
	}

	// end of line
	emitEol(m_analyzer, line, length);
	return true;
}

}; // namespace PlsSql
}; // namespace Common
