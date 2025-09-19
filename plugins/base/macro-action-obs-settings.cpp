#include "macro-action-obs-settings.hpp"
#include "layout-helpers.hpp"
#include "profile-helpers.hpp"
#include "ui-helpers.hpp"

#include <obs-frontend-api.h>
#include <util/config-file.h>

namespace advss {

const std::string MacroActionOBSSettings::id = "obs_settings";

bool MacroActionOBSSettings::_registered = MacroActionFactory::Register(
	MacroActionOBSSettings::id,
	{MacroActionOBSSettings::Create, MacroActionOBSSettingsEdit::Create,
	 "AdvSceneSwitcher.action.obsSetting"});

const static std::map<MacroActionOBSSettings::Action, std::string> actionTypes = {
	{MacroActionOBSSettings::Action::FPS_TYPE,
	 "AdvSceneSwitcher.action.obsSetting.action.setFPSType"},
	{MacroActionOBSSettings::Action::FPS_COMMON_VALUE,
	 "AdvSceneSwitcher.action.obsSetting.action.setFPSCommonValue"},
	{MacroActionOBSSettings::Action::FPS_INT_VALUE,
	 "AdvSceneSwitcher.action.obsSetting.action.setFPSIntegerValue"},
	{MacroActionOBSSettings::Action::FPS_NUM_VALUE,
	 "AdvSceneSwitcher.action.obsSetting.action.setFPSFractionNumeratorValue"},
	{MacroActionOBSSettings::Action::FPS_DEN_VALUE,
	 "AdvSceneSwitcher.action.obsSetting.action.setFPSFractionDenominatorValue"},
	{MacroActionOBSSettings::Action::BASE_CANVAS_X_VALUE,
	 "AdvSceneSwitcher.action.obsSetting.action.setBaseCanvasX"},
	{MacroActionOBSSettings::Action::BASE_CANVAS_Y_VALUE,
	 "AdvSceneSwitcher.action.obsSetting.action.setBaseCanvasY"},
	{MacroActionOBSSettings::Action::OUTPUT_X_VALUE,
	 "AdvSceneSwitcher.action.obsSetting.action.setOutputCanvasX"},
	{MacroActionOBSSettings::Action::OUTPUT_Y_VALUE,
	 "AdvSceneSwitcher.action.obsSetting.action.setOutputCanvasY"},
	{MacroActionOBSSettings::Action::ENABLE_PREVIEW,
	 "AdvSceneSwitcher.action.obsSetting.action.enablePreview"},
	{MacroActionOBSSettings::Action::DISABLE_PREVIEW,
	 "AdvSceneSwitcher.action.obsSetting.action.disablePreview"},
	{MacroActionOBSSettings::Action::TOGGLE_PREVIEW,
	 "AdvSceneSwitcher.action.obsSetting.action.togglePreview"},
};

template<typename Func, typename... Args>
static void setConfigValueHelper(Func func, Args... args)
{
	auto config = obs_frontend_get_profile_config();
	func(config, args...);
	if (config_save(config) != CONFIG_SUCCESS) {
		blog(LOG_WARNING, "failed to save config!");
	}
}

template<typename Func, typename... Args>
static auto getConfigValueHelper(Func func, Func defaultFunc, Args... args)
	-> std::optional<std::invoke_result_t<Func, config_t *, Args...>>
{
	using Ret = std::invoke_result_t<Func, config_t *, Args...>;

	auto config = obs_frontend_get_profile_config();
	if (config_has_user_value(config, args...)) {
		return func(config, args...);
	}
	if (config_has_default_value(config, args...)) {
		return defaultFunc(config, args...);
	}
	return std::optional<Ret>{};
}

static void setPreviewEnabled(bool enabled)
{
	static const auto enable = [](void *) {
		obs_frontend_set_preview_enabled(true);
	};
	static const auto disable = [](void *) {
		obs_frontend_set_preview_enabled(false);
	};
	obs_queue_task(OBS_TASK_UI, enabled ? enable : disable, nullptr, false);
}

bool MacroActionOBSSettings::PerformAction()
{
	switch (_action) {
	case Action::FPS_TYPE:
		setConfigValueHelper(config_set_uint, "Video", "FPSType",
				     static_cast<int>(_fpsType));
		obs_frontend_reset_video();
		break;
	case Action::FPS_COMMON_VALUE:
		setConfigValueHelper(config_set_string, "Video", "FPSCommon",
				     _fpsStringValue.c_str());
		obs_frontend_reset_video();
		break;
	case Action::FPS_INT_VALUE:
		setConfigValueHelper(config_set_uint, "Video", "FPSInt",
				     static_cast<int>(_fpsIntValue));
		obs_frontend_reset_video();
		break;
	case Action::FPS_NUM_VALUE:
		setConfigValueHelper(config_set_uint, "Video", "FPSNum",
				     static_cast<int>(_fpsIntValue));
		obs_frontend_reset_video();
		break;
	case Action::FPS_DEN_VALUE:
		setConfigValueHelper(config_set_uint, "Video", "FPSDen",
				     static_cast<int>(_fpsIntValue));
		obs_frontend_reset_video();
		break;
	case Action::BASE_CANVAS_X_VALUE:
		setConfigValueHelper(config_set_uint, "Video", "BaseCX",
				     static_cast<int>(_canvasSizeValue));
		obs_frontend_reset_video();
		break;
	case Action::BASE_CANVAS_Y_VALUE:
		setConfigValueHelper(config_set_uint, "Video", "BaseCY",
				     static_cast<int>(_canvasSizeValue));
		obs_frontend_reset_video();
		break;
	case Action::OUTPUT_X_VALUE:
		setConfigValueHelper(config_set_uint, "Video", "OutputCX",
				     static_cast<int>(_canvasSizeValue));
		obs_frontend_reset_video();
		break;
	case Action::OUTPUT_Y_VALUE:
		setConfigValueHelper(config_set_uint, "Video", "OutputCY",
				     static_cast<int>(_canvasSizeValue));
		obs_frontend_reset_video();
		break;
	case Action::ENABLE_PREVIEW:
		setPreviewEnabled(true);
		break;
	case Action::DISABLE_PREVIEW:
		setPreviewEnabled(false);
		break;
	case Action::TOGGLE_PREVIEW:
		setPreviewEnabled(!obs_frontend_preview_enabled());
		break;
	default:
		break;
	}
	return true;
}

void MacroActionOBSSettings::LogAction() const
{
	auto it = actionTypes.find(_action);
	if (it != actionTypes.end()) {
		ablog(LOG_INFO, "performed action \"%s\"", it->second.c_str());
	} else {
		blog(LOG_WARNING, "ignored unknown obs setting action %d",
		     static_cast<int>(_action));
	}
}

bool MacroActionOBSSettings::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	_fpsIntValue.Save(obj, "fpsIntValue");
	_fpsStringValue.Save(obj, "fpsStringValue");
	_canvasSizeValue.Save(obj, "canvasSizeValue");
	return true;
}

