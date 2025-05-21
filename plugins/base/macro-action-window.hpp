#pragma once
#include "macro-action-edit.hpp"
#include "regex-config.hpp"
#include "variable-string.hpp"
#include "window-selection.hpp"

namespace advss {

class MacroActionWindow : public MacroAction {
public:
	MacroActionWindow(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m);
	std::shared_ptr<MacroAction> Copy() const;
	void ResolveVariablesToFixedValues();

	enum class Action {
		SET_FOCUS_WINDOW,
		MAXIMIZE_WINDOW,
		MINIMIZE_WINDOW,
		CLOSE_WINDOW,
	};
	Action _action = Action::SET_FOCUS_WINDOW;
	StringVariable _window;
	RegexConfig _regex;

private:
	std::optional<std::string> GetMatchingWindow() const;

	static bool _registered;
	static const std::string id;
};

class MacroActionWindowEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionWindowEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionWindow> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionWindowEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionWindow>(action));
	}

private slots:
	void ActionChanged(int value);
	void WindowChanged(const QString &text);
	void RegexChanged(const RegexConfig &);
signals:
	void HeaderInfoChanged(const QString &);

private:
	void SetWidgetVisibility();

	QComboBox *_actions;
	WindowSelectionWidget *_windows;
	RegexConfigWidget *_regex;
	QHBoxLayout *_infoLayout;

	std::shared_ptr<MacroActionWindow> _entryData;
	bool _loading = true;
};

} // namespace advss
