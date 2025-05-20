#include "macro-action-filter.hpp"
#include "layout-helpers.hpp"
#include "json-helpers.hpp"
#include "source-settings-helpers.hpp"
#include "selection-helpers.hpp"

namespace advss {

const std::string MacroActionFilter::id = "filter";

bool MacroActionFilter::_registered = MacroActionFactory::Register(
	MacroActionFilter::id,
	{MacroActionFilter::Create, MacroActionFilterEdit::Create,
	 "AdvSceneSwitcher.action.filter"});

const static std::map<MacroActionFilter::Action, std::string> actionTypes = {
	{MacroActionFilter::Action::ENABLE,
	 "AdvSceneSwitcher.action.filter.type.enable"},
	{MacroActionFilter::Action::DISABLE,
	 "AdvSceneSwitcher.action.filter.type.disable"},
	{MacroActionFilter::Action::TOGGLE,
	 "AdvSceneSwitcher.action.filter.type.toggle"},
	{MacroActionFilter::Action::SETTINGS,
	 "AdvSceneSwitcher.action.filter.type.settings"},
	{MacroActionFilter::Action::SETTINGS_BUTTON,
	 "AdvSceneSwitcher.action.filter.type.pressSettingsButton"},
};

const static std::map<MacroActionFilter::SettingsInputMethod, std::string>
	inputMethods = {
		{MacroActionFilter::SettingsInputMethod::INDIVIDUAL_MANUAL,
		 "AdvSceneSwitcher.action.filter.inputMethod.individualManual"},
		{MacroActionFilter::SettingsInputMethod::INDIVIDUAL_TEMPVAR,
		 "AdvSceneSwitcher.action.filter.inputMethod.individualTempvar"},
		{MacroActionFilter::SettingsInputMethod::JSON_STRING,
		 "AdvSceneSwitcher.action.filter.inputMethod.json"},
};

static void performActionHelper(
	MacroActionFilter::Action action, const OBSWeakSource &filter,
	MacroActionFilter::SettingsInputMethod settingsInputMethod,
	const SourceSetting &setting, const std::string &manualSettingValue,
	const TempVariableRef &tempVar, Macro *macro,
	const StringVariable &settingsString, const SourceSettingButton &button)
{
	OBSSourceAutoRelease source = obs_weak_source_get_source(filter);
	switch (action) {
	case MacroActionFilter::Action::ENABLE:
		obs_source_set_enabled(source, true);
		break;
	case MacroActionFilter::Action::DISABLE:
		obs_source_set_enabled(source, false);
		break;
	case MacroActionFilter::Action::TOGGLE:
		obs_source_set_enabled(source, !obs_source_enabled(source));
		break;
	case MacroActionFilter::Action::SETTINGS:
		switch (settingsInputMethod) {
		case MacroActionFilter::SettingsInputMethod::INDIVIDUAL_MANUAL:
			SetSourceSetting(source, setting, manualSettingValue);
			break;
		case MacroActionFilter::SettingsInputMethod::INDIVIDUAL_TEMPVAR: {
			auto var = tempVar.GetTempVariable(macro);
			if (!var) {
				break;
			}
			auto value = var->Value();
			if (!value) {
				break;
			}
			SetSourceSetting(source, setting, *value);
			break;
		}
		case MacroActionFilter::SettingsInputMethod::JSON_STRING:
			SetSourceSettings(source, settingsString);
			break;
		}
		break;
	case MacroActionFilter::Action::SETTINGS_BUTTON:
		PressSourceButton(button, source);
		break;
	default:
		break;
	}
}

bool MacroActionFilter::PerformAction()
{
	auto filters = _filter.GetFilters(_source);
	for (const auto &filter : filters) {
		performActionHelper(_action, filter, _settingsInputMethod,
				    _setting, _manualSettingValue, _tempVar,
				    GetMacro(), _settingsString, _button);
	}
	return true;
}

void MacroActionFilter::LogAction() const
{
	auto it = actionTypes.find(_action);
	if (it != actionTypes.end()) {
		ablog(LOG_INFO,
		      "performed action \"%s\" for filter \"%s\" on source \"%s\"",
		      it->second.c_str(), _filter.ToString().c_str(),
		      _source.ToString(true).c_str());
	} else {
		blog(LOG_WARNING, "ignored unknown filter action %d",
		     static_cast<int>(_action));
	}
}

bool MacroActionFilter::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	_source.Save(obj);
	_filter.Save(obj, "filter");
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	obs_data_set_int(obj, "inputMethod",
			 static_cast<int>(_settingsInputMethod));
	_setting.Save(obj);
	_manualSettingValue.Save(obj, "manualSettingValue");
	_tempVar.Save(obj);
	_settingsString.Save(obj, "settings");
	_button.Save(obj);
	obs_data_set_int(obj, "version", 1);
	return true;
}

