#pragma once
#include "obs-script-helpers.hpp"
#include "variable-text-edit.hpp"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <obs-data.h>

namespace advss {

class InlineScript {
public:
	InlineScript();
	InlineScript(const InlineScript &);

	enum Type { INLINE, FILE };

	void Save(obs_data_t *) const;
	void Load(obs_data_t *);

	void SetType(Type);
	Type GetType() const { return _type; }
	void SetLanguage(obs_script_lang);
	obs_script_lang GetLanguage() const { return _language; }
	void SetText(const std::string &);
	const StringVariable &GetText() const;
	void SetPath(const std::string &);
	const std::string &GetPath() const { return _file; }

	bool Run();

	void ResolveVariablesToFixedValues();

private:
	void Setup();
	void SetupFile();
	void SetupInline();
	std::string GetID() const;

	Type _type = INLINE;
	obs_script_lang _language = OBS_SCRIPT_LANG_PYTHON;
	std::string _file;
	StringVariable _textPython = _defaultPythonScript.data();
	StringVariable _textLUA = _defaultLUAScript.data();
	std::string _lastResolvedText;
	std::string _lastPath;
	std::string _fileId;
	const uint64_t _instanceId;

	struct ScriptDeleter {
		void operator()(obs_script_t *);
		std::string tempScriptPath;
	};
	std::unique_ptr<obs_script_t, ScriptDeleter> _script;

	static std::atomic_uint64_t _instanceIdCounter;
	static const std::string_view _defaultPythonScript;
	static const std::string_view _defaultLUAScript;
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
