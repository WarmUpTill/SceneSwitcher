#pragma once
#include "macro-action-edit.hpp"
#include "file-selection.hpp"

#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QStringList>

class MacroActionRun : public MacroAction {
public:
	bool PerformAction();
	void LogAction();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetShortDesc();
	std::string GetId() { return id; };
	static std::shared_ptr<MacroAction> Create()
	{
		return std::make_shared<MacroActionRun>();
	}

	std::string _path = obs_module_text("AdvSceneSwitcher.enterPath");
	QStringList _args;

private:
	static bool _registered;
	static const std::string id;
};

class MacroActionRunEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionRunEdit(QWidget *parent,
			   std::shared_ptr<MacroActionRun> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionRunEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionRun>(action));
	}

private slots:
	void PathChanged(const QString &text);
	void AddArg();
	void RemoveArg();
	void ArgUp();
	void ArgDown();
signals:
	void HeaderInfoChanged(const QString &);

protected:
	std::shared_ptr<MacroActionRun> _entryData;

private:
	FileSelection *_filePath;
	QListWidget *_argList;
	QPushButton *_addArg;
	QPushButton *_removeArg;
	QPushButton *_argUp;
	QPushButton *_argDown;
	bool _loading = true;
};
