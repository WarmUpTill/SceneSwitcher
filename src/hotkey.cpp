#include "hotkey.hpp"
#include "advanced-scene-switcher.hpp"
#include <obs-module.h>
#include <fstream>
#include <regex>

namespace advss {

std::vector<std::weak_ptr<Hotkey>> Hotkey::_registeredHotkeys = {};
uint32_t Hotkey::_hotkeyCounter = 1;

std::shared_ptr<Hotkey> Hotkey::GetHotkey(const std::string &description,
					  bool ignoreExistingHotkeys)
{
	// Clean up expired hotkeys
	auto it = _registeredHotkeys.begin();
	while (it != _registeredHotkeys.end()) {
		if (it->expired()) {
			it = _registeredHotkeys.erase(it);
		} else {
			it++;
		}
	}

	// Check for existing hotkey with same description
	for (const auto &h : _registeredHotkeys) {
		auto hotkey = h.lock();
		if (!hotkey) {
			continue;
		}
		if (hotkey->_description == description) {
			hotkey->_ignoreExistingHotkeys = ignoreExistingHotkeys;
			return hotkey;
		}
	}

	// Create new hotkey
	auto hotkey = std::make_shared<Hotkey>(description);
	_registeredHotkeys.emplace_back(hotkey);
	hotkey->_ignoreExistingHotkeys = ignoreExistingHotkeys;
	return hotkey;
}

Hotkey::Hotkey(const std::string &description) : _description(description)
{
	std::string name =
		"macro_condition_hotkey_" + std::to_string(_hotkeyCounter);
	_hotkeyID = obs_hotkey_register_frontend(
		name.c_str(), _description.c_str(), Callback, this);
	_hotkeyCounter++;
}

bool Hotkey::Save(obs_data_t *obj) const
{
	obs_data_set_string(obj, "desc", _description.c_str());
	obs_data_array_t *hotkeyData = obs_hotkey_save(_hotkeyID);
	obs_data_set_array(obj, "keyBind", hotkeyData);
	obs_data_array_release(hotkeyData);
	return true;
}

bool Hotkey::Load(obs_data_t *obj)
{
	auto description = obs_data_get_string(obj, "desc");
	if (!DescriptionAvailable(description)) {
		return false;
	}
	_description = description;
	obs_data_array_t *hotkeyData = obs_data_get_array(obj, "keyBind");
	obs_hotkey_load(_hotkeyID, hotkeyData);
	obs_data_array_release(hotkeyData);
	obs_hotkey_set_description(_hotkeyID, _description.c_str());
	_ignoreExistingHotkeys = false;
	return true;
}

Hotkey::~Hotkey()
{
	obs_hotkey_unregister(_hotkeyID);
}

bool Hotkey::UpdateDescription(const std::string &descritpion)
{
	if (!DescriptionAvailable(descritpion)) {
		return false;
	}
	_description = descritpion;
	obs_hotkey_set_description(_hotkeyID, descritpion.c_str());
	return true;
}

bool Hotkey::DescriptionAvailable(const std::string &descritpion)
{
	for (const auto &hotkey : _registeredHotkeys) {
		auto h = hotkey.lock();
		if (!h) {
			continue;
		}
		if (!h->_ignoreExistingHotkeys &&
		    h->_description == descritpion) {
			return false;
		}
	}
	return true;
}

void Hotkey::Callback(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	auto hotkey = static_cast<Hotkey *>(data);
	if (pressed) {
		hotkey->_lastPressed =
			std::chrono::high_resolution_clock::now();
	}
	hotkey->_pressed = pressed;
}

void Hotkey::ClearAllHotkeys()
{
	_registeredHotkeys.clear();
}

void startHotkeyFunc(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (pressed) {
		if (!(switcher->th && switcher->th->isRunning())) {
			switcher->Start();
		}
	}
}

void stopHotkeyFunc(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (pressed) {
		if (switcher->th && switcher->th->isRunning()) {
			switcher->Stop();
		}
	}
}

void startStopToggleHotkeyFunc(void *, obs_hotkey_id, obs_hotkey_t *,
			       bool pressed)
{
	if (pressed) {
		if (switcher->th && switcher->th->isRunning()) {
			switcher->Stop();
		} else {
			switcher->Start();
		}
	}
}

void upMacroSegmentHotkeyFunc(void *, obs_hotkey_id, obs_hotkey_t *,
			      bool pressed)
{
	if (pressed && switcher->settingsWindowOpened &&
	    AdvSceneSwitcher::window) {
		QMetaObject::invokeMethod(AdvSceneSwitcher::window,
					  "UpMacroSegementHotkey",
					  Qt::QueuedConnection);
	}
}

void downMacroSegmentHotkeyFunc(void *, obs_hotkey_id, obs_hotkey_t *,
				bool pressed)
{
	if (pressed && switcher->settingsWindowOpened &&
	    AdvSceneSwitcher::window) {
		QMetaObject::invokeMethod(AdvSceneSwitcher::window,
					  "DownMacroSegementHotkey",
					  Qt::QueuedConnection);
	}
}

void removeMacroSegmentHotkeyFunc(void *, obs_hotkey_id, obs_hotkey_t *,
				  bool pressed)
{
	if (pressed && switcher->settingsWindowOpened &&
	    AdvSceneSwitcher::window) {
		QMetaObject::invokeMethod(AdvSceneSwitcher::window,
					  "DeleteMacroSegementHotkey",
					  Qt::QueuedConnection);
	}
}

void registerHotkeys()
{
	switcher->startHotkey = obs_hotkey_register_frontend(
		"startSwitcherHotkey",
		obs_module_text("AdvSceneSwitcher.hotkey.startSwitcherHotkey"),
		startHotkeyFunc, NULL);
	switcher->stopHotkey = obs_hotkey_register_frontend(
		"stopSwitcherHotkey",
		obs_module_text("AdvSceneSwitcher.hotkey.stopSwitcherHotkey"),
		stopHotkeyFunc, NULL);
	switcher->toggleHotkey = obs_hotkey_register_frontend(
		"startStopToggleSwitcherHotkey",
		obs_module_text(
			"AdvSceneSwitcher.hotkey.startStopToggleSwitcherHotkey"),
		startStopToggleHotkeyFunc, NULL);
	switcher->upMacroSegment = obs_hotkey_register_frontend(
		"upMacroSegmentSwitcherHotkey",
		obs_module_text("AdvSceneSwitcher.hotkey.upMacroSegmentHotkey"),
		upMacroSegmentHotkeyFunc, NULL);
	switcher->downMacroSegment = obs_hotkey_register_frontend(
		"downMacroSegmentSwitcherHotkey",
		obs_module_text(
			"AdvSceneSwitcher.hotkey.downMacroSegmentHotkey"),
		downMacroSegmentHotkeyFunc, NULL);
	switcher->removeMacroSegment = obs_hotkey_register_frontend(
		"removeMacroSegmentSwitcherHotkey",
		obs_module_text(
			"AdvSceneSwitcher.hotkey.removeMacroSegmentHotkey"),
		removeMacroSegmentHotkeyFunc, NULL);

	switcher->hotkeysRegistered = true;
}

void saveHotkey(obs_data_t *obj, obs_hotkey_id id, const char *name)
{
	obs_data_array_t *a = obs_hotkey_save(id);
	obs_data_set_array(obj, name, a);
	obs_data_array_release(a);
}

void SwitcherData::saveHotkeys(obs_data_t *obj)
{
	saveHotkey(obj, startHotkey, "startHotkey");
	saveHotkey(obj, stopHotkey, "stopHotkey");
	saveHotkey(obj, toggleHotkey, "toggleHotkey");
	saveHotkey(obj, upMacroSegment, "upMacroSegmentHotkey");
	saveHotkey(obj, downMacroSegment, "downMacroSegmentHotkey");
	saveHotkey(obj, removeMacroSegment, "removeMacroSegmentHotkey");
}

void loadHotkey(obs_data_t *obj, obs_hotkey_id id, const char *name)
{
	obs_data_array_t *a = obs_data_get_array(obj, name);
	obs_hotkey_load(id, a);
	obs_data_array_release(a);
}

void SwitcherData::loadHotkeys(obs_data_t *obj)
{
	if (!hotkeysRegistered) {
		registerHotkeys();
	}
	loadHotkey(obj, startHotkey, "startHotkey");
	loadHotkey(obj, stopHotkey, "stopHotkey");
	loadHotkey(obj, toggleHotkey, "toggleHotkey");
	loadHotkey(obj, upMacroSegment, "upMacroSegmentHotkey");
	loadHotkey(obj, downMacroSegment, "downMacroSegmentHotkey");
	loadHotkey(obj, removeMacroSegment, "removeMacroSegmentHotkey");
}

} // namespace advss
