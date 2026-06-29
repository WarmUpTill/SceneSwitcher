#include "variable-color.hpp"

#include <obs.hpp>

namespace advss {

ColorVariable::ColorVariable(const QColor &value) : _value(value) {}

void ColorVariable::Save(obs_data_t *obj, const char *name) const
{
	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_int(data, "version", 1);
	obs_data_set_int(data, "type", static_cast<int>(_type));
	auto var = _variable.lock();
	if (var) {
		obs_data_set_string(data, "variable", var->Name().c_str());
	}
	obs_data_set_int(data, "red", _value.red());
	obs_data_set_int(data, "green", _value.green());
	obs_data_set_int(data, "blue", _value.blue());
	obs_data_set_obj(obj, name, data);
}

void ColorVariable::Load(obs_data_t *obj, const char *name)
{
	OBSDataAutoRelease data = obs_data_get_obj(obj, name);
	_value.setRed(obs_data_get_int(data, "red"));
	_value.setGreen(obs_data_get_int(data, "green"));
	_value.setBlue(obs_data_get_int(data, "blue"));
	if (!obs_data_has_user_value(data, "version")) {
		// Old format: no variable support, just R/G/B
		_type = Type::FIXED_VALUE;
		return;
	}
	auto variableName = obs_data_get_string(data, "variable");
	_variable = GetWeakVariableByName(variableName);
	_type = static_cast<Type>(obs_data_get_int(data, "type"));
}

QColor ColorVariable::GetValue() const
{
	if (_type == Type::FIXED_VALUE) {
		return _value;
	}
	auto var = _variable.lock();
	if (!var) {
		return Qt::black;
	}
	const QColor color(QString::fromStdString(var->Value()));
	return color.isValid() ? color : Qt::black;
}

} // namespace advss
