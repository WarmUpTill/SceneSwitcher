#pragma once
#include "macro.hpp"
#include "resizing-text-edit.hpp"

#include <QCheckBox>

class MacroConditionWebsocket : public MacroCondition {
public:
	MacroConditionWebsocket(Macro *m) : MacroCondition(m) {}
	bool CheckCondition();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetShortDesc();
	std::string GetId() { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionWebsocket>(m);
	}

	std::string _message = obs_module_text("AdvSceneSwitcher.enterText");
	bool _useRegex = false;

private:
	static bool _registered;
	static const std::string id;
};

class MacroConditionWebsocketEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionWebsocketEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionWebsocket> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionWebsocketEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionWebsocket>(
				cond));
	}

private slots:
	void MessageChanged();
	void UseRegexChanged(int state);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	ResizingPlainTextEdit *_message;
	QCheckBox *_useRegex;
	std::shared_ptr<MacroConditionWebsocket> _entryData;

private:
	bool _loading = true;
};
