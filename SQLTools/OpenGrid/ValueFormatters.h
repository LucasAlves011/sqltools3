#pragma once
#ifndef __VALUEFORMATTERS_H__
#define __VALUEFORMATTERS_H__

#include <string>
#include <vector>
#include <cstdint>

namespace OG2
{

enum EValueFormat
{
    VF_BINARY = 0,
    VF_JSON   = 1,
    VF_TEXT   = 2,
    VF_XML    = 3
};

enum EValueEncoding
{
    VE_UTF8 = 0,
    VE_ANSI = 1
};

class ValueFormatters
{
public:
    static bool HexToBytes(const std::string& input, std::vector<uint8_t>& outBytes);
    static bool IsLikelyHex(const std::string& input);

    static EValueFormat DetectFormat(const std::vector<uint8_t>& bytes, const std::string& rawText);

    static std::wstring BytesToWString(const std::vector<uint8_t>& bytes, EValueEncoding encoding);
    static std::string  BytesToString(const std::vector<uint8_t>& bytes);

    static std::wstring FormatXml(const std::string& text, bool autoFormat, bool compact);
    static std::wstring FormatJson(const std::string& text, bool autoFormat, bool compact);
    static std::wstring FormatBinary(const std::vector<uint8_t>& bytes);
    static std::wstring FormatText(const std::string& text, bool compact);

    static std::string FormatXmlRtf(const std::string& text, bool autoFormat, bool compact);
    static std::string FormatJsonRtf(const std::string& text, bool autoFormat, bool compact);
    static std::string FormatBinaryRtf(const std::vector<uint8_t>& bytes);
    static std::string FormatTextRtf(const std::string& text, bool compact);

    static std::wstring ProcessValue(
        const std::string& rawInput,
        EValueFormat format,
        bool autoFormat,
        bool compact,
        EValueEncoding encoding,
        std::vector<uint8_t>& outRawBytes
    );

    static std::string ProcessValueRtf(
        const std::string& rawInput,
        EValueFormat format,
        bool autoFormat,
        bool compact,
        EValueEncoding encoding,
        std::vector<uint8_t>& outRawBytes
    );
};

} // namespace OG2

#endif // __VALUEFORMATTERS_H__