bool MacroActionFilter::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_source.Load(obj);
	_filter.Load(obj, _source, "filter");
	// TODO: Remove this fallback in future version
	if (!obs_data_has_user_value(obj, "version")) {
		const auto value = obs_data_get_int(obj, "action");
		if (value == 2) {
			_action = Action::SETTINGS;
		} else {
			_action = static_cast<Action>(value);
		}
	} else {
		_action = static_cast<Action>(obs_data_get_int(obj, "action"));
	}
	// TODO: Remove fallback in future version
	if (obs_data_has_user_value(obj, "inputMethod")) {
		_settingsInputMethod = static_cast<SettingsInputMethod>(
			obs_data_get_int(obj, "inputMethod"));
	} else {
		_settingsInputMethod = SettingsInputMethod::JSON_STRING;
	}
	_setting.Load(obj);
	_settingsString.Load(obj, "settings");
	_manualSettingValue.Load(obj, "manualSettingValue");
	_tempVar.Load(obj, GetMacro());
	_button.Load(obj);
	return true;
}

std::string MacroActionFilter::GetShortDesc() const
{
	if (!_filter.ToString().empty() && !_source.ToString().empty()) {
		return _source.ToString() + " - " + _filter.ToString();
	}
	return "";
}

std::shared_ptr<MacroAction> MacroActionFilter::Create(Macro *m)
{
	return std::make_shared<MacroActionFilter>(m);
}

std::shared_ptr<MacroAction> MacroActionFilter::Copy() const
{
	return std::make_shared<MacroActionFilter>(*this);
}

void MacroActionFilter::ResolveVariablesToFixedValues()
{
	_source.ResolveVariables();
	_filter.ResolveVariables();
	_settingsString.ResolveVariables();
	_manualSettingValue.ResolveVariables();
}

static inline void populateActionSelection(QComboBox *list)
{
	for (const auto &[_, name] : actionTypes) {
		list->addItem(obs_module_text(name.c_str()));
	}
}

static inline void populateSettingsInputMethods(QComboBox *list)
{
	list->clear();
	for (const auto &[value, name] : inputMethods) {
		list->addItem(obs_module_text(name.c_str()),
			      static_cast<int>(value));
	}
}

