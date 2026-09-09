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

#pragma once
#ifndef __PlSqlParserEx_h__
#define __PlSqlParserEx_h__

#include "Common/PlSqlParser.h" // Token, EToken, SyntaxAnalyser, TokenMapPtr, ReservedPtr

namespace Common
{
namespace PlsSql
{
	/*
	 * PlSqlParserEx is a standalone SQL tokeniser for the formatter.
	 * It does NOT inherit from PlSqlParser; it owns its own copy of all
	 * required state so there is no risk of interfering with the rest of
	 * the project.
	 *
	 * Improvements over PlSqlParser:
	 *   - signed numeric literals:   +2  -1000  -1.23E4  -1.23E-4
	 *   - multi-line string literals: 'line1\nline2'  (value preserves \n)
	 *   - escaped single quotes:     'Oracle''s Database'
	 *   - alternative q-quoting:     q'[...]'  q'{...}'  q'(...)'  q'<...>'
	 *   - multi-line q-literals spanning multiple PutLine calls
	 *
	 * Token values emitted for QUOTED_STRING always contain the complete
	 * literal text including opening/closing delimiters and embedded newlines.
	 */
	class PlSqlParserEx
	{
	public:
		PlSqlParserEx (SyntaxAnalyser* analyzer, TokenMapPtr tokenMap, ReservedPtr reserved);

		void Clear ();
		bool PutLine (int line, const wchar_t* str, int length);
		void PutEOF  (int line);

	private:
		// emit one token to the analyser
		void emit (EToken type, int line, int offset, int length,
				   const wstring& value);

		// returns true if [digitStart..end) is a valid number body (no sign);
		// on success sets tokenEnd past the last digit/exponent char
		bool scanNumberBody (const wchar_t* str, int length,
							 int digitStart, int& tokenEnd) const;

		SyntaxAnalyser* m_analyzer;
		TokenMapPtr     m_tokenMap;
		ReservedPtr     m_reserved;

		// multi-line string state
		enum EStringState { esNone, esSQuote, esQQuote, esComment, esDQuote };
		EStringState m_strState;
		wchar_t      m_qClose;     // closing delimiter for q-literal
		Token        m_strToken;   // accumulates the multi-line token
	};

}; // namespace PlsSql
}; // namespace Common

#endif//__PlSqlParserEx_h__
