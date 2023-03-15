#include "macro-action-hotkey.hpp"
#include "advanced-scene-switcher.hpp"
#include "utility.hpp"
#include <obs-interaction.h>

const std::string MacroActionHotkey::id = "hotkey";

bool MacroActionHotkey::_registered = MacroActionFactory::Register(
	MacroActionHotkey::id,
	{MacroActionHotkey::Create, MacroActionHotkeyEdit::Create,
	 "AdvSceneSwitcher.action.hotkey"});

static std::unordered_map<HotkeyType, long> keyTable = {
	// Chars
	{HotkeyType::Key_A, OBS_KEY_A},
	{HotkeyType::Key_B, OBS_KEY_B},
	{HotkeyType::Key_C, OBS_KEY_C},
	{HotkeyType::Key_D, OBS_KEY_D},
	{HotkeyType::Key_E, OBS_KEY_E},
	{HotkeyType::Key_F, OBS_KEY_F},
	{HotkeyType::Key_G, OBS_KEY_G},
	{HotkeyType::Key_H, OBS_KEY_H},
	{HotkeyType::Key_I, OBS_KEY_I},
	{HotkeyType::Key_J, OBS_KEY_J},
	{HotkeyType::Key_K, OBS_KEY_K},
	{HotkeyType::Key_L, OBS_KEY_L},
	{HotkeyType::Key_M, OBS_KEY_M},
	{HotkeyType::Key_N, OBS_KEY_N},
	{HotkeyType::Key_O, OBS_KEY_O},
	{HotkeyType::Key_P, OBS_KEY_P},
	{HotkeyType::Key_Q, OBS_KEY_Q},
	{HotkeyType::Key_R, OBS_KEY_R},
	{HotkeyType::Key_S, OBS_KEY_S},
	{HotkeyType::Key_T, OBS_KEY_T},
	{HotkeyType::Key_U, OBS_KEY_U},
	{HotkeyType::Key_V, OBS_KEY_V},
	{HotkeyType::Key_W, OBS_KEY_W},
	{HotkeyType::Key_X, OBS_KEY_X},
	{HotkeyType::Key_Y, OBS_KEY_Y},
	{HotkeyType::Key_Z, OBS_KEY_Z},

	// Numbers
	{HotkeyType::Key_0, OBS_KEY_0},
	{HotkeyType::Key_1, OBS_KEY_1},
	{HotkeyType::Key_2, OBS_KEY_2},
	{HotkeyType::Key_3, OBS_KEY_3},
	{HotkeyType::Key_4, OBS_KEY_4},
	{HotkeyType::Key_5, OBS_KEY_5},
	{HotkeyType::Key_6, OBS_KEY_6},
	{HotkeyType::Key_7, OBS_KEY_7},
	{HotkeyType::Key_8, OBS_KEY_8},
	{HotkeyType::Key_9, OBS_KEY_9},

	{HotkeyType::Key_F1, OBS_KEY_F1},
	{HotkeyType::Key_F2, OBS_KEY_F2},
	{HotkeyType::Key_F3, OBS_KEY_F3},
	{HotkeyType::Key_F4, OBS_KEY_F4},
	{HotkeyType::Key_F5, OBS_KEY_F5},
	{HotkeyType::Key_F6, OBS_KEY_F6},
	{HotkeyType::Key_F7, OBS_KEY_F7},
	{HotkeyType::Key_F8, OBS_KEY_F8},
	{HotkeyType::Key_F9, OBS_KEY_F9},
	{HotkeyType::Key_F10, OBS_KEY_F10},
	{HotkeyType::Key_F11, OBS_KEY_F11},
	{HotkeyType::Key_F12, OBS_KEY_F12},
	{HotkeyType::Key_F13, OBS_KEY_F13},
	{HotkeyType::Key_F14, OBS_KEY_F14},
	{HotkeyType::Key_F15, OBS_KEY_F15},
	{HotkeyType::Key_F16, OBS_KEY_F16},
	{HotkeyType::Key_F17, OBS_KEY_F17},
	{HotkeyType::Key_F18, OBS_KEY_F18},
	{HotkeyType::Key_F19, OBS_KEY_F19},
	{HotkeyType::Key_F20, OBS_KEY_F20},
	{HotkeyType::Key_F21, OBS_KEY_F21},
	{HotkeyType::Key_F22, OBS_KEY_F22},
	{HotkeyType::Key_F23, OBS_KEY_F23},
	{HotkeyType::Key_F24, OBS_KEY_F24},

	{HotkeyType::Key_Escape, OBS_KEY_ESCAPE},
	{HotkeyType::Key_Space, OBS_KEY_SPACE},
	{HotkeyType::Key_Return, OBS_KEY_RETURN},
	{HotkeyType::Key_Backspace, OBS_KEY_BACKSPACE},
	{HotkeyType::Key_Tab, OBS_KEY_TAB},

	{HotkeyType::Key_Shift_L, OBS_KEY_SHIFT},
	{HotkeyType::Key_Shift_R, OBS_KEY_SHIFT},
	{HotkeyType::Key_Control_L, OBS_KEY_CONTROL},
	{HotkeyType::Key_Control_R, OBS_KEY_CONTROL},
	{HotkeyType::Key_Alt_L, OBS_KEY_ALT},
	{HotkeyType::Key_Alt_R, OBS_KEY_ALT},
	{HotkeyType::Key_Win_L, OBS_KEY_META},
	{HotkeyType::Key_Win_R, OBS_KEY_META},
	{HotkeyType::Key_Apps, OBS_KEY_APPLICATIONLEFT},

	{HotkeyType::Key_CapsLock, OBS_KEY_CAPSLOCK},
	{HotkeyType::Key_NumLock, OBS_KEY_NUMLOCK},
	{HotkeyType::Key_ScrollLock, OBS_KEY_SCROLLLOCK},

	{HotkeyType::Key_PrintScreen, OBS_KEY_PRINT},
	{HotkeyType::Key_Pause, OBS_KEY_PAUSE},

	{HotkeyType::Key_Insert, OBS_KEY_INSERT},
	{HotkeyType::Key_Delete, OBS_KEY_DELETE},
	{HotkeyType::Key_PageUP, OBS_KEY_PAGEUP},
	{HotkeyType::Key_PageDown, OBS_KEY_PAGEDOWN},
	{HotkeyType::Key_Home, OBS_KEY_HOME},
	{HotkeyType::Key_End, OBS_KEY_END},

	{HotkeyType::Key_Left, OBS_KEY_LEFT},
	{HotkeyType::Key_Up, OBS_KEY_UP},
	{HotkeyType::Key_Right, OBS_KEY_RIGHT},
	{HotkeyType::Key_Down, OBS_KEY_DOWN},

	{HotkeyType::Key_Numpad0, OBS_KEY_NUM0},
	{HotkeyType::Key_Numpad1, OBS_KEY_NUM1},
	{HotkeyType::Key_Numpad2, OBS_KEY_NUM2},
	{HotkeyType::Key_Numpad3, OBS_KEY_NUM3},
	{HotkeyType::Key_Numpad4, OBS_KEY_NUM4},
	{HotkeyType::Key_Numpad5, OBS_KEY_NUM5},
	{HotkeyType::Key_Numpad6, OBS_KEY_NUM6},
	{HotkeyType::Key_Numpad7, OBS_KEY_NUM7},
	{HotkeyType::Key_Numpad8, OBS_KEY_NUM8},
	{HotkeyType::Key_Numpad9, OBS_KEY_NUM9},

	{HotkeyType::Key_NumpadAdd, OBS_KEY_NUMPLUS},
	{HotkeyType::Key_NumpadSubtract, OBS_KEY_NUMMINUS},
	{HotkeyType::Key_NumpadMultiply, OBS_KEY_NUMASTERISK},
	{HotkeyType::Key_NumpadDivide, OBS_KEY_NUMSLASH},
	{HotkeyType::Key_NumpadDecimal, OBS_KEY_NUMPERIOD},
	//{HotkeyType::Key_NumpadEnter, ???},
};

