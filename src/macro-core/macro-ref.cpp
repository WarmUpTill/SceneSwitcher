#include "macro-ref.hpp"
#include "macro.hpp"

MacroRef::MacroRef(std::string name) : _name(name)
{
	UpdateRef();
}
void MacroRef::UpdateRef()
{
	_ref = GetMacroByName(_name.c_str());
}
void MacroRef::UpdateRef(std::string newName)
{
	_name = newName;
	UpdateRef();
}
void MacroRef::UpdateRef(QString newName)
{
	_name = newName.toStdString();
	UpdateRef();
}

void MacroRef::Save(obs_data_t *obj) const
{
	if (_ref) {
		obs_data_set_string(obj, "macro", _ref->Name().c_str());
	}
}
void MacroRef::Load(obs_data_t *obj)
{
	_name = obs_data_get_string(obj, "macro");
	UpdateRef();
}

Macro *MacroRef::get()
{
	return _ref;
}

Macro *MacroRef::operator->()
{
	return _ref;
}

void SaveMacroList(obs_data_t *obj, const std::vector<MacroRef> &macros,
		   const std::string &name)
{
	obs_data_array_t *array = obs_data_array_create();
	for (auto &m : macros) {
		if (!m.get()) {
			continue;
		}
		obs_data_t *array_obj = obs_data_create();
		m.Save(array_obj);
		obs_data_array_push_back(array, array_obj);
		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, name.c_str(), array);
	obs_data_array_release(array);
}

void LoadMacroList(obs_data_t *obj, std::vector<MacroRef> &macros,
		   const std::string &name)
{
	obs_data_array_t *array = obs_data_get_array(obj, name.c_str());
	size_t count = obs_data_array_count(array);
	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj = obs_data_array_item(array, i);
		MacroRef ref;
		ref.Load(array_obj);
		macros.push_back(ref);
		obs_data_release(array_obj);
	}
	obs_data_array_release(array);
}
