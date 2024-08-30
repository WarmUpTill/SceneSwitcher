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

static const std::map<MacroConditionSource::Condition, std::string>
	sourceConditionTypes = {
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
		{MacroConditionSource::Condition::HEIGHT,
		 "AdvSceneSwitcher.condition.source.type.height"},
		{MacroConditionSource::Condition::WIDTH,
		 "AdvSceneSwitcher.condition.source.type.width"},
};

static const std::map<MacroConditionSource::SizeComparision, std::string>
	compareMethods = {
		{MacroConditionSource::SizeComparision::LESS,
		 "AdvSceneSwitcher.condition.source.sizeCompare.less"},
		{MacroConditionSource::SizeComparision::EQUAL,
		 "AdvSceneSwitcher.condition.source.sizeCompare.equal"},
		{MacroConditionSource::SizeComparision::MORE,
		 "AdvSceneSwitcher.condition.source.sizeCompare.more"},
};

static bool compareSourceSize(MacroConditionSource::SizeComparision method,
			      int sourceValue, int compareValue)
{
	switch (method) {
	case MacroConditionSource::SizeComparision::LESS:
		return sourceValue < compareValue;
	case MacroConditionSource::SizeComparision::EQUAL:
		return sourceValue == compareValue;
	case MacroConditionSource::SizeComparision::MORE:
		return sourceValue > compareValue;
	default:
		break;
	}
	return false;
}

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
	case Condition::ALL_SETTINGS_MATCH: {
		ret = CompareSourceSettings(_source.GetSource(), _settings,
					    _regex);
		const auto settings = GetSourceSettings(_source.GetSource());
		SetVariableValue(settings);
		SetTempVarValue("settings", settings);
		break;
	}
	case Condition::SETTINGS_CHANGED: {
		const auto settings = GetSourceSettings(_source.GetSource());
		ret = !_currentSettings.empty() && settings != _currentSettings;
		_currentSettings = settings;
		SetVariableValue(settings);
		SetTempVarValue("settings", settings);
		break;
	}
	case Condition::INDIVIDUAL_SETTING_MATCH: {
		const auto value =
			GetSourceSettingValue(_source.GetSource(), _setting);
		if (!value) {
			return false;
		}
		ret = _regex.Enabled() ? _regex.Matches(*value, _settings)
				       : value == std::string(_settings);
		SetVariableValue(*value);
		SetTempVarValue("setting", *value);
		break;
	}
	case Condition::INDIVIDUAL_SETTING_CHANGED: {
		const auto value =
			GetSourceSettingValue(_source.GetSource(), _setting);
		if (!value) {
			return false;
		}
		ret = _currentSettingsValue != *value;
		_currentSettingsValue = *value;
		SetVariableValue(*value);
		SetTempVarValue("setting", *value);
		break;
	}
	case Condition::HEIGHT: {
		const auto height = obs_source_get_height(s);
		ret = compareSourceSize(_comparision, height, _size);
		SetTempVarValue("height", std::to_string(height));
		break;
	}
	case Condition::WIDTH: {
		const auto width = obs_source_get_width(s);
		ret = compareSourceSize(_comparision, width, _size);
		SetTempVarValue("width", std::to_string(width));
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
	_size.Save(obj, "size");
	obs_data_set_int(obj, "sizeComparisionMethod",
			 static_cast<int>(_comparision));
	return true;
}

bool MacroConditionSource::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_source.Load(obj);
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
	_size.Load(obj, "size");
	_comparision = static_cast<SizeComparision>(
		obs_data_get_int(obj, "sizeComparisionMethod"));
	return true;
}

std::string MacroConditionSource::GetShortDesc() const
{
	return _source.ToString();
}

void MacroConditionSource::SetCondition(Condition cond)
{
	_condition = cond;
	SetupTempVars();
}

void MacroConditionSource::SetupTempVars()
{
	MacroCondition::SetupTempVars();
	switch (_condition) {
	case Condition::ACTIVE:
		break;
	case Condition::SHOWING:
		break;
	case Condition::ALL_SETTINGS_MATCH:
	case Condition::SETTINGS_CHANGED:
		AddTempvar("settings",
			   obs_module_text(
				   "AdvSceneSwitcher.tempVar.source.settings"));
		break;
	case Condition::INDIVIDUAL_SETTING_MATCH:
	case Condition::INDIVIDUAL_SETTING_CHANGED:
		AddTempvar("setting",
			   obs_module_text(
				   "AdvSceneSwitcher.tempVar.source.setting"));
		break;
	case Condition::HEIGHT:
		AddTempvar("height",
			   obs_module_text(
				   "AdvSceneSwitcher.tempVar.source.height"));
		break;
	case Condition::WIDTH:
		AddTempvar("width",
			   obs_module_text(
				   "AdvSceneSwitcher.tempVar.source.width"));
		break;
	default:
		break;
	}
}

