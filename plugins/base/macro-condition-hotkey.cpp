#include "macro-condition-hotkey.hpp"
#include "layout-helpers.hpp"
#include "macro-helpers.hpp"

namespace advss {

const std::string MacroConditionHotkey::id = "hotkey";

bool MacroConditionHotkey::_registered = MacroConditionFactory::Register(
	MacroConditionHotkey::id,
	{MacroConditionHotkey::Create, MacroConditionHotkeyEdit::Create,
	 "AdvSceneSwitcher.condition.hotkey"});

static uint32_t count = 1;

MacroConditionHotkey::MacroConditionHotkey(Macro *m) : MacroCondition(m)
{
	auto name = obs_module_text("AdvSceneSwitcher.condition.hotkey.name") +
		    std::string(" ") + std::to_string(count);
	_hotkey = Hotkey::GetHotkey(name, true);
	count++;
}

bool MacroConditionHotkey::CheckCondition()
{
	const bool keyStateCurrentlyMatches =
		_checkPressed ? _hotkey->GetPressed() : !_hotkey->GetPressed();
	const auto lastKeyStateMatch = _checkPressed
					       ? _hotkey->GetLastPressed()
					       : _hotkey->GetLastReleased();
	const bool hotkeySateChangedSinceLastCheck = lastKeyStateMatch >
						     _lastCheck;
	const bool macroWasPausedSinceLastCheck =
		MacroWasPausedSince(GetMacro(), _lastCheck);
	bool ret = keyStateCurrentlyMatches ||
		   (hotkeySateChangedSinceLastCheck &&
		    !macroWasPausedSinceLastCheck);
	_lastCheck = std::chrono::high_resolution_clock::now();
	return ret;
}

bool MacroConditionHotkey::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	_hotkey->Save(obj);
	obs_data_set_bool(obj, "checkPressed", _checkPressed);
	return true;
}

bool MacroConditionHotkey::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	if (!_hotkey->Load(obj)) {
		auto description = obs_data_get_string(obj, "desc");
		_hotkey = Hotkey::GetHotkey(description);
		vblog(LOG_WARNING,
		      "hotkey name conflict for \"%s\" - using previous key bind",
		      description);
	}
	if (obs_data_has_user_value(obj, "checkPressed")) {
		_checkPressed = obs_data_get_bool(obj, "checkPressed");
	} else { // TODO: Remove fallback at some point in the future
		_checkPressed = true;
	}
	return true;
}

MacroConditionHotkeyEdit::MacroConditionHotkeyEdit(
	QWidget *parent, std::shared_ptr<MacroConditionHotkey> entryData)
	: QWidget(parent),
	  _name(new QLineEdit()),
	  _keyState(new QComboBox())
{
	_keyState->addItems(
		QStringList()
		<< obs_module_text("AdvSceneSwitcher.condition.hotkey.pressed")
		<< obs_module_text(
			   "AdvSceneSwitcher.condition.hotkey.released"));

	QWidget::connect(_name, SIGNAL(editingFinished()), this,
			 SLOT(NameChanged()));
	QWidget::connect(_keyState, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(KeyStateChanged(int)));

	auto keyStateLayout = new QHBoxLayout;
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.hotkey.entry.keyState"),
		keyStateLayout, {{"{{keyState}}", _keyState}});

	auto nameLayout = new QHBoxLayout;
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.condition.hotkey.entry.name"),
		nameLayout, {{"{{name}}", _name}});

	auto mainLayout = new QVBoxLayout;
	mainLayout->addLayout(keyStateLayout);
	mainLayout->addLayout(nameLayout);
	mainLayout->addWidget(new QLabel(
		obs_module_text("AdvSceneSwitcher.condition.hotkey.tip")));
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionHotkeyEdit::NameChanged()
{
	GUARD_LOADING_AND_LOCK();
	const auto name = _name->text().toStdString();
	// In case a hotkey is used by multiple conditions create a new hotkey
	// with the new description or get an existing hotkey matching this
	// description.
	// If the hotkey is only used by this single condition instance try to
	// update the description.
	// If updating the description runs into a conflict with an existing
	// hotkey use its settings instead.
	if (_entryData->_hotkey.use_count() > 1 ||
	    !_entryData->_hotkey->UpdateDescription(name)) {
		_entryData->_hotkey = Hotkey::GetHotkey(name);
	}
}

void MacroConditionHotkeyEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_name->setText(
		QString::fromStdString(_entryData->_hotkey->GetDescription()));
	_keyState->setCurrentIndex(_entryData->_checkPressed ? 0 : 1);
}

void MacroConditionHotkeyEdit::KeyStateChanged(int index)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_checkPressed = index == 0;
}

} // namespace advss
