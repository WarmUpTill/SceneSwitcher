#pragma once
#include "macro-entry.hpp"
#include <QWidget>
#include <QComboBox>

class MacroConditionWindow : public MacroCondition {
public:
	bool CheckCondition();
	bool Save();
	bool Load();
	int GetId() { return id; };
	static std::shared_ptr<MacroCondition> Create()
	{
		return std::make_shared<MacroConditionWindow>();
	}

	std::string _window;

private:
	static bool _registered;
	static const int id;
};

class MacroConditionWindowEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionWindowEdit(
		std::shared_ptr<MacroConditionWindow> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionWindowEdit(
			std::dynamic_pointer_cast<MacroConditionWindow>(cond));
	}

private slots:
	void WindowChanged(const QString &text);

protected:
	QComboBox *_windowSelection;
	std::shared_ptr<MacroConditionWindow> _entryData;

private:
	bool _loading = true;
};
