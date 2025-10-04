#include "macro-dock-window.hpp"
#include "log-helper.hpp"
#include "plugin-state-helpers.hpp"

#include <obs-frontend-api.h>

#include <QDockWidget>
#include <QLayout>
#include <mutex>

namespace advss {

static std::unordered_map<std::string, MacroDockWindow *> windows;
static std::mutex mutex;

static bool setup()
{
	AddSaveStep([](obs_data_t *data) {});
	AddLoadStep([](obs_data_t *data) {});
	AddPostLoadStep([]() {});
}

static bool setupDone = setup();

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
}

void MacroDockWindow::RemoveMacroDock(QWidget *widget)
{
	auto docks = _window->findChildren<QDockWidget *>();
	for (const auto dock : docks) {
		if (dock->widget() == widget) {
			_window->removeDockWidget(dock);
			dock->deleteLater();
			break;
		}
	}

	if (OBSIsShuttingDown()) {
		return;
	}

	if (!_window->findChildren<QDockWidget *>().isEmpty()) {
		return;
	}

	// Clean up empty dock windows
	std::lock_guard<std::mutex> lock(mutex);
	auto it = windows.find(_name);
	if (it != windows.end()) {
		windows.erase(it);
	}

	const auto id = "advss-dock-window-" + _name;
	obs_frontend_remove_dock(id.c_str());
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
