#include "macro-dock-window.hpp"
#include "log-helper.hpp"
#include "plugin-state-helpers.hpp"

#include <obs-frontend-api.h>

#include <QDockWidget>
#include <QLayout>
#include <mutex>

namespace advss {

static std::unordered_map<std::string, MacroDockWindow *> windows;
static std::unordered_map<std::string, QByteArray> windowGeometries;
static std::mutex mutex;

static void saveDocks(obs_data_t *obj)
{
	std::lock_guard<std::mutex> lock(mutex);
	OBSDataAutoRelease data = obs_data_create();
	OBSDataArrayAutoRelease array = obs_data_array_create();
	for (const auto &[name, window] : windows) {
		OBSDataAutoRelease dockData = obs_data_create();
		obs_data_set_string(dockData, "name", name.c_str());
		obs_data_set_string(dockData, "geometry",
				    window->GetWindow()
					    ->saveState()
					    .toBase64()
					    .toStdString()
					    .c_str());
		obs_data_array_push_back(array, dockData);
	}
	obs_data_set_array(data, "docks", array);
	obs_data_set_obj(obj, "dockWindows", data);
}

static void restoreDockGeometry()
{
	std::lock_guard<std::mutex> lock(mutex);
	for (const auto &[name, dock] : windows) {
		const auto it = windowGeometries.find(name);
		if (it == windowGeometries.end()) {
			continue;
		}

		dock->GetWindow()->restoreState(it->second);
	}
}

static void loadDocks(obs_data_t *obj)
{
	std::lock_guard<std::mutex> lock(mutex);
	windowGeometries.clear();
	OBSDataAutoRelease data = obs_data_get_obj(obj, "dockWindows");
	OBSDataArrayAutoRelease array = obs_data_get_array(data, "docks");
	auto size = obs_data_array_count(array);
	for (size_t i = 0; i < size; ++i) {
		OBSDataAutoRelease dockData = obs_data_array_item(array, i);
		const auto name = obs_data_get_string(dockData, "name");
		const auto geometry = QByteArray::fromBase64(
			obs_data_get_string(dockData, "geometry"));
		windowGeometries[name] = geometry;
	}
	AddPostLoadStep(restoreDockGeometry);
}

[[maybe_unused]] static bool _ = []() {
	AddPluginInitStep([]() {
		AddSaveStep(saveDocks);
		AddLoadStep(loadDocks);
	});
	return true;
}();

MacroDockWindow::MacroDockWindow(const std::string &name)
	: QFrame(),
	  _name(name),
	  _window(new QMainWindow())
{
	setFrameShape(QFrame::StyledPanel);
	setFrameShadow(QFrame::Sunken);
	_window->setDockNestingEnabled(true);
	auto layout = new QVBoxLayout;
	layout->addWidget(_window);
	setLayout(layout);
}

QWidget *MacroDockWindow::AddMacroDock(QWidget *widget, const QString &title)
{
	auto dock = new QDockWidget();
	dock->setWindowTitle(title);
	dock->setWidget(widget);
	dock->setVisible(true);
	dock->setFeatures(QDockWidget::DockWidgetMovable);
	dock->setObjectName(title);
	_window->addDockWidget(Qt::RightDockWidgetArea, dock);
	return dock;
}

void MacroDockWindow::RenameMacro(const std::string &oldName,
				  const std::string &newName)
{
	auto docks = _window->findChildren<QDockWidget *>();
	for (const auto dock : docks) {
		if (dock->windowTitle() == QString::fromStdString(oldName)) {
			dock->setWindowTitle(QString::fromStdString(newName));
			break;
		}
	}
}

void MacroDockWindow::RemoveMacroDock(QWidget *widget)
{
	bool removedDock = false;
	auto docks = _window->findChildren<QDockWidget *>();
	for (const auto dock : docks) {
		if (dock->widget() == widget) {
			_window->removeDockWidget(dock);
			dock->deleteLater();
			removedDock = true;
			break;
		}
	}

	if (OBSIsShuttingDown()) {
		return;
	}

	const bool shouldRemoveDockWindow = docks.isEmpty() ||
					    (removedDock && docks.count() == 1);
	if (!shouldRemoveDockWindow) {
		return;
	}

	std::lock_guard<std::mutex> lock(mutex);
	auto it = windows.find(_name);
	if (it != windows.end()) {
		windows.erase(it);
	}

	const auto id = "advss-dock-window-" + _name;
	obs_frontend_remove_dock(id.c_str());
}

QMainWindow *MacroDockWindow::GetWindow() const
{
	return _window;
}

MacroDockWindow *GetDockWindowByName(const std::string &name)
{
	std::lock_guard<std::mutex> lock(mutex);
	auto it = windows.find(name);
	if (it != windows.end()) {
		return it->second;
	}

	auto window = new MacroDockWindow(name);
	const auto id = "advss-dock-window-" + name;
	if (!obs_frontend_add_dock_by_id(id.c_str(), name.c_str(), window)) {
		blog(LOG_INFO, "failed to add macro dock window '%s'",
		     id.c_str());
		return nullptr;
	}

	windows[name] = window;
	return window;
}

} // namespace advss
