#include "Versioning.h"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
struct ParsedVersion
{
    std::uint64_t major = 0;
    std::uint64_t minor = 0;
    std::uint64_t patch = 0;
    std::vector<std::string> prerelease;
};

bool isDigit(char value)
{
    return value >= '0' && value <= '9';
}

bool isAlphaNumeric(char value)
{
    return (value >= '0' && value <= '9') ||
        (value >= 'a' && value <= 'z') ||
        (value >= 'A' && value <= 'Z') || value == '-';
}

bool parseNumber(std::string_view text, std::uint64_t &number)
{
    if (text.empty() || (text.size() > 1 && text.front() == '0'))
        return false;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), number);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

bool parseVersion(std::string_view text, ParsedVersion &version)
{
    const std::size_t prereleaseStart = text.find('-');
    const std::string_view core = text.substr(0, prereleaseStart);
    const std::size_t firstDot = core.find('.');
    if (firstDot == std::string_view::npos)
        return false;
    const std::size_t secondDot = core.find('.', firstDot + 1);
    if (secondDot == std::string_view::npos || core.find('.', secondDot + 1) != std::string_view::npos)
        return false;
    if (!parseNumber(core.substr(0, firstDot), version.major) ||
        !parseNumber(core.substr(firstDot + 1, secondDot - firstDot - 1), version.minor) ||
        !parseNumber(core.substr(secondDot + 1), version.patch))
        return false;

    if (prereleaseStart == std::string_view::npos)
        return true;

    std::string_view identifiers = text.substr(prereleaseStart + 1);
    if (identifiers.empty())
        return false;
    while (!identifiers.empty())
    {
        const std::size_t separator = identifiers.find('.');
        const std::string_view identifier = identifiers.substr(0, separator);
        if (identifier.empty() || !std::all_of(identifier.begin(), identifier.end(), isAlphaNumeric))
            return false;
        const bool numeric = std::all_of(identifier.begin(), identifier.end(), isDigit);
        if (numeric && identifier.size() > 1 && identifier.front() == '0')
            return false;
        version.prerelease.emplace_back(identifier);
        if (separator == std::string_view::npos)
            break;
        identifiers.remove_prefix(separator + 1);
        if (identifiers.empty())
            return false;
    }
    return true;
}

int compareIdentifier(const std::string &left, const std::string &right)
{
    const bool leftNumeric = std::all_of(left.begin(), left.end(), isDigit);
    const bool rightNumeric = std::all_of(right.begin(), right.end(), isDigit);
    if (leftNumeric && rightNumeric)
    {
        if (left.size() != right.size())
            return left.size() < right.size() ? -1 : 1;
        if (left == right)
            return 0;
        return left < right ? -1 : 1;
    }
    if (leftNumeric != rightNumeric)
        return leftNumeric ? -1 : 1;
    if (left == right)
        return 0;
    return left < right ? -1 : 1;
}
}

bool isValidVersion(std::string_view version)
{
    ParsedVersion parsed;
    return parseVersion(version, parsed);
}

int compareVersions(std::string_view left, std::string_view right)
{
    ParsedVersion parsedLeft;
    ParsedVersion parsedRight;
    if (!parseVersion(left, parsedLeft) || !parseVersion(right, parsedRight))
        throw std::invalid_argument("compareVersions requires valid semantic versions");

    if (parsedLeft.major != parsedRight.major)
        return parsedLeft.major < parsedRight.major ? -1 : 1;
    if (parsedLeft.minor != parsedRight.minor)
        return parsedLeft.minor < parsedRight.minor ? -1 : 1;
    if (parsedLeft.patch != parsedRight.patch)
        return parsedLeft.patch < parsedRight.patch ? -1 : 1;

    if (parsedLeft.prerelease.empty() != parsedRight.prerelease.empty())
        return parsedLeft.prerelease.empty() ? 1 : -1;
    const std::size_t commonCount = std::min(parsedLeft.prerelease.size(), parsedRight.prerelease.size());
    for (std::size_t index = 0; index < commonCount; ++index)
    {
        const int comparison = compareIdentifier(parsedLeft.prerelease[index], parsedRight.prerelease[index]);
        if (comparison != 0)
            return comparison;
    }
    if (parsedLeft.prerelease.size() == parsedRight.prerelease.size())
        return 0;
    return parsedLeft.prerelease.size() < parsedRight.prerelease.size() ? -1 : 1;
}
