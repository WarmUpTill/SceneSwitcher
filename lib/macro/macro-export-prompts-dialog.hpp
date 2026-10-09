#pragma once

#include "macro-import-prompts.hpp"

#include <obs-data.h>
#include <QDialog>
#include <string>
#include <vector>

class QComboBox;
class QTableWidget;
class QListWidget;

namespace advss {

struct ExportPromptsState {
	struct Entry {
		ImportPrompt prompt;
		std::vector<std::string> originalValues;
		std::string scope;
	};

	void Load(obs_data_t *data);
	void Apply(obs_data_t *data) const;

	std::string macrosArrayJson;
	std::vector<Entry> entries;
};

class MacroExportPromptsDialog : public QDialog {
	Q_OBJECT

public:
	static bool Edit(QWidget *parent, ExportPromptsState &state);

private slots:
	void ScopeChanged(int);
	void AddPrompt();
	void RemovePrompt();

private:
	MacroExportPromptsDialog(QWidget *parent,
				 const ExportPromptsState &state);

	void RefreshValuesTable();
	void RefreshPromptsList();
	std::string CurrentScopeJson() const;
	void ApplyCurrentScopeJson(const std::string &json);

	ExportPromptsState _state;

	QComboBox *_scope;
	QTableWidget *_values;
	QListWidget *_promptsList;
};

} // namespace advss
