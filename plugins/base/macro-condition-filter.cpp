#include "macro-condition-filter.hpp"
#include "layout-helpers.hpp"
#include "json-helpers.hpp"
#include "text-helpers.hpp"
#include "source-settings-helpers.hpp"
#include "selection-helpers.hpp"
#include "ui-helpers.hpp"

#include <regex>

namespace advss {

const std::string MacroConditionFilter::id = "filter";

bool MacroConditionFilter::_registered = MacroConditionFactory::Register(
	MacroConditionFilter::id,
	{MacroConditionFilter::Create, MacroConditionFilterEdit::Create,
	 "AdvSceneSwitcher.condition.filter"});

static const std::vector<std::pair<MacroConditionFilter::Condition, std::string>>
	filterConditionTypes = {
		{MacroConditionFilter::Condition::ENABLED,
		 "AdvSceneSwitcher.condition.filter.type.active"},
		{MacroConditionFilter::Condition::DISABLED,
		 "AdvSceneSwitcher.condition.filter.type.showing"},
		{MacroConditionFilter::Condition::SETTINGS_MATCH,
		 "AdvSceneSwitcher.condition.filter.type.settingsMatch"},
		{MacroConditionFilter::Condition::SETTINGS_CHANGED,
		 "AdvSceneSwitcher.condition.filter.type.settingsChanged"},
		{MacroConditionFilter::Condition::INDIVIDUAL_SETTING_MATCH,
		 "AdvSceneSwitcher.condition.filter.type.individualSettingMatches"},
		{MacroConditionFilter::Condition::INDIVIDUAL_SETTING_CHANGED,
		 "AdvSceneSwitcher.condition.filter.type.individualSettingChanged"},
		{MacroConditionFilter::Condition::
			 INDIVIDUAL_SETTING_LIST_ENTRY_MATCH,
		 "AdvSceneSwitcher.condition.filter.type.individualSettingListSelectionMatches"},
};

bool MacroConditionFilter::CheckConditionHelper(const OBSWeakSource &filter)
{
	bool ret = false;
	OBSSourceAutoRelease filterSource = obs_weak_source_get_source(filter);
	if (!filterSource.Get()) {
		return false;
	}

	switch (_condition) {
	case Condition::ENABLED:
		ret = obs_source_enabled(filterSource);
		break;
	case Condition::DISABLED:
		ret = !obs_source_enabled(filterSource);
		break;
	case Condition::SETTINGS_MATCH: {
		const auto settings =
			GetSourceSettings(filter, _includeDefaults);
		if (!settings) {
			break;
		}

		ret = CompareSourceSettings(*settings, _settings, _regex);
		SetVariableValue(*settings);
		SetTempVarValue("settings", *settings);
		break;
	}
	case Condition::SETTINGS_CHANGED: {
		const auto settings =
			GetSourceSettings(filter, _includeDefaults);
		ret = _currentSettings.has_value() &&
		      settings != _currentSettings;
		_currentSettings = settings;
		if (!settings) {
			break;
		}
		SetVariableValue(*settings);
		SetTempVarValue("settings", *settings);
		break;
	}
	case Condition::INDIVIDUAL_SETTING_MATCH: {
		auto value = GetSourceSettingValue(filter, _setting);
		if (!value) {
			return false;
		}
		ret = _regex.Enabled() ? _regex.Matches(*value, _settings)
				       : value == std::string(_settings);
		SetVariableValue(*value);
		SetTempVarValue("setting", *value);
		break;
	}
	case Condition::INDIVIDUAL_SETTING_LIST_ENTRY_MATCH: {
		const auto entryName =
			GetSourceSettingListEntryName(filter, _setting);
		if (!entryName) {
			return false;
		}
		ret = _regex.Enabled() ? _regex.Matches(*entryName, _settings)
				       : entryName == std::string(_settings);
		SetVariableValue(*entryName);
		SetTempVarValue("setting", *entryName);
		break;
	}
	case Condition::INDIVIDUAL_SETTING_CHANGED: {
		auto value = GetSourceSettingValue(filter, _setting);
		if (!value) {
			return false;
		}
		ret = _currentSettingsValue != value;
		_currentSettingsValue = *value;
		SetVariableValue(*value);
		SetTempVarValue("setting", *value);
		break;
	}
	default:
		break;
	}
	return ret;
}

bool MacroConditionFilter::CheckCondition()
{
	auto filters = _filter.GetFilters(_source);
	if (filters.empty()) {
		return false;
	}

	bool ret = true;
	for (const auto &filter : filters) {
		ret = ret && CheckConditionHelper(filter);
	}

	if (GetVariableValue().empty()) {
		SetVariableValue(ret ? "true" : "false");
	}

	return ret;
}

bool MacroConditionFilter::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	_source.Save(obj);
	_filter.Save(obj, "filter");
	obs_data_set_int(obj, "condition", static_cast<int>(_condition));
	_settings.Save(obj, "settings");
	_regex.Save(obj);
	_setting.Save(obj);
	obs_data_set_bool(obj, "includeDefaults", _includeDefaults);
	return true;
}

