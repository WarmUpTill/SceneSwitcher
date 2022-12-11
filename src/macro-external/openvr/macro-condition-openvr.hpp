#pragma once
#include <macro.hpp>

#include <QDoubleSpinBox>
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

	double _minX = 0, _minY = 0, _minZ = 0, _maxX = 0, _maxY = 0, _maxZ = 0;

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
	void MinXChanged(double pos);
	void MinYChanged(double pos);
	void MinZChanged(double pos);
	void MaxXChanged(double pos);
	void MaxYChanged(double pos);
	void MaxZChanged(double pos);
	void UpdateOpenVRPos();

protected:
	QDoubleSpinBox *_minX;
	QDoubleSpinBox *_minY;
	QDoubleSpinBox *_minZ;
	QDoubleSpinBox *_maxX;
	QDoubleSpinBox *_maxY;
	QDoubleSpinBox *_maxZ;
	QLabel *_xPos;
	QLabel *_yPos;
	QLabel *_zPos;
	QLabel *_errLabel;
	std::shared_ptr<MacroConditionOpenVR> _entryData;

private:
	QTimer _timer;
	bool _loading = true;
};
