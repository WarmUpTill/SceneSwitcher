#pragma once
#include <memory>
#include <string>

namespace advss {
class Macro;
}

namespace advss::test {

// Replaces all macros with the given layout.
// Layout syntax: "A, G1[B, C], G2{D}" where [] is an expanded and {} a
// collapsed group.
void SetupMacros(const std::string &layout);

// Describes the current macros using the same syntax as SetupMacros()
std::string DescribeMacros();

std::shared_ptr<Macro> GetMacro(const std::string &name);

} // namespace advss::test
