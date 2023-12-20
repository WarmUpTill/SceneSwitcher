#pragma once
#include "file-selection.hpp"
#include "string-list.hpp"

#include <obs-data.h>
#include <obs-module-helper.hpp>

#include <QPushButton>
#include <QListWidget>
#include <QStringList>
#include <QVBoxLayout>
#include <variant>

namespace advss {

class ProcessConfig {
public:
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);

	std::string Path() const { return _path; }
	std::string UnresolvedPath() const { return _path.UnresolvedValue(); }
	std::string WorkingDir() const { return _workingDirectory; }
	QStringList Args() const; // Resolves variables

	enum class ProcStartError {
		NONE,
		FAILED_TO_START,
		TIMEOUT,
		CRASH,
	};

	std::variant<int, ProcStartError>
	StartProcessAndWait(int timeoutInMs) const;
	bool StartProcessDetached() const;

private:
	StringVariable _path = obs_module_text("AdvSceneSwitcher.enterPath");
	StringVariable _workingDirectory = "";
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
	void AdvancedSettingsEnabled();

private:
	void ShowAdvancedSettings(bool);

	ProcessConfig _conf;

	FileSelection *_filePath;
	QPushButton *_showAdvancedSettings;
	QVBoxLayout *_advancedSettingsLayout;
	StringListEdit *_argList;
	FileSelection *_workingDirectory;
};

} // namespace advss
