#pragma once
#include "export-symbol-helper.hpp"
#include "variable-number.hpp"

#include <QWidget>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>

namespace advss {

class ADVSS_EXPORT GenericVaraiableSpinbox : public QWidget {
	Q_OBJECT
public:
	GenericVaraiableSpinbox(QWidget *parent, bool wholeNumber);
	void SetValue(const NumberVariable<int> &);
	void SetValue(const NumberVariable<double> &);
	void DisableVariableSelection();

	void setMinimum(double value);
	void setMaximum(double value);

	void setPrefix(const QString &prefix);
	void setSuffix(const QString &suffix);

	void specialValueText(const QString &text);

public slots:
	void SetFixedValue(int);
	void SetFixedValue(double);
	void VariableChanged(const QString &);
	void ToggleTypeClicked(bool);
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

class ADVSS_EXPORT VariableSpinBox : public GenericVaraiableSpinbox {
	Q_OBJECT

public:
	VariableSpinBox(QWidget *parent = nullptr);
	NumberVariable<int> Value() { return _numberInt; }
	QSpinBox *SpinBox() { return _fixedValueInt; }
};

class ADVSS_EXPORT VariableDoubleSpinBox : public GenericVaraiableSpinbox {
	Q_OBJECT

public:
	VariableDoubleSpinBox(QWidget *parent = nullptr);
	NumberVariable<double> Value() { return _numberDouble; }
	QDoubleSpinBox *SpinBox() { return _fixedValueDouble; }
	void setDecimals(int prec);
};

} // namespace advss
