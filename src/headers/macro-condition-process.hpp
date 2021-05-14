#pragma once
#include "macro.hpp"
#include <QWidget>
#include <QComboBox>

class MacroConditionProcess : public MacroCondition {
public:
	bool CheckCondition();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	int GetId() { return id; };
	static std::shared_ptr<MacroCondition> Create()
	{
		return std::make_shared<MacroConditionProcess>();
	}

	std::string _process;
	bool _focus = true;

private:
	static bool _registered;
	static const int id;
};

class MacroConditionProcessEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionProcessEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionProcess> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionProcessEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionProcess>(cond));
	}

private slots:
	void ProcessChanged(const QString &text);
	void FocusChanged(int state);

protected:
	QComboBox *_processSelection;
	QCheckBox *_focused;
	std::shared_ptr<MacroConditionProcess> _entryData;

private:
	bool _loading = true;
};