MacroActionFilterEdit::MacroActionFilterEdit(
	QWidget *parent, std::shared_ptr<MacroActionFilter> entryData)
	: QWidget(parent),
	  _sources(new SourceSelectionWidget(this, QStringList(), true)),
	  _filters(new FilterSelectionWidget(this, _sources, true)),
	  _actions(new QComboBox()),
	  _getSettings(new QPushButton(obs_module_text(
		  "AdvSceneSwitcher.action.filter.getSettings"))),
	  _settingsLayout(new QHBoxLayout()),
	  _settingsInputMethods(new QComboBox(this)),
	  _manualSettingValue(new VariableTextEdit(this, 5, 1, 1)),
	  _tempVars(new TempVariableSelection(this)),
	  _filterSettings(new SourceSettingSelection(this)),
	  _settingsString(new VariableTextEdit(this)),
	  _refreshSettingSelection(new QPushButton(
		  obs_module_text("AdvSceneSwitcher.action.filter.refresh"))),
	  _settingsButtons(new SourceSettingsButtonSelection(this))
{
	_filters->setSizeAdjustPolicy(QComboBox::AdjustToContents);

	populateActionSelection(_actions);
	auto sources = GetSourcesWithFilterNames();
	sources.sort();
	_sources->SetSourceNameList(sources);
	_refreshSettingSelection->setToolTip(obs_module_text(
		"AdvSceneSwitcher.action.filter.refresh.tooltip"));

	populateSettingsInputMethods(_settingsInputMethods);

	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_sources,
			 SIGNAL(SourceChanged(const SourceSelection &)), this,
			 SLOT(SourceChanged(const SourceSelection &)));
	QWidget::connect(_filters,
			 SIGNAL(FilterChanged(const FilterSelection &)), this,
			 SLOT(FilterChanged(const FilterSelection &)));
	QWidget::connect(_getSettings, SIGNAL(clicked()), this,
			 SLOT(GetSettingsClicked()));
	QWidget::connect(_settingsString, SIGNAL(textChanged()), this,
			 SLOT(SettingsStringChanged()));
	QWidget::connect(_tempVars,
			 SIGNAL(SelectionChanged(const TempVariableRef &)),
			 this, SLOT(SelectionChanged(const TempVariableRef &)));
	QWidget::connect(_settingsInputMethods,
			 SIGNAL(currentIndexChanged(int)), this,
			 SLOT(SettingsInputMethodChanged(int)));
	QWidget::connect(_manualSettingValue, SIGNAL(textChanged()), this,
			 SLOT(ManualSettingsValueChanged()));
	QWidget::connect(_filterSettings,
			 SIGNAL(SelectionChanged(const SourceSetting &)), this,
			 SLOT(SelectionChanged(const SourceSetting &)));
	QWidget::connect(_refreshSettingSelection, SIGNAL(clicked()), this,
			 SLOT(RefreshVariableSourceSelectionValue()));
	QWidget::connect(_settingsButtons,
			 SIGNAL(SelectionChanged(const SourceSettingButton &)),
			 this,
			 SLOT(ButtonChanged(const SourceSettingButton &)));

	const std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{sources}}", _sources},
		{"{{filters}}", _filters},
		{"{{actions}}", _actions},
		{"{{settings}}", _filterSettings},
		{"{{settingsInputMethod}}", _settingsInputMethods},
		{"{{settingValue}}", _manualSettingValue},
		{"{{tempVar}}", _tempVars},
		{"{{refresh}}", _refreshSettingSelection},
		{"{{settingsButtons}}", _settingsButtons}};

	auto entrylayout = new QHBoxLayout;
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.action.filter.entry"),
		     entrylayout, widgetPlaceholders);
	_settingsLayout->setContentsMargins(0, 0, 0, 0);
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.action.filter.entry.settings"),
		     _settingsLayout, widgetPlaceholders);

	auto buttonLayout = new QHBoxLayout;
	buttonLayout->addWidget(_getSettings);
	buttonLayout->addStretch();
	buttonLayout->setContentsMargins(0, 0, 0, 0);

	auto mainLayout = new QVBoxLayout;
	mainLayout->addLayout(entrylayout);
	mainLayout->addLayout(_settingsLayout);
	mainLayout->addWidget(_settingsString);
	mainLayout->addLayout(buttonLayout);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionFilterEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_actions->setCurrentIndex(static_cast<int>(_entryData->_action));
	_sources->SetSource(_entryData->_source);
	_filters->SetFilter(_entryData->_source, _entryData->_filter);
	_settingsString->setPlainText(_entryData->_settingsString);
	const auto filters =
		_entryData->_filter.GetFilters(_entryData->_source);
	OBSWeakSource firstFilter = filters.empty() ? nullptr : filters.at(0);
	_filterSettings->SetSelection(firstFilter, _entryData->_setting);
	_settingsInputMethods->setCurrentIndex(_settingsInputMethods->findData(
		static_cast<int>(_entryData->_settingsInputMethod)));
	_tempVars->SetVariable(_entryData->_tempVar);
	_manualSettingValue->setPlainText(_entryData->_manualSettingValue);
	_settingsButtons->SetSelection(firstFilter, _entryData->_button);
	SetWidgetVisibility();
}

void MacroActionFilterEdit::SourceChanged(const SourceSelection &source)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_source = source;
}

void MacroActionFilterEdit::FilterChanged(const FilterSelection &filter)
{
	if (_loading || !_entryData) {
		return;
	}

	{
		auto lock = LockContext();
		_entryData->_filter = filter;
	}

	const auto filters =
		_entryData->_filter.GetFilters(_entryData->_source);
	OBSWeakSource firstFilter = filters.empty() ? nullptr : filters.at(0);
	_filterSettings->SetSource(firstFilter);
	_settingsButtons->SetSource(firstFilter);

	SetWidgetVisibility();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionFilterEdit::ActionChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_action = static_cast<MacroActionFilter::Action>(value);
	SetWidgetVisibility();
}