bool MacroActionOBSSettings::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_action = static_cast<Action>(obs_data_get_int(obj, "action"));
	_fpsIntValue.Load(obj, "fpsIntValue");
	_fpsStringValue.Load(obj, "fpsStringValue");
	_canvasSizeValue.Load(obj, "canvasSizeValue");
	return true;
}

std::shared_ptr<MacroAction> MacroActionOBSSettings::Create(Macro *m)
{
	return std::make_shared<MacroActionOBSSettings>(m);
}

std::shared_ptr<MacroAction> MacroActionOBSSettings::Copy() const
{
	return std::make_shared<MacroActionOBSSettings>(*this);
}

void MacroActionOBSSettings::ResolveVariablesToFixedValues()
{
	_fpsIntValue.ResolveVariables();
	_canvasSizeValue.ResolveVariables();
}

static void populateActionSelection(QComboBox *list)
{
	for (const auto &[value, name] : actionTypes) {
		list->addItem(obs_module_text(name.c_str()),
			      static_cast<int>(value));
	}
}

static void populateFPSTypeSelection(QComboBox *list)
{
	static const std::map<MacroActionOBSSettings::FPSType, std::string>
		types = {
			{MacroActionOBSSettings::FPSType::COMMON,
			 "Basic.Settings.Video.FPSCommon"},
			{MacroActionOBSSettings::FPSType::INTEGER,
			 "Basic.Settings.Video.FPSInteger"},
			{MacroActionOBSSettings::FPSType::FRACTION,
			 "Basic.Settings.Video.FPSFraction"},
		};

	for (const auto &[value, name] : types) {
		list->addItem(obs_module_text(name.c_str()),
			      static_cast<int>(value));
	}
}

