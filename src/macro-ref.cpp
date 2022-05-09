#include "headers/macro-ref.hpp"
#include "headers/macro.hpp"

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
void MacroRef::Save(obs_data_t *obj)
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
