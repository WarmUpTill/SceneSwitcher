#include "inline-script.hpp"
#include "log-helper.hpp"
#include "obs-module-helper.hpp"

#include <obs-module.h>
#include <obs.hpp>

#include <QDir>
#include <QFileInfo>

namespace advss {

static constexpr std::string_view signalName = "advss_run_temp_script";

std::atomic_uint64_t InlineScript::_instanceIdCounter = 0;
const std::string_view InlineScript::_defaultPythonScript =
	"import obspython as obs\n"
	"\n"
	"def run():\n"
	"    obs.script_log(obs.LOG_WARNING, \"Hello from Python!\")\n"
	"    return True\n";
const std::string_view InlineScript::_defaultLUAScript =
	"obs = obslua\n"
	"\n"
	"function run()\n"
	"    obs.script_log(obs.LOG_WARNING, \"Hello from LUA!\")\n"
	"    return true\n"
	"end";
;

static bool setup()
{
	auto sh = obs_get_signal_handler();
	auto signalDecl =
		std::string("void ") + signalName.data() + "(string id)";
	signal_handler_add(sh, signalDecl.c_str());

	return true;
}
static bool setupDone = setup();

static void cleanupScriptFile(const std::string &path)
{
	const QFileInfo fileInfo(QString::fromStdString(path));
	if (!fileInfo.isFile()) {
		return;
	}
	QFile file(fileInfo.absoluteFilePath());
	if (!file.remove()) {
		vblog(LOG_INFO, "failed to clean up script file %s",
		      fileInfo.absoluteFilePath().toStdString().c_str());
	}
}

static std::optional<std::string>
getScriptTempFilePath(obs_script_lang language)
{
	static int counter = 0;
	++counter;
	static const QString filenamePattern =
		"scripting/advss-tmp-script%1.%2";
	const QString filename = filenamePattern.arg(counter).arg(
		language == OBS_SCRIPT_LANG_PYTHON ? "py" : "lua");
	auto settingsFile =
		obs_module_config_path(filename.toStdString().c_str());

	if (!settingsFile) {
		blog(LOG_WARNING,
		     "could not create temp script file! (obs_module_config_path)");
		return {};
	}

	std::string path = settingsFile;
	bfree(settingsFile);
	return path;
}

static bool createScriptFile(const char *settingsFile, const char *content)
{
	const QFileInfo fileInfo(settingsFile);
	const QString dirPath = fileInfo.absolutePath();
	const QDir dir(dirPath);
	if (!dir.exists() && !dir.mkpath(dirPath)) {
		blog(LOG_WARNING, "could not create script file! (mkpath)");
		return false;
	}

	QFile file(settingsFile);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		return false;
	}

	auto out = QTextStream(&file);
	out << content;

	return true;
}

InlineScript::InlineScript() : _instanceId(_instanceIdCounter++)
{
	Setup();
}

InlineScript::InlineScript(const InlineScript &other)
	: _language(other._language),
	  _textPython(other._textPython),
	  _textLUA(other._textLUA),
	  _instanceId(_instanceIdCounter++)
{
	Setup();
}

void InlineScript::Save(obs_data_t *data) const
{
	OBSDataAutoRelease obj = obs_data_create();
	obs_data_set_int(obj, "type", _type);
	obs_data_set_int(obj, "language", _language);
	_textPython.Save(obj, "scriptPython");
	_textLUA.Save(obj, "scriptLUA");
	obs_data_set_string(obj, "file", _file.c_str());
	obs_data_set_obj(data, "script", obj);
}

void InlineScript::Load(obs_data_t *data)
{
	OBSDataAutoRelease obj = obs_data_get_obj(data, "script");
	_type = static_cast<Type>(obs_data_get_int(obj, "type"));
	_language =
		static_cast<obs_script_lang>(obs_data_get_int(obj, "language"));
	_textPython.Load(obj, "scriptPython");
	_textLUA.Load(obj, "scriptLUA");
	_file = obs_data_get_string(obj, "file");
	Setup();
}

void InlineScript::SetType(Type type)
{
	_type = type;
	Setup();
}

void InlineScript::SetLanguage(obs_script_lang language)
{
	_language = language;
	Setup();
}

void InlineScript::SetText(const std::string &text)
{
	switch (_language) {
	case OBS_SCRIPT_LANG_UNKNOWN:
		break;
	case OBS_SCRIPT_LANG_LUA:
		_textLUA = text;
		break;
	case OBS_SCRIPT_LANG_PYTHON:
		_textPython = text;
		break;
	default:
		break;
	}
	Setup();
}

const StringVariable &InlineScript::GetText() const
{
	static const StringVariable defaultRet;

	switch (_language) {
	case OBS_SCRIPT_LANG_UNKNOWN:
		break;
	case OBS_SCRIPT_LANG_LUA:
		return _textLUA;
	case OBS_SCRIPT_LANG_PYTHON:
		return _textPython;
	default:
		break;
	}
	return defaultRet;
}

