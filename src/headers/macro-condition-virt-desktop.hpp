#pragma once
#include "macro.hpp"
#include <QWidget>
#include <QComboBox>

class MacroConditionVirtDesktop : public MacroCondition {
public:
	bool CheckCondition();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	int GetId() { return id; };
	static std::shared_ptr<MacroCondition> Create()
	{
		return std::make_shared<MacroConditionVirtDesktop>();
	}
	long _desktop = 0;

private:
	static bool _registered;
	static const int id;
};

class MacroConditionVirtDesktopEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionVirtDesktopEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionVirtDesktop> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionVirtDesktopEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionVirtDesktop>(cond));
	}

private slots:
	void DesktopChanged(int value);

protected:
	QComboBox *_virtDesktops;
	std::shared_ptr<MacroConditionVirtDesktop> _entryData;

private:
	bool _loading = true;
};
