#include "macro-action-source.hpp"
#include "utility.hpp"

Q_DECLARE_METATYPE(advss::SourceSettingButton);

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
};

const static std::map<obs_deinterlace_mode, std::string> deinterlaceModes = {
	{OBS_DEINTERLACE_MODE_DISABLE,
	 "AdvSceneSwitcher.action.source.deinterlaceMode.disable"},
	{OBS_DEINTERLACE_MODE_DISCARD,
	 "AdvSceneSwitcher.action.source.deinterlaceMode.disable"},
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

static std::vector<SourceSettingButton> getSourceButtons(OBSWeakSource source)
{
	auto s = obs_weak_source_get_source(source);
	std::vector<SourceSettingButton> buttons;
	obs_properties_t *sourceProperties = obs_source_properties(s);
	auto it = obs_properties_first(sourceProperties);
	do {
		if (!it || obs_property_get_type(it) != OBS_PROPERTY_BUTTON) {
			continue;
		}
		SourceSettingButton button = {obs_property_name(it),
					      obs_property_description(it)};
		buttons.emplace_back(button);
	} while (obs_property_next(&it));
	obs_source_release(s);
	return buttons;
}

static void pressSourceButton(const SourceSettingButton &button,
			      obs_source_t *source)
{
	obs_properties_t *sourceProperties = obs_source_properties(source);
	obs_property_t *property =
		obs_properties_get(sourceProperties, button.id.c_str());
	if (!obs_property_button_clicked(property, source)) {
		blog(LOG_WARNING, "Failed to press settings button '%s' for %s",
		     button.id.c_str(), obs_source_get_name(source));
	}
	obs_properties_destroy(sourceProperties);
}

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
		SetSourceSettings(s, _settings);
		break;
	case Action::REFRESH_SETTINGS:
		refreshSourceSettings(s);
		break;
	case Action::SETTINGS_BUTTON:
		pressSourceButton(_button, s);
		break;
	case Action::DEINTERLACE_MODE:
		obs_source_set_deinterlace_mode(s, _deinterlaceMode);
		break;
	case Action::DEINTERLACE_FIELD_ORDER:
		obs_source_set_deinterlace_field_order(s, _deinterlaceOrder);
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
		vblog(LOG_INFO, "performed action \"%s\" for Source \"%s\"",
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
	_settings.Save(obj, "settings");
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
	_settings.Load(obj, "settings");
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

static inline void populateSourceButtonSelection(QComboBox *list,
						 OBSWeakSource source)
{
	list->clear();
	auto buttons = getSourceButtons(source);
	if (buttons.empty()) {
		list->addItem(obs_module_text(
			"AdvSceneSwitcher.action.source.noSettingsButtons"));
	}

	for (const auto &button : buttons) {
		QVariant value;
		value.setValue(button);
		list->addItem(QString::fromStdString(button.ToString()), value);
	}
}

static inline void populateDeinterlaceModeSelection(QComboBox *list)
{
	list->clear();
	for (const auto &[value, name] : deinterlaceModes) {
		list->addItem(obs_module_text(name.c_str()),
			      static_cast<int>(value));
	}
}

static inline void populateDeinterlaceFieldOrderSelection(QComboBox *list)
{
	list->clear();
	for (const auto &[value, name] : deinterlaceFieldOrders) {
		list->addItem(obs_module_text(name.c_str()),
			      static_cast<int>(value));
	}
}

MacroActionSourceEdit::MacroActionSourceEdit(
	QWidget *parent, std::shared_ptr<MacroActionSource> entryData)
	: QWidget(parent),
	  _sources(new SourceSelectionWidget(this, QStringList(), true)),
	  _actions(new QComboBox()),
	  _settingsButtons(new QComboBox()),
	  _getSettings(new QPushButton(obs_module_text(
		  "AdvSceneSwitcher.action.source.getSettings"))),
	  _settings(new VariableTextEdit(this)),
	  _deinterlaceMode(new QComboBox()),
	  _deinterlaceOrder(new QComboBox()),
	  _warning(new QLabel(
		  obs_module_text("AdvSceneSwitcher.action.source.warning")))
{
	populateActionSelection(_actions);
	auto sources = GetSourceNames();
	sources.sort();
	_sources->SetSourceNameList(sources);
	populateDeinterlaceModeSelection(_deinterlaceMode);
	populateDeinterlaceFieldOrderSelection(_deinterlaceOrder);

	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_settingsButtons, SIGNAL(currentIndexChanged(int)),
			 this, SLOT(ButtonChanged(int)));
	QWidget::connect(_sources,
			 SIGNAL(SourceChanged(const SourceSelection &)), this,
			 SLOT(SourceChanged(const SourceSelection &)));
	QWidget::connect(_getSettings, SIGNAL(clicked()), this,
			 SLOT(GetSettingsClicked()));
	QWidget::connect(_settings, SIGNAL(textChanged()), this,
			 SLOT(SettingsChanged()));
	QWidget::connect(_deinterlaceMode, SIGNAL(currentIndexChanged(int)),
			 this, SLOT(DeinterlaceModeChanged(int)));
	QWidget::connect(_deinterlaceOrder, SIGNAL(currentIndexChanged(int)),
			 this, SLOT(DeinterlaceOrderChanged(int)));

	QVBoxLayout *mainLayout = new QVBoxLayout;
	QHBoxLayout *entryLayout = new QHBoxLayout;
	QHBoxLayout *buttonLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{sources}}", _sources},
		{"{{actions}}", _actions},
		{"{{settings}}", _settings},
		{"{{getSettings}}", _getSettings},
		{"{{settingsButtons}}", _settingsButtons},
		{"{{deinterlaceMode}}", _deinterlaceMode},
		{"{{deinterlaceOrder}}", _deinterlaceOrder},
	};
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.action.source.entry"),
		     entryLayout, widgetPlaceholders);
	mainLayout->addLayout(entryLayout);
	mainLayout->addWidget(_warning);
	mainLayout->addWidget(_settings);
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

	populateSourceButtonSelection(_settingsButtons,
				      _entryData->_source.GetSource());
	_actions->setCurrentIndex(static_cast<int>(_entryData->_action));
	_sources->SetSource(_entryData->_source);
	_settingsButtons->setCurrentText(
		QString::fromStdString(_entryData->_button.ToString()));
	_settings->setPlainText(_entryData->_settings);
	_deinterlaceMode->setCurrentIndex(_deinterlaceMode->findData(
		static_cast<int>(_entryData->_deinterlaceMode)));
	_deinterlaceOrder->setCurrentIndex(_deinterlaceOrder->findData(
		static_cast<int>(_entryData->_deinterlaceOrder)));
	SetWidgetVisibility();
}

