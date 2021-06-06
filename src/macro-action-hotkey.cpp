#include "headers/macro-action-hotkey.hpp"
#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

const std::string MacroActionHotkey::id = "hotkey";

bool MacroActionHotkey::_registered = MacroActionFactory::Register(
	MacroActionHotkey::id,
	{MacroActionHotkey::Create, MacroActionHotkeyEdit::Create,
	 "AdvSceneSwitcher.action.hotkey"});

bool MacroActionHotkey::PerformAction()
{
	std::vector<HotkeyType> keys;
	if (_leftShift) {
		keys.push_back(HotkeyType::Key_Shift_L);
	}
	if (_rightShift) {
		keys.push_back(HotkeyType::Key_Shift_R);
	}
	if (_leftCtrl) {
		keys.push_back(HotkeyType::Key_Control_L);
	}
	if (_rightCtrl) {
		keys.push_back(HotkeyType::Key_Control_R);
	}
	if (_leftAlt) {
		keys.push_back(HotkeyType::Key_Alt_L);
	}
	if (_rightAlt) {
		keys.push_back(HotkeyType::Key_Alt_R);
	}
	if (_key != HotkeyType::Key_NoKey) {
		keys.push_back(_key);
	}

	if (!keys.empty()) {
		std::thread t([keys]() { PressKeys(keys); });
		t.detach();
	}

	return true;
}

void MacroActionHotkey::LogAction()
{
	vblog(LOG_INFO, "sent hotkey");
}

bool MacroActionHotkey::Save(obs_data_t *obj)
{
	MacroAction::Save(obj);
	obs_data_set_int(obj, "key", static_cast<int>(_key));
	obs_data_set_bool(obj, "left_shift", _leftShift);
	obs_data_set_bool(obj, "right_shift", _rightShift);
	obs_data_set_bool(obj, "left_ctrl", _leftCtrl);
	obs_data_set_bool(obj, "right_ctrl", _rightCtrl);
	obs_data_set_bool(obj, "left_alt", _leftAlt);
	obs_data_set_bool(obj, "right_alt", _rightAlt);
	obs_data_set_bool(obj, "left_meta", _leftMeta);
	obs_data_set_bool(obj, "right_meta", _rightMeta);
	return true;
}

bool MacroActionHotkey::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_key = static_cast<HotkeyType>(obs_data_get_int(obj, "key"));
	_leftShift = obs_data_get_bool(obj, "left_shift");
	_rightShift = obs_data_get_bool(obj, "right_shift");
	_leftCtrl = obs_data_get_bool(obj, "left_ctrl");
	_rightCtrl = obs_data_get_bool(obj, "right_ctrl");
	_leftAlt = obs_data_get_bool(obj, "left_alt");
	_rightAlt = obs_data_get_bool(obj, "right_alt");
	_leftMeta = obs_data_get_bool(obj, "left_meta");
	_rightMeta = obs_data_get_bool(obj, "right_meta");
	return true;
}

