#pragma once
#include <QSpinBox>
#include <QPlainTextEdit>
#include "macro-action-edit.hpp"
#include "file-selection.hpp"

enum class FileAction {
	WRITE,
	APPEND,
};

class MacroActionFile : public MacroAction {
public:
	MacroActionFile(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetShortDesc();
	std::string GetId() { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m)
	{
		return std::make_shared<MacroActionFile>(m);
	}

	std::string _file = obs_module_text("AdvSceneSwitcher.enterPath");
	std::string _text = obs_module_text("AdvSceneSwitcher.enterText");
	FileAction _action = FileAction::WRITE;

private:
	static bool _registered;
	static const std::string id;
};

class MacroActionFileEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionFileEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionFile> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionFileEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionFile>(action));
	}

private slots:
	void PathChanged(const QString &text);
	void TextChanged();
	void ActionChanged(int value);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	FileSelection *_filePath;
	QPlainTextEdit *_text;
	QComboBox *_actions;
	std::shared_ptr<MacroActionFile> _entryData;

private:
	bool _loading = true;
};
