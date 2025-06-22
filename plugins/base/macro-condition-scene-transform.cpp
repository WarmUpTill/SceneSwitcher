#include "macro-condition-scene-transform.hpp"
#include "layout-helpers.hpp"
#include "json-helpers.hpp"
#include "math-helpers.hpp"
#include "text-helpers.hpp"
#include "scene-item-transform-helpers.hpp"

namespace advss {

const std::string MacroConditionSceneTransform::id = "scene_transform";

bool MacroConditionSceneTransform::_registered =
	MacroConditionFactory::Register(
		MacroConditionSceneTransform::id,
		{MacroConditionSceneTransform::Create,
		 MacroConditionSceneTransformEdit::Create,
		 "AdvSceneSwitcher.condition.sceneTransform"});

static const std::map<MacroConditionSceneTransform::SettingsType, std::string>
	settingsTypes = {
		{MacroConditionSceneTransform::SettingsType::ALL,
		 "AdvSceneSwitcher.condition.sceneTransform.settingsType.all"},
		{MacroConditionSceneTransform::SettingsType::SINGLE,
		 "AdvSceneSwitcher.condition.sceneTransform.settingsType.single"},
};
static const std::map<MacroConditionSceneTransform::Compare, std::string>
	compareTypes = {
		{MacroConditionSceneTransform::Compare::LESS,
		 "AdvSceneSwitcher.condition.sceneTransform.compare.less"},
		{MacroConditionSceneTransform::Compare::EQUAL,
		 "AdvSceneSwitcher.condition.sceneTransform.compare.equal"},
		{MacroConditionSceneTransform::Compare::MORE,
		 "AdvSceneSwitcher.condition.sceneTransform.compare.more"},
};
static const std::map<MacroConditionSceneTransform::Condition, std::string>
	conditionTypes = {
		{MacroConditionSceneTransform::Condition::MATCHES,
		 "AdvSceneSwitcher.condition.sceneTransform.condition.match"},
		{MacroConditionSceneTransform::Condition::CHANGED,
		 "AdvSceneSwitcher.condition.sceneTransform.condition.changed"},
};

static bool doesTransformOfAnySceneItemMatch(
	const std::vector<OBSSceneItem> &items, const std::string &jsonCompare,
	const RegexConfig &regex, std::string &newVariable)
{
	bool ret = false;
	std::string json;
	for (const auto &item : items) {
		json = GetSceneItemTransform(item);
		if (MatchJson(json, jsonCompare, regex)) {
			ret = true;
		}
	}
	newVariable = json;
	return ret;
}

static bool
didTransformOfAnySceneItemChange(const std::vector<OBSSceneItem> &items,
				 std::vector<std::string> &previousTransform,
				 std::string &newVariable)
{
	bool ret = false;
	std::string json;
	RegexConfig regex;
	auto numItems = items.size();
	if (previousTransform.size() < numItems) {
		ret = true;
		previousTransform.resize(numItems);
	}
	for (size_t idx = 0; idx < numItems; ++idx) {
		auto const &item = items[idx];
		json = GetSceneItemTransform(item);
		if (!MatchJson(json, previousTransform[idx], regex)) {
			ret = true;
			previousTransform[idx] = json;
		}
	}
	newVariable = json;
	return ret;
}

bool MacroConditionSceneTransform::CheckAllSettings(
	const std::vector<OBSSceneItem> &items)
{
	std::string newVariable = "";
	bool ret = false;
	switch (_condition) {
	case Condition::MATCHES:
		ret = doesTransformOfAnySceneItemMatch(items, _transformString,
						       _regex, newVariable);
		break;
	case Condition::CHANGED:
		ret = didTransformOfAnySceneItemChange(
			items, _previousTransform, newVariable);
		break;

	default:
		return false;
	}

	SetVariableValue(newVariable);
	SetTempVarValue("settings", newVariable);
	return ret;
}

bool MacroConditionSceneTransform::AnySceneItemTransformSettingChanged(
	const std::vector<OBSSceneItem> &items)
{
	bool ret = false;
	if (_previousSettingValues.size() < items.size()) {
		_previousSettingValues.resize(items.size());
		return true;
	}

	std::vector<std::string> varValues;

	for (size_t i = 0; i < items.size(); i++) {
		const auto &item = items[i];
		auto &previousValue = _previousSettingValues[i];

		const auto currentValue =
			GetTransformSettingValue(item, _setting);
		if (!currentValue) {
			continue;
		}
		varValues.emplace_back(*currentValue);

		if (*currentValue == previousValue) {
			continue;
		}
		previousValue = *currentValue;
		ret = true;
	}

	SetTempVarHelper(varValues);
	return ret;
}

static bool compareValue(MacroConditionSceneTransform::Compare compareMethod,
			 double value1, double value2)
{
	switch (compareMethod) {
	case MacroConditionSceneTransform::Compare::LESS:
		return value1 < value2;
	case MacroConditionSceneTransform::Compare::EQUAL:
		return DoubleEquals(value1, value2, 0.00001);
	case MacroConditionSceneTransform::Compare::MORE:
		return value1 > value2;
	default:
		break;
	}
	return false;
}

bool MacroConditionSceneTransform::AnySceneItemTransformSettingMatches(
	const std::vector<OBSSceneItem> &items)
{
	bool ret = false;
	std::vector<std::string> varValues;

	for (const auto &item : items) {
		const auto currentValue =
			GetTransformSettingValue(item, _setting);
		if (!currentValue) {
			continue;
		}
		try {
			if (compareValue(_compare, std::stod(*currentValue),
					 std::stod(_singleSetting))) {
				ret = true;
			}
		} catch (std::invalid_argument &) {
		} catch (std::out_of_range &) {
		}

		varValues.emplace_back(*currentValue);
	}

	SetTempVarHelper(varValues);
	return ret;
}

void MacroConditionSceneTransform::SetTempVarHelper(
	const std::vector<std::string> &values)
{
	std::string varValue;
	bool addSeperator = false;
	for (const auto &value : values) {
		if (addSeperator) {
			varValue += ";";
		}
		varValue += value;
		addSeperator = true;
	}
	SetTempVarValue("setting", varValue);
}

bool MacroConditionSceneTransform::CheckSingleSetting(
	const std::vector<OBSSceneItem> &items)
{
	if (_condition == Condition::CHANGED) {
		return AnySceneItemTransformSettingChanged(items);
	}
	return AnySceneItemTransformSettingMatches(items);
}

bool MacroConditionSceneTransform::CheckCondition()
{
	auto items = _source.GetSceneItems(_scene);
	if (items.empty()) {
		return false;
	}

	switch (_settingsType) {
	case SettingsType::ALL:
		return CheckAllSettings(items);
	case SettingsType::SINGLE:
		return CheckSingleSetting(items);
	default:
		break;
	}

	return false;
}

bool MacroConditionSceneTransform::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	_scene.Save(obj);
	_source.Save(obj);
	_transformString.Save(obj, "transformString");
	_singleSetting.Save(obj, "singleSetting");
	_regex.Save(obj);
	_setting.Save(obj);
	obs_data_set_int(obj, "settingsType", static_cast<int>(_settingsType));
	obs_data_set_int(obj, "compare", static_cast<int>(_compare));
	obs_data_set_int(obj, "condition", static_cast<int>(_condition));
	obs_data_set_int(obj, "version", 1);
	return true;
}

