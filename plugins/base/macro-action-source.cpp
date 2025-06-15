#include "macro-action-source.hpp"
#include "layout-helpers.hpp"
#include "json-helpers.hpp"
#include "selection-helpers.hpp"
#include "source-settings-helpers.hpp"

#include <obs-frontend-api.h>

namespace advss {

const std::string MacroActionSource::id = "source";

bool MacroActionSource::_registered = MacroActionFactory::Register(
	MacroActionSource::id,
	{MacroActionSource::Create, MacroActionSourceEdit::Create,
	 "AdvSceneSwitcher.action.source"});

const static std::map<MacroActionSource::Action, std::string> actionTypes = {
	{MacroActionSource::Action::ENABLE,
	 "AdvSceneSwitcher.action.source.type.enable"},
	{MacroActionSource::Action::DISABLE,
	 "AdvSceneSwitcher.action.source.type.disable"},
	{MacroActionSource::Action::SETTINGS,
	 "AdvSceneSwitcher.action.source.type.settings"},
	{MacroActionSource::Action::REFRESH_SETTINGS,
	 "AdvSceneSwitcher.action.source.type.refreshSettings"},
	{MacroActionSource::Action::SETTINGS_BUTTON,
	 "AdvSceneSwitcher.action.source.type.pressSettingsButton"},
	{MacroActionSource::Action::DEINTERLACE_MODE,
	 "AdvSceneSwitcher.action.source.type.deinterlaceMode"},
	{MacroActionSource::Action::DEINTERLACE_FIELD_ORDER,
	 "AdvSceneSwitcher.action.source.type.deinterlaceOrder"},
	{MacroActionSource::Action::OPEN_INTERACTION_DIALOG,
	 "AdvSceneSwitcher.action.source.type.openInteractionDialog"},
	{MacroActionSource::Action::OPEN_FILTER_DIALOG,
	 "AdvSceneSwitcher.action.source.type.openFilterDialog"},
	{MacroActionSource::Action::OPEN_PROPERTIES_DIALOG,
	 "AdvSceneSwitcher.action.source.type.openPropertiesDialog"},
};

const static std::map<obs_deinterlace_mode, std::string> deinterlaceModes = {
	{OBS_DEINTERLACE_MODE_DISABLE,
	 "AdvSceneSwitcher.action.source.deinterlaceMode.disable"},
	{OBS_DEINTERLACE_MODE_DISCARD,
	 "AdvSceneSwitcher.action.source.deinterlaceMode.discard"},
	{OBS_DEINTERLACE_MODE_RETRO,
	 "AdvSceneSwitcher.action.source.deinterlaceMode.retro"},
	{OBS_DEINTERLACE_MODE_BLEND,
	 "AdvSceneSwitcher.action.source.deinterlaceMode.blend"},
	{OBS_DEINTERLACE_MODE_BLEND_2X,
	 "AdvSceneSwitcher.action.source.deinterlaceMode.blend2x"},
	{OBS_DEINTERLACE_MODE_LINEAR,
	 "AdvSceneSwitcher.action.source.deinterlaceMode.linear"},
	{OBS_DEINTERLACE_MODE_LINEAR_2X,
	 "AdvSceneSwitcher.action.source.deinterlaceMode.linear2x"},
	{OBS_DEINTERLACE_MODE_YADIF,
	 "AdvSceneSwitcher.action.source.deinterlaceMode.yadif"},
	{OBS_DEINTERLACE_MODE_YADIF_2X,
	 "AdvSceneSwitcher.action.source.deinterlaceMode.yadif2x"},
};

const static std::map<obs_deinterlace_field_order, std::string>
	deinterlaceFieldOrders = {
		{OBS_DEINTERLACE_FIELD_ORDER_TOP,
		 "AdvSceneSwitcher.action.source.deinterlaceOrder.topFieldFirst"},
		{OBS_DEINTERLACE_FIELD_ORDER_BOTTOM,
		 "AdvSceneSwitcher.action.source.deinterlaceOrder.bottomFieldFirst"},
};

const static std::map<MacroActionSource::SettingsInputMethod, std::string>
	inputMethods = {
		{MacroActionSource::SettingsInputMethod::INDIVIDUAL_MANUAL,
		 "AdvSceneSwitcher.action.source.inputMethod.individualManual"},
		{MacroActionSource::SettingsInputMethod::INDIVIDUAL_TEMPVAR,
		 "AdvSceneSwitcher.action.source.inputMethod.individualTempvar"},
		{MacroActionSource::SettingsInputMethod::JSON_STRING,
		 "AdvSceneSwitcher.action.source.inputMethod.json"},
};

static void refreshSourceSettings(obs_source_t *s)
{
	if (!s) {
		return;
	}

	obs_data_t *data = obs_source_get_settings(s);
	obs_source_update(s, data);
	obs_data_release(data);

	// Refresh of browser sources based on:
	// https://github.com/obsproject/obs-websocket/pull/666/files
	if (strcmp(obs_source_get_id(s), "browser_source") == 0) {
		obs_properties_t *sourceProperties = obs_source_properties(s);
		obs_property_t *property =
			obs_properties_get(sourceProperties, "refreshnocache");
		obs_property_button_clicked(property, s);
		obs_properties_destroy(sourceProperties);
	}
}

static bool isInteractable(obs_source_t *source)
{
	uint32_t flags = obs_source_get_output_flags(source);
	return (flags & OBS_SOURCE_INTERACTION) != 0;
}

bool MacroActionSource::PerformAction()
{
	auto s = obs_weak_source_get_source(_source.GetSource());
	switch (_action) {
	case Action::ENABLE:
		obs_source_set_enabled(s, true);
		break;
	case Action::DISABLE:
		obs_source_set_enabled(s, false);
		break;
	case Action::SETTINGS:
		switch (_settingsInputMethod) {
		case SettingsInputMethod::INDIVIDUAL_MANUAL:
			SetSourceSetting(s, _setting, _manualSettingValue);
			break;
		case SettingsInputMethod::INDIVIDUAL_TEMPVAR: {
			auto var = _tempVar.GetTempVariable(GetMacro());
			if (!var) {
				break;
			}
			auto value = var->Value();
			if (!value) {
				break;
			}
			SetSourceSetting(s, _setting, *value);
			break;
		}
		case SettingsInputMethod::JSON_STRING:
			SetSourceSettings(s, _settingsString);
			break;
		}
		break;
	case Action::REFRESH_SETTINGS:
		refreshSourceSettings(s);
		break;
	case Action::SETTINGS_BUTTON:
		PressSourceButton(_button, s);
		break;
	case Action::DEINTERLACE_MODE:
		obs_source_set_deinterlace_mode(s, _deinterlaceMode);
		break;
	case Action::DEINTERLACE_FIELD_ORDER:
		obs_source_set_deinterlace_field_order(s, _deinterlaceOrder);
		break;
	case Action::OPEN_INTERACTION_DIALOG:
		if (isInteractable(s)) {
			obs_frontend_open_source_interaction(s);
		} else {
			blog(LOG_INFO,
			     "refusing to open interaction dialog "
			     "for non intractable source \"%s\"",
			     _source.ToString().c_str());
		}
		break;
	case Action::OPEN_FILTER_DIALOG:
		obs_frontend_open_source_filters(s);
		break;
	case Action::OPEN_PROPERTIES_DIALOG:
		obs_frontend_open_source_properties(s);
		break;
	default:
		break;
	}
	obs_source_release(s);
	return true;
}

void MacroActionSource::LogAction() const
{
	auto it = actionTypes.find(_action);
	if (it != actionTypes.end()) {
		ablog(LOG_INFO, "performed action \"%s\" for Source \"%s\"",
		      it->second.c_str(), _source.ToString(true).c_str());
	} else {
		blog(LOG_WARNING, "ignored unknown source action %d",
		     static_cast<int>(_action));
	}
}

bool MacroActionSource::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	_source.Save(obj);
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	_button.Save(obj);
	obs_data_set_int(obj, "inputMethod",
			 static_cast<int>(_settingsInputMethod));
	_setting.Save(obj);
	_manualSettingValue.Save(obj, "manualSettingValue");
	_tempVar.Save(obj);
	_settingsString.Save(obj, "settings");
	obs_data_set_int(obj, "deinterlaceMode",
			 static_cast<int>(_deinterlaceMode));
	obs_data_set_int(obj, "deinterlaceOrder",
			 static_cast<int>(_deinterlaceOrder));
	return true;
}

