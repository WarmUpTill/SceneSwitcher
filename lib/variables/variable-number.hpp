#pragma once
#include "variable.hpp"

namespace advss {

template<typename T> class NumberVariable {
public:
	NumberVariable() = default;
	NumberVariable(T);

	void Save(obs_data_t *obj, const char *name) const;
	void Load(obs_data_t *obj, const char *name);

	T GetValue() const;
	T GetFixedValue() const { return _value; }
	bool HasValidValue() const;
	void SetValue(T val) { _value = val; }
	void SetValue(const std::weak_ptr<Variable> &var) { _variable = var; }
	operator T() const;

	enum class Type { FIXED_VALUE, VARIABLE };
	Type GetType() const { return _type; }
	bool IsFixedType() const { return _type == Type::FIXED_VALUE; }
	std::weak_ptr<Variable> GetVariable() const { return _variable; }
	void ResolveVariables();

private:
	Type _type = Type::FIXED_VALUE;
	T _value = {};
	std::weak_ptr<Variable> _variable;

	friend class GenericVaraiableSpinbox;
	friend class VariableSpinBox;
	friend class VariableDoubleSpinBox;
};

#include "variable-number.tpp"

using IntVariable = NumberVariable<int>;
using DoubleVariable = NumberVariable<double>;

} // namespace advss
