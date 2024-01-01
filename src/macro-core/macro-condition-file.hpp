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
#include <QStringList>

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

	enum class ConditionType {
		MATCH,
		CONTENT_CHANGE,
		DATE_CHANGE,
		CHANGES_MATCH,
	};

	enum class ChangeMatchType {
		ANY,
		ADDED,
		REMOVED,
	};

	StringVariable _file = obs_module_text("AdvSceneSwitcher.enterPath");
	StringVariable _text = obs_module_text("AdvSceneSwitcher.enterText");
	FileType _fileType = FileType::LOCAL;
	ChangeMatchType _changeMatchType = ChangeMatchType::ANY;
	ConditionType _condition = ConditionType::MATCH;
	RegexConfig _regex;

private:
	bool CheckFileContentMatch();
	bool CheckChangeContent();
	bool CheckChangeDate();
	bool CheckChangedMatch();

	QDateTime _lastMod;
	size_t _lastHash = 0;
	QStringList _previousContent;
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
	void ChangeMatchTypeChanged(int index);
	void ConditionChanged(int index);
	void PathChanged(const QString &text);
	void MatchTextChanged();
	void RegexChanged(RegexConfig);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	QComboBox *_fileTypes;
	QComboBox *_changeMatchTypes;
	QComboBox *_conditions;
	FileSelection *_filePath;
	VariableTextEdit *_matchText;
	RegexConfigWidget *_regex;
	std::shared_ptr<MacroConditionFile> _entryData;

private:
	void SetWidgetVisibility();

	bool _loading = true;
};

} // namespace advss
