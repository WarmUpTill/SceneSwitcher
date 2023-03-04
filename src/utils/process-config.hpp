#pragma once
#include "file-selection.hpp"
#include "string-list.hpp"

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
	StringList Args() const { return _args; }

private:
	std::string _path = obs_module_text("AdvSceneSwitcher.enterPath");
	std::string _workingDirectory = "";
	StringList _args;

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
	void WorkingDirectoryChanged(const QString &);
	void ArgsChanged(const StringList &);
signals:
	void ConfigChanged(const ProcessConfig &);

private:
	void ShowAdvancedSettings(bool);

	ProcessConfig _conf;

	FileSelection *_filePath;
	QPushButton *_showAdvancedSettings;
	QVBoxLayout *_advancedSettingsLayout;
	StringListEdit *_argList;
	FileSelection *_workingDirectory;
};
