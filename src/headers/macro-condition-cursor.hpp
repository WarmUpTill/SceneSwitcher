#pragma once
#include "macro.hpp"

#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QTimer>
#include <QFrame>

enum class CursorCondition {
	REGION,
	MOVING,
};

class MacroConditionCursor : public MacroCondition {
public:
	bool CheckCondition();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetId() { return id; };
	static std::shared_ptr<MacroCondition> Create()
	{
		return std::make_shared<MacroConditionCursor>();
	}

	CursorCondition _condition = CursorCondition::REGION;
	int _minX = 0, _minY = 0, _maxX = 0, _maxY = 0;

private:
	static bool _registered;
	static const std::string id;
};

class MacroConditionCursorEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionCursorEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionCursor> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionCursorEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionCursor>(cond));
	}

private slots:
	void ConditionChanged(int cond);
	void MinXChanged(int pos);
	void MinYChanged(int pos);
	void MaxXChanged(int pos);
	void MaxYChanged(int pos);
	void UpdateCursorPos();
	void ToggleFrame();

protected:
	QSpinBox *_minX;
	QSpinBox *_minY;
	QSpinBox *_maxX;
	QSpinBox *_maxY;
	QComboBox *_conditions;
	QPushButton *_frameToggle;
	QLabel *_xPos;
	QLabel *_yPos;
	std::shared_ptr<MacroConditionCursor> _entryData;

private:
	void SetRegionSelectionVisible(bool);
	void SetupFrame();
	QTimer _timer;
	QFrame _frame;
	bool _loading = true;
};
