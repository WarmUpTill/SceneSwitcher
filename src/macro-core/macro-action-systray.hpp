#pragma once
#include "macro-action-edit.hpp"

#include <QLineEdit>

class MacroActionSystray : public MacroAction {
public:
	MacroActionSystray(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m)
	{
		return std::make_shared<MacroActionSystray>(m);
	}

	std::string _msg = "";

private:
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

protected:
	std::shared_ptr<MacroActionSystray> _entryData;

private:
	QLineEdit *_msg;
	bool _loading = true;
};
