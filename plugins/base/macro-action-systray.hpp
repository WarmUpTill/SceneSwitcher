#pragma once
#include "macro-action-edit.hpp"
#include "variable-string.hpp"
#include "file-selection.hpp"

#include <QLineEdit>

namespace advss {

class MacroActionSystray : public MacroAction {
public:
	MacroActionSystray(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m);
	std::shared_ptr<MacroAction> Copy() const;
	void ResolveVariablesToFixedValues();

	StringVariable _message = "";
	StringVariable _title = obs_module_text("AdvSceneSwitcher.pluginName");
	StringVariable _iconPath = "";

private:
	QIcon _icon;
	std::string _lastPath;

	static bool _registered;
	static const std::string id;
};

class MacroActionSystrayEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionSystrayEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionSystray> entryData = nullptr);
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionSystrayEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionSystray>(action));
	}

private slots:
	void MessageChanged();
	void TitleChanged();
	void IconPathChanged(const QString &text);
	void CheckIfTrayIsDisabled();

protected:
	std::shared_ptr<MacroActionSystray> _entryData;

private:
	VariableLineEdit *_message;
	VariableLineEdit *_title;
	FileSelection *_iconPath;
	QLabel *_trayDisableWarning;

	QTimer _checkTrayDisableTimer;
	bool _loading = true;
};

} // namespace advss
