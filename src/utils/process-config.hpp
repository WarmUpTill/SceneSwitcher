#pragma once
#include "file-selection.hpp"

#include <obs-data.h>
#include <obs-module.h>

#include <QPushButton>
#include <QListWidget>
#include <QStringList>
#include <QVBoxLayout>

class ProcessConfig {
public:
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);

	std::string Path() const { return _path; }
	std::string WorkingDir() const { return _workingDirectory; }
	QStringList Args() const { return _args; }

private:
	std::string _path = obs_module_text("AdvSceneSwitcher.enterPath");
	std::string _workingDirectory = "";
	QStringList _args;

	friend class ProcessConfigEdit;
};

class ProcessConfigEdit : public QWidget {
	Q_OBJECT

public:
	ProcessConfigEdit(QWidget *parent);
	void SetProcessConfig(const ProcessConfig &);

private slots:
	void PathChanged(const QString &);
	void ShowAdvancedSettingsClicked();
	void AddArg();
	void RemoveArg();
	void ArgUp();
	void ArgDown();
	void ArgItemClicked(QListWidgetItem *);
	void WorkingDirectoryChanged(const QString &);
signals:
	void ConfigChanged(const ProcessConfig &);

private:
	void SetArgListSize();
	void ShowAdvancedSettings(bool);

	ProcessConfig _conf;

	FileSelection *_filePath;
	QPushButton *_showAdvancedSettings;
	QVBoxLayout *_advancedSettingsLayout;
	QListWidget *_argList;
	QPushButton *_addArg;
	QPushButton *_removeArg;
	QPushButton *_argUp;
	QPushButton *_argDown;
	FileSelection *_workingDirectory;
};