void InlineScript::SetPath(const std::string &path)
{
	_file = path;
	Setup();
}

bool InlineScript::Run()
{
	static auto sh = obs_get_signal_handler();

	if (_type == INLINE && _lastResolvedText != std::string(GetText())) {
		Setup();
	}

	if (_type == FILE && _lastPath != _file) {
		Setup();
	}

	auto cd = calldata_create();
	calldata_set_string(cd, "id", GetID().c_str());
	signal_handler_signal(sh, signalName.data(), cd);
	bool result = calldata_bool(cd, "result");
	calldata_destroy(cd);
	return result;
}

void InlineScript::ResolveVariablesToFixedValues()
{
	_textPython.ResolveVariables();
	_textLUA.ResolveVariables();
}

static std::string preprocessScriptText(const std::string &text,
					obs_script_lang language,
					const std::string &id)
{
	const std::string footerPython =
		std::string("\n\n"
			    "## AUTO GENERATED ##\n"
			    "def script_load(settings):\n"
			    "    def run_wrapper(data):\n"
			    "        id = obs.calldata_string(data, \"id\")\n"
			    "        if id == \"") +
		id +
		"\":\n"
		"            ret = run()\n"
		"            obs.calldata_set_bool(data, \"result\", ret)\n"
		"    sh = obs.obs_get_signal_handler()\n"
		"    obs.signal_handler_connect(sh, \"" +
		signalName.data() + "\", run_wrapper)\n\n";

	const std::string footerLUA =
		std::string(
			"\n\n"
			"-- AUTO GENERATED --\n"
			"function script_load(settings)\n"
			"    local run_wrapper = (function(data)\n"
			"        local id = obs.calldata_string(data, \"id\")\n"
			"        if id == \"") +
		id +
		"\" then\n"
		"            local ret = run()\n"
		"            obs.calldata_set_bool(data, \"result\", ret)\n"
		"        end\n"
		"    end)\n"
		"    local sh = obs.obs_get_signal_handler()\n"
		"    obs.signal_handler_connect(sh, \"" +
		signalName.data() +
		"\" , run_wrapper)\n"
		"end\n";

	std::string scriptText =
		language == OBS_SCRIPT_LANG_PYTHON ? footerPython : footerLUA;
	return text + scriptText;
}

void InlineScript::SetupFile()
{
	const auto path = GetLUACompatiblePath(_file);
	_fileId = path;

	if (path.empty()) {
		return;
	}

	if (!QFileInfo(QString::fromStdString(path)).exists()) {
		const auto text = preprocessScriptText(
			_language == OBS_SCRIPT_LANG_PYTHON
				? _defaultPythonScript.data()
				: _defaultLUAScript.data(),
			_language, GetID());
		(void)createScriptFile(_file.c_str(), text.c_str());
	}

	_script = std::unique_ptr<obs_script_t, ScriptDeleter>(
		CreateOBSScript(path.c_str(), nullptr), {});
	_lastPath = _file;
}

void InlineScript::SetupInline()
{
	const StringVariable &text =
		_language == OBS_SCRIPT_LANG_PYTHON ? _textPython : _textLUA;
	const auto scriptText = preprocessScriptText(text, _language, GetID());

	auto path_ = getScriptTempFilePath(_language);
	if (!path_) {
		return;
	}

	auto path = GetLUACompatiblePath(*path_);
	if (!createScriptFile(path.c_str(), scriptText.c_str())) {
		return;
	}

	_script = std::unique_ptr<obs_script_t, ScriptDeleter>(
		CreateOBSScript(path.c_str(), nullptr), {path});
	_lastResolvedText = text;
}

void InlineScript::Setup()
{
	_script.reset();
	_lastResolvedText = "";
	_lastPath = "";

	if (_type == FILE) {
		SetupFile();
	} else {
		SetupInline();
	}
}

std::string InlineScript::GetID() const
{
	if (_type == FILE) {
		return _fileId;
	}
	return std::to_string(_instanceId);
}

void InlineScript::ScriptDeleter::operator()(obs_script_t *script)
{
	DestroyOBSScript(script);
	if (!tempScriptPath.empty()) {
		cleanupScriptFile(tempScriptPath);
	}
}

ScriptEditor::ScriptEditor(QWidget *parent) : VariableTextEdit(parent, 15, 5)
{
	installEventFilter(this);
}

bool ScriptEditor::eventFilter(QObject *obj, QEvent *event)
{
	if (event->type() == QEvent::FocusOut) {
		emit ScriptChanged();
	}
	return QObject::eventFilter(obj, event);
}

} // namespace advss
