#include "macro-action-plugin-state.hpp"
#include "switcher-data.hpp"
#include "utility.hpp"

#include <thread>
#include <QMainWindow>

using namespace std::chrono_literals;

namespace advss {

const std::string MacroActionPluginState::id = "plugin_state";

bool MacroActionPluginState::_registered = MacroActionFactory::Register(
	MacroActionPluginState::id,
	{MacroActionPluginState::Create, MacroActionPluginStateEdit::Create,
	 "AdvSceneSwitcher.action.pluginState"});

const static std::map<PluginStateAction, std::string> actionTypes = {
	{PluginStateAction::STOP,
	 "AdvSceneSwitcher.action.pluginState.type.stop"},
	{PluginStateAction::NO_MATCH_BEHAVIOUR,
	 "AdvSceneSwitcher.action.pluginState.type.noMatch"},
	{PluginStateAction::IMPORT_SETTINGS,
	 "AdvSceneSwitcher.action.pluginState.type.import"},
	{PluginStateAction::TERMINATE,
	 "AdvSceneSwitcher.action.pluginState.type.terminate"},
};

const static std::map<SwitcherData::NoMatch, std::string> noMatchValues = {
	{SwitcherData::NoMatch::NO_SWITCH,
	 "AdvSceneSwitcher.generalTab.generalBehavior.onNoMet.dontSwitch"},
	{SwitcherData::NoMatch::SWITCH,
	 "AdvSceneSwitcher.generalTab.generalBehavior.onNoMet.switchTo"},
	{SwitcherData::NoMatch::RANDOM_SWITCH,
	 "AdvSceneSwitcher.generalTab.generalBehavior.onNoMet.switchToRandom"},
};

static void stopPlugin()
{
	std::thread t([]() { switcher->Stop(); });
	t.detach();
}

static void importSettings(const std::string &path)
{
	if (switcher->settingsWindowOpened) {
		return;
	}
	obs_data_t *obj = obs_data_create_from_json_file(path.c_str());
	if (!obj) {
		return;
	}
	switcher->LoadSettings(obj);
	obs_data_release(obj);
}

static void setNoMatchBehaviour(int value, OBSWeakSource &scene)
{
	switcher->switchIfNotMatching =
		static_cast<SwitcherData::NoMatch>(value);
	if (switcher->switchIfNotMatching == SwitcherData::NoMatch::SWITCH) {
		switcher->nonMatchingScene = scene;
	}
}

static void closeOBSWindow()
{
	blog(LOG_WARNING, "closing OBS window now!");
	auto obsWindow =
		static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (obsWindow) {
		obsWindow->close();
	} else {
		blog(LOG_WARNING,
		     "OBS shutdown was aborted - failed to get QMainWindow");
	}
}

static void terminateOBS(void *)
{
	static std::mutex mtx;
	static std::mutex waitMutex;
	static std::condition_variable cv;
	static bool abortTerminate;
	static bool stopWaiting;
	static std::chrono::high_resolution_clock::time_point
		lastShutdownAttempt{};

	// Don't allow multiple simultaneous instances of the shutdown dialog
	std::unique_lock<std::mutex> lock(mtx, std::try_to_lock);
	if (!lock.owns_lock()) {
		blog(LOG_INFO,
		     "OBS shutdown dialog already triggered - ignoring additional request");
		return;
	}

	// Prevent the user from locking himself out of controlling OBS by
	// continuously opening the shutdown dialog
	auto now = std::chrono::high_resolution_clock::now();
	if (lastShutdownAttempt + 5s > now) {
		blog(LOG_INFO,
		     "OBS shutdown dialog already triggered recently - ignoring request");
		return;
	}
	lastShutdownAttempt = now;

	abortTerminate = false;
	stopWaiting = false;

	// Give the user a 10s grace period during which the shutdown can be aborted
	std::thread thread([]() {
		std::unique_lock<std::mutex> lock(waitMutex);
		if (cv.wait_for(lock, 10s, [] { return stopWaiting; })) {
			if (abortTerminate) {
				blog(LOG_INFO, "OBS shutdown was aborted");
			} else {
				closeOBSWindow();
			}
		} else {
			closeOBSWindow();
		}
	});
	thread.detach();

	// Ask the user how to proceed
	abortTerminate = !DisplayMessage(
		obs_module_text(
			"AdvSceneSwitcher.action.pluginState.terminateConfirm"),
		true, false);
	stopWaiting = true;

	// This closes OBS immediately if shutdown was confirmed
	cv.notify_all();

	lock.unlock();
}

bool MacroActionPluginState::PerformAction()
{
	switch (_action) {
	case PluginStateAction::STOP:
		stopPlugin();
		break;
	case PluginStateAction::NO_MATCH_BEHAVIOUR:
		setNoMatchBehaviour(_value, _scene);
		break;
	case PluginStateAction::IMPORT_SETTINGS:
		importSettings(_settingsPath);
		// There is no point in continuing
		// The settings will be invalid
		return false;
	case PluginStateAction::TERMINATE: {
		std::thread thread([]() {
			obs_queue_task(OBS_TASK_UI, terminateOBS, nullptr,
				       false);
		});
		thread.detach();
		break;
	}
	default:
		break;
	}
	return true;
}

void MacroActionPluginState::LogAction() const
{
	switch (_action) {
	case PluginStateAction::STOP:
		blog(LOG_INFO, "stop() called by macro");
		break;
	case PluginStateAction::NO_MATCH_BEHAVIOUR:
		vblog(LOG_INFO, "setting no match to %d", _value);
		break;
	case PluginStateAction::IMPORT_SETTINGS:
		vblog(LOG_INFO, "importing settings from %s",
		      _settingsPath.c_str());
		break;
	case PluginStateAction::TERMINATE:
		vblog(LOG_INFO, "sending terminate signal to OBS in 10s");
		break;
	default:
		blog(LOG_WARNING, "ignored unknown pluginState action %d",
		     static_cast<int>(_action));
		break;
	}
}

bool MacroActionPluginState::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	obs_data_set_int(obj, "value", _value);
	obs_data_set_string(obj, "scene", GetWeakSourceName(_scene).c_str());
	_settingsPath.Save(obj, "settingsPath");
	return true;
}