MacroActionOBSSettingsEdit::MacroActionOBSSettingsEdit(
	QWidget *parent, std::shared_ptr<MacroActionOBSSettings> entryData)
	: QWidget(parent),
	  _actions(new QComboBox(this)),
	  _fpsType(new QComboBox(this)),
	  _fpsIntValue(new VariableSpinBox(this)),
	  _fpsStringValue(new VariableLineEdit(this)),
	  _canvasSizeValue(new VariableSpinBox(this)),
	  _getCurrentValue(new QPushButton(obs_module_text(
		  "AdvSceneSwitcher.action.obsSetting.getCurrentValue")))
{
	populateActionSelection(_actions);
	populateFPSTypeSelection(_fpsType);

	_fpsIntValue->setMaximum(1000);
	_canvasSizeValue->setMaximum(100000);

	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_fpsType, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(FPSTypeChanged(int)));
	QWidget::connect(
		_fpsIntValue,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(FPSIntValueChanged(const NumberVariable<int> &)));
	QWidget::connect(_fpsStringValue, SIGNAL(editingFinished()), this,
			 SLOT(FPSStringValueChanged()));
	QWidget::connect(
		_canvasSizeValue,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this,
		SLOT(CanvasSizeValueChanged(const NumberVariable<int> &)));
	QWidget::connect(_getCurrentValue, SIGNAL(clicked()), this,
			 SLOT(GetCurrentValueClicked()));

	auto layout = new QHBoxLayout();
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.action.obsSettings.layout"),
		layout,
		{{"{{actions}}", _actions},
		 {"{{fpsType}}", _fpsType},
		 {"{{fpsIntValue}}", _fpsIntValue},
		 {"{{fpsStringValue}}", _fpsStringValue},
		 {"{{canvasSizeValue}}", _canvasSizeValue},
		 {"{{getCurrentValue}}", _getCurrentValue}});
	setLayout(layout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionOBSSettingsEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_actions->setCurrentIndex(
		_actions->findData(static_cast<int>(_entryData->_action)));
	_fpsType->setCurrentIndex(
		_actions->findData(static_cast<int>(_entryData->_fpsType)));
	_fpsIntValue->SetValue(_entryData->_fpsIntValue);
	_fpsStringValue->setText(_entryData->_fpsStringValue);
	_canvasSizeValue->SetValue(_entryData->_canvasSizeValue);
	SetWidgetVisibility();
}

void MacroActionOBSSettingsEdit::FPSTypeChanged(int idx)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_fpsType = static_cast<MacroActionOBSSettings::FPSType>(
		_fpsType->itemData(idx).toInt());
	SetWidgetVisibility();
}

void MacroActionOBSSettingsEdit::FPSIntValueChanged(
	const NumberVariable<int> &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_fpsIntValue = value;
}

void MacroActionOBSSettingsEdit::FPSStringValueChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_fpsStringValue = _fpsStringValue->text().toStdString();
}

void MacroActionOBSSettingsEdit::CanvasSizeValueChanged(
	const NumberVariable<int> &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_canvasSizeValue = value;
}

