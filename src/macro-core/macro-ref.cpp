#include "macro-ref.hpp"
#include "macro.hpp"

MacroRef::MacroRef(std::string name)
{
	_macro = GetWeakMacroByName(name.c_str());
}

void MacroRef::Save(obs_data_t *obj) const
{
	if (auto macro = _macro.lock()) {
		obs_data_set_string(obj, "macro", macro->Name().c_str());
	}
}
void MacroRef::Load(obs_data_t *obj)
{
	auto name = obs_data_get_string(obj, "macro");
	_postLoadName = name;
	_macro = GetWeakMacroByName(name);
}

void MacroRef::PostLoad()
{
	_macro = GetWeakMacroByName(_postLoadName.c_str());
}

void MacroRef::operator=(const QString &name)
{
	_macro = GetWeakMacroByName(name.toStdString().c_str());
}

void MacroRef::operator=(const std::shared_ptr<Macro> &macro)
{
	_macro = macro;
}

std::shared_ptr<Macro> MacroRef::GetMacro() const
{
	return _macro.lock();
}

std::string MacroRef::Name() const
{
	if (auto macro = GetMacro()) {
		return macro->Name();
	}
	return "";
}

void SaveMacroList(obs_data_t *obj, const std::vector<MacroRef> &macros,
		   const std::string &name)
{
	obs_data_array_t *array = obs_data_array_create();
	for (auto &m : macros) {
		if (!m.GetMacro()) {
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
