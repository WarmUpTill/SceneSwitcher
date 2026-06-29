#pragma once
#include "variable.hpp"

#include <obs-data.h>
#include <QColor>

namespace advss {

class VariableColorButton;

class ADVSS_EXPORT ColorVariable {
public:
	enum class Type { FIXED_VALUE, VARIABLE };

	ColorVariable() = default;
	ColorVariable(const QColor &value);

	void Save(obs_data_t *obj, const char *name) const;
	void Load(obs_data_t *obj, const char *name);

	QColor GetValue() const;
	QColor GetFixedValue() const { return _value; }
	bool IsFixedType() const { return _type == Type::FIXED_VALUE; }
	Type GetType() const { return _type; }
	std::weak_ptr<Variable> GetVariable() const { return _variable; }

private:
	Type _type = Type::FIXED_VALUE;
	QColor _value = Qt::black;
	std::weak_ptr<Variable> _variable;

	friend class VariableColorButton;
};

} // namespace advss