bool MacroActionSource::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_source.Load(obj);
	_action = static_cast<Action>(obs_data_get_int(obj, "action"));
	_button.Load(obj);
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
	_deinterlaceMode = static_cast<obs_deinterlace_mode>(
		obs_data_get_int(obj, "deinterlaceMode"));
	_deinterlaceOrder = static_cast<obs_deinterlace_field_order>(
		obs_data_get_int(obj, "deinterlaceOrder"));
	return true;
}

std::string MacroActionSource::GetShortDesc() const
{
	_source.ToString();
	return "";
}

std::shared_ptr<MacroAction> MacroActionSource::Create(Macro *m)
{
	return std::make_shared<MacroActionSource>(m);
}

std::shared_ptr<MacroAction> MacroActionSource::Copy() const
{
	return std::make_shared<MacroActionSource>(*this);
}

void MacroActionSource::ResolveVariablesToFixedValues()
{
	_source.ResolveVariables();
	_settingsString.ResolveVariables();
	_manualSettingValue.ResolveVariables();
}

static inline void populateActionSelection(QComboBox *list)
{
	for (auto &[actionType, name] : actionTypes) {
		list->addItem(obs_module_text(name.c_str()));
		if (actionType == MacroActionSource::Action::REFRESH_SETTINGS) {
			list->setItemData(
				list->count() - 1,
				obs_module_text(
					"AdvSceneSwitcher.action.source.type.refreshSettings.tooltip"),
				Qt::ToolTipRole);
		}
	}
}

