#include "stdafx.h"
#include "ValueFormatters.h"
#include <sstream>
#include <iomanip>
#include <cctype>
#include <algorithm>

namespace OG2
{

static inline int HexVal(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool ValueFormatters::IsLikelyHex(const std::string& input)
{
    if (input.empty() || input == "[NULL]" || input == "[BLOB]")
        return false;

    size_t start = 0;
    size_t end = input.size();

    // Strip optional enclosing single quotes
    if (end >= 2 && input[0] == '\'' && input[end - 1] == '\'')
    {
        start = 1;
        end = end - 1;
    }

    size_t hexCount = 0;
    for (size_t i = start; i < end; ++i)
    {
        char c = input[i];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
            continue;
        if (HexVal(c) == -1)
            return false;
        ++hexCount;
    }

    // Must have even number of hex characters and at least 4 chars
    return (hexCount >= 4 && (hexCount % 2 == 0));
}

bool ValueFormatters::HexToBytes(const std::string& input, std::vector<uint8_t>& outBytes)
{
    outBytes.clear();
    if (input.empty())
        return false;

    size_t start = 0;
    size_t end = input.size();

    if (end >= 2 && input[0] == '\'' && input[end - 1] == '\'')
    {
        start = 1;
        end = end - 1;
    }

    // Clean hex string
    std::string clean;
    clean.reserve(end - start);
    for (size_t i = start; i < end; ++i)
    {
        char c = input[i];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
            continue;
        if (HexVal(c) == -1)
            return false;
        clean.push_back(c);
    }

    if (clean.empty() || clean.size() % 2 != 0)
        return false;

    outBytes.reserve(clean.size() / 2);
    for (size_t i = 0; i < clean.size(); i += 2)
    {
        int hi = HexVal(clean[i]);
        int lo = HexVal(clean[i + 1]);
        if (hi == -1 || lo == -1)
        {
            outBytes.clear();
            return false;
        }
        outBytes.push_back(static_cast<uint8_t>((hi << 4) | lo));
    }

    return true;
}

std::wstring ValueFormatters::BytesToWString(const std::vector<uint8_t>& bytes, EValueEncoding encoding)
{
    if (bytes.empty())
        return std::wstring();

    UINT codePage = (encoding == VE_ANSI) ? CP_ACP : CP_UTF8;

    int wlen = MultiByteToWideChar(codePage, 0, (const char*)bytes.data(), (int)bytes.size(), NULL, 0);
    if (wlen > 0)
    {
        std::wstring result(wlen, L'\0');
        MultiByteToWideChar(codePage, 0, (const char*)bytes.data(), (int)bytes.size(), &result[0], wlen);
        return result;
    }

    // Fallback if UTF-8 failed: try CP_ACP
    if (codePage == CP_UTF8)
    {
        wlen = MultiByteToWideChar(CP_ACP, 0, (const char*)bytes.data(), (int)bytes.size(), NULL, 0);
        if (wlen > 0)
        {
            std::wstring result(wlen, L'\0');
            MultiByteToWideChar(CP_ACP, 0, (const char*)bytes.data(), (int)bytes.size(), &result[0], wlen);
            return result;
        }
    }

    // Ultimate fallback: direct byte-by-byte conversion
    std::wstring fallback;
    fallback.reserve(bytes.size());
    for (uint8_t b : bytes)
        fallback.push_back((wchar_t)b);
    return fallback;
}

std::string ValueFormatters::BytesToString(const std::vector<uint8_t>& bytes)
{
    if (bytes.empty())
        return std::string();
    return std::string((const char*)bytes.data(), bytes.size());
}

EValueFormat ValueFormatters::DetectFormat(const std::vector<uint8_t>& bytes, const std::string& rawText)
{
    std::string text;
    if (!bytes.empty())
        text = BytesToString(bytes);
    else
        text = rawText;

    if (text.empty())
        return VF_TEXT;

    // Skip leading whitespace and BOM
    size_t i = 0;
    if (text.size() >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF)
        i = 3;

    while (i < text.size() && (text[i] == ' ' || text[i] == '\t' || text[i] == '\r' || text[i] == '\n'))
        ++i;

    if (i < text.size())
    {
        char firstChar = text[i];
        if (firstChar == '<')
        {
            // Check if looks like XML
            if (i + 1 < text.size())
            {
                char nextChar = text[i + 1];
                if (nextChar == '?' || nextChar == '!' || isalpha((unsigned char)nextChar) || nextChar == '_')
                    return VF_XML;
            }
        }
        else if (firstChar == '{' || firstChar == '[')
        {
            return VF_JSON;
        }
    }

    // Check ratio of printable characters to determine Binary vs Text
    size_t checkLen = (std::min)(bytes.size(), size_t(2048));
    if (checkLen > 0)
    {
        size_t printableCount = 0;
        size_t nullCount = 0;
        for (size_t k = 0; k < checkLen; ++k)
        {
            uint8_t b = bytes[k];
            if (b == 0)
                ++nullCount;
            else if (b == '\r' || b == '\n' || b == '\t' || (b >= 32 && b <= 126) || b >= 128)
                ++printableCount;
        }

        if (nullCount > 0 || (printableCount * 100 / checkLen < 80))
            return VF_BINARY;
    }

    return VF_TEXT;
}

std::wstring ValueFormatters::FormatXml(const std::string& text, bool autoFormat, bool compact)
{
    if (text.empty())
        return std::wstring();

    if (!autoFormat && !compact)
    {
        std::vector<uint8_t> b(text.begin(), text.end());
        return BytesToWString(b, VE_UTF8);
    }

    if (compact)
    {
        // Remove whitespace between tags
        std::string out;
        out.reserve(text.size());
        bool inTag = false;
        bool inQuotes = false;
        char quoteChar = 0;

        for (size_t i = 0; i < text.size(); ++i)
        {
            char c = text[i];
            if (inTag)
            {
                if (inQuotes)
                {
                    if (c == quoteChar) inQuotes = false;
                    out.push_back(c);
                }
                else
                {
                    if (c == '\'' || c == '"') { inQuotes = true; quoteChar = c; }
                    if (c == '>') inTag = false;
                    out.push_back(c);
                }
            }
            else
            {
                if (c == '<')
                {
                    inTag = true;
                    // trim trailing whitespace from out before '<'
                    while (!out.empty() && (out.back() == ' ' || out.back() == '\t' || out.back() == '\r' || out.back() == '\n'))
                        out.pop_back();
                    out.push_back(c);
                }
                else
                {
                    out.push_back(c);
                }
            }
        }
        std::vector<uint8_t> b(out.begin(), out.end());
        return BytesToWString(b, VE_UTF8);
    }

    // Pretty Print XML with 2 spaces indent
    std::wstring wtext;
    {
        std::vector<uint8_t> b(text.begin(), text.end());
        wtext = BytesToWString(b, VE_UTF8);
    }

    std::wostringstream out;
    int indent = 0;
    size_t i = 0;
    const size_t len = wtext.size();

    // Skip BOM if present
    if (len > 0 && wtext[0] == 0xFEFF)
        i = 1;

    auto makeIndent = [](int lvl) -> std::wstring {
        if (lvl < 0) lvl = 0;
        return std::wstring(lvl * 2, L' ');
    };

    while (i < len)
    {
        // Skip whitespace between elements
        while (i < len && (wtext[i] == L' ' || wtext[i] == L'\t' || wtext[i] == L'\r' || wtext[i] == L'\n'))
            ++i;
        if (i >= len) break;

        if (wtext[i] == L'<')
        {
            size_t tagStart = i;
            size_t tagEnd = wtext.find(L'>', tagStart);
            if (tagEnd == std::wstring::npos)
            {
                // Unclosed tag: output remaining
                out << makeIndent(indent) << wtext.substr(i) << L"\r\n";
                break;
            }

            std::wstring tag = wtext.substr(tagStart, tagEnd - tagStart + 1);
            i = tagEnd + 1;

            if (tag.size() >= 2 && tag[1] == L'?')
            {
                // XML declaration: <?xml ... ?>
                out << makeIndent(indent) << tag << L"\r\n";
            }
            else if (tag.size() >= 4 && tag[1] == L'!' && tag[2] == L'-' && tag[3] == L'-')
            {
                // Comment: <!-- ... -->
                out << makeIndent(indent) << tag << L"\r\n";
            }
            else if (tag.size() >= 9 && tag.substr(0, 9) == L"<![CDATA[")
            {
                // CDATA
                out << makeIndent(indent) << tag << L"\r\n";
            }
            else if (tag.size() >= 2 && tag[1] == L'/')
            {
                // Closing tag: </tag>
                --indent;
                out << makeIndent(indent) << tag << L"\r\n";
            }
            else if (tag.size() >= 3 && tag[tag.size() - 2] == L'/')
            {
                // Self closing tag: <tag ... />
                out << makeIndent(indent) << tag << L"\r\n";
            }
            else
            {
                // Opening tag: <tag ...>
                // Check if followed by text and closing tag on same line: e.g. <id>123</id>
                size_t nextTagStart = wtext.find(L'<', i);
                if (nextTagStart != std::wstring::npos && nextTagStart > i)
                {
                    size_t nextTagEnd = wtext.find(L'>', nextTagStart);
                    if (nextTagEnd != std::wstring::npos && (nextTagStart + 1 < nextTagEnd) && wtext[nextTagStart + 1] == L'/')
                    {
                        // Extract tag name to ensure matching close tag
                        std::wstring innerText = wtext.substr(i, nextTagStart - i);
                        std::wstring closeTag = wtext.substr(nextTagStart, nextTagEnd - nextTagStart + 1);

                        // If inner text does not have multiple lines, keep on single line
                        if (innerText.find(L'\n') == std::wstring::npos)
                        {
                            out << makeIndent(indent) << tag << innerText << closeTag << L"\r\n";
                            i = nextTagEnd + 1;
                            continue;
                        }
                    }
                }

                out << makeIndent(indent) << tag << L"\r\n";
                ++indent;
            }
        }
        else
        {
            // Text outside tags
            size_t nextTag = wtext.find(L'<', i);
            std::wstring content;
            if (nextTag == std::wstring::npos)
            {
                content = wtext.substr(i);
                i = len;
            }
            else
            {
                content = wtext.substr(i, nextTag - i);
                i = nextTag;
            }

            // Trim leading/trailing whitespace
            size_t s = content.find_first_not_of(L" \t\r\n");
            if (s != std::wstring::npos)
            {
                size_t e = content.find_last_not_of(L" \t\r\n");
                out << makeIndent(indent) << content.substr(s, e - s + 1) << L"\r\n";
            }
        }
    }

    return out.str();
}

std::wstring ValueFormatters::FormatJson(const std::string& text, bool autoFormat, bool compact)
{
    if (text.empty())
        return std::wstring();

    std::wstring wtext;
    {
        std::vector<uint8_t> b(text.begin(), text.end());
        wtext = BytesToWString(b, VE_UTF8);
    }

    if (!autoFormat && !compact)
        return wtext;

    if (compact)
    {
        std::wostringstream out;
        bool inString = false;
        bool escape = false;

        for (wchar_t c : wtext)
        {
            if (inString)
            {
                out << c;
                if (escape)
                    escape = false;
                else if (c == L'\\')
                    escape = true;
                else if (c == L'"')
                    inString = false;
            }
            else
            {
                if (c == L'"')
                {
                    inString = true;
                    out << c;
                }
                else if (c != L' ' && c != L'\t' && c != L'\r' && c != L'\n')
                {
                    out << c;
                }
            }
        }
        return out.str();
    }

    // Pretty Print JSON with 2 spaces indent
    std::wostringstream out;
    int indent = 0;
    bool inString = false;
    bool escape = false;

    auto makeIndent = [](int lvl) -> std::wstring {
        if (lvl < 0) lvl = 0;
        return std::wstring(lvl * 2, L' ');
    };

    for (size_t i = 0; i < wtext.size(); ++i)
    {
        wchar_t c = wtext[i];

        if (inString)
        {
            out << c;
            if (escape)
                escape = false;
            else if (c == L'\\')
                escape = true;
            else if (c == L'"')
                inString = false;
            continue;
        }

        if (c == L' ' || c == L'\t' || c == L'\r' || c == L'\n')
            continue;

        if (c == L'"')
        {
            inString = true;
            out << c;
        }
        else if (c == L'{' || c == L'[')
        {
            // Check if immediately closed: {} or []
            size_t next = i + 1;
            while (next < wtext.size() && (wtext[next] == L' ' || wtext[next] == L'\t' || wtext[next] == L'\r' || wtext[next] == L'\n'))
                ++next;

            if (next < wtext.size() && ((c == L'{' && wtext[next] == L'}') || (c == L'[' && wtext[next] == L']')))
            {
                out << c << wtext[next];
                i = next;
            }
            else
            {
                out << c << L"\r\n";
                ++indent;
                out << makeIndent(indent);
            }
        }
        else if (c == L'}' || c == L']')
        {
            --indent;
            out << L"\r\n" << makeIndent(indent) << c;
        }
        else if (c == L',')
        {
            out << L",\r\n" << makeIndent(indent);
        }
        else if (c == L':')
        {
            out << L": ";
        }
        else
        {
            out << c;
        }
    }

    return out.str();
}

std::wstring ValueFormatters::FormatBinary(const std::vector<uint8_t>& bytes)
{
    if (bytes.empty())
        return std::wstring();

    std::wostringstream out;
    const size_t len = bytes.size();

    for (size_t offset = 0; offset < len; offset += 16)
    {
        // 8-digit hex offset
        wchar_t buf[64];
        swprintf_s(buf, L"%08IX  ", offset);
        out << buf;

        // 16 bytes hex: 8 bytes, space, 8 bytes
        for (size_t j = 0; j < 16; ++j)
        {
            if (offset + j < len)
            {
                swprintf_s(buf, L"%02X ", bytes[offset + j]);
                out << buf;
            }
            else
            {
                out << L"   ";
            }

            if (j == 7)
                out << L" ";
        }

        out << L" |";

        // ASCII column
        for (size_t j = 0; j < 16 && (offset + j < len); ++j)
        {
            uint8_t b = bytes[offset + j];
            if (b >= 32 && b <= 126)
                out << (wchar_t)b;
            else
                out << L'.';
        }

        out << L"|\r\n";
    }

    return out.str();
}

std::wstring ValueFormatters::FormatText(const std::string& text, bool compact)
{
    if (text.empty())
        return std::wstring();

    std::vector<uint8_t> b(text.begin(), text.end());
    std::wstring wtext = BytesToWString(b, VE_UTF8);

    // Normalize newlines to \r\n
    std::wostringstream out;
    for (size_t i = 0; i < wtext.size(); ++i)
    {
        wchar_t c = wtext[i];
        if (c == L'\r')
        {
            if (i + 1 < wtext.size() && wtext[i + 1] == L'\n')
                ++i;
            out << L"\r\n";
        }
        else if (c == L'\n')
        {
            out << L"\r\n";
        }
        else
        {
            out << c;
        }
    }

    return out.str();
}

std::wstring ValueFormatters::ProcessValue(
    const std::string& rawInput,
    EValueFormat format,
    bool autoFormat,
    bool compact,
    EValueEncoding encoding,
    std::vector<uint8_t>& outRawBytes)
{
    outRawBytes.clear();

    if (rawInput.empty() || rawInput == "[NULL]")
        return L"";

    // Check if input is hex representation of binary/blob
    if (IsLikelyHex(rawInput))
    {
        HexToBytes(rawInput, outRawBytes);
    }
    else
    {
        outRawBytes.assign(rawInput.begin(), rawInput.end());
    }

    std::string textData = BytesToString(outRawBytes);

    switch (format)
    {
    case VF_XML:
        return FormatXml(textData, autoFormat, compact);

    case VF_JSON:
        return FormatJson(textData, autoFormat, compact);

    case VF_BINARY:
        return FormatBinary(outRawBytes);

    case VF_TEXT:
    default:
        return FormatText(textData, compact);
    }
}

static const char* s_rtfHeader =
    "{\\rtf1\\ansi\\ansicpg1252\\deff0"
    "{\\fonttbl{\\f0\\fnil\\fcharset0 Consolas;}}"
    "{\\colortbl ;"
    "\\red0\\green0\\blue180;"     // cf1: delimiters < > / = { } [ ] , :
    "\\red128\\green0\\blue64;"    // cf2: tag names / json keys
    "\\red160\\green60\\blue0;"    // cf3: attr names / numbers / booleans
    "\\red0\\green110\\blue0;"     // cf4: attr values / string values
    "\\red0\\green128\\blue0;"     // cf5: comments / offsets
    "\\red100\\green100\\blue100;" // cf6: declaration <?xml ... ?>
    "}"
    "\\viewkind4\\uc1\\pard\\f0\\fs19 ";

static inline void AppendRtfChar(std::string& out, wchar_t c)
{
    if (c == L'\\') out += "\\\\";
    else if (c == L'{') out += "\\{";
    else if (c == L'}') out += "\\}";
    else if (c == L'\r') {}
    else if (c == L'\n') out += "\\par\r\n";
    else if (c == L'\t') out += "\\tab ";
    else if (c >= 32 && c <= 126) out.push_back((char)c);
    else
    {
        char buf[32];
        sprintf_s(buf, "\\u%d?", (short)c);
        out += buf;
    }
}

static inline void AppendRtfString(std::string& out, const std::wstring& str)
{
    for (wchar_t c : str)
        AppendRtfChar(out, c);
}

std::string ValueFormatters::FormatXmlRtf(const std::string& text, bool autoFormat, bool compact)
{
    std::wstring wtext = FormatXml(text, autoFormat, compact);
    if (wtext.empty())
        return "";

    std::string out = s_rtfHeader;
    size_t i = 0;
    const size_t len = wtext.size();

    while (i < len)
    {
        if (wtext[i] == L'<')
        {
            // Check for <?xml ... ?>
            if (i + 1 < len && wtext[i + 1] == L'?')
            {
                size_t endDecl = wtext.find(L"?>", i);
                if (endDecl != std::wstring::npos) endDecl += 2;
                else endDecl = (wtext.find(L'>', i) != std::wstring::npos) ? (wtext.find(L'>', i) + 1) : len;

                out += "\\cf6 ";
                AppendRtfString(out, wtext.substr(i, endDecl - i));
                i = endDecl;
                continue;
            }

            // Check for <!-- ... -->
            if (i + 3 < len && wtext[i + 1] == L'!' && wtext[i + 2] == L'-' && wtext[i + 3] == L'-')
            {
                size_t endComment = wtext.find(L"-->", i);
                if (endComment != std::wstring::npos) endComment += 3;
                else endComment = (wtext.find(L'>', i) != std::wstring::npos) ? (wtext.find(L'>', i) + 1) : len;

                out += "\\cf5 ";
                AppendRtfString(out, wtext.substr(i, endComment - i));
                i = endComment;
                continue;
            }

            // Check for <![CDATA[ ... ]]>
            if (i + 8 < len && wtext.substr(i, 9) == L"<![CDATA[")
            {
                size_t endCdata = wtext.find(L"]]>", i);
                if (endCdata != std::wstring::npos) endCdata += 3;
                else endCdata = len;

                out += "\\cf6 <![CDATA[\\cf0 ";
                size_t contentStart = i + 9;
                size_t contentEnd = (endCdata >= 3) ? (endCdata - 3) : contentStart;
                AppendRtfString(out, wtext.substr(contentStart, contentEnd - contentStart));
                out += "\\cf6 ]]>\\cf0 ";
                i = endCdata;
                continue;
            }

            // Regular tag: <tag ... > or </tag> or <tag ... />
            out += "\\cf1 <";
            ++i;
            if (i < len && wtext[i] == L'/')
            {
                out += "/";
                ++i;
            }

            // Read tag name
            size_t tagStart = i;
            while (i < len && wtext[i] != L' ' && wtext[i] != L'\t' && wtext[i] != L'\r' && wtext[i] != L'\n' && wtext[i] != L'>' && wtext[i] != L'/')
                ++i;
            if (i > tagStart)
            {
                out += "\\cf2 ";
                AppendRtfString(out, wtext.substr(tagStart, i - tagStart));
            }

            // Read attributes until > or />
            while (i < len && wtext[i] != L'>')
            {
                if (wtext[i] == L'/' && i + 1 < len && wtext[i + 1] == L'>')
                {
                    out += "\\cf1 />";
                    i += 2;
                    break;
                }

                if (wtext[i] == L' ' || wtext[i] == L'\t' || wtext[i] == L'\r' || wtext[i] == L'\n')
                {
                    AppendRtfChar(out, wtext[i]);
                    ++i;
                    continue;
                }

                // Read attribute name
                size_t attrStart = i;
                while (i < len && wtext[i] != L'=' && wtext[i] != L' ' && wtext[i] != L'\t' && wtext[i] != L'\r' && wtext[i] != L'\n' && wtext[i] != L'>' && wtext[i] != L'/')
                    ++i;
                if (i > attrStart)
                {
                    out += "\\cf3 ";
                    AppendRtfString(out, wtext.substr(attrStart, i - attrStart));
                }

                // Skip whitespace before =
                while (i < len && (wtext[i] == L' ' || wtext[i] == L'\t'))
                {
                    AppendRtfChar(out, wtext[i]);
                    ++i;
                }

                if (i < len && wtext[i] == L'=')
                {
                    out += "\\cf1 =";
                    ++i;
                }

                // Skip whitespace after =
                while (i < len && (wtext[i] == L' ' || wtext[i] == L'\t'))
                {
                    AppendRtfChar(out, wtext[i]);
                    ++i;
                }

                // Attribute value: "..." or '...'
                if (i < len && (wtext[i] == L'"' || wtext[i] == L'\''))
                {
                    wchar_t quote = wtext[i];
                    size_t valStart = i;
                    ++i;
                    while (i < len && wtext[i] != quote)
                        ++i;
                    if (i < len && wtext[i] == quote)
                        ++i;

                    out += "\\cf4 ";
                    AppendRtfString(out, wtext.substr(valStart, i - valStart));
                }
            }

            if (i < len && wtext[i] == L'>')
            {
                out += "\\cf1 >";
                ++i;
            }
        }
        else
        {
            // Content between tags
            out += "\\cf0 ";
            AppendRtfChar(out, wtext[i]);
            ++i;
        }
    }

    out += "\r\n}";
    return out;
}

std::string ValueFormatters::FormatJsonRtf(const std::string& text, bool autoFormat, bool compact)
{
    std::wstring wtext = FormatJson(text, autoFormat, compact);
    if (wtext.empty())
        return "";

    std::string out = s_rtfHeader;
    size_t i = 0;
    const size_t len = wtext.size();

    while (i < len)
    {
        wchar_t c = wtext[i];

        if (c == L'"')
        {
            // Read string literal
            size_t strStart = i;
            ++i;
            while (i < len)
            {
                if (wtext[i] == L'\\')
                {
                    i += 2;
                }
                else if (wtext[i] == L'"')
                {
                    ++i;
                    break;
                }
                else
                {
                    ++i;
                }
            }
            std::wstring strLit = wtext.substr(strStart, i - strStart);

            // Check if next non-whitespace is ':' -> Key name!
            size_t next = i;
            while (next < len && (wtext[next] == L' ' || wtext[next] == L'\t' || wtext[next] == L'\r' || wtext[next] == L'\n'))
                ++next;

            if (next < len && wtext[next] == L':')
            {
                out += "\\cf2 ";
                AppendRtfString(out, strLit);
                while (i < next)
                {
                    AppendRtfChar(out, wtext[i]);
                    ++i;
                }
                out += "\\cf1 :";
                i = next + 1;
            }
            else
            {
                out += "\\cf4 ";
                AppendRtfString(out, strLit);
            }
        }
        else if (c == L'{' || c == L'}' || c == L'[' || c == L']' || c == L',' || c == L':')
        {
            out += "\\cf1 ";
            AppendRtfChar(out, c);
            ++i;
        }
        else if ((c >= L'0' && c <= L'9') || c == L'-')
        {
            size_t numStart = i;
            while (i < len && ((wtext[i] >= L'0' && wtext[i] <= L'9') || wtext[i] == L'.' || wtext[i] == L'e' || wtext[i] == L'E' || wtext[i] == L'+' || wtext[i] == L'-'))
                ++i;
            out += "\\cf3 ";
            AppendRtfString(out, wtext.substr(numStart, i - numStart));
        }
        else if (iswalpha(c))
        {
            size_t wordStart = i;
            while (i < len && iswalpha(wtext[i]))
                ++i;
            out += "\\cf3 ";
            AppendRtfString(out, wtext.substr(wordStart, i - wordStart));
        }
        else
        {
            out += "\\cf0 ";
            AppendRtfChar(out, c);
            ++i;
        }
    }

    out += "\r\n}";
    return out;
}

std::string ValueFormatters::FormatBinaryRtf(const std::vector<uint8_t>& bytes)
{
    std::wstring wtext = FormatBinary(bytes);
    if (wtext.empty())
        return "";

    std::string out = s_rtfHeader;
    std::wistringstream in(wtext);
    std::wstring line;

    while (std::getline(in, line))
    {
        if (line.empty()) continue;

        if (line.size() >= 10)
        {
            out += "\\cf5 "; // offset
            AppendRtfString(out, line.substr(0, 10));

            size_t pipe1 = line.find(L'|', 10);
            if (pipe1 != std::wstring::npos)
            {
                out += "\\cf1 "; // hex bytes
                AppendRtfString(out, line.substr(10, pipe1 - 10));

                out += "\\cf5 |"; // delimiter
                size_t pipe2 = line.rfind(L'|');
                if (pipe2 != std::wstring::npos && pipe2 > pipe1)
                {
                    out += "\\cf4 "; // ascii
                    AppendRtfString(out, line.substr(pipe1 + 1, pipe2 - pipe1 - 1));
                    out += "\\cf5 |";
                }
            }
            else
            {
                out += "\\cf0 ";
                AppendRtfString(out, line.substr(10));
            }
        }
        else
        {
            out += "\\cf0 ";
            AppendRtfString(out, line);
        }

        out += "\\par\r\n";
    }

    out += "\r\n}";
    return out;
}

std::string ValueFormatters::FormatTextRtf(const std::string& text, bool compact)
{
    std::wstring wtext = FormatText(text, compact);
    if (wtext.empty())
        return "";

    std::string out = s_rtfHeader;
    out += "\\cf0 ";
    AppendRtfString(out, wtext);
    out += "\r\n}";
    return out;
}

std::string ValueFormatters::ProcessValueRtf(
    const std::string& rawInput,
    EValueFormat format,
    bool autoFormat,
    bool compact,
    EValueEncoding encoding,
    std::vector<uint8_t>& outRawBytes)
{
    outRawBytes.clear();

    if (rawInput.empty() || rawInput == "[NULL]")
        return "";

    if (IsLikelyHex(rawInput))
    {
        HexToBytes(rawInput, outRawBytes);
    }
    else
    {
        outRawBytes.assign(rawInput.begin(), rawInput.end());
    }

    std::string textData = BytesToString(outRawBytes);

    switch (format)
    {
    case VF_XML:
        return FormatXmlRtf(textData, autoFormat, compact);

    case VF_JSON:
        return FormatJsonRtf(textData, autoFormat, compact);

    case VF_BINARY:
        return FormatBinaryRtf(outRawBytes);

    case VF_TEXT:
    default:
        return FormatTextRtf(textData, compact);
    }
}

} // namespace OG2
