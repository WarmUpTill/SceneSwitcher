#pragma once
#include "macro-action-edit.hpp"

#include <QLineEdit>
#include <QPushButton>

class MacroActionRun : public MacroAction {
public:
	bool PerformAction();
	void LogAction();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetId() { return id; };
	static std::shared_ptr<MacroAction> Create()
	{
		return std::make_shared<MacroActionRun>();
	}

	std::string _path = obs_module_text("AdvSceneSwitcher.enterPath");

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
	void FilePathChanged();
	void BrowseButtonClicked();

protected:
	std::shared_ptr<MacroActionRun> _entryData;

private:
	QLineEdit *_filePath;
	QPushButton *_browseButton;
	bool _loading = true;
};