template<class T>
static inline void populateModeSelection(QComboBox *list,
					 const std::map<T, std::string> &modes)
{
	list->clear();
	for (const auto &[value, name] : modes) {
		list->addItem(obs_module_text(name.c_str()),
			      static_cast<int>(value));
	}
}

static QStringList getSourcesList()
{
	auto sources = GetSourceNames();
	sources.sort();
	return sources;
}

MacroActionSourceEdit::MacroActionSourceEdit(
	QWidget *parent, std::shared_ptr<MacroActionSource> entryData)
	: QWidget(parent),
	  _sources(new SourceSelectionWidget(this, getSourcesList, true)),
	  _actions(new QComboBox()),
	  _settingsButtons(new SourceSettingsButtonSelection(this)),
	  _settingsLayout(new QHBoxLayout()),
	  _settingsInputMethods(new QComboBox(this)),
	  _manualSettingValue(new VariableTextEdit(this, 5, 1, 1)),
	  _tempVars(new TempVariableSelection(this)),
	  _sourceSettings(new SourceSettingSelection(this)),
	  _settingsString(new VariableTextEdit(this)),
	  _getSettings(new QPushButton(obs_module_text(
		  "AdvSceneSwitcher.action.source.getSettings"))),
	  _deinterlaceMode(new QComboBox()),
	  _deinterlaceOrder(new QComboBox()),
	  _warning(new QLabel(
		  obs_module_text("AdvSceneSwitcher.action.source.warning"))),
	  _refreshSettingSelection(new QPushButton(
		  obs_module_text("AdvSceneSwitcher.action.source.refresh")))
{
	populateActionSelection(_actions);

	populateModeSelection(_deinterlaceMode, deinterlaceModes);
	populateModeSelection(_deinterlaceOrder, deinterlaceFieldOrders);
	populateModeSelection(_settingsInputMethods, inputMethods);
	_refreshSettingSelection->setToolTip(obs_module_text(
		"AdvSceneSwitcher.action.source.refresh.tooltip"));

	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_settingsButtons,
			 SIGNAL(SelectionChanged(const SourceSettingButton &)),
			 this,
			 SLOT(ButtonChanged(const SourceSettingButton &)));
	QWidget::connect(_sources,
			 SIGNAL(SourceChanged(const SourceSelection &)), this,
			 SLOT(SourceChanged(const SourceSelection &)));
	QWidget::connect(_getSettings, SIGNAL(clicked()), this,
			 SLOT(GetSettingsClicked()));
	QWidget::connect(_settingsString, SIGNAL(textChanged()), this,
			 SLOT(SettingsStringChanged()));
	QWidget::connect(_deinterlaceMode, SIGNAL(currentIndexChanged(int)),
			 this, SLOT(DeinterlaceModeChanged(int)));
	QWidget::connect(_deinterlaceOrder, SIGNAL(currentIndexChanged(int)),
			 this, SLOT(DeinterlaceOrderChanged(int)));
	QWidget::connect(_tempVars,
			 SIGNAL(SelectionChanged(const TempVariableRef &)),
			 this, SLOT(SelectionChanged(const TempVariableRef &)));
	QWidget::connect(_settingsInputMethods,
			 SIGNAL(currentIndexChanged(int)), this,
			 SLOT(SettingsInputMethodChanged(int)));
	QWidget::connect(_manualSettingValue, SIGNAL(textChanged()), this,
			 SLOT(ManualSettingsValueChanged()));
	QWidget::connect(_sourceSettings,
			 SIGNAL(SelectionChanged(const SourceSetting &)), this,
			 SLOT(SelectionChanged(const SourceSetting &)));
	QWidget::connect(_refreshSettingSelection, SIGNAL(clicked()), this,
			 SLOT(RefreshVariableSourceSelectionValue()));

	auto entryLayout = new QHBoxLayout;
	entryLayout->setContentsMargins(0, 0, 0, 0);
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{sources}}", _sources},
		{"{{actions}}", _actions},
		{"{{settings}}", _sourceSettings},
		{"{{settingsInputMethod}}", _settingsInputMethods},
		{"{{settingValue}}", _manualSettingValue},
		{"{{tempVar}}", _tempVars},
		{"{{getSettings}}", _getSettings},
		{"{{settingsButtons}}", _settingsButtons},
		{"{{deinterlaceMode}}", _deinterlaceMode},
		{"{{deinterlaceOrder}}", _deinterlaceOrder},
		{"{{refresh}}", _refreshSettingSelection}};

	PlaceWidgets(obs_module_text("AdvSceneSwitcher.action.source.entry"),
		     entryLayout, widgetPlaceholders);
	_settingsLayout->setContentsMargins(0, 0, 0, 0);
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.action.source.entry.settings"),
		     _settingsLayout, widgetPlaceholders);
	auto mainLayout = new QVBoxLayout;
	mainLayout->addLayout(entryLayout);
	mainLayout->addLayout(_settingsLayout);
	mainLayout->addWidget(_warning);
	mainLayout->addWidget(_settingsString);
	auto buttonLayout = new QHBoxLayout;
	buttonLayout->setContentsMargins(0, 0, 0, 0);
	buttonLayout->addWidget(_getSettings);
	buttonLayout->addStretch();
	mainLayout->addLayout(buttonLayout);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionSourceEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	const auto weakSource = _entryData->_source.GetSource();
	_settingsButtons->SetSelection(weakSource, _entryData->_button);
	_actions->setCurrentIndex(static_cast<int>(_entryData->_action));
	_sources->SetSource(_entryData->_source);
	_sourceSettings->SetSelection(weakSource, _entryData->_setting);
	_settingsString->setPlainText(_entryData->_settingsString);
	_deinterlaceMode->setCurrentIndex(_deinterlaceMode->findData(
		static_cast<int>(_entryData->_deinterlaceMode)));
	_deinterlaceOrder->setCurrentIndex(_deinterlaceOrder->findData(
		static_cast<int>(_entryData->_deinterlaceOrder)));
	_settingsInputMethods->setCurrentIndex(_settingsInputMethods->findData(
		static_cast<int>(_entryData->_settingsInputMethod)));
	_tempVars->SetVariable(_entryData->_tempVar);
	_manualSettingValue->setPlainText(_entryData->_manualSettingValue);

	SetWidgetVisibility();
}