bool MacroConditionSceneTransform::Load(obs_data_t *obj)
{
	// Convert old data format
	// TODO: Remove in future version
	if (obs_data_has_user_value(obj, "source")) {
		auto sourceName = obs_data_get_string(obj, "source");
		obs_data_set_string(obj, "sceneItem", sourceName);
	}

	MacroCondition::Load(obj);
	_scene.Load(obj);
	_source.Load(obj);
	_transformString.Load(obj, "transformString");
	_singleSetting.Load(obj, "singleSetting");
	_regex.Load(obj);
	_setting.Load(obj);
	SetSettingsType(static_cast<SettingsType>(
		obs_data_get_int(obj, "settingsType")));
	_compare = static_cast<Compare>(obs_data_get_int(obj, "compare"));
	SetCondition(
		static_cast<Condition>(obs_data_get_int(obj, "condition")));

	// TOOD: remove in future version
	if (obs_data_has_user_value(obj, "regex")) {
		_regex.CreateBackwardsCompatibleRegex(
			obs_data_get_bool(obj, "regex"));
	}

	if (!obs_data_has_user_value(obj, "version")) {
		SetCondition(
			static_cast<Condition>(obs_data_get_int(obj, "type")));
		_transformString.Load(obj, "settings");
		SetSettingsType(SettingsType::ALL);
		_compare = Compare::EQUAL;
	}

	SetupTempVars();

	return true;
}

