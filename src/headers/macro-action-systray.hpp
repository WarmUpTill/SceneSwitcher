#pragma once
#include "macro-action-edit.hpp"

#include <QLineEdit>

class MacroActionSystray : public MacroAction {
public:
	bool PerformAction();
	void LogAction();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetId() { return id; };
	static std::shared_ptr<MacroAction> Create()
	{
		return std::make_shared<MacroActionSystray>();
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
