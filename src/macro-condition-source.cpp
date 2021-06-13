#include "headers/macro-condition-edit.hpp"
#include "headers/macro-condition-source.hpp"
#include "headers/utility.hpp"
#include "headers/advanced-scene-switcher.hpp"

#include <regex>

const std::string MacroConditionSource::id = "source";

bool MacroConditionSource::_registered = MacroConditionFactory::Register(
	MacroConditionSource::id,
	{MacroConditionSource::Create, MacroConditionSourceEdit::Create,
	 "AdvSceneSwitcher.condition.source"});

static std::map<SourceCondition, std::string> sourceConditionTypes = {
	{SourceCondition::ACTIVE,
	 "AdvSceneSwitcher.condition.source.type.active"},
	{SourceCondition::SHOWING,
	 "AdvSceneSwitcher.condition.source.type.showing"},
	{SourceCondition::SETTINGS,
	 "AdvSceneSwitcher.condition.source.type.settings"},
};

bool MacroConditionSource::CheckCondition()
{
	if (!_source) {
		return false;
	}

	bool ret = false;
	auto s = obs_weak_source_get_source(_source);

	switch (_condition) {
	case SourceCondition::ACTIVE:
		ret = obs_source_active(s);
		break;
	case SourceCondition::SHOWING:
		ret = obs_source_showing(s);
		break;
	case SourceCondition::SETTINGS:
		ret = compareSourceSettings(_source, _settings, _regex);
		break;
	default:
		break;
	}

	obs_source_release(s);

	return ret;
}

bool MacroConditionSource::Save(obs_data_t *obj)
{
	MacroCondition::Save(obj);
	obs_data_set_string(obj, "source", GetWeakSourceName(_source).c_str());
	obs_data_set_int(obj, "condition", static_cast<int>(_condition));
	obs_data_set_string(obj, "settings", _settings.c_str());
	obs_data_set_bool(obj, "regex", _regex);
	return true;
}

bool MacroConditionSource::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	const char *sourceName = obs_data_get_string(obj, "source");
	_source = GetWeakSourceByName(sourceName);
	_condition = static_cast<SourceCondition>(
		obs_data_get_int(obj, "condition"));
	_settings = obs_data_get_string(obj, "settings");
	_regex = obs_data_get_bool(obj, "regex");
	return true;
}

static inline void populateConditionSelection(QComboBox *list)
{
	for (auto entry : sourceConditionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroConditionSourceEdit::MacroConditionSourceEdit(
	QWidget *parent, std::shared_ptr<MacroConditionSource> entryData)
	: QWidget(parent)
{
	_sources = new QComboBox();
	_conditions = new QComboBox();
	_getSettings = new QPushButton(obs_module_text(
		"AdvSceneSwitcher.condition.source.getSettings"));
	_settings = new QPlainTextEdit();
	_regex = new QCheckBox(
		obs_module_text("AdvSceneSwitcher.condition.source.regex"));

	populateConditionSelection(_conditions);
	populateSourceSelection(_sources);

	QWidget::connect(_sources, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(SourceChanged(const QString &)));
	QWidget::connect(_conditions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(_getSettings, SIGNAL(clicked()), this,
			 SLOT(GetSettingsClicked()));
	QWidget::connect(_settings, SIGNAL(textChanged()), this,
			 SLOT(SettingsChanged()));
	QWidget::connect(_regex, SIGNAL(stateChanged(int)), this,
			 SLOT(RegexChanged(int)));

	QVBoxLayout *mainLayout = new QVBoxLayout;
	QHBoxLayout *line1Layout = new QHBoxLayout;
	QHBoxLayout *line2Layout = new QHBoxLayout;
	QHBoxLayout *line3Layout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{sources}}", _sources},   {"{{conditions}}", _conditions},
		{"{{settings}}", _settings}, {"{{getSettings}}", _getSettings},
		{"{{regex}}", _regex},
	};
	placeWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.source.entry.line1"),
		     line1Layout, widgetPlaceholders);
	placeWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.source.entry.line2"),
		     line2Layout, widgetPlaceholders, false);
	placeWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.source.entry.line3"),
		     line3Layout, widgetPlaceholders);
	mainLayout->addLayout(line1Layout);
	mainLayout->addLayout(line2Layout);
	mainLayout->addLayout(line3Layout);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionSourceEdit::SourceChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_source = GetWeakSourceByQString(text);
}

void MacroConditionSourceEdit::ConditionChanged(int index)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_condition = static_cast<SourceCondition>(index);
	SetSettingsSelectionVisible(_entryData->_condition ==
				    SourceCondition::SETTINGS);
}

void MacroConditionSourceEdit::GetSettingsClicked()
{
	if (_loading || !_entryData || !_entryData->_source) {
		return;
	}

	_settings->setPlainText(
		QString::fromStdString(getSourceSettings(_entryData->_source)));
}

void MacroConditionSourceEdit::SettingsChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_settings = _settings->toPlainText().toStdString();
}

void MacroConditionSourceEdit::RegexChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_regex = state;
}

void MacroConditionSourceEdit::SetSettingsSelectionVisible(bool visible)
{
	_settings->setVisible(visible);
	_getSettings->setVisible(visible);
	_regex->setVisible(visible);
	adjustSize();
}

void MacroConditionSourceEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_sources->setCurrentText(
		GetWeakSourceName(_entryData->_source).c_str());
	_conditions->setCurrentIndex(static_cast<int>(_entryData->_condition));
	_settings->setPlainText(QString::fromStdString(_entryData->_settings));
	_regex->setChecked(_entryData->_regex);
	SetSettingsSelectionVisible(_entryData->_condition ==
				    SourceCondition::SETTINGS);
}
