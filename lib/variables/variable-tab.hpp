#pragma once
#include "resource-table.hpp"
#include "regex-config.hpp"

#include <QComboBox>

namespace advss {

class VariableTable final : public ResourceTable {
	Q_OBJECT

public:
	struct Settings {
		void Save(obs_data_t *data, const char *name);
		void Load(obs_data_t *data, const char *name);

		enum SearchType { ALL = -1, NAME = 0, VALUE = 1 };

		SearchType searchType = SearchType::ALL;
		std::string searchString;
		RegexConfig regex;
	};

	VariableTable(Settings &settings, QWidget *parent = nullptr);
	static VariableTable *CreateTabTable();

private slots:
	void Add();
	void Remove();
	void Filter();

private:
	QLineEdit *_searchField;
	QComboBox *_searchType;
	RegexConfigWidget *_regexWidget;

	Settings &_settings;
};

} // namespace advss
