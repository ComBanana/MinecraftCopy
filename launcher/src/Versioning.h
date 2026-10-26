#pragma once

#include <string_view>

bool isValidVersion(std::string_view version);
int compareVersions(std::string_view left, std::string_view right);
