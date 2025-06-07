#include "inline-script-helpers.hpp"
#include "log-helper.hpp"
#include <obs-module.h>
#include "obs-module-helper.hpp"

#include <obs.hpp>

#include <QDir>
#include <QFileInfo>
#include <QLibrary>

#include <string_view>

namespace advss {

static const char *libName =
#if defined(WIN32)
	"obs-scripting.dll";
#elif __APPLE__
	"obs-scripting.dylib";
#else
	"obs-scripting.so";
#endif

typedef obs_script_t *(*obs_script_create_t)(const char *, obs_data_t *);
typedef void (*obs_script_destroy_t)(obs_script_t *);

obs_script_create_t obs_script_create = nullptr;
obs_script_destroy_t obs_script_destroy = nullptr;

static constexpr std::string_view signalName = "advss_run_temp_script";

static bool setup()
{
	QLibrary scriptingLib(libName);

	obs_script_create =
		(obs_script_create_t)scriptingLib.resolve("obs_script_create");
	if (!obs_script_create) {
		blog(LOG_WARNING,
		     "could not resolve obs_script_create symbol!");
	}

	obs_script_destroy = (obs_script_destroy_t)scriptingLib.resolve(
		"obs_script_destroy");
	if (!obs_script_destroy) {
		blog(LOG_WARNING,
		     "could not resolve obs_script_destroy symbol!");
	}

	auto sh = obs_get_signal_handler();
	auto signalDecl = std::string("void ") + signalName.data() + "(int id)";
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

static obs_script_t *createOBSScript(const char *path, obs_data_t *settings)
{
	if (!obs_script_create) {
		return nullptr;
	}
	return obs_script_create(path, settings);
}

static void destroyOBSScript(obs_script_t *script)
{
	if (!obs_script_destroy) {
		return;
	}
	obs_script_destroy(script);
}

static std::optional<std::string> createScriptTempFile(const char *content,
						       obs_script_lang language)
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
		     "could not create script file! (obs_module_config_path)");
		return {};
	}

	const QFileInfo fileInfo(settingsFile);
	const QString dirPath = fileInfo.absolutePath();
	const QDir dir(dirPath);
	if (!dir.exists() && !dir.mkpath(dirPath)) {
		blog(LOG_WARNING, "could not create script file! (mkpath)");
		bfree(settingsFile);
		return {};
	}

	QFile file(settingsFile);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		bfree(settingsFile);
		return {};
	}

	auto out = QTextStream(&file);
	out << content;
	// Can't use settingsFile here as LUA will complain if Windows style
	// paths (C:\some\path) are used.
	// QFileInfo::absoluteFilePath will convert those paths so LUA won't
	// complain. (C:/some/path)
	std::string path = fileInfo.absoluteFilePath().toStdString();
	bfree(settingsFile);
	return path;
}

std::atomic_uint64_t OBSScript::_instanceIdCounter = 0;

OBSScript::OBSScript() : _instanceId(_instanceIdCounter++)
{
	Setup();
}

OBSScript::OBSScript(const OBSScript &other)
	: _language(other._language),
	  _textPython(other._textPython),
	  _textLUA(other._textLUA),
	  _instanceId(_instanceIdCounter++)
{
	Setup();
}

void OBSScript::Save(obs_data_t *data) const
{
	OBSDataAutoRelease obj = obs_data_create();
	obs_data_set_int(obj, "language", _language);
	_textPython.Save(obj, "scriptPython");
	_textLUA.Save(obj, "scriptLUA");
	obs_data_set_obj(data, "script", obj);
}

void OBSScript::Load(obs_data_t *data)
{
	OBSDataAutoRelease obj = obs_data_get_obj(data, "script");
	_language =
		static_cast<obs_script_lang>(obs_data_get_int(obj, "language"));
	_textPython.Load(obj, "scriptPython");
	_textLUA.Load(obj, "scriptLUA");
	Setup();
}

void OBSScript::SetLanguage(obs_script_lang language)
{
	_language = language;
	Setup();
}

void OBSScript::SetText(const std::string &text)
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

const StringVariable &OBSScript::GetText() const
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

bool OBSScript::Run()
{
	static auto sh = obs_get_signal_handler();

	if (_lastResolvedText != std::string(GetText())) {
		Setup();
	}

	auto cd = calldata_create();
	calldata_set_int(cd, "id", _instanceId);
	signal_handler_signal(sh, signalName.data(), cd);
	calldata_destroy(cd);
	return true;
}

void OBSScript::ResolveVariablesToFixedValues()
{
	_textPython.ResolveVariables();
	_textLUA.ResolveVariables();
}

static std::string preprocessScriptText(const std::string &text,
					obs_script_lang language, uint64_t id)
{
	const std::string footerPython =
		std::string("\n\n"
			    "def script_load(settings):\n"
			    "    def run_wrapper(data):\n"
			    "        id = obs.calldata_int(data, \"id\")\n"
			    "        if id == ") +
		std::to_string(id) +
		":\n"
		"            run()\n"
		"    sh = obs.obs_get_signal_handler()\n"
		"    obs.signal_handler_connect(sh, \"" +
		signalName.data() + "\", run_wrapper)\n\n";

	static const std::string footerLUA =
		std::string(
			"\n\n"
			"function script_load(settings)\n"
			"    local run_wrapper = (function(data)\n"
			"        local id = obs.calldata_int(data, \"id\")\n"
			"        if id == ") +
		std::to_string(id) +
		" then\n"
		"            run()\n"
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

void OBSScript::Setup()
{
	const StringVariable &text =
		_language == OBS_SCRIPT_LANG_PYTHON ? _textPython : _textLUA;
	const auto scriptText =
		preprocessScriptText(text, _language, _instanceId);

	auto path = createScriptTempFile(scriptText.c_str(), _language);
	if (!path) {
		_lastResolvedText = "";
		return;
	}
	_script = std::unique_ptr<obs_script_t, ScriptDeleter>(
		createOBSScript(path->c_str(), nullptr), {*path});
	_lastResolvedText = text;
}

void OBSScript::ScriptDeleter::operator()(obs_script_t *script)
{
	destroyOBSScript(script);
	if (!path.empty()) {
		cleanupScriptFile(path);
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
