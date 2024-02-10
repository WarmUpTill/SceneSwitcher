#include "macro-condition-source.hpp"
#include "layout-helpers.hpp"
#include "json-helpers.hpp"
#include "text-helpers.hpp"
#include "selection-helpers.hpp"
#include "source-settings-helpers.hpp"

namespace advss {

const std::string MacroConditionSource::id = "source";

bool MacroConditionSource::_registered = MacroConditionFactory::Register(
	MacroConditionSource::id,
	{MacroConditionSource::Create, MacroConditionSourceEdit::Create,
	 "AdvSceneSwitcher.condition.source"});

const static std::map<MacroConditionSource::Condition, std::string>
	sourceCnditionTypes = {
		{MacroConditionSource::Condition::ACTIVE,
		 "AdvSceneSwitcher.condition.source.type.active"},
		{MacroConditionSource::Condition::SHOWING,
		 "AdvSceneSwitcher.condition.source.type.showing"},
		{MacroConditionSource::Condition::ALL_SETTINGS_MATCH,
		 "AdvSceneSwitcher.condition.source.type.settingsMatch"},
		{MacroConditionSource::Condition::SETTINGS_CHANGED,
		 "AdvSceneSwitcher.condition.source.type.settingsChanged"},
		{MacroConditionSource::Condition::INDIVIDUAL_SETTING_MATCH,
		 "AdvSceneSwitcher.condition.source.type.individualSettingMatches"},
		{MacroConditionSource::Condition::INDIVIDUAL_SETTING_CHANGED,
		 "AdvSceneSwitcher.condition.source.type.individualSettingChanged"},
};

bool MacroConditionSource::CheckCondition()
{
	if (!_source.GetSource()) {
		return false;
	}

	bool ret = false;
	OBSSourceAutoRelease s =
		obs_weak_source_get_source(_source.GetSource());

	switch (_condition) {
	case Condition::ACTIVE:
		ret = obs_source_active(s);
		break;
	case Condition::SHOWING:
		ret = obs_source_showing(s);
		break;
	case Condition::ALL_SETTINGS_MATCH:
		ret = CompareSourceSettings(_source.GetSource(), _settings,
					    _regex);
		if (IsReferencedInVars()) {
			SetVariableValue(
				GetSourceSettings(_source.GetSource()));
		}
		break;
	case Condition::SETTINGS_CHANGED: {
		std::string settings = GetSourceSettings(_source.GetSource());
		ret = !_currentSettings.empty() && settings != _currentSettings;
		_currentSettings = settings;
		SetVariableValue(settings);
		break;
	}
	case Condition::INDIVIDUAL_SETTING_MATCH: {
		auto value =
			GetSourceSettingValue(_source.GetSource(), _setting);
		if (!value) {
			return false;
		}
		ret = _regex.Enabled() ? _regex.Matches(*value, _settings)
				       : value == std::string(_settings);
		SetVariableValue(*value);
		break;
	}
	case Condition::INDIVIDUAL_SETTING_CHANGED: {
		auto value =
			GetSourceSettingValue(_source.GetSource(), _setting);
		if (!value) {
			return false;
		}
		ret = _currentSettingsValue != *value;
		_currentSettingsValue = *value;
		SetVariableValue(*value);
		break;
	}
	default:
		break;
	}

	if (GetVariableValue().empty()) {
		SetVariableValue(ret ? "true" : "false");
	}

	return ret;
}

bool MacroConditionSource::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	_source.Save(obj);
	obs_data_set_int(obj, "condition", static_cast<int>(_condition));
	_settings.Save(obj, "settings");
	_regex.Save(obj);
	_setting.Save(obj);
	return true;
}

bool MacroConditionSource::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_source.Load(obj);
	_condition = static_cast<Condition>(obs_data_get_int(obj, "condition"));
	_settings.Load(obj, "settings");
	_regex.Load(obj);
	// TOOD: remove in future version
	if (obs_data_has_user_value(obj, "regex")) {
		_regex.CreateBackwardsCompatibleRegex(
			obs_data_get_bool(obj, "regex"));
	}
	_setting.Load(obj);
	return true;
}

std::string MacroConditionSource::GetShortDesc() const
{
	return _source.ToString();
}

static inline void populateConditionSelection(QComboBox *list)
{
	for (const auto &[_, name] : sourceCnditionTypes) {
		list->addItem(obs_module_text(name.c_str()));
	}
}

