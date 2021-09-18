#pragma once
#include "macro.hpp"
#include "file-selection.hpp"

#include <QWidget>
#include <QComboBox>
#include <QDateTime>
#include <QLineEdit>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QCheckBox>

enum class FileType {
	LOCAL,
	REMOTE,
};

class MacroConditionFile : public MacroCondition {
public:
	bool CheckCondition();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetShortDesc();
	std::string GetId() { return id; };
	static std::shared_ptr<MacroCondition> Create()
	{
		return std::make_shared<MacroConditionFile>();
	}

	std::string _file = obs_module_text("AdvSceneSwitcher.enterPath");
	std::string _text = obs_module_text("AdvSceneSwitcher.enterText");
	FileType _fileType = FileType::LOCAL;
	bool _useRegex = false;
	bool _useTime = false;
	bool _onlyMatchIfChanged = false;

private:
	bool matchFileContent(QString &filedata);
	bool checkRemoteFileContent();
	bool checkLocalFileContent();

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
	void PathChanged(const QString &text);
	void MatchTextChanged();
	void UseRegexChanged(int state);
	void CheckModificationDateChanged(int state);
	void OnlyMatchIfChangedChanged(int state);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	QComboBox *_fileType;
	FileSelection *_filePath;
	QPlainTextEdit *_matchText;
	QCheckBox *_useRegex;
	QCheckBox *_checkModificationDate;
	QCheckBox *_checkFileContent;
	std::shared_ptr<MacroConditionFile> _entryData;

private:
	bool _loading = true;
};