std::string MacroConditionSceneTransform::GetShortDesc() const
{
	if (_source.ToString().empty()) {
		return "";
	}

	return _scene.ToString() + " - " + _source.ToString();
}

void MacroConditionSceneTransform::SetSettingsType(SettingsType type)
{
	_settingsType = type;
	SetupTempVars();
}

void MacroConditionSceneTransform::SetCondition(Condition condition)
{
	_condition = condition;
	_previousTransform.clear();
	_previousSettingValues.clear();
}

void MacroConditionSceneTransform::SetupTempVars()
{
	MacroCondition::SetupTempVars();
	if (_settingsType == SettingsType::ALL) {
		AddTempvar(
			"settings",
			obs_module_text(
				"AdvSceneSwitcher.tempVar.transform.settings"));
	} else {
		AddTempvar(
			"setting",
			obs_module_text(
				"AdvSceneSwitcher.tempVar.transform.setting"));
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

MacroConditionSceneTransformEdit::MacroConditionSceneTransformEdit(
	QWidget *parent,
	std::shared_ptr<MacroConditionSceneTransform> entryData)
	: QWidget(parent),
	  _scenes(new SceneSelectionWidget(this, true, false, false, true)),
	  _sources(new SceneItemSelectionWidget(
		  parent, true, SceneItemSelectionWidget::Placeholder::ANY)),
	  _settingsType(new QComboBox()),
	  _compare(new QComboBox()),
	  _conditions(new QComboBox()),
	  _getSettings(new QPushButton(obs_module_text(
		  "AdvSceneSwitcher.condition.sceneTransform.getTransform"))),
	  _getCurrentValue(new QPushButton(obs_module_text(
		  "AdvSceneSwitcher.condition.sceneTransform.getCurrentValue"))),
	  _transformString(new VariableTextEdit(this)),
	  _singleSettingValue(new VariableLineEdit(this)),
	  _regex(new RegexConfigWidget(parent)),
	  _settingSelection(new TransformSettingSelection(this)),
	  _compareLayout(new QHBoxLayout())
{
	populateSelection(_settingsType, settingsTypes);
	populateSelection(_compare, compareTypes);
	populateSelection(_conditions, conditionTypes);

	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 this, SLOT(SceneChanged(const SceneSelection &)));
	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 _sources, SLOT(SceneChanged(const SceneSelection &)));
	QWidget::connect(_sources,
			 SIGNAL(SceneItemChanged(const SceneItemSelection &)),
			 this, SLOT(SourceChanged(const SceneItemSelection &)));
	QWidget::connect(_settingsType, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(SettingsTypeChanged(int)));
	QWidget::connect(_compare, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(CompareChanged(int)));
	QWidget::connect(_conditions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(
		_settingSelection,
		SIGNAL(SelectionChanged(const TransformSetting &)), this,
		SLOT(SettingSelectionChanged(const TransformSetting &)));
	QWidget::connect(_getSettings, SIGNAL(clicked()), this,
			 SLOT(GetSettingsClicked()));
	QWidget::connect(_getCurrentValue, SIGNAL(clicked()), this,
			 SLOT(GetCurrentValueClicked()));
	QWidget::connect(_transformString, SIGNAL(textChanged()), this,
			 SLOT(TransformChanged()));
	QWidget::connect(_singleSettingValue, SIGNAL(editingFinished()), this,
			 SLOT(SettingValueChanged()));
	QWidget::connect(_regex,
			 SIGNAL(RegexConfigChanged(const RegexConfig &)), this,
			 SLOT(RegexChanged(const RegexConfig &)));

	const std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{scenes}}", _scenes},
		{"{{sources}}", _sources},
		{"{{compare}}", _compare},
		{"{{setting}}", _settingSelection},
		{"{{settingsType}}", _settingsType},
		{"{{conditions}}", _conditions},
		{"{{transformString}}", _transformString},
		{"{{singleSettingValue}}", _singleSettingValue},
		{"{{settings}}", _singleSettingValue},
		{"{{getSettings}}", _getSettings},
		{"{{getCurrentValue}}", _getCurrentValue},
		{"{{regex}}", _regex},
	};

	auto entryLayout = new QHBoxLayout;
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.sceneTransform.entry"),
		     entryLayout, widgetPlaceholders);
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.sceneTransform.entry.singleSettingCompare"),
		_compareLayout, widgetPlaceholders, false);
	auto optionsLayout = new QHBoxLayout;
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.sceneTransform.entry.options"),
		optionsLayout, widgetPlaceholders);

	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(entryLayout);
	mainLayout->addLayout(_compareLayout);
	mainLayout->addWidget(_transformString);
	mainLayout->addLayout(optionsLayout);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionSceneTransformEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_scenes->SetScene(_entryData->_scene);
	_sources->SetSceneItem(_entryData->_source);
	_settingsType->setCurrentIndex(
		static_cast<int>(_entryData->GetSettingsType()));
	_compare->setCurrentIndex(static_cast<int>(_entryData->_compare));
	_conditions->setCurrentIndex(
		static_cast<int>(_entryData->GetCondition()));
	_regex->SetRegexConfig(_entryData->_regex);
	UpdateSettingSelection();
	_settingSelection->SetSetting(_entryData->_setting);
	_transformString->setPlainText(_entryData->_transformString);
	_singleSettingValue->setText(_entryData->_singleSetting);
	SetWidgetVisibility();
}