void MacroActionFilterEdit::GetSettingsClicked()
{
	if (_loading || !_entryData ||
	    _entryData->_filter.GetFilters(_entryData->_source).empty()) {
		return;
	}

	switch (_entryData->_settingsInputMethod) {
	case MacroActionFilter::SettingsInputMethod::INDIVIDUAL_MANUAL: {
		const auto filters =
			_entryData->_filter.GetFilters(_entryData->_source);
		_manualSettingValue->setPlainText(
			GetSourceSettingValue(filters.empty() ? nullptr
							      : filters.at(0),
					      _entryData->_setting)
				.value_or(""));
		break;
	}
	case MacroActionFilter::SettingsInputMethod::INDIVIDUAL_TEMPVAR:
		break;
	case MacroActionFilter::SettingsInputMethod::JSON_STRING: {
		const auto filters =
			_entryData->_filter.GetFilters(_entryData->_source);
		_settingsString->setPlainText(
			FormatJsonString(GetSourceSettings(
				filters.empty() ? nullptr : filters.at(0))));
		break;
	}
	}
}

void MacroActionFilterEdit::SettingsStringChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_settingsString =
		_settingsString->toPlainText().toStdString();

	adjustSize();
	updateGeometry();
}

void MacroActionFilterEdit::SettingsInputMethodChanged(int idx)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_settingsInputMethod =
		static_cast<MacroActionFilter::SettingsInputMethod>(
			_settingsInputMethods->itemData(idx).toInt());
	SetWidgetVisibility();
}

void MacroActionFilterEdit::SelectionChanged(const TempVariableRef &var)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_tempVar = var;
}

void MacroActionFilterEdit::SelectionChanged(const SourceSetting &setting)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_setting = setting;
}

void MacroActionFilterEdit::ManualSettingsValueChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_manualSettingValue =
		_manualSettingValue->toPlainText().toStdString();

	adjustSize();
	updateGeometry();
}

void MacroActionFilterEdit::RefreshVariableSourceSelectionValue()
{
	const auto filters =
		_entryData->_filter.GetFilters(_entryData->_source);
	_filterSettings->SetSource(filters.empty() ? nullptr : filters.at(0));
}

void MacroActionFilterEdit::ButtonChanged(const SourceSettingButton &button)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_button = button;
}

void MacroActionFilterEdit::SetWidgetVisibility()
{
	SetLayoutVisible(_settingsLayout,
			 _entryData->_action ==
				 MacroActionFilter::Action::SETTINGS);
	_filterSettings->setVisible(
		_entryData->_action == MacroActionFilter::Action::SETTINGS &&
		_entryData->_settingsInputMethod !=
			MacroActionFilter::SettingsInputMethod::JSON_STRING);
	_settingsString->setVisible(
		_entryData->_action == MacroActionFilter::Action::SETTINGS &&
		_entryData->_settingsInputMethod ==
			MacroActionFilter::SettingsInputMethod::JSON_STRING);
	_getSettings->setVisible(
		_entryData->_action == MacroActionFilter::Action::SETTINGS &&
		_entryData->_settingsInputMethod !=
			MacroActionFilter::SettingsInputMethod::
				INDIVIDUAL_TEMPVAR);
	_tempVars->setVisible(_entryData->_action ==
				      MacroActionFilter::Action::SETTINGS &&
			      _entryData->_settingsInputMethod ==
				      MacroActionFilter::SettingsInputMethod::
					      INDIVIDUAL_TEMPVAR);

	if (_entryData->_action == MacroActionFilter::Action::SETTINGS &&
	    _entryData->_settingsInputMethod ==
		    MacroActionFilter::SettingsInputMethod::INDIVIDUAL_MANUAL) {
		RemoveStretchIfPresent(_settingsLayout);
		_manualSettingValue->show();
	} else {
		AddStretchIfNecessary(_settingsLayout);
		_manualSettingValue->hide();
	}

	_refreshSettingSelection->setVisible(
		_entryData->_settingsInputMethod ==
			MacroActionFilter::SettingsInputMethod::INDIVIDUAL_MANUAL &&
		(_entryData->_source.GetType() ==
			 SourceSelection::Type::VARIABLE ||
		 _entryData->_filter.GetType() ==
			 FilterSelection::Type::VARIABLE));
	_settingsButtons->setVisible(
		_entryData->_action ==
		MacroActionFilter::Action::SETTINGS_BUTTON);

	adjustSize();
	updateGeometry();
}

} // namespace advss
