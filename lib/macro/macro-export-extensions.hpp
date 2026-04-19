#pragma once

#include "export-symbol-helper.hpp"

#include <functional>
#include <obs-data.h>
#include <QList>
#include <QPair>
#include <QString>
#include <vector>

namespace advss {

struct MacroExportExtension {
	const char *displayNameKey;
	const char *jsonKey;

	// Export callback.
	// Write content into 'data'.  'selectedIds' contains the item IDs the
	// user explicitly checked; if empty (no per-item widget provided) save
	// everything.
	std::function<void(obs_data_t *data, const QStringList &selectedIds)>
		save;

	// Import callback.
	// Read content from 'data'.  'selectedIds' contains the item IDs the
	// user explicitly checked; if empty import every item found in 'data'.
	std::function<void(obs_data_t *data, const QStringList &selectedIds)>
		load;

	// Optional: enumerate items available for export as (id, displayName)
	// pairs.  When nullptr the extension appears as a single all-or-nothing
	// checkbox in the export dialog.
	std::function<QList<QPair<QString, QString>>()> getExportItems =
		nullptr;
};

EXPORT void AddMacroExportExtension(const MacroExportExtension &ext);
const std::vector<MacroExportExtension> &GetMacroExportExtensions();

} // namespace advss
