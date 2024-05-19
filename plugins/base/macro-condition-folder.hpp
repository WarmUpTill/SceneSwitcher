#pragma once
#include "macro-condition-edit.hpp"
#include "file-selection.hpp"
#include "regex-config.hpp"
#include "variable-line-edit.hpp"

#include <QFileSystemWatcher>

namespace advss {

class MacroConditionFolder : public QObject, public MacroCondition {
	Q_OBJECT

public:
	MacroConditionFolder(Macro *m);
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionFolder>(m);
	}
	void SetFolder(const std::string &);
	StringVariable GetFolder() const { return _folder; }

	enum class Condition {
		ANY,
		FILE_ADD,
		FILE_CHANGE,
		FILE_REMOVE,
		FOLDER_ADD,
		FOLDER_REMOVE,
	};

	Condition _condition = Condition::ANY;
	bool _enableFilter = false;
	RegexConfig _regex = RegexConfig(true);
	StringVariable _filter = ".*";

private slots:
	void DirectoryChanged(const QString &);
	void FileChanged(const QString &);

private:
	void SetupWatcher();
	void SetTempVarValues();
	void SetupTempVars();

	StringVariable _folder = obs_module_text("AdvSceneSwitcher.enterPath");

	std::unique_ptr<QFileSystemWatcher> _watcher;
	std::string _lastWatchedValue = "";

	std::mutex _mutex;
	bool _matched = false;
	QSet<QString> _newFiles;
	QSet<QString> _changedFiles;
	QSet<QString> _removedFiles;
	QSet<QString> _newDirs;
	QSet<QString> _removedDirs;
	QSet<QString> _currentFiles;
	QSet<QString> _currentDirs;

	static bool _registered;
	static const std::string id;
};

class MacroConditionFolderEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionFolderEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionFolder> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionFolderEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionFolder>(cond));
	}

private slots:
	void ConditionChanged(int index);
	void PathChanged(const QString &text);
	void EnableFilterChanged(int value);
	void RegexChanged(const RegexConfig &);
	void FilterChanged();
signals:
	void HeaderInfoChanged(const QString &);

private:
	void SetWidgetVisibility();

	QComboBox *_conditions;
	FileSelection *_folder;
	QCheckBox *_enableFilter;
	QHBoxLayout *_filterLayout;
	RegexConfigWidget *_regex;
	VariableLineEdit *_filter;

	std::shared_ptr<MacroConditionFolder> _entryData;
	bool _loading = true;
};

} // namespace advss