void MacroActionSourceEdit::SourceChanged(const SourceSelection &source)
{
	{
		GUARD_LOADING_AND_LOCK();
		_entryData->_source = source;
	}

	const auto weakSource = _entryData->_source.GetSource();
	_sourceSettings->SetSource(weakSource);
	_settingsButtons->SetSource(weakSource);
	SetWidgetVisibility();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionSourceEdit::ActionChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_action = static_cast<MacroActionSource::Action>(value);
	SetWidgetVisibility();
}

void MacroActionSourceEdit::ButtonChanged(const SourceSettingButton &button)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_button = button;
}

void MacroActionSourceEdit::GetSettingsClicked()
{
	if (_loading || !_entryData || !_entryData->_source.GetSource()) {
		return;
	}

	switch (_entryData->_settingsInputMethod) {
	case MacroActionSource::SettingsInputMethod::INDIVIDUAL_MANUAL:
		_manualSettingValue->setPlainText(
			GetSourceSettingValue(_entryData->_source.GetSource(),
					      _entryData->_setting)
				.value_or(""));
		break;
	case MacroActionSource::SettingsInputMethod::INDIVIDUAL_TEMPVAR:
		break;
	case MacroActionSource::SettingsInputMethod::JSON_STRING:
		_settingsString->setPlainText(FormatJsonString(
			GetSourceSettings(_entryData->_source.GetSource())));
		break;
	}
}