static inline void populateKeySelection(QComboBox *list)
{
	list->addItem("Key_NoKey");
	list->addItem("Key_A");
	list->addItem("Key_B");
	list->addItem("Key_C");
	list->addItem("Key_D");
	list->addItem("Key_E");
	list->addItem("Key_F");
	list->addItem("Key_G");
	list->addItem("Key_H");
	list->addItem("Key_I");
	list->addItem("Key_J");
	list->addItem("Key_K");
	list->addItem("Key_L");
	list->addItem("Key_M");
	list->addItem("Key_N");
	list->addItem("Key_O");
	list->addItem("Key_P");
	list->addItem("Key_Q");
	list->addItem("Key_R");
	list->addItem("Key_S");
	list->addItem("Key_T");
	list->addItem("Key_U");
	list->addItem("Key_V");
	list->addItem("Key_W");
	list->addItem("Key_X");
	list->addItem("Key_Y");
	list->addItem("Key_Z");
	list->addItem("Key_0");
	list->addItem("Key_1");
	list->addItem("Key_2");
	list->addItem("Key_3");
	list->addItem("Key_4");
	list->addItem("Key_5");
	list->addItem("Key_6");
	list->addItem("Key_7");
	list->addItem("Key_8");
	list->addItem("Key_9");
	list->addItem("Key_F1");
	list->addItem("Key_F2");
	list->addItem("Key_F3");
	list->addItem("Key_F4");
	list->addItem("Key_F5");
	list->addItem("Key_F6");
	list->addItem("Key_F7");
	list->addItem("Key_F8");
	list->addItem("Key_F9");
	list->addItem("Key_F10");
	list->addItem("Key_F11");
	list->addItem("Key_F12");
	list->addItem("Key_F13");
	list->addItem("Key_F14");
	list->addItem("Key_F15");
	list->addItem("Key_F16");
	list->addItem("Key_F17");
	list->addItem("Key_F18");
	list->addItem("Key_F19");
	list->addItem("Key_F20");
	list->addItem("Key_F21");
	list->addItem("Key_F22");
	list->addItem("Key_F23");
	list->addItem("Key_F24");
	list->addItem("Key_Escape");
	list->addItem("Key_Space");
	list->addItem("Key_Return");
	list->addItem("Key_Backspace");
	list->addItem("Key_Tab");
	list->addItem("Key_Shift_L");
	list->addItem("Key_Shift_R");
	list->addItem("Key_Control_L");
	list->addItem("Key_Control_R");
	list->addItem("Key_Alt_L");
	list->addItem("Key_Alt_R");
	list->addItem("Key_Win_L");
	list->addItem("Key_Win_R");
	list->addItem("Key_Apps");
	list->addItem("Key_CapsLock");
	list->addItem("Key_NumLock");
	list->addItem("Key_ScrollLock");
	list->addItem("Key_PrintScreen");
	list->addItem("Key_Pause");
	list->addItem("Key_Insert");
	list->addItem("Key_Delete");
	list->addItem("Key_PageUP");
	list->addItem("Key_PageDown");
	list->addItem("Key_Home");
	list->addItem("Key_End");
	list->addItem("Key_Left");
	list->addItem("Key_Right");
	list->addItem("Key_Up");
	list->addItem("Key_Down");
	list->addItem("Key_Numpad0");
	list->addItem("Key_Numpad1");
	list->addItem("Key_Numpad2");
	list->addItem("Key_Numpad3");
	list->addItem("Key_Numpad4");
	list->addItem("Key_Numpad5");
	list->addItem("Key_Numpad6");
	list->addItem("Key_Numpad7");
	list->addItem("Key_Numpad8");
	list->addItem("Key_Numpad9");
	list->addItem("Key_NumpadAdd");
	list->addItem("Key_NumpadSubtract");
	list->addItem("Key_NumpadMultiply");
	list->addItem("Key_NumpadDivide");
	list->addItem("Key_NumpadDecimal");
	list->addItem("Key_NumpadEnter");
}