template<class T>
static inline void populateSelection(QComboBox *list,
				     const std::map<T, std::string> &map)
{
	for (const auto &[_, name] : map) {
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
	  _refreshSettingSelection(new QPushButton(obs_module_text(
		  "AdvSceneSwitcher.condition.source.refresh"))),
	  _size(new VariableSpinBox(this)),
	  _sizeCompareMethods(new QComboBox())
{
	populateSelection(_conditions, sourceConditionTypes);
	populateSelection(_sizeCompareMethods, compareMethods);
	auto sources = GetSourceNames();
	sources.sort();
	auto scenes = GetSceneNames();
	scenes.sort();
	_sources->SetSourceNameList(sources + scenes);
	_refreshSettingSelection->setToolTip(obs_module_text(
		"AdvSceneSwitcher.condition.source.refreshTooltip"));
	_size->setMaximum(999999);

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
	QWidget::connect(
		_size,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(SizeChanged(const NumberVariable<int> &)));
	QWidget::connect(_sizeCompareMethods, SIGNAL(currentIndexChanged(int)),
			 this, SLOT(CompareMethodChanged(int)));

	auto line1Layout = new QHBoxLayout;
	line1Layout->setContentsMargins(0, 0, 0, 0);
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.source.entry.line1"),
		     line1Layout,
		     {{"{{sources}}", _sources},
		      {"{{conditions}}", _conditions},
		      {"{{settingSelection}}", _settingSelection},
		      {"{{refresh}}", _refreshSettingSelection},
		      {"{{size}}", _size},
		      {"{{sizeCompareMethods}}", _sizeCompareMethods}});
	auto line2Layout = new QHBoxLayout;
	line2Layout->setContentsMargins(0, 0, 0, 0);
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.source.entry.line2"),
		     line2Layout, {{"{{settings}}", _settings}}, false);
	auto line3Layout = new QHBoxLayout;
	line3Layout->setContentsMargins(0, 0, 0, 0);
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.source.entry.line3"),
		     line3Layout,
		     {{"{{getSettings}}", _getSettings}, {"{{regex}}", _regex}},
		     true);
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
	_entryData->SetCondition(
		static_cast<MacroConditionSource::Condition>(index));
	SetWidgetVisibility();
}

void MacroConditionSourceEdit::GetSettingsClicked()
{
	if (_loading || !_entryData || !_entryData->_source.GetSource()) {
		return;
	}

	QString value;
	if (_entryData->GetCondition() ==
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

void MacroConditionSourceEdit::SizeChanged(const NumberVariable<int> &value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_size = value;
}

void MacroConditionSourceEdit::CompareMethodChanged(int index)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_comparision =
		static_cast<MacroConditionSource::SizeComparision>(index);
}

void MacroConditionSourceEdit::SetWidgetVisibility()
{
	const bool settingsMatch =
		_entryData->GetCondition() ==
			MacroConditionSource::Condition::ALL_SETTINGS_MATCH ||
		_entryData->GetCondition() ==
			MacroConditionSource::Condition::INDIVIDUAL_SETTING_MATCH;
	_settings->setVisible(settingsMatch);
	_getSettings->setVisible(settingsMatch);
	_regex->setVisible(settingsMatch);
	_settingSelection->setVisible(
		_entryData->GetCondition() ==
			MacroConditionSource::Condition::INDIVIDUAL_SETTING_MATCH ||
		_entryData->GetCondition() ==
			MacroConditionSource::Condition::
				INDIVIDUAL_SETTING_CHANGED);

	setToolTip(
		(_entryData->GetCondition() ==
			 MacroConditionSource::Condition::ACTIVE ||
		 _entryData->GetCondition() ==
			 MacroConditionSource::Condition::SHOWING)
			? obs_module_text(
				  "AdvSceneSwitcher.condition.source.sceneVisibilityHint")
			: "");

	_refreshSettingSelection->setVisible(
		_entryData->GetCondition() ==
			MacroConditionSource::Condition::INDIVIDUAL_SETTING_MATCH &&
		_entryData->_source.GetType() ==
			SourceSelection::Type::VARIABLE);

	const bool isSizeCheck =
		_entryData->GetCondition() ==
			MacroConditionSource::Condition::WIDTH ||
		_entryData->GetCondition() ==
			MacroConditionSource::Condition::HEIGHT;
	_size->setVisible(isSizeCheck);
	_sizeCompareMethods->setVisible(isSizeCheck);

	adjustSize();
	updateGeometry();
}

void MacroConditionSourceEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_sources->SetSource(_entryData->_source);
	_conditions->setCurrentIndex(
		static_cast<int>(_entryData->GetCondition()));
	_settings->setPlainText(_entryData->_settings);
	_regex->SetRegexConfig(_entryData->_regex);
	_settingSelection->SetSource(_entryData->_source.GetSource());
	_settingSelection->SetSetting(_entryData->_setting);
	_size->SetValue(_entryData->_size);
	_sizeCompareMethods->setCurrentIndex(
		static_cast<int>(_entryData->_comparision));
	SetWidgetVisibility();
}

} // namespace advss