bool MacroActionPluginState::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_action =
		static_cast<PluginStateAction>(obs_data_get_int(obj, "action"));
	_value = obs_data_get_int(obj, "value");
	const char *sceneName = obs_data_get_string(obj, "scene");
	_scene = GetWeakSourceByName(sceneName);
	_settingsPath.Load(obj, "settingsPath");
	return true;
}

static inline void populateActionSelection(QComboBox *list)
{
	for (auto entry : actionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

static inline void populateValueSelection(QComboBox *list,
					  PluginStateAction action)
{
	if (action == PluginStateAction::NO_MATCH_BEHAVIOUR) {
		for (auto entry : noMatchValues) {
			list->addItem(obs_module_text(entry.second.c_str()));
		}
	}
}

MacroActionPluginStateEdit::MacroActionPluginStateEdit(
	QWidget *parent, std::shared_ptr<MacroActionPluginState> entryData)
	: QWidget(parent)
{
	_actions = new QComboBox();
	_values = new QComboBox();
	_scenes = new QComboBox();
	_settings = new FileSelection();
	_settingsWarning = new QLabel(obs_module_text(
		"AdvSceneSwitcher.action.pluginState.importWarning"));

	populateActionSelection(_actions);
	PopulateSceneSelection(_scenes);

	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_values, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ValueChanged(int)));
	QWidget::connect(_scenes, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(SceneChanged(const QString &)));
	QWidget::connect(_settings, SIGNAL(PathChanged(const QString &)), this,
			 SLOT(PathChanged(const QString &)));

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{actions}}", _actions},
		{"{{values}}", _values},
		{"{{scenes}}", _scenes},
		{"{{settings}}", _settings},
		{"{{settingsWarning}}", _settingsWarning},
	};
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.action.pluginState.entry"),
		mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionPluginStateEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}
	_actions->setCurrentIndex(static_cast<int>(_entryData->_action));
	populateValueSelection(_values, _entryData->_action);
	_values->setCurrentIndex(_entryData->_value);
	_scenes->setCurrentText(GetWeakSourceName(_entryData->_scene).c_str());
	_settings->SetPath(_entryData->_settingsPath);
	SetWidgetVisibility();
}

void MacroActionPluginStateEdit::ActionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	{
		auto lock = LockContext();
		_entryData->_action = static_cast<PluginStateAction>(value);
		SetWidgetVisibility();
	}

	_values->clear();
	populateValueSelection(_values, _entryData->_action);
}

void MacroActionPluginStateEdit::ValueChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_value = value;
	SetWidgetVisibility();
}

void MacroActionPluginStateEdit::SceneChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_scene = GetWeakSourceByQString(text);
}

void MacroActionPluginStateEdit::PathChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_settingsPath = text.toStdString();
}

void MacroActionPluginStateEdit::SetWidgetVisibility()
{
	if (!_entryData) {
		return;
	}

	_values->hide();
	_scenes->hide();
	_settings->hide();
	_settingsWarning->hide();
	switch (_entryData->_action) {
	case PluginStateAction::STOP:
		break;
	case PluginStateAction::NO_MATCH_BEHAVIOUR:
		_values->show();
		if (static_cast<SwitcherData::NoMatch>(_entryData->_value) ==
		    SwitcherData::NoMatch::SWITCH) {
			_scenes->show();
		}
		break;
	case PluginStateAction::IMPORT_SETTINGS:
		_settings->show();
		_settingsWarning->show();
		break;
	default:
		break;
	}
}

} // namespace advss
