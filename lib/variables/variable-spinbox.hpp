#pragma once
#include "variable-number.hpp"

#include <QWidget>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>

namespace advss {

class GenericVaraiableSpinbox : public QWidget {
	Q_OBJECT
public:
	EXPORT GenericVaraiableSpinbox(QWidget *parent, bool wholeNumber);
	EXPORT void SetValue(const NumberVariable<int> &);
	EXPORT void SetValue(const NumberVariable<double> &);
	EXPORT void DisableVariableSelection();

	EXPORT void setMinimum(double value);
	EXPORT void setMaximum(double value);

	EXPORT void setPrefix(const QString &prefix);
	EXPORT void setSuffix(const QString &suffix);

	EXPORT void specialValueText(const QString &text);

public slots:
	EXPORT void SetFixedValue(int);
	EXPORT void SetFixedValue(double);
	EXPORT void VariableChanged(const QString &);
	EXPORT void ToggleTypeClicked(bool);
signals:
	void NumberVariableChanged(const NumberVariable<int> &);
	void NumberVariableChanged(const NumberVariable<double> &);

	void FixedValueChanged(int);
	void FixedValueChanged(double);

	void VariableAdded(const QString &);
	void VariableRenamed(const QString &oldName, const QString &newName);
	void VariableRemoved(const QString &);

protected:
	QSpinBox *_fixedValueInt;
	QDoubleSpinBox *_fixedValueDouble;
	NumberVariable<int> _numberInt;
	NumberVariable<double> _numberDouble;

private:
	void EmitSignals();
	void SetVisibility();
	void SetVisibilityInt();
	void SetVisibilityDouble();

	QPushButton *_toggleType;
	VariableSelection *_variable;
	bool _wholeNumber;
	bool _hideTypeToggle = false;
};

class VariableSpinBox : public GenericVaraiableSpinbox {
public:
	EXPORT VariableSpinBox(QWidget *parent = nullptr);
	EXPORT NumberVariable<int> Value() { return _numberInt; }
	EXPORT QSpinBox *SpinBox() { return _fixedValueInt; }
};

class VariableDoubleSpinBox : public GenericVaraiableSpinbox {
public:
	EXPORT VariableDoubleSpinBox(QWidget *parent = nullptr);
	EXPORT NumberVariable<double> Value() { return _numberDouble; }
	EXPORT QDoubleSpinBox *SpinBox() { return _fixedValueDouble; }
	EXPORT void setDecimals(int prec);
};

} // namespace advss
