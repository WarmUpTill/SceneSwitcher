#pragma once

#include "macro-import-prompts.hpp"

#include <functional>
#include <QDialog>
#include <string>
#include <unordered_map>
#include <vector>

namespace advss {

class MacroImportPromptsDialog : public QDialog {
	Q_OBJECT

public:
	MacroImportPromptsDialog(QWidget *parent,
				 const std::vector<ImportPrompt> &prompts);

	std::unordered_map<std::string, std::string> GetReplacements() const;

private:
	struct Row {
		ImportPrompt prompt;
		std::function<std::vector<std::string>()> getValues;
	};

	std::vector<Row> _rows;
};

} // namespace advss
