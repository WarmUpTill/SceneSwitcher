#include "headers/macro-condition-edit.hpp"
#include "headers/macro-condition-hotkey.hpp"
#include "headers/utility.hpp"
#include "headers/advanced-scene-switcher.hpp"

const std::string MacroConditionHotkey::id = "hotkey";

bool MacroConditionHotkey::_registered = MacroConditionFactory::Register(
	MacroConditionHotkey::id,
	{MacroConditionHotkey::Create, MacroConditionHotkeyEdit::Create,
	 "AdvSceneSwitcher.condition.hotkey", false});

static void hotkeyCB(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (pressed) {
		auto c = static_cast<MacroConditionHotkey *>(data);
		c->SetPressed();
	}
}

static uint32_t count = 1;

MacroConditionHotkey::MacroConditionHotkey(Macro *m) : MacroCondition(m)
{
	if (_hotkeyID != OBS_INVALID_HOTKEY_ID) {
		obs_hotkey_unregister(_hotkeyID);
	}

	std::string hotkeyName =
		"macro_condition_hotkey_" + std::to_string(count);

	_name = obs_module_text("AdvSceneSwitcher.condition.hotkey.name") +
		std::string(" ") + std::to_string(count);
	_hotkeyID = obs_hotkey_register_frontend(hotkeyName.c_str(),
						 _name.c_str(), hotkeyCB, this);
	count++;
}

MacroConditionHotkey::~MacroConditionHotkey()
{
	obs_hotkey_unregister(_hotkeyID);
}

bool MacroConditionHotkey::CheckCondition()
{
	if (_pressed) {
		_pressed = false;
		return true;
	}
	return false;
}

bool MacroConditionHotkey::Save(obs_data_t *obj)
{
	MacroCondition::Save(obj);
	obs_data_set_string(obj, "desc", _name.c_str());
	obs_data_array_t *pauseHotkey = obs_hotkey_save(_hotkeyID);
	obs_data_set_array(obj, "keyBind", pauseHotkey);
	obs_data_array_release(pauseHotkey);
	return true;
}

bool MacroConditionHotkey::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_name = obs_data_get_string(obj, "desc");
	obs_data_array_t *pauseHotkey = obs_data_get_array(obj, "keyBind");
	obs_hotkey_load(_hotkeyID, pauseHotkey);
	obs_data_array_release(pauseHotkey);
	obs_hotkey_set_description(_hotkeyID, _name.c_str());
	return true;
}

MacroConditionHotkeyEdit::MacroConditionHotkeyEdit(
	QWidget *parent, std::shared_ptr<MacroConditionHotkey> entryData)
	: QWidget(parent)
{
	_name = new QLineEdit();
	QLabel *hint = new QLabel(
		obs_module_text("AdvSceneSwitcher.condition.hotkey.tip"));
	QWidget::connect(_name, SIGNAL(editingFinished()), this,
			 SLOT(NameChanged()));

	QHBoxLayout *switchLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{name}}", _name},
	};
	placeWidgets(obs_module_text("AdvSceneSwitcher.condition.hotkey.entry"),
		     switchLayout, widgetPlaceholders);

	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(switchLayout);
	mainLayout->addWidget(hint);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionHotkeyEdit::NameChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_name = _name->text().toStdString();
	obs_hotkey_set_description(_entryData->_hotkeyID,
				   _entryData->_name.c_str());
}

void MacroConditionHotkeyEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_name->setText(QString::fromStdString(_entryData->_name));
}