obs_key_combination keysToOBSKeycombo(const std::vector<HotkeyType> &keys)
{
	obs_key_combination combo{};
	auto it = keyTable.find(keys.back());
	if (it != keyTable.end()) {
		combo.key = (obs_key_t)it->second;
	}
	if (keys.size() == 1) {
		return combo;
	}

	for (uint32_t i = 0; i < keys.size() - 1; i++) {
		switch (keys[i]) {
		case HotkeyType::Key_Shift_L:
			combo.modifiers |= INTERACT_SHIFT_KEY;
			break;
		case HotkeyType::Key_Shift_R:
			combo.modifiers |= INTERACT_SHIFT_KEY;
			break;
		case HotkeyType::Key_Control_L:
			combo.modifiers |= INTERACT_CONTROL_KEY;
			break;
		case HotkeyType::Key_Control_R:
			combo.modifiers |= INTERACT_CONTROL_KEY;
			break;
		case HotkeyType::Key_Alt_L:
			combo.modifiers |= INTERACT_ALT_KEY;
			break;
		case HotkeyType::Key_Alt_R:
			combo.modifiers |= INTERACT_ALT_KEY;
			break;
		case HotkeyType::Key_Win_L:
			combo.modifiers |= INTERACT_COMMAND_KEY;
			break;
		case HotkeyType::Key_Win_R:
			combo.modifiers |= INTERACT_COMMAND_KEY;
			break;
		case HotkeyType::Key_CapsLock:
			combo.modifiers |= INTERACT_CAPS_KEY;
			break;
		default:
			break;
		}
	}

	return combo;
}

