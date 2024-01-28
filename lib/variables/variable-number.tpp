
template class NumberVariable<int>;
template class NumberVariable<double>;

template<typename T>
inline NumberVariable<T>::NumberVariable(T value) : _value(value)
{
}

template<typename T>
void NumberVariable<T>::Save(obs_data_t *obj, const char *name) const
{
	auto data = obs_data_create();
	if constexpr (std::is_same<T, int>::value) {
		obs_data_set_int(data, "value", _value);
	} else if constexpr (std::is_same<T, double>::value) {
		obs_data_set_double(data, "value", _value);
	} else {
		assert(false);
	}
	auto var = _variable.lock();
	if (var) {
		obs_data_set_string(data, "variable", var->Name().c_str());
	}
	obs_data_set_int(data, "type", static_cast<int>(_type));
	obs_data_set_obj(obj, name, data);
	obs_data_release(data);
}

template<typename T>
void NumberVariable<T>::Load(obs_data_t *obj, const char *name)
{
	auto data = obs_data_get_obj(obj, name);
	if constexpr (std::is_same<T, int>::value) {
		_value = obs_data_get_int(data, "value");
	} else if constexpr (std::is_same<T, double>::value) {
		_value = obs_data_get_double(data, "value");
	} else {
		assert(false);
	}
	auto variableName = obs_data_get_string(data, "variable");
	_variable = GetWeakVariableByName(variableName);
	_type = static_cast<Type>(obs_data_get_int(data, "type"));
	obs_data_release(data);
}

template<typename T> T NumberVariable<T>::GetValue() const
{
	if (_type == Type::FIXED_VALUE) {
		return _value;
	}

	auto var = _variable.lock();
	if (!var) {
		return {};
	}

	if constexpr (std::is_same<T, int>::value) {
		auto value = var->IntValue();
		return value.value_or(0);
	} else if constexpr (std::is_same<T, double>::value) {
		auto value = var->DoubleValue();
		return value.value_or(0.0);
	}

	assert(false);
	return 0;
}

template<typename T> bool NumberVariable<T>::HasValidValue() const
{
	if (_type == Type::FIXED_VALUE) {
		return true;
	}

	auto var = _variable.lock();
	if (!var) {
		return false;
	}

	if constexpr (std::is_same<T, int>::value) {
		auto value = var->IntValue();
		return value.has_value();
	} else if constexpr (std::is_same<T, double>::value) {
		auto value = var->IntValue();
		return value.has_value();
	}
	assert(false);
	return false;
}

template<typename T> NumberVariable<T>::operator T() const
{
	return GetValue();
}