bool MacroConditionFilter::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_source.Load(obj);
	_filter.Load(obj, _source, "filter");
	SetCondition(
		static_cast<Condition>(obs_data_get_int(obj, "condition")));
	_settings.Load(obj, "settings");
	_regex.Load(obj);
	// TOOD: remove in future version
	if (obs_data_has_user_value(obj, "regex")) {
		_regex.CreateBackwardsCompatibleRegex(
			obs_data_get_bool(obj, "regex"));
	}
	_setting.Load(obj);
	_includeDefaults = obs_data_get_bool(obj, "includeDefaults");
	return true;
}

std::string MacroConditionFilter::GetShortDesc() const
{
	if (!_filter.ToString().empty() && !_source.ToString().empty()) {
		return _source.ToString() + " - " + _filter.ToString();
	}
	return "";
}

void MacroConditionFilter::SetCondition(Condition cond)
{
	_condition = cond;
	SetupTempVars();
}

void MacroConditionFilter::SetupTempVars()
{
	MacroCondition::SetupTempVars();
	switch (_condition) {
	case Condition::ENABLED:
		break;
	case Condition::DISABLED:
		break;
	case Condition::SETTINGS_MATCH:
	case Condition::SETTINGS_CHANGED:
		AddTempvar("settings",
			   obs_module_text(
				   "AdvSceneSwitcher.tempVar.filter.settings"));
		break;
	case Condition::INDIVIDUAL_SETTING_MATCH:
	case Condition::INDIVIDUAL_SETTING_LIST_ENTRY_MATCH:
	case Condition::INDIVIDUAL_SETTING_CHANGED:
		AddTempvar("setting",
			   obs_module_text(
				   "AdvSceneSwitcher.tempVar.filter.setting"));
		break;
	default:
		break;
	}
}

static inline void populateConditionSelection(QComboBox *list)
{
	for (const auto &[value, name] : filterConditionTypes) {
		list->addItem(obs_module_text(name.c_str()),
			      static_cast<int>(value));
	}
}

static QStringList getFilterSourcesList()
{
	auto sources = GetSourcesWithFilterNames();
	sources.sort();
	return sources;
}

