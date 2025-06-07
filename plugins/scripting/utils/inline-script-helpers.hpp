#pragma once
#include "variable-text-edit.hpp"

#include <memory>
#include <optional>
#include <string>

#include <obs-data.h>

// Based on obs-scripting.h

struct obs_script;
typedef struct obs_script obs_script_t;

enum obs_script_lang {
	OBS_SCRIPT_LANG_UNKNOWN,
	OBS_SCRIPT_LANG_LUA,
	OBS_SCRIPT_LANG_PYTHON
};

namespace advss {

class OBSScript {
public:
	OBSScript();
	OBSScript(const OBSScript &);

	void Save(obs_data_t *) const;
	void Load(obs_data_t *);

	void SetLanguage(obs_script_lang);
	obs_script_lang GetLanguage() const { return _language; }
	void SetText(const std::string &);
	const StringVariable &GetText() const;

	bool Run();

	void ResolveVariablesToFixedValues();

private:
	void Setup();

	obs_script_lang _language = OBS_SCRIPT_LANG_PYTHON;
	StringVariable _textPython =
		"import obspython as obs\n"
		"\n"
		"def run():\n"
		"    obs.script_log(obs.LOG_WARNING, \"Hello from inline script!\")\n"
		"    return True\n";
	StringVariable _textLUA =
		"obs = obslua\n"
		"\n"
		"function run()\n"
		"    obs.script_log(obs.LOG_WARNING, \"hello from inline script!\")\n"
		"    return true\n"
		"end";
	std::string _lastResolvedText;

	struct ScriptDeleter {
		void operator()(obs_script_t *);
		std::string path;
	};
	std::unique_ptr<obs_script_t, ScriptDeleter> _script;

	static std::atomic_uint64_t _instanceIdCounter;
	uint64_t _instanceId;
};

class ScriptEditor : public VariableTextEdit {
	Q_OBJECT
public:
	ScriptEditor(QWidget *parent);

signals:
	void ScriptChanged();

private:
	bool eventFilter(QObject *obj, QEvent *event) override;
};

} // namespace advss
