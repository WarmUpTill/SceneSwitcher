#pragma once
#include "macro.hpp"
#include <QWidget>
#include <QLineEdit>

class MacroConditionHotkey : public MacroCondition {
public:
	MacroConditionHotkey();
	~MacroConditionHotkey();
	bool CheckCondition();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetId() { return id; };
	static std::shared_ptr<MacroCondition> Create()
	{
		return std::make_shared<MacroConditionHotkey>();
	}
	void SetPressed() { _pressed = true; }

	std::string _name;
	obs_hotkey_id _hotkeyID = OBS_INVALID_HOTKEY_ID;

private:
	bool _pressed = false;
	static bool _registered;
	static const std::string id;
};

class MacroConditionHotkeyEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionHotkeyEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionHotkey> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionHotkeyEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionHotkey>(cond));
	}

private slots:
	void NameChanged();

protected:
	QLineEdit *_name;
	std::shared_ptr<MacroConditionHotkey> _entryData;

private:
	bool _loading = true;
};
