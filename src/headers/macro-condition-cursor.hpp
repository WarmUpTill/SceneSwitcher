#pragma once
#include "macro.hpp"
#include <QWidget>
#include <QComboBox>

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
	void MinXChanged(int pos);
	void MinYChanged(int pos);
	void MaxXChanged(int pos);
	void MaxYChanged(int pos);

protected:
	QSpinBox *_minX;
	QSpinBox *_minY;
	QSpinBox *_maxX;
	QSpinBox *_maxY;
	std::shared_ptr<MacroConditionCursor> _entryData;

private:
	bool _loading = true;
};
