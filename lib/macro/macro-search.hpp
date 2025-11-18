#pragma once
#include "regex-config.hpp"

#include <functional>
#include <string>

class QComboBox;
class QLayout;
class QLineEdit;
class QPushButton;

namespace advss {

class Macro;

struct MacroSearchSettings {
	void Save(obs_data_t *data, const char *name);
	void Load(obs_data_t *data, const char *name);

	enum class SearchType {
		NAME = 0,
		ALL_SEGMENTS = 10,
		CONDITIONS = 20,
		ACTIONS = 30,
		LABEL = 40,
	};

	SearchType searchType = SearchType::NAME;
	std::string searchString;
	RegexConfig regex;
	bool showAlways = false;
};

void CheckMacroSearchVisibility();
void SetupMacroSearchWidgets(QLayout *searchLayout, QLineEdit *searchText,
			     QPushButton *searchClear, QComboBox *searchType,
			     RegexConfigWidget *searchRegex,
			     QPushButton *showSettings,
			     const std::function<void()> &refresh);
MacroSearchSettings &GetMacroSearchSettings();
bool MacroMatchesSearchFilter(Macro *macro);

} // namespace advss
