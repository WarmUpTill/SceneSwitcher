#pragma once
#include "macro-condition-edit.hpp"
#include "hotkey-helpers.hpp"

#include <QWidget>
#include <QLineEdit>

namespace advss {

class MacroConditionHotkey : public MacroCondition {
public:
	MacroConditionHotkey(Macro *m);
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionHotkey>(m);
	}

	std::shared_ptr<Hotkey> _hotkey;

private:
	std::chrono::high_resolution_clock::time_point _lastCheck{};

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

} // namespace advss
