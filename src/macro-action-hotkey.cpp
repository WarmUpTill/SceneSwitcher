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
	list->addItem("No key");
	list->addItem("A");
	list->addItem("B");
	list->addItem("C");
	list->addItem("D");
	list->addItem("E");
	list->addItem("F");
	list->addItem("G");
	list->addItem("H");
	list->addItem("I");
	list->addItem("J");
	list->addItem("K");
	list->addItem("L");
	list->addItem("M");
	list->addItem("N");
	list->addItem("O");
	list->addItem("P");
	list->addItem("Q");
	list->addItem("R");
	list->addItem("S");
	list->addItem("T");
	list->addItem("U");
	list->addItem("V");
	list->addItem("W");
	list->addItem("X");
	list->addItem("Y");
	list->addItem("Z");
	list->addItem("0");
	list->addItem("1");
	list->addItem("2");
	list->addItem("3");
	list->addItem("4");
	list->addItem("5");
	list->addItem("6");
	list->addItem("7");
	list->addItem("8");
	list->addItem("9");
	list->addItem("F1");
	list->addItem("F2");
	list->addItem("F3");
	list->addItem("F4");
	list->addItem("F5");
	list->addItem("F6");
	list->addItem("F7");
	list->addItem("F8");
	list->addItem("F9");
	list->addItem("F10");
	list->addItem("F11");
	list->addItem("F12");
	list->addItem("F13");
	list->addItem("F14");
	list->addItem("F15");
	list->addItem("F16");
	list->addItem("F17");
	list->addItem("F18");
	list->addItem("F19");
	list->addItem("F20");
	list->addItem("F21");
	list->addItem("F22");
	list->addItem("F23");
	list->addItem("F24");
	list->addItem("Escape");
	list->addItem("Space");
	list->addItem("Return");
	list->addItem("Backspace");
	list->addItem("Tab");
	list->addItem("Shift_L");
	list->addItem("Shift_R");
	list->addItem("Control_L");
	list->addItem("Control_R");
	list->addItem("Alt_L");
	list->addItem("Alt_R");
	list->addItem("Win_L");
	list->addItem("Win_R");
	list->addItem("Apps");
	list->addItem("CapsLock");
	list->addItem("NumLock");
	list->addItem("ScrollLock");
	list->addItem("PrintScreen");
	list->addItem("Pause");
	list->addItem("Insert");
	list->addItem("Delete");
	list->addItem("PageUP");
	list->addItem("PageDown");
	list->addItem("Home");
	list->addItem("End");
	list->addItem("Left");
	list->addItem("Right");
	list->addItem("Up");
	list->addItem("Down");
	list->addItem("Numpad0");
	list->addItem("Numpad1");
	list->addItem("Numpad2");
	list->addItem("Numpad3");
	list->addItem("Numpad4");
	list->addItem("Numpad5");
	list->addItem("Numpad6");
	list->addItem("Numpad7");
	list->addItem("Numpad8");
	list->addItem("Numpad9");
	list->addItem("NumpadAdd");
	list->addItem("NumpadSubtract");
	list->addItem("NumpadMultiply");
	list->addItem("NumpadDivide");
	list->addItem("NumpadDecimal");
	list->addItem("NumpadEnter");
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

	if (!canSimulateKeyPresses) {
		mainLayout->addWidget(new QLabel(obs_module_text(
			"AdvSceneSwitcher.action.hotkey.disabled")));
	}

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