void MacroActionOBSSettingsEdit::GetCurrentValueClicked()
{
	const auto setSpinVideoValue = [this](VariableSpinBox *spin,
					      const char *name) {
		auto value = getConfigValueHelper(config_get_uint,
						  config_get_default_uint,
						  "Video", name);
		if (!value) {
			return;
		}
		spin->SetFixedValue(static_cast<int>(*value));
	};
	const auto setFpsValue = [&](const char *name) {
		setSpinVideoValue(_fpsIntValue, name);
	};
	const auto setCanvasValue = [&](const char *name) {
		setSpinVideoValue(_canvasSizeValue, name);
	};

	switch (_entryData->_action) {
	case MacroActionOBSSettings::Action::FPS_COMMON_VALUE: {
		auto value = getConfigValueHelper(config_get_string,
						  config_get_default_string,
						  "Video", "FPSCommon");
		if (!value) {
			break;
		}
		_fpsStringValue->setText(QString(*value));
		break;
	}
	case MacroActionOBSSettings::Action::FPS_INT_VALUE:
		setFpsValue("FPSInt");
		break;
	case MacroActionOBSSettings::Action::FPS_NUM_VALUE:
		setFpsValue("FPSNum");
		break;
	case MacroActionOBSSettings::Action::FPS_DEN_VALUE:
		setFpsValue("FPSDen");
		break;
	case MacroActionOBSSettings::Action::BASE_CANVAS_X_VALUE:
		setCanvasValue("BaseCX");
		break;
	case MacroActionOBSSettings::Action::BASE_CANVAS_Y_VALUE:
		setCanvasValue("BaseCY");
		break;
	case MacroActionOBSSettings::Action::OUTPUT_X_VALUE:
		setCanvasValue("OutputCX");
		break;
	case MacroActionOBSSettings::Action::OUTPUT_Y_VALUE:
		setCanvasValue("OutputCY");
		break;
	default:
		break;
	}
}

void MacroActionOBSSettingsEdit::SetWidgetVisibility()
{
	if (!_entryData) {
		return;
	}

	_fpsType->setVisible(_entryData->_action ==
			     MacroActionOBSSettings::Action::FPS_TYPE);
	_fpsIntValue->setVisible(
		_entryData->_action ==
			MacroActionOBSSettings::Action::FPS_INT_VALUE ||
		_entryData->_action ==
			MacroActionOBSSettings::Action::FPS_NUM_VALUE ||
		_entryData->_action ==
			MacroActionOBSSettings::Action::FPS_DEN_VALUE);
	_fpsStringValue->setVisible(
		_entryData->_action ==
		MacroActionOBSSettings::Action::FPS_COMMON_VALUE);

	const bool needsSizeSelection =
		_entryData->_action ==
			MacroActionOBSSettings::Action::BASE_CANVAS_X_VALUE ||
		_entryData->_action ==
			MacroActionOBSSettings::Action::BASE_CANVAS_Y_VALUE ||
		_entryData->_action ==
			MacroActionOBSSettings::Action::OUTPUT_X_VALUE ||
		_entryData->_action ==
			MacroActionOBSSettings::Action::OUTPUT_Y_VALUE;
	_canvasSizeValue->setVisible(needsSizeSelection);

	const bool canGetSettingsValue =
		_entryData->_action ==
			MacroActionOBSSettings::Action::FPS_COMMON_VALUE ||
		_entryData->_action ==
			MacroActionOBSSettings::Action::FPS_INT_VALUE ||
		_entryData->_action ==
			MacroActionOBSSettings::Action::FPS_NUM_VALUE ||
		_entryData->_action ==
			MacroActionOBSSettings::Action::FPS_DEN_VALUE ||
		_entryData->_action ==
			MacroActionOBSSettings::Action::BASE_CANVAS_X_VALUE ||
		_entryData->_action ==
			MacroActionOBSSettings::Action::BASE_CANVAS_Y_VALUE ||
		_entryData->_action ==
			MacroActionOBSSettings::Action::OUTPUT_X_VALUE ||
		_entryData->_action ==
			MacroActionOBSSettings::Action::OUTPUT_Y_VALUE;
	_getCurrentValue->setVisible(canGetSettingsValue);
}

void MacroActionOBSSettingsEdit::ActionChanged(int idx)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_action = static_cast<MacroActionOBSSettings::Action>(
		_actions->itemData(idx).toInt());
	SetWidgetVisibility();
}

} // namespace advss