MacroConditionFilterEdit::MacroConditionFilterEdit(
	QWidget *parent, std::shared_ptr<MacroConditionFilter> entryData)
	: QWidget(parent),
	  _sources(new SourceSelectionWidget(this, getFilterSourcesList, true)),
	  _filters(new FilterSelectionWidget(this, _sources, true)),
	  _conditions(new QComboBox()),
	  _getSettings(new QPushButton(obs_module_text(
		  "AdvSceneSwitcher.condition.filter.getSettings"))),
	  _settings(new VariableTextEdit(this)),
	  _regex(new RegexConfigWidget(parent)),
	  _settingSelection(new SourceSettingSelection()),
	  _refreshSettingSelection(new QPushButton(obs_module_text(
		  "AdvSceneSwitcher.condition.filter.refresh"))),
	  _includeDefaults(new QCheckBox(
		  obs_module_text(
			  "AdvSceneSwitcher.condition.filter.includeDefaults"),
		  this))
{
	populateConditionSelection(_conditions);

	_refreshSettingSelection->setToolTip(obs_module_text(
		"AdvSceneSwitcher.condition.filter.refresh.tooltip"));

	QWidget::connect(_sources,
			 SIGNAL(SourceChanged(const SourceSelection &)), this,
			 SLOT(SourceChanged(const SourceSelection &)));
	QWidget::connect(_filters,
			 SIGNAL(FilterChanged(const FilterSelection &)), this,
			 SLOT(FilterChanged(const FilterSelection &)));
	QWidget::connect(_conditions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(_getSettings, SIGNAL(clicked()), this,
			 SLOT(GetSettingsClicked()));
	QWidget::connect(_settings, SIGNAL(textChanged()), this,
			 SLOT(SettingsChanged()));
	QWidget::connect(_regex,
			 SIGNAL(RegexConfigChanged(const RegexConfig &)), this,
			 SLOT(RegexChanged(const RegexConfig &)));
	QWidget::connect(_settingSelection,
			 SIGNAL(SelectionChanged(const SourceSetting &)), this,
			 SLOT(SettingSelectionChanged(const SourceSetting &)));
	QWidget::connect(_refreshSettingSelection, SIGNAL(clicked()), this,
			 SLOT(RefreshVariableSourceSelectionValue()));
	QWidget::connect(_includeDefaults, SIGNAL(stateChanged(int)), this,
			 SLOT(IncludeDefaultsChanged(int)));

	const std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{sources}}", _sources},
		{"{{filters}}", _filters},
		{"{{conditions}}", _conditions},
		{"{{settings}}", _settings},
		{"{{getSettings}}", _getSettings},
		{"{{regex}}", _regex},
		{"{{settingSelection}}", _settingSelection},
		{"{{refresh}}", _refreshSettingSelection}};

	auto line1Layout = new QHBoxLayout;
	line1Layout->setContentsMargins(0, 0, 0, 0);
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.filter.entry.line1"),
		     line1Layout, widgetPlaceholders);
	auto line2Layout = new QHBoxLayout;
	line2Layout->setContentsMargins(0, 0, 0, 0);
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.filter.entry.line2"),
		     line2Layout, widgetPlaceholders, false);
	auto line3Layout = new QHBoxLayout;
	line3Layout->setContentsMargins(0, 0, 0, 0);
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.filter.entry.line3"),
		     line3Layout, widgetPlaceholders);

	auto mainLayout = new QVBoxLayout;
	mainLayout->addLayout(line1Layout);
	mainLayout->addLayout(line2Layout);
	mainLayout->addLayout(line3Layout);
	mainLayout->addWidget(_includeDefaults);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionFilterEdit::SourceChanged(const SourceSelection &source)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_source = source;
}

void MacroConditionFilterEdit::FilterChanged(const FilterSelection &filter)
{
	{
		GUARD_LOADING_AND_LOCK();
		_entryData->_filter = filter;
	}
	const auto filters =
		_entryData->_filter.GetFilters(_entryData->_source);
	_settingSelection->SetSource(filters.empty() ? nullptr : filters.at(0));
	SetWidgetVisibility();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionFilterEdit::ConditionChanged(int index)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->SetCondition(static_cast<MacroConditionFilter::Condition>(
		_conditions->itemData(index).toInt()));
	SetWidgetVisibility();
}

void MacroConditionFilterEdit::GetSettingsClicked()
{
	if (_loading || !_entryData ||
	    _entryData->_filter.GetFilters(_entryData->_source).empty()) {
		return;
	}

	const auto filters =
		_entryData->_filter.GetFilters(_entryData->_source);
	if (filters.empty()) {
		_settings->setPlainText(QString(""));
		return;
	}

	QString value;
	if (_entryData->GetCondition() ==
	    MacroConditionFilter::Condition::SETTINGS_MATCH) {
		const auto settings = GetSourceSettings(
			filters.at(0), _entryData->_includeDefaults);
		if (settings) {
			value = FormatJsonString(*settings);
		}
	} else if (_entryData->GetCondition() ==
		   MacroConditionFilter::Condition::
			   INDIVIDUAL_SETTING_LIST_ENTRY_MATCH) {
		value = QString::fromStdString(
			GetSourceSettingListEntryName(
				filters.empty() ? nullptr : filters.at(0),
				_entryData->_setting)
				.value_or(""));
	} else {
		value = QString::fromStdString(
			GetSourceSettingValue(filters.at(0),
					      _entryData->_setting)
				.value_or(""));
	}

	if (_entryData->_regex.Enabled()) {
		value = EscapeForRegex(value);
	}
	_settings->setPlainText(value);
}

