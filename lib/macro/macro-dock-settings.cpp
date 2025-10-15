#include "macro-dock-settings.hpp"
#include "macro-dock.hpp"
#include "macro-dock-window.hpp"
#include "macro.hpp"
#include "plugin-state-helpers.hpp"

#include <obs-frontend-api.h>
#include <util/platform.h>

#if LIBOBS_API_VER < MAKE_SEMANTIC_VERSION(30, 0, 0)
#include <QDockWidget>

namespace {

struct DockMapEntry {
	QAction *action = nullptr;
	QWidget *dock = nullptr;
};

} // namespace

static std::unordered_map<const char *, DockMapEntry> dockIds;
static std::mutex dockMutex;

static bool obs_frontend_add_dock_by_id(const char *id, const char *title,
					QWidget *widget)
{
	std::lock_guard<std::mutex> lock(dockMutex);
	if (dockIds.count(id) > 0) {
		return false;
	}

	widget->setObjectName(id);

	auto dock = new QDockWidget();
	dock->setWindowTitle(title);
	dock->setWidget(widget);
	dock->setFloating(true);
	dock->setVisible(false);
	dock->setFeatures(QDockWidget::DockWidgetClosable |
			  QDockWidget::DockWidgetMovable |
			  QDockWidget::DockWidgetFloatable);

	auto action = static_cast<QAction *>(obs_frontend_add_dock(dock));
	if (!action) {
		return false;
	}
	dockIds[id] = {action, dock};
	return true;
}

static void obs_frontend_remove_dock(const char *id)
{
	std::lock_guard<std::mutex> lock(dockMutex);
	auto it = dockIds.find(id);
	if (it == dockIds.end()) {
		return;
	}
	it->second.action->deleteLater();
	it->second.dock->deleteLater();
	dockIds.erase(it);
}

#endif