void MacroConditionSceneTransformEdit::SceneChanged(const SceneSelection &s)
{
	{
		GUARD_LOADING_AND_LOCK();
		_entryData->_scene = s;
	}
	UpdateSettingSelection();
}

void MacroConditionSceneTransformEdit::SourceChanged(
	const SceneItemSelection &item)
{
	{
		GUARD_LOADING_AND_LOCK();
		_entryData->_source = item;
	}

	UpdateSettingSelection();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
	adjustSize();
	updateGeometry();
}

void MacroConditionSceneTransformEdit::SettingsTypeChanged(int type)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->SetSettingsType(
		static_cast<MacroConditionSceneTransform::SettingsType>(type));
	SetWidgetVisibility();
}

void MacroConditionSceneTransformEdit::CompareChanged(int type)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_compare =
		static_cast<MacroConditionSceneTransform::Compare>(type);
}

void MacroConditionSceneTransformEdit::ConditionChanged(int index)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->SetCondition(
		static_cast<MacroConditionSceneTransform::Condition>(index));
	SetWidgetVisibility();
}

void MacroConditionSceneTransformEdit::SettingSelectionChanged(
	const TransformSetting &setting)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_setting = setting;
}

void MacroConditionSceneTransformEdit::GetSettingsClicked()
{
	if (_loading || !_entryData || !_entryData->_scene.GetScene(false)) {
		return;
	}

	auto items = _entryData->_source.GetSceneItems(_entryData->_scene);
	if (items.empty()) {
		return;
	}

	auto settings = FormatJsonString(GetSceneItemTransform(items[0]));
	if (_entryData->_regex.Enabled()) {
		settings = EscapeForRegex(settings);
	}
	_transformString->setPlainText(settings);

	adjustSize();
	updateGeometry();
}

