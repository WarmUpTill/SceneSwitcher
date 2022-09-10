#pragma once
#include "macro.hpp"
#include "file-selection.hpp"
#include "resizing-text-edit.hpp"
#include "regex-config.hpp"

#include <QWidget>
#include <QComboBox>
#include <QDateTime>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>

class MacroConditionFile : public MacroCondition {
public:
	MacroConditionFile(Macro *m) : MacroCondition(m) {}
	bool CheckCondition();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetShortDesc();
	std::string GetId() { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionFile>(m);
	}

	enum class FileType {
		LOCAL,
		REMOTE,
	};

	enum class ConditionType {
		MATCH,
		CONTENT_CHANGE,
		DATE_CHANGE,
	};

	std::string _file = obs_module_text("AdvSceneSwitcher.enterPath");
	std::string _text = obs_module_text("AdvSceneSwitcher.enterText");
	FileType _fileType = FileType::LOCAL;
	ConditionType _condition = ConditionType::MATCH;
	RegexConfig _regex;

	// TODO: Remove in future version
	bool _useTime = false;
	bool _onlyMatchIfChanged = false;

private:
	bool MatchFileContent(QString &filedata);
	bool CheckRemoteFileContent();
	bool CheckLocalFileContent();
	bool CheckChangeContent();
	bool CheckChangeDate();

	QDateTime _lastMod;
	size_t _lastHash = 0;
	static bool _registered;
	static const std::string id;
};

class MacroConditionFileEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionFileEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionFile> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionFileEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionFile>(cond));
	}

private slots:
	void FileTypeChanged(int index);
	void ConditionChanged(int index);
	void PathChanged(const QString &text);
	void MatchTextChanged();
	void RegexChanged(RegexConfig);
	void CheckModificationDateChanged(int state);
	void OnlyMatchIfChangedChanged(int state);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	QComboBox *_fileTypes;
	QComboBox *_conditions;
	FileSelection *_filePath;
	ResizingPlainTextEdit *_matchText;
	RegexConfigWidget *_regex;
	QCheckBox *_checkModificationDate;
	QCheckBox *_checkFileContent;
	std::shared_ptr<MacroConditionFile> _entryData;

private:
	void SetWidgetVisibility();

	bool _loading = true;
};
