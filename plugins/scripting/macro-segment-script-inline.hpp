#pragma once
#include "file-selection.hpp"
#include "inline-script.hpp"
#include "sync-helpers.hpp"
#include "variable-text-edit.hpp"

#include <QLayout>

namespace advss {

class MacroSegmentScriptInline : public Lockable {
public:
	InlineScript::Type GetType() const { return _script.GetType(); }
	void SetType(InlineScript::Type);
	obs_script_lang GetLanguage() const { return _script.GetLanguage(); }
	void SetLanguage(obs_script_lang);
	StringVariable GetScript() const { return _script.GetText(); }
	void SetScript(const std::string &);
	std::string GetPath() const { return _script.GetPath(); }
	void SetPath(const std::string &);

protected:
	InlineScript _script;
};

class MacroSegmentScriptInlineEdit : public QWidget {
	Q_OBJECT

public:
	MacroSegmentScriptInlineEdit(
		QWidget *, std::shared_ptr<MacroSegmentScriptInline> = nullptr);
	virtual ~MacroSegmentScriptInlineEdit() = default;

protected slots:
	void ScriptTypeChanged(int);
	void LanguageChanged(int);
	void ScriptChanged();
	void PathChanged(const QString &);

protected:
	void PopulateWidgets();
	void SetupWidgetConnections();
	void SetupLayout();
	void SetWidgetVisibility();

	QComboBox *_scriptType;
	QComboBox *_language;
	ScriptEditor *_script;
	FileSelection *_path;
	QPushButton *_openFile;
	QHBoxLayout *_fileLayout;

	std::shared_ptr<MacroSegmentScriptInline> _entryData;
	bool _loading = true;
};

} // namespace advss
