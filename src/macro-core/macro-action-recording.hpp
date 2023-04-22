#pragma once
#include "macro-action-edit.hpp"
#include "file-selection.hpp"
#include "variable-line-edit.hpp"

#include <QDir>
#include <QComboBox>
#include <QHBoxLayout>

namespace advss {

class MacroActionRecord : public MacroAction {
public:
	MacroActionRecord(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m)
	{
		return std::make_shared<MacroActionRecord>(m);
	}

	enum class Action {
		STOP,
		START,
		PAUSE,
		UNPAUSE,
		SPLIT,
		FOLDER,
		FILE_FORMAT,
	};
	Action _action = Action::STOP;

	StringVariable _folder = QDir::homePath().toStdString() + "/Videos";
	StringVariable _fileFormat = "%CCYY-%MM-%DD %hh-%mm-%ss";

private:
	static bool _registered;
	static const std::string id;
};

class MacroActionRecordEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionRecordEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionRecord> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionRecordEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionRecord>(action));
	}

private slots:
	void ActionChanged(int value);
	void FolderChanged(const QString &);
	void FormatStringChanged();

protected:
	QComboBox *_actions;
	QLabel *_pauseHint;
	QLabel *_splitHint;
	FileSelection *_recordFolder;
	VariableLineEdit *_recordFileFormat;
	std::shared_ptr<MacroActionRecord> _entryData;

private:
	void SetWidgetVisibility();

	QHBoxLayout *_mainLayout;
	bool _loading = true;
};

} // namespace advss
