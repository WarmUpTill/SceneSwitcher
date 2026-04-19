#include "macro-export-extensions.hpp"

namespace advss {

static std::vector<MacroExportExtension> extensions;

void AddMacroExportExtension(const MacroExportExtension &ext)
{
	extensions.emplace_back(ext);
}

const std::vector<MacroExportExtension> &GetMacroExportExtensions()
{
	return extensions;
}

} // namespace advss