namespace advss {

MacroDockSettings::MacroDockSettings(Macro *macro) : _macro(macro) {}

MacroDockSettings::~MacroDockSettings()
{
	// Keep the dock widgets in case of shutdown so they can be restored by
	// OBS on startup
	if (!OBSIsShuttingDown()) {
		RemoveDock();
	}
}

void MacroDockSettings::Save(obs_data_t *obj, bool saveForCopy) const
{
	OBSDataAutoRelease dockSettings = obs_data_create();
	obs_data_set_bool(dockSettings, "register", _registerDock);
	obs_data_set_bool(dockSettings, "standaloneDock", _standaloneDock);
	obs_data_set_string(dockSettings, "dockWindow", _dockWindow.c_str());
	obs_data_set_bool(dockSettings, "hasRunButton", _hasRunButton);
	obs_data_set_bool(dockSettings, "hasPauseButton", _hasPauseButton);
	obs_data_set_bool(dockSettings, "hasStatusLabel", _hasStatusLabel);
	obs_data_set_bool(dockSettings, "highlightIfConditionsTrue",
			  _highlight);
	_runButtonText.Save(dockSettings, "runButtonText");
	_pauseButtonText.Save(dockSettings, "pauseButtonText");
	_unpauseButtonText.Save(dockSettings, "unpauseButtonText");
	_conditionsTrueStatusText.Save(dockSettings,
				       "conditionsTrueStatusText");
	_conditionsFalseStatusText.Save(dockSettings,
					"conditionsFalseStatusText");
	if (saveForCopy) {
		auto uuid = GenerateId();
		obs_data_set_string(dockSettings, "dockId", uuid.c_str());

	} else {
		obs_data_set_string(dockSettings, "dockId", _id.c_str());
	}
	obs_data_set_int(dockSettings, "version", 1);
	obs_data_set_obj(obj, "dockSettings", dockSettings);
}

void MacroDockSettings::Load(obs_data_t *obj)
{
	OBSDataAutoRelease dockSettings = obs_data_get_obj(obj, "dockSettings");
	if (!dockSettings) {
		// TODO: Remove this fallback
		_hasRunButton = obs_data_get_bool(obj, "dockHasRunButton");
		_hasPauseButton = obs_data_get_bool(obj, "dockHasPauseButton");
		_registerDock = obs_data_get_bool(obj, "registerDock");
		ResetDockIfEnabled();
		return;
	}

	_macroName = _macro->Name();
	if (!obs_data_has_user_value(dockSettings, "version")) {
		assert(_macro);
		_id = std::string("ADVSS-") + _macroName;
	} else {
		_id = obs_data_get_string(dockSettings, "dockId");
	}

	_registerDock = obs_data_get_bool(dockSettings, "register");

	// TODO: remove these default settings in a future version
	obs_data_set_default_bool(dockSettings, "standaloneDock", true);
	obs_data_set_default_string(dockSettings, "dockWindow", "Dock");
	obs_data_set_default_string(
		dockSettings, "runButtonText",
		obs_module_text("AdvSceneSwitcher.macroDock.run"));
	obs_data_set_default_string(
		dockSettings, "pauseButtonText",
		obs_module_text("AdvSceneSwitcher.macroDock.pause"));
	obs_data_set_default_string(
		dockSettings, "unpauseButtonText",
		obs_module_text("AdvSceneSwitcher.macroDock.unpause"));
	_standaloneDock = obs_data_get_bool(dockSettings, "standaloneDock");
	_dockWindow = obs_data_get_string(dockSettings, "dockWindow");
	_runButtonText.Load(dockSettings, "runButtonText");
	_pauseButtonText.Load(dockSettings, "pauseButtonText");
	_unpauseButtonText.Load(dockSettings, "unpauseButtonText");
	_conditionsTrueStatusText.Load(dockSettings,
				       "conditionsTrueStatusText");
	_conditionsFalseStatusText.Load(dockSettings,
					"conditionsFalseStatusText");
	if (_registerDock) {
		_hasRunButton = obs_data_get_bool(dockSettings, "hasRunButton");
		_hasPauseButton =
			obs_data_get_bool(dockSettings, "hasPauseButton");
		_hasStatusLabel =
			obs_data_get_bool(dockSettings, "hasStatusLabel");
		_highlight = obs_data_get_bool(dockSettings,
					       "highlightIfConditionsTrue");
	}

	ResetDockIfEnabled();
}

void MacroDockSettings::EnableDock(bool enable)
{
	// Only apply "on change" to avoid recreation of the dock widget
	if (_registerDock == enable) {
		return;
	}

	RemoveDock();

	if (!enable) {
		_registerDock = enable;
		return;
	}

	assert(_macro);
	_macroName = _macro->Name();
	_dock = new MacroDock(GetWeakMacroByName(_macroName.c_str()),
			      _runButtonText, _pauseButtonText,
			      _unpauseButtonText, _conditionsTrueStatusText,
			      _conditionsFalseStatusText, _highlight);

	if (!_standaloneDock) {
		auto window = GetDockWindowByName(_dockWindow);
		if (!window) {
			return;
		}
		window->AddMacroDock(_dock, QString::fromStdString(_macroName));
		_registerDock = enable;
		return;
	}

	if (!obs_frontend_add_dock_by_id(_id.c_str(), _macroName.c_str(),
					 _dock)) {
		blog(LOG_INFO, "failed to add macro dock for macro %s",
		     _macroName.c_str());
		_dock->deleteLater();
		_dock = nullptr;
		_registerDock = false;
		return;
	}

	_registerDock = enable;
}

void MacroDockSettings::SetIsStandaloneDock(bool value)
{
	if (_standaloneDock == value) {
		return;
	}

	RemoveDock();
	_standaloneDock = value;
	ResetDockIfEnabled();
}

void MacroDockSettings::SetDockWindowName(const std::string &name)
{
	if (_dockWindow == name) {
		return;
	}

	RemoveDock();
	_dockWindow = name;
	ResetDockIfEnabled();
}

void MacroDockSettings::SetHasRunButton(bool value)
{
	_hasRunButton = value;
	if (!_dock) {
		return;
	}

	_dock->ShowRunButton(value);
}

void MacroDockSettings::SetHasPauseButton(bool value)
{
	_hasPauseButton = value;
	if (!_dock) {
		return;
	}

	_dock->ShowPauseButton(value);
}

void MacroDockSettings::SetHasStatusLabel(bool value)
{
	_hasStatusLabel = value;
	if (!_dock) {
		return;
	}

	_dock->ShowStatusLabel(value);
}

void MacroDockSettings::SetHighlightEnable(bool value)
{
	_highlight = value;
	if (!_dock) {
		return;
	}

	_dock->EnableHighlight(value);
}

void MacroDockSettings::SetRunButtonText(const std::string &text)
{
	_runButtonText = text;
	if (!_dock) {
		return;
	}

	_dock->SetRunButtonText(text);
}

void MacroDockSettings::SetPauseButtonText(const std::string &text)
{
	_pauseButtonText = text;
	if (!_dock) {
		return;
	}

	_dock->SetPauseButtonText(text);
}

void MacroDockSettings::SetUnpauseButtonText(const std::string &text)
{
	_unpauseButtonText = text;
	if (!_dock) {
		return;
	}

	_dock->SetUnpauseButtonText(text);
}

void MacroDockSettings::SetConditionsTrueStatusText(const std::string &text)
{
	_conditionsTrueStatusText = text;
	if (!_dock) {
		return;
	}

	_dock->SetConditionsTrueText(text);
}

StringVariable MacroDockSettings::ConditionsTrueStatusText() const
{
	return _conditionsTrueStatusText;
}

void MacroDockSettings::SetConditionsFalseStatusText(const std::string &text)
{
	_conditionsFalseStatusText = text;
	if (!_dock) {
		return;
	}

	_dock->SetConditionsFalseText(text);
}

StringVariable MacroDockSettings::ConditionsFalseStatusText() const
{
	return _conditionsFalseStatusText;
}

void MacroDockSettings::HandleMacroNameChange()
{
	const auto newName = _macro->Name();

	if (!_standaloneDock) {
		auto window = GetDockWindowByName(_dockWindow);
		if (!window) {
			return;
		}

		window->RenameMacro(_macroName, newName);
		_macroName = newName;
		return;
	}

	if (_macroName != newName) {
		RemoveDock();
		_id = GenerateId();
		_macroName = newName;
	}

	ResetDockIfEnabled();
}

void MacroDockSettings::ResetDockIfEnabled()
{
	if (_registerDock) {
		_registerDock = false;
		EnableDock(true);
	}
}

void MacroDockSettings::RemoveDock()
{
	if (_standaloneDock) {
		obs_frontend_remove_dock(_id.c_str());
		_dock = nullptr;
		return;
	}

	auto window = GetDockWindowByName(_dockWindow);
	if (window) {
		window->RemoveMacroDock(_dock);
	}

	if (_dock) {
		_dock = nullptr;
	}
}

std::string MacroDockSettings::GenerateId()
{
#if LIBOBS_API_VER > MAKE_SEMANTIC_VERSION(30, 0, 0)
	auto uuid = os_generate_uuid();
	auto id = std::string("advss-macro-dock-") + std::string(uuid);
	bfree(uuid);
	return id;

#else
	static std::atomic_int16_t idCounter = 0;
	return std::to_string(++idCounter);
#endif
}

} // namespace advss
