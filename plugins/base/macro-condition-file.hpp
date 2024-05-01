#pragma once
#include "macro-condition-edit.hpp"
#include "file-selection.hpp"
#include "variable-text-edit.hpp"
#include "regex-config.hpp"

#include <QWidget>
#include <QComboBox>
#include <QDateTime>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>

namespace advss {

class MacroConditionFile : public MacroCondition {
public:
	MacroConditionFile(Macro *m) : MacroCondition(m, true) {}
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionFile>(m);
	}

	enum class FileType {
		LOCAL,
		REMOTE,
	};

	enum class Condition {
		MATCH,
		CONTENT_CHANGE,
		DATE_CHANGE,
	};
	void SetCondition(Condition condition);
	Condition GetCondition() const { return _condition; }

	StringVariable _file = obs_module_text("AdvSceneSwitcher.enterPath");
	StringVariable _text = obs_module_text("AdvSceneSwitcher.enterText");
	FileType _fileType = FileType::LOCAL;
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
	void SetupTempVars();

	Condition _condition = Condition::MATCH;
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
	void RegexChanged(const RegexConfig &);
	void CheckModificationDateChanged(int state);
	void OnlyMatchIfChangedChanged(int state);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	QComboBox *_fileTypes;
	QComboBox *_conditions;
	FileSelection *_filePath;
	VariableTextEdit *_matchText;
	RegexConfigWidget *_regex;
	QCheckBox *_checkModificationDate;
	QCheckBox *_checkFileContent;
	std::shared_ptr<MacroConditionFile> _entryData;

private:
	void SetWidgetVisibility();

	bool _loading = true;
};

} // namespace advss
