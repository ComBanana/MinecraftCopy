#include "Versioning.h"

#include <iostream>

int main()
{
    const auto require = [](bool condition, const char *message)
    {
        if (!condition)
            std::cerr << "FAIL: " << message << '\n';
        return condition;
    };

    bool passed = true;
    passed &= require(compareVersions("0.0.1-alpha", "0.0.1") < 0, "a prerelease sorts before its final release");
    passed &= require(compareVersions("0.0.10-alpha", "0.0.9-alpha") > 0, "patch versions compare numerically");
    passed &= require(compareVersions("0.0.1-alpha.2", "0.0.1-alpha.10") < 0, "numeric prerelease identifiers compare numerically");
    passed &= require(compareVersions("0.0.1-alpha.2", "0.0.1-alpha.beta") < 0, "numeric prerelease identifiers sort before text identifiers");
    passed &= require(compareVersions("0.0.1-alpha.beta", "0.0.1-alpha") > 0, "a longer matching prerelease sorts after its prefix");
    passed &= require(compareVersions("0.0.1", "0.0.1") == 0, "identical versions compare equal");
    passed &= require(!isValidVersion("01.0.0"), "numeric components reject leading zeroes");
    passed &= require(!isValidVersion("0.0.1-"), "empty prerelease suffix is rejected");
    passed &= require(!isValidVersion("0.0.1/../../bad"), "path separators are rejected");
    return passed ? 0 : 1;
}