void MacroActionSourceEdit::SettingsStringChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_settingsString =
		_settingsString->toPlainText().toStdString();

	adjustSize();
	updateGeometry();
}

void MacroActionSourceEdit::DeinterlaceModeChanged(int idx)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_deinterlaceMode = static_cast<obs_deinterlace_mode>(
		_deinterlaceMode->itemData(idx).toInt());
}

void MacroActionSourceEdit::DeinterlaceOrderChanged(int idx)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_deinterlaceOrder =
		static_cast<obs_deinterlace_field_order>(
			_deinterlaceOrder->itemData(idx).toInt());
}

void MacroActionSourceEdit::SelectionChanged(const TempVariableRef &var)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_tempVar = var;
}

void MacroActionSourceEdit::SettingsInputMethodChanged(int idx)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_settingsInputMethod =
		static_cast<MacroActionSource::SettingsInputMethod>(
			_settingsInputMethods->itemData(idx).toInt());
	SetWidgetVisibility();
}

void MacroActionSourceEdit::SelectionChanged(const SourceSetting &setting)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_setting = setting;
}

void MacroActionSourceEdit::ManualSettingsValueChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_manualSettingValue =
		_manualSettingValue->toPlainText().toStdString();

	adjustSize();
	updateGeometry();
}

void MacroActionSourceEdit::RefreshVariableSourceSelectionValue()
{
	_sourceSettings->SetSource(_entryData->_source.GetSource());
}

void MacroActionSourceEdit::SetWidgetVisibility()
{
	SetLayoutVisible(_settingsLayout,
			 _entryData->_action ==
				 MacroActionSource::Action::SETTINGS);
	_sourceSettings->setVisible(
		_entryData->_action == MacroActionSource::Action::SETTINGS &&
		_entryData->_settingsInputMethod !=
			MacroActionSource::SettingsInputMethod::JSON_STRING);
	_settingsString->setVisible(
		_entryData->_action == MacroActionSource::Action::SETTINGS &&
		_entryData->_settingsInputMethod ==
			MacroActionSource::SettingsInputMethod::JSON_STRING);
	_getSettings->setVisible(
		_entryData->_action == MacroActionSource::Action::SETTINGS &&
		_entryData->_settingsInputMethod !=
			MacroActionSource::SettingsInputMethod::
				INDIVIDUAL_TEMPVAR);
	_tempVars->setVisible(_entryData->_action ==
				      MacroActionSource::Action::SETTINGS &&
			      _entryData->_settingsInputMethod ==
				      MacroActionSource::SettingsInputMethod::
					      INDIVIDUAL_TEMPVAR);

	if (_entryData->_action == MacroActionSource::Action::SETTINGS &&
	    _entryData->_settingsInputMethod ==
		    MacroActionSource::SettingsInputMethod::INDIVIDUAL_MANUAL) {
		RemoveStretchIfPresent(_settingsLayout);
		_manualSettingValue->show();
	} else {
		AddStretchIfNecessary(_settingsLayout);
		_manualSettingValue->hide();
	}

	const bool showWarning =
		_entryData->_action == MacroActionSource::Action::ENABLE ||
		_entryData->_action == MacroActionSource::Action::DISABLE;
	_warning->setVisible(showWarning);
	_settingsButtons->setVisible(
		_entryData->_action ==
		MacroActionSource::Action::SETTINGS_BUTTON);
	_deinterlaceMode->setVisible(
		_entryData->_action ==
		MacroActionSource::Action::DEINTERLACE_MODE);
	_deinterlaceOrder->setVisible(
		_entryData->_action ==
		MacroActionSource::Action::DEINTERLACE_FIELD_ORDER);

	_refreshSettingSelection->setVisible(
		_entryData->_settingsInputMethod ==
			MacroActionSource::SettingsInputMethod::INDIVIDUAL_MANUAL &&
		_entryData->_source.GetType() ==
			SourceSelection::Type::VARIABLE);

	adjustSize();
	updateGeometry();
}

} // namespace advss