void MacroConditionSceneTransformEdit::GetCurrentValueClicked()
{
	if (_loading || !_entryData || !_entryData->_scene.GetScene(false)) {
		return;
	}

	auto items = _entryData->_source.GetSceneItems(_entryData->_scene);
	if (items.empty()) {
		return;
	}

	auto value =
		GetTransformSettingValue(items.at(0), _entryData->_setting);
	if (!value) {
		return;
	}

	_singleSettingValue->setText(QString::fromStdString(*value));
	SettingValueChanged();
}

void MacroConditionSceneTransformEdit::TransformChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_transformString =
		_transformString->toPlainText().toStdString();

	adjustSize();
	updateGeometry();
}

void MacroConditionSceneTransformEdit::SettingValueChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_singleSetting = _singleSettingValue->text().toStdString();

	adjustSize();
	updateGeometry();
}

void MacroConditionSceneTransformEdit::RegexChanged(const RegexConfig &conf)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_regex = conf;

	adjustSize();
	updateGeometry();
}

void MacroConditionSceneTransformEdit::SetWidgetVisibility()
{
	const auto condition = _entryData->GetCondition();
	const auto settingsType = _entryData->GetSettingsType();
	_transformString->setVisible(
		condition == MacroConditionSceneTransform::Condition::MATCHES);
	_regex->setVisible(
		condition == MacroConditionSceneTransform::Condition::MATCHES &&
		settingsType ==
			MacroConditionSceneTransform::SettingsType::ALL);
	_getSettings->setVisible(
		condition == MacroConditionSceneTransform::Condition::MATCHES &&
		settingsType ==
			MacroConditionSceneTransform::SettingsType::ALL);
	_getCurrentValue->setVisible(
		condition == MacroConditionSceneTransform::Condition::MATCHES &&
		settingsType ==
			MacroConditionSceneTransform::SettingsType::SINGLE);
	SetLayoutVisible(
		_compareLayout,
		condition == MacroConditionSceneTransform::Condition::MATCHES &&
			settingsType == MacroConditionSceneTransform::
						SettingsType::SINGLE);
	_settingSelection->setVisible(
		settingsType ==
		MacroConditionSceneTransform::SettingsType::SINGLE);
	if (condition == MacroConditionSceneTransform::Condition::MATCHES &&
	    settingsType ==
		    MacroConditionSceneTransform::SettingsType::SINGLE) {
		RemoveStretchIfPresent(_compareLayout);
	} else if (condition ==
			   MacroConditionSceneTransform::Condition::CHANGED &&
		   settingsType ==
			   MacroConditionSceneTransform::SettingsType::SINGLE) {
		AddStretchIfNecessary(_compareLayout);
	}
	_transformString->setVisible(
		condition == MacroConditionSceneTransform::Condition::MATCHES &&
		settingsType ==
			MacroConditionSceneTransform::SettingsType::ALL);

	adjustSize();
	updateGeometry();
}

void MacroConditionSceneTransformEdit::UpdateSettingSelection() const
{
	if (!_entryData) {
		_settingSelection->SetSource(nullptr);
		return;
	}
	auto items = _entryData->_source.GetSceneItems(_entryData->_scene);
	if (items.empty()) {
		_settingSelection->SetSource(nullptr);
		return;
	}
	_settingSelection->SetSource(items.at(0));
}

} // namespace advss