void MacroActionSourceEdit::SourceChanged(const SourceSelection &source)
{
	if (_loading || !_entryData) {
		return;
	}

	{
		auto lock = LockContext();
		_entryData->_source = source;
	}
	populateSourceButtonSelection(_settingsButtons,
				      _entryData->_source.GetSource());
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionSourceEdit::ActionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_action = static_cast<MacroActionSource::Action>(value);
	SetWidgetVisibility();
}

void MacroActionSourceEdit::ButtonChanged(int idx)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_button = qvariant_cast<SourceSettingButton>(
		_settingsButtons->itemData(idx));
}

void MacroActionSourceEdit::GetSettingsClicked()
{
	if (_loading || !_entryData || !_entryData->_source.GetSource()) {
		return;
	}

	_settings->setPlainText(FormatJsonString(
		GetSourceSettings(_entryData->_source.GetSource())));
}

void MacroActionSourceEdit::SettingsChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_settings = _settings->toPlainText().toStdString();

	adjustSize();
	updateGeometry();
}

void MacroActionSourceEdit::DeinterlaceModeChanged(int idx)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_deinterlaceMode = static_cast<obs_deinterlace_mode>(
		_deinterlaceMode->itemData(idx).toInt());
}

void MacroActionSourceEdit::DeinterlaceOrderChanged(int idx)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_deinterlaceOrder =
		static_cast<obs_deinterlace_field_order>(
			_deinterlaceOrder->itemData(idx).toInt());
}

void MacroActionSourceEdit::SetWidgetVisibility()
{
	const bool showSettings = _entryData->_action ==
				  MacroActionSource::Action::SETTINGS;
	const bool showWarning =
		_entryData->_action == MacroActionSource::Action::ENABLE ||
		_entryData->_action == MacroActionSource::Action::DISABLE;
	_settings->setVisible(showSettings);
	_getSettings->setVisible(showSettings);
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
	adjustSize();
	updateGeometry();
}

bool SourceSettingButton::Save(obs_data_t *obj) const
{
	auto data = obs_data_create();
	obs_data_set_string(data, "id", id.c_str());
	obs_data_set_string(data, "description", description.c_str());
	obs_data_set_obj(obj, "sourceSettingButton", data);
	obs_data_release(data);
	return true;
}

bool SourceSettingButton::Load(obs_data_t *obj)
{
	auto data = obs_data_get_obj(obj, "sourceSettingButton");
	id = obs_data_get_string(data, "id");
	description = obs_data_get_string(data, "description");
	obs_data_release(data);
	return true;
}

std::string SourceSettingButton::ToString() const
{
	if (id.empty()) {
		return "";
	}
	return "[" + id + "] " + description;
}

} // namespace advss