MacroConditionSourceEdit::MacroConditionSourceEdit(
	QWidget *parent, std::shared_ptr<MacroConditionSource> entryData)
	: QWidget(parent),
	  _sources(new SourceSelectionWidget(this, QStringList(), true)),
	  _conditions(new QComboBox()),
	  _getSettings(new QPushButton(obs_module_text(
		  "AdvSceneSwitcher.condition.source.getSettings"))),
	  _settings(new VariableTextEdit(this)),
	  _regex(new RegexConfigWidget(parent)),
	  _settingSelection(new SourceSettingSelection()),
	  _refreshSettingSelection(new QPushButton(
		  obs_module_text("AdvSceneSwitcher.condition.source.refresh")))
{
	populateConditionSelection(_conditions);
	auto sources = GetSourceNames();
	sources.sort();
	auto scenes = GetSceneNames();
	scenes.sort();
	_sources->SetSourceNameList(sources + scenes);
	_refreshSettingSelection->setToolTip(obs_module_text(
		"AdvSceneSwitcher.condition.source.refreshTooltip"));

	QWidget::connect(_sources,
			 SIGNAL(SourceChanged(const SourceSelection &)), this,
			 SLOT(SourceChanged(const SourceSelection &)));
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

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{sources}}", _sources},
		{"{{conditions}}", _conditions},
		{"{{settings}}", _settings},
		{"{{getSettings}}", _getSettings},
		{"{{regex}}", _regex},
		{"{{settingSelection}}", _settingSelection},
		{"{{refresh}}", _refreshSettingSelection}};

	auto line1Layout = new QHBoxLayout;
	line1Layout->setContentsMargins(0, 0, 0, 0);
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.source.entry.line1"),
		     line1Layout, widgetPlaceholders);
	auto line2Layout = new QHBoxLayout;
	line2Layout->setContentsMargins(0, 0, 0, 0);
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.source.entry.line2"),
		     line2Layout, widgetPlaceholders, false);
	auto line3Layout = new QHBoxLayout;
	line3Layout->setContentsMargins(0, 0, 0, 0);
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.source.entry.line3"),
		     line3Layout, widgetPlaceholders);
	auto mainLayout = new QVBoxLayout;
	mainLayout->addLayout(line1Layout);
	mainLayout->addLayout(line2Layout);
	mainLayout->addLayout(line3Layout);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionSourceEdit::SourceChanged(const SourceSelection &source)
{
	if (_loading || !_entryData) {
		return;
	}

	{
		auto lock = LockContext();
		_entryData->_source = source;
	}
	_settingSelection->SetSource(_entryData->_source.GetSource());
	SetWidgetVisibility();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionSourceEdit::ConditionChanged(int index)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_condition =
		static_cast<MacroConditionSource::Condition>(index);
	SetWidgetVisibility();
}

void MacroConditionSourceEdit::GetSettingsClicked()
{
	if (_loading || !_entryData || !_entryData->_source.GetSource()) {
		return;
	}

	QString value;
	if (_entryData->_condition ==
	    MacroConditionSource::Condition::ALL_SETTINGS_MATCH) {
		value = FormatJsonString(
			GetSourceSettings(_entryData->_source.GetSource()));
	} else {
		value = QString::fromStdString(
			GetSourceSettingValue(_entryData->_source.GetSource(),
					      _entryData->_setting)
				.value_or(""));
	}

	if (_entryData->_regex.Enabled()) {
		value = EscapeForRegex(value);
	}
	_settings->setPlainText(value);
}

void MacroConditionSourceEdit::SettingsChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_settings = _settings->toPlainText().toStdString();

	adjustSize();
	updateGeometry();
}

void MacroConditionSourceEdit::RegexChanged(const RegexConfig &conf)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_regex = conf;

	adjustSize();
	updateGeometry();
}

void MacroConditionSourceEdit::SettingSelectionChanged(
	const SourceSetting &setting)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_setting = setting;
}

void MacroConditionSourceEdit::RefreshVariableSourceSelectionValue()
{
	_settingSelection->SetSource(_entryData->_source.GetSource());
}

void MacroConditionSourceEdit::SetWidgetVisibility()
{
	const bool settingsMatch =
		_entryData->_condition ==
			MacroConditionSource::Condition::ALL_SETTINGS_MATCH ||
		_entryData->_condition ==
			MacroConditionSource::Condition::INDIVIDUAL_SETTING_MATCH;
	_settings->setVisible(settingsMatch);
	_getSettings->setVisible(settingsMatch);
	_regex->setVisible(settingsMatch);
	_settingSelection->setVisible(
		_entryData->_condition == MacroConditionSource::Condition::
						  INDIVIDUAL_SETTING_MATCH ||
		_entryData->_condition == MacroConditionSource::Condition::
						  INDIVIDUAL_SETTING_CHANGED);

	setToolTip(
		(_entryData->_condition ==
			 MacroConditionSource::Condition::ACTIVE ||
		 _entryData->_condition ==
			 MacroConditionSource::Condition::SHOWING)
			? obs_module_text(
				  "AdvSceneSwitcher.condition.source.sceneVisibilityHint")
			: "");

	_refreshSettingSelection->setVisible(
		_entryData->_condition == MacroConditionSource::Condition::
						  INDIVIDUAL_SETTING_MATCH &&
		_entryData->_source.GetType() ==
			SourceSelection::Type::VARIABLE);

	adjustSize();
	updateGeometry();
}

void MacroConditionSourceEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_sources->SetSource(_entryData->_source);
	_conditions->setCurrentIndex(static_cast<int>(_entryData->_condition));
	_settings->setPlainText(_entryData->_settings);
	_regex->SetRegexConfig(_entryData->_regex);
	_settingSelection->SetSource(_entryData->_source.GetSource());
	_settingSelection->SetSetting(_entryData->_setting);
	SetWidgetVisibility();
}

} // namespace advss
