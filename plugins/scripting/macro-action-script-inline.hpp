#pragma once
#include "macro-action-edit.hpp"
#include "variable-text-edit.hpp"

struct obs_script;
typedef struct obs_script obs_script_t;
enum obs_script_lang {
	OBS_SCRIPT_LANG_UNKNOWN,
	OBS_SCRIPT_LANG_LUA,
	OBS_SCRIPT_LANG_PYTHON
};

namespace advss {

class MacroActionScriptInline : public MacroAction {
public:
	MacroActionScriptInline(Macro *m) : MacroAction(m) {}
	MacroActionScriptInline(const MacroActionScriptInline &);
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetId() const { return _id; };
	static std::shared_ptr<MacroAction> Create(Macro *m);
	std::shared_ptr<MacroAction> Copy() const;
	void ResolveVariablesToFixedValues();

	obs_script_lang GetLanguage() const { return _language; }
	void SetLanguage(obs_script_lang);
	StringVariable GetScript() const { return _scriptText; }
	void SetScript(const QString &);

private:
	void SetupScript();
	static void CleanupScriptFile(const std::string &);
	void WaitForCompletion() const;

	obs_script_lang _language = OBS_SCRIPT_LANG_PYTHON;
	StringVariable _scriptText =
		"import obspython as obs\n"
		"\n"
		"def script_load(settings):\n"
		"    obs.script_log(obs.LOG_INFO, \"Hello from inline script!\")";

	struct ScriptDeleter {
		void operator()(obs_script_t *script)
		{
			obs_script_destroy(script);
			if (!path.empty()) {
				CleanupScriptFile(path);
			}
		}
		std::string path;
	};
	std::unique_ptr<obs_script_t, ScriptDeleter> _script;

	std::string _lastResolvedText;

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
	VariableTextEdit *_script;

	std::shared_ptr<MacroActionScriptInline> _entryData;
	bool _loading = true;
};

} // namespace advss
