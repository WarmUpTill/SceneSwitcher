#pragma once
#include "macro-action-edit.hpp"
#include "inline-script-helpers.hpp"
#include "variable-text-edit.hpp"

namespace advss {

class MacroActionScriptInline : public MacroAction {
public:
	MacroActionScriptInline(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetId() const { return _id; };
	static std::shared_ptr<MacroAction> Create(Macro *m);
	std::shared_ptr<MacroAction> Copy() const;
	void ResolveVariablesToFixedValues();

	obs_script_lang GetLanguage() const { return _script.GetLanguage(); }
	void SetLanguage(obs_script_lang);
	StringVariable GetScript() const { return _script.GetText(); }
	void SetScript(const QString &);

private:
	void SetupScript();
	void WaitForCompletion() const;

	OBSScript _script;

	static bool _registered;
	static const std::string _id;
};

class MacroActionScriptInlineEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionScriptInlineEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionScriptInline> entryData = nullptr);
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action);

private slots:
	void LanguageChanged(int);
	void ScriptChanged();

private:
	void PopulateWidgets();
	void SetupWidgetConnections();
	void SetupLayout();
	void SetWidgetVisibility();

	QComboBox *_language;
	ScriptEditor *_script;

	std::shared_ptr<MacroActionScriptInline> _entryData;
	bool _loading = true;
};

} // namespace advss
