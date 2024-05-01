#include "hotkey-helpers.hpp"
#include "obs-module-helper.hpp"
#include "plugin-state-helpers.hpp"

namespace advss {

std::vector<std::weak_ptr<Hotkey>> Hotkey::_registeredHotkeys = {};
uint32_t Hotkey::_hotkeyCounter = 0;

static bool setup()
{
	AddLoadStep([](obs_data_t *) { Hotkey::ClearAllHotkeys(); });
	return true;
}
static bool setupDone = setup();

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
	_hotkeyID = obs_hotkey_register_frontend(
		GetNameFromDescription(description).c_str(),
		_description.c_str(), Callback, this);
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
	obs_hotkey_set_name(_hotkeyID,
			    GetNameFromDescription(descritpion).c_str());
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
	} else {
		hotkey->_lastReleased =
			std::chrono::high_resolution_clock::now();
	}
	hotkey->_pressed = pressed;
}

std::string Hotkey::GetNameFromDescription(const std::string &description)
{
	return "macro_condition_hotkey_" + description;
}

void Hotkey::ClearAllHotkeys()
{
	_registeredHotkeys.clear();
}

} // namespace advss