void InjectKeys(const std::vector<HotkeyType> &keys, int duration)
{
	auto combo = keysToOBSKeycombo(keys);
	if (obs_key_combination_is_empty(combo)) {
		return;
	}
	// I am not sure why this is necessary
	obs_hotkey_inject_event(combo, false);

	obs_hotkey_inject_event(combo, true);
	std::this_thread::sleep_for(std::chrono::milliseconds(duration));
	obs_hotkey_inject_event(combo, false);
}

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
	if (_leftMeta) {
		keys.push_back(HotkeyType::Key_Win_L);
	}
	if (_rightMeta) {
		keys.push_back(HotkeyType::Key_Win_R);
	}
	if (_key != HotkeyType::Key_NoKey) {
		keys.push_back(_key);
	}

	if (!keys.empty()) {
		int dur = _duration.Milliseconds();
		if (_onlySendToObs || !canSimulateKeyPresses) {
			std::thread t([keys, dur]() { InjectKeys(keys, dur); });
			t.detach();
		} else {
			std::thread t([keys, dur]() { PressKeys(keys, dur); });
			t.detach();
		}
	}

	return true;
}

void MacroActionHotkey::LogAction() const
{
	vblog(LOG_INFO, "sent hotkey");
}

bool MacroActionHotkey::Save(obs_data_t *obj) const
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
	_duration.Save(obj);
	obs_data_set_bool(obj, "onlyOBS", _onlySendToObs);
	obs_data_set_int(obj, "version", 1);
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
	if (!obs_data_has_user_value(obj, "version")) {
		_duration = obs_data_get_int(obj, "duration") / 1000.0;
	} else {
		_duration.Load(obj);
	}
	_onlySendToObs = obs_data_get_bool(obj, "onlyOBS");
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
	: QWidget(parent),
	  _keys(new QComboBox()),
	  _leftShift(new QCheckBox(
		  obs_module_text("AdvSceneSwitcher.action.hotkey.leftShift"))),
	  _rightShift(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.action.hotkey.rightShift"))),
	  _leftCtrl(new QCheckBox(
		  obs_module_text("AdvSceneSwitcher.action.hotkey.leftCtrl"))),
	  _rightCtrl(new QCheckBox(
		  obs_module_text("AdvSceneSwitcher.action.hotkey.rightCtrl"))),
	  _leftAlt(new QCheckBox(
		  obs_module_text("AdvSceneSwitcher.action.hotkey.leftAlt"))),
	  _rightAlt(new QCheckBox(
		  obs_module_text("AdvSceneSwitcher.action.hotkey.rightAlt"))),
	  _leftMeta(new QCheckBox(
		  obs_module_text("AdvSceneSwitcher.action.hotkey.leftMeta"))),
	  _rightMeta(new QCheckBox(
		  obs_module_text("AdvSceneSwitcher.action.hotkey.rightMeta"))),
	  _duration(new DurationSelection(this, false)),
	  _onlySendToOBS(new QCheckBox(
		  obs_module_text("AdvSceneSwitcher.action.hotkey.onlyOBS"))),
	  _noKeyPressSimulationWarning(new QLabel(
		  obs_module_text("AdvSceneSwitcher.action.hotkey.disabled")))
{
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
	QWidget::connect(_duration, SIGNAL(DurationChanged(const Duration &)),
			 this, SLOT(DurationChanged(const Duration &)));
	QWidget::connect(_onlySendToOBS, SIGNAL(stateChanged(int)), this,
			 SLOT(OnlySendToOBSChanged(int)));

	QHBoxLayout *line1Layout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{keys}}", _keys},
		{"{{duration}}", _duration},
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
	mainLayout->addWidget(_onlySendToOBS);
	mainLayout->addWidget(_noKeyPressSimulationWarning);
	setLayout(mainLayout);

	_onlySendToOBS->setEnabled(canSimulateKeyPresses);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionHotkeyEdit::SetWarningVisibility()
{
	_noKeyPressSimulationWarning->setVisible(!_entryData->_onlySendToObs &&
						 !canSimulateKeyPresses);
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
	_duration->SetDuration(_entryData->_duration);
	_onlySendToOBS->setChecked(_entryData->_onlySendToObs ||
				   !canSimulateKeyPresses);
	SetWarningVisibility();
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

void MacroActionHotkeyEdit::DurationChanged(const Duration &dur)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_duration = dur;
}

void MacroActionHotkeyEdit::OnlySendToOBSChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_onlySendToObs = state;
	SetWarningVisibility();
}

void MacroActionHotkeyEdit::KeyChanged(int key)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_key = static_cast<HotkeyType>(key);
}
