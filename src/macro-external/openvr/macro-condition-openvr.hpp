#pragma once
#include <macro.hpp>
#include <variable-spinbox.hpp>

#include <QComboBox>
#include <QPushButton>
#include <QTimer>

class MacroConditionOpenVR : public MacroCondition {
public:
	MacroConditionOpenVR(Macro *m) : MacroCondition(m) {}
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionOpenVR>(m);
	}

	NumberVariable<double> _minX = 0.0, _minY = 0.0, _minZ = 0.0,
			       _maxX = 0.0, _maxY = 0.0, _maxZ = 0.0;

private:
	static bool _registered;
	static const std::string id;
};

class MacroConditionOpenVREdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionOpenVREdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionOpenVR> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionOpenVREdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionOpenVR>(cond));
	}

private slots:
	void MinXChanged(const NumberVariable<double> &pos);
	void MinYChanged(const NumberVariable<double> &pos);
	void MinZChanged(const NumberVariable<double> &pos);
	void MaxXChanged(const NumberVariable<double> &pos);
	void MaxYChanged(const NumberVariable<double> &pos);
	void MaxZChanged(const NumberVariable<double> &pos);
	void UpdateOpenVRPos();

protected:
	VariableDoubleSpinBox *_minX;
	VariableDoubleSpinBox *_minY;
	VariableDoubleSpinBox *_minZ;
	VariableDoubleSpinBox *_maxX;
	VariableDoubleSpinBox *_maxY;
	VariableDoubleSpinBox *_maxZ;
	QLabel *_xPos;
	QLabel *_yPos;
	QLabel *_zPos;
	QLabel *_errLabel;
	std::shared_ptr<MacroConditionOpenVR> _entryData;

private:
	QTimer _timer;
	bool _loading = true;
};