MacroActionHotkeyEdit::MacroActionHotkeyEdit(
	QWidget *parent, std::shared_ptr<MacroActionHotkey> entryData)
	: QWidget(parent)
{
	_keys = new QComboBox();
	_leftShift = new QCheckBox(
		obs_module_text("AdvSceneSwitcher.action.hotkey.leftShift"));
	_rightShift = new QCheckBox(
		obs_module_text("AdvSceneSwitcher.action.hotkey.rightShift"));
	_leftCtrl = new QCheckBox(
		obs_module_text("AdvSceneSwitcher.action.hotkey.leftCtrl"));
	_rightCtrl = new QCheckBox(
		obs_module_text("AdvSceneSwitcher.action.hotkey.rightCtrl"));
	_leftAlt = new QCheckBox(
		obs_module_text("AdvSceneSwitcher.action.hotkey.leftAlt"));
	_rightAlt = new QCheckBox(
		obs_module_text("AdvSceneSwitcher.action.hotkey.rightAlt"));
	_leftMeta = new QCheckBox(
		obs_module_text("AdvSceneSwitcher.action.hotkey.leftMeta"));
	_rightMeta = new QCheckBox(
		obs_module_text("AdvSceneSwitcher.action.hotkey.rightMeta"));

	populateKeySelection(_keys);

	QWidget::connect(_keys, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(KeyChanged(int)));
	QWidget::connect(_leftShift, SIGNAL(stateChanged(int)), this,
			 SLOT(LShiftChanged(int)));
	QWidget::connect(_rightShift, SIGNAL(stateChanged(int)), this,
			 SLOT(RShiftChanged(int)));
	QWidget::connect(_leftCtrl, SIGNAL(stateChanged(int)), this,
			 SLOT(LCtrlChanged(int)));
	QWidget::connect(_rightCtrl, SIGNAL(stateChanged(int)), this,
			 SLOT(RCtrlChanged(int)));
	QWidget::connect(_leftAlt, SIGNAL(stateChanged(int)), this,
			 SLOT(LAltChanged(int)));
	QWidget::connect(_rightAlt, SIGNAL(stateChanged(int)), this,
			 SLOT(RAltChanged(int)));
	QWidget::connect(_leftMeta, SIGNAL(stateChanged(int)), this,
			 SLOT(LMetaChanged(int)));
	QWidget::connect(_rightMeta, SIGNAL(stateChanged(int)), this,
			 SLOT(RMetaChanged(int)));

	QHBoxLayout *line1Layout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{keys}}", _keys},
	};
	placeWidgets(obs_module_text("AdvSceneSwitcher.action.hotkey.entry"),
		     line1Layout, widgetPlaceholders);

	QHBoxLayout *line2Layout = new QHBoxLayout;
	line2Layout->addWidget(_leftShift);
	line2Layout->addWidget(_rightShift);
	line2Layout->addWidget(_leftCtrl);
	line2Layout->addWidget(_rightCtrl);
	line2Layout->addWidget(_leftAlt);
	line2Layout->addWidget(_rightAlt);
	line2Layout->addWidget(_leftMeta);
	line2Layout->addWidget(_rightMeta);
	line2Layout->addStretch();

	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(line1Layout);
	mainLayout->addLayout(line2Layout);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionHotkeyEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_keys->setCurrentIndex(static_cast<int>(_entryData->_key));
	_leftShift->setChecked(_entryData->_leftShift);
	_rightShift->setChecked(_entryData->_rightShift);
	_leftCtrl->setChecked(_entryData->_leftCtrl);
	_rightCtrl->setChecked(_entryData->_rightCtrl);
	_leftAlt->setChecked(_entryData->_leftAlt);
	_rightAlt->setChecked(_entryData->_rightAlt);
	_leftMeta->setChecked(_entryData->_leftMeta);
	_rightMeta->setChecked(_entryData->_rightMeta);
}

void MacroActionHotkeyEdit::LShiftChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_leftShift = state;
}

void MacroActionHotkeyEdit::RShiftChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_rightShift = state;
}

void MacroActionHotkeyEdit::LCtrlChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_leftCtrl = state;
}

void MacroActionHotkeyEdit::RCtrlChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_rightCtrl = state;
}

void MacroActionHotkeyEdit::LAltChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_leftAlt = state;
}

void MacroActionHotkeyEdit::RAltChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_rightAlt = state;
}

void MacroActionHotkeyEdit::LMetaChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_leftMeta = state;
}

void MacroActionHotkeyEdit::RMetaChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_rightMeta = state;
}

void MacroActionHotkeyEdit::KeyChanged(int key)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_key = static_cast<HotkeyType>(key);
}
