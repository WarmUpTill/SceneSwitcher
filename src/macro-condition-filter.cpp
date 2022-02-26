#include "headers/macro-condition-edit.hpp"
#include "headers/macro-condition-filter.hpp"
#include "headers/utility.hpp"
#include "headers/advanced-scene-switcher.hpp"

#include <regex>

const std::string MacroConditionFilter::id = "filter";

bool MacroConditionFilter::_registered = MacroConditionFactory::Register(
	MacroConditionFilter::id,
	{MacroConditionFilter::Create, MacroConditionFilterEdit::Create,
	 "AdvSceneSwitcher.condition.filter"});

static std::map<FilterCondition, std::string> filterConditionTypes = {
	{FilterCondition::ENABLED,
	 "AdvSceneSwitcher.condition.filter.type.active"},
	{FilterCondition::DISABLED,
	 "AdvSceneSwitcher.condition.filter.type.showing"},
	{FilterCondition::SETTINGS,
	 "AdvSceneSwitcher.condition.filter.type.settings"},
};

bool MacroConditionFilter::CheckCondition()
{
	if (!_source) {
		return false;
	}

	bool ret = false;
	auto s = obs_weak_source_get_source(_filter);

	switch (_condition) {
	case FilterCondition::ENABLED:
		ret = obs_source_enabled(s);
		break;
	case FilterCondition::DISABLED:
		ret = !obs_source_enabled(s);
		break;
	case FilterCondition::SETTINGS:
		ret = compareSourceSettings(_filter, _settings, _regex);
		break;
	default:
		break;
	}

	obs_source_release(s);

	return ret;
}

bool MacroConditionFilter::Save(obs_data_t *obj)
{
	MacroCondition::Save(obj);
	obs_data_set_string(obj, "source", GetWeakSourceName(_source).c_str());
	obs_data_set_string(obj, "filter", GetWeakSourceName(_filter).c_str());
	obs_data_set_int(obj, "condition", static_cast<int>(_condition));
	obs_data_set_string(obj, "settings", _settings.c_str());
	obs_data_set_bool(obj, "regex", _regex);
	return true;
}

bool MacroConditionFilter::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	const char *sourceName = obs_data_get_string(obj, "source");
	_source = GetWeakSourceByName(sourceName);
	const char *filterName = obs_data_get_string(obj, "filter");
	_filter = GetWeakFilterByQString(_source, filterName);
	_condition = static_cast<FilterCondition>(
		obs_data_get_int(obj, "condition"));
	_settings = obs_data_get_string(obj, "settings");
	_regex = obs_data_get_bool(obj, "regex");
	return true;
}

std::string MacroConditionFilter::GetShortDesc()
{
	if (_filter && _source) {
		return GetWeakSourceName(_source) + " - " +
		       GetWeakSourceName(_filter);
	}
	return "";
}

static inline void populateConditionSelection(QComboBox *list)
{
	for (auto entry : filterConditionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroConditionFilterEdit::MacroConditionFilterEdit(
	QWidget *parent, std::shared_ptr<MacroConditionFilter> entryData)
	: QWidget(parent)
{
	_sources = new QComboBox();
	_filters = new QComboBox();
	_filters->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	_conditions = new QComboBox();
	_getSettings = new QPushButton(obs_module_text(
		"AdvSceneSwitcher.condition.filter.getSettings"));
	_settings = new ResizingPlainTextEdit(this);
	_regex = new QCheckBox(
		obs_module_text("AdvSceneSwitcher.condition.filter.regex"));

	populateConditionSelection(_conditions);
	populateSourcesWithFilterSelection(_sources);

	QWidget::connect(_sources, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(SourceChanged(const QString &)));
	QWidget::connect(_filters, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(FilterChanged(const QString &)));
	QWidget::connect(_conditions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(_getSettings, SIGNAL(clicked()), this,
			 SLOT(GetSettingsClicked()));
	QWidget::connect(_settings, SIGNAL(textChanged()), this,
			 SLOT(SettingsChanged()));
	QWidget::connect(_regex, SIGNAL(stateChanged(int)), this,
			 SLOT(RegexChanged(int)));

	QHBoxLayout *line1Layout = new QHBoxLayout;
	QHBoxLayout *line2Layout = new QHBoxLayout;
	QHBoxLayout *line3Layout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{sources}}", _sources},         {"{{filters}}", _filters},
		{"{{conditions}}", _conditions},   {"{{settings}}", _settings},
		{"{{getSettings}}", _getSettings}, {"{{regex}}", _regex},
	};
	placeWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.filter.entry.line1"),
		     line1Layout, widgetPlaceholders);
	placeWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.filter.entry.line2"),
		     line2Layout, widgetPlaceholders, false);
	placeWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.filter.entry.line3"),
		     line3Layout, widgetPlaceholders);

	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(line1Layout);
	mainLayout->addLayout(line2Layout);
	mainLayout->addLayout(line3Layout);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionFilterEdit::SourceChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}
	{
		std::lock_guard<std::mutex> lock(switcher->m);
		_entryData->_source = GetWeakSourceByQString(text);
	}
	_filters->clear();
	populateFilterSelection(_filters, _entryData->_source);
	_filters->adjustSize();
}

void MacroConditionFilterEdit::FilterChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_filter = GetWeakFilterByQString(_entryData->_source, text);
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionFilterEdit::ConditionChanged(int index)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_condition = static_cast<FilterCondition>(index);
	SetSettingsSelectionVisible(_entryData->_condition ==
				    FilterCondition::SETTINGS);
}

void MacroConditionFilterEdit::GetSettingsClicked()
{
	if (_loading || !_entryData || !_entryData->_source) {
		return;
	}

	QString json = formatJsonString(getSourceSettings(_entryData->_filter));
	if (_entryData->_regex) {
		json = escapeForRegex(json);
	}
	_settings->setPlainText(json);
}

void MacroConditionFilterEdit::SettingsChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_settings = _settings->toPlainText().toStdString();

	adjustSize();
	updateGeometry();
}

void MacroConditionFilterEdit::RegexChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_regex = state;
}

void MacroConditionFilterEdit::SetSettingsSelectionVisible(bool visible)
{
	_settings->setVisible(visible);
	_getSettings->setVisible(visible);
	_regex->setVisible(visible);
	adjustSize();
}

void MacroConditionFilterEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_sources->setCurrentText(
		GetWeakSourceName(_entryData->_source).c_str());
	populateFilterSelection(_filters, _entryData->_source);
	_filters->setCurrentText(
		GetWeakSourceName(_entryData->_filter).c_str());
	_conditions->setCurrentIndex(static_cast<int>(_entryData->_condition));
	_settings->setPlainText(QString::fromStdString(_entryData->_settings));
	_regex->setChecked(_entryData->_regex);
	SetSettingsSelectionVisible(_entryData->_condition ==
				    FilterCondition::SETTINGS);

	adjustSize();
	updateGeometry();
}