void MacroConditionFilterEdit::SettingsChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_settings = _settings->toPlainText().toStdString();

	adjustSize();
	updateGeometry();
}

void MacroConditionFilterEdit::RegexChanged(const RegexConfig &conf)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_regex = conf;

	adjustSize();
	updateGeometry();
}

void MacroConditionFilterEdit::SettingSelectionChanged(
	const SourceSetting &setting)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_setting = setting;
	SetWidgetVisibility();
}

void MacroConditionFilterEdit::RefreshVariableSourceSelectionValue()
{
	const auto filters =
		_entryData->_filter.GetFilters(_entryData->_source);
	_settingSelection->SetSource(filters.empty() ? nullptr : filters.at(0));
}

void MacroConditionFilterEdit::IncludeDefaultsChanged(int state)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_includeDefaults = state;
}

static QString GetIndividualListEntryName()
{
	static const auto matchesInput =
		[](const std::pair<MacroConditionFilter::Condition, std::string>
			   &p) {
			return p.first ==
			       MacroConditionFilter::Condition::
				       INDIVIDUAL_SETTING_LIST_ENTRY_MATCH;
		};
	static const QString listValueText(obs_module_text(
		std::find_if(filterConditionTypes.begin(),
			     filterConditionTypes.end(), matchesInput)
			->second.c_str()));
	return listValueText;
}

void MacroConditionFilterEdit::SetWidgetVisibility()
{
	const bool showSettingsControls =
		_entryData->GetCondition() ==
			MacroConditionFilter::Condition::SETTINGS_MATCH ||
		_entryData->GetCondition() ==
			MacroConditionFilter::Condition::INDIVIDUAL_SETTING_MATCH ||
		_entryData->GetCondition() ==
			MacroConditionFilter::Condition::
				INDIVIDUAL_SETTING_LIST_ENTRY_MATCH;
	_settings->setVisible(showSettingsControls);
	_getSettings->setVisible(showSettingsControls);
	_regex->setVisible(showSettingsControls);
	_settingSelection->setVisible(
		_entryData->GetCondition() ==
			MacroConditionFilter::Condition::INDIVIDUAL_SETTING_MATCH ||
		_entryData->GetCondition() ==
			MacroConditionFilter::Condition::
				INDIVIDUAL_SETTING_LIST_ENTRY_MATCH ||
		_entryData->GetCondition() ==
			MacroConditionFilter::Condition::
				INDIVIDUAL_SETTING_CHANGED);
	_refreshSettingSelection->setVisible(
		(_entryData->GetCondition() ==
			 MacroConditionFilter::Condition::
				 INDIVIDUAL_SETTING_MATCH ||
		 _entryData->GetCondition() ==
			 MacroConditionFilter::Condition::
				 INDIVIDUAL_SETTING_LIST_ENTRY_MATCH) &&
		(_entryData->_source.GetType() ==
			 SourceSelection::Type::VARIABLE ||
		 _entryData->_filter.GetType() ==
			 FilterSelection::Type::VARIABLE));

	SetRowVisibleByValue(_conditions, GetIndividualListEntryName(),
			     _entryData->_setting.IsList());

	_includeDefaults->setVisible(
		_entryData->GetCondition() ==
		MacroConditionFilter::Condition::SETTINGS_MATCH);

	adjustSize();
	updateGeometry();
}

void MacroConditionFilterEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_sources->SetSource(_entryData->_source);
	_filters->SetFilter(_entryData->_source, _entryData->_filter);
	_conditions->setCurrentIndex(_conditions->findData(
		static_cast<int>(_entryData->GetCondition())));
	_settings->setPlainText(_entryData->_settings);
	_regex->SetRegexConfig(_entryData->_regex);
	const auto filters =
		_entryData->_filter.GetFilters(_entryData->_source);
	_settingSelection->SetSelection(filters.empty() ? nullptr
							: filters.at(0),
					_entryData->_setting);
	_includeDefaults->setChecked(_entryData->_includeDefaults);
	SetWidgetVisibility();

	adjustSize();
	updateGeometry();
}

} // namespace advss
