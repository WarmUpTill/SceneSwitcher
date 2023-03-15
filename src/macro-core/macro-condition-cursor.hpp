#pragma once
#include "macro.hpp"
#include "striped-frame.hpp"
#include "variable-spinbox.hpp"

#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QTimer>

class MacroConditionCursor : public MacroCondition {
public:
	MacroConditionCursor(Macro *m) : MacroCondition(m, true) {}
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionCursor>(m);
	}

	enum class Condition {
		REGION,
		MOVING,
		CLICK,
	};

	enum class Button {
		LEFT,
		MIDDLE,
		RIGHT,
	};

	Condition _condition = Condition::REGION;
	Button _button = Button::LEFT;
	NumberVariable<int> _minX = 0, _minY = 0, _maxX = 0, _maxY = 0;

private:
	bool CheckClick();
	std::chrono::high_resolution_clock::time_point _lastCheckTime{};

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
	void ConditionChanged(int idx);
	void ButtonChanged(int idx);
	void MinXChanged(const NumberVariable<int> &pos);
	void MinYChanged(const NumberVariable<int> &pos);
	void MaxXChanged(const NumberVariable<int> &pos);
	void MaxYChanged(const NumberVariable<int> &pos);
	void UpdateCursorPos();
	void ToggleFrame();

protected:
	VariableSpinBox *_minX;
	VariableSpinBox *_minY;
	VariableSpinBox *_maxX;
	VariableSpinBox *_maxY;
	QComboBox *_conditions;
	QComboBox *_buttons;
	QPushButton *_frameToggle;
	QLabel *_xPos;
	QLabel *_yPos;
	QHBoxLayout *_curentPosLayout;
	std::shared_ptr<MacroConditionCursor> _entryData;

private:
	void SetWidgetVisibility();
	void SetupFrame();
	QTimer _timer;
	StripedFrame _frame;
	bool _loading = true;
};
