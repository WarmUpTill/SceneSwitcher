#include "tab-helpers.hpp"
#include "log-helper.hpp"
#include "obs-module-helper.hpp"

#include <obs.hpp>
#include <QTabBar>
#include <string>
#include <unordered_map>
#include <vector>

namespace advss {

static std::vector<std::string> tabNames = {
	"generalTab",      "macroTab",      "windowTitleTab",   "executableTab",
	"screenRegionTab", "mediaTab",      "fileTab",          "randomTab",
	"timeTab",         "idleTab",       "sceneSequenceTab", "audioTab",
	"videoTab",        "sceneGroupTab", "transitionsTab",   "pauseTab",
};

static std::vector<int> &getTabOrderVector()
{
	static std::vector<int> tabOrder = std::vector<int>(tabNames.size());
	return tabOrder;
}

namespace {
struct TabCallbacks {
	std::function<QWidget *()> createWidget;
	std::function<void(QTabWidget *)> setupTab;
};
} // namespace

static std::unordered_map<const char *, TabCallbacks> createTabCallbacks;

static int lastOpenedTab = -1;

void SetTabVisibleByName(QTabWidget *tabWidget, bool visible,
			 const QString &name)
{
	for (int idx = 0; idx < tabWidget->count(); idx++) {
		if (tabWidget->tabText(idx) != name) {
			continue;
		}

		tabWidget->setTabVisible(idx, visible);
	}
}

void AddSetupTabCallback(const char *tabName,
			 std::function<QWidget *()> createWidget,
			 std::function<void(QTabWidget *)> setupTab)
{
	if (std::find(tabNames.begin(), tabNames.end(), tabName) !=
	    tabNames.end()) {
		return;
	}
	tabNames.emplace_back(tabName);
	TabCallbacks callbacks = {createWidget, setupTab};
	createTabCallbacks.emplace(tabName, callbacks);
}

void SaveLastOpenedTab(QTabWidget *tabWidget)
{
	lastOpenedTab = tabWidget->currentIndex();
}

void ResetLastOpenedTab()
{
	lastOpenedTab = -1;
}

static bool tabWidgetOrderValid()
{
	auto &tabOrder = getTabOrderVector();

	if (tabNames.size() != tabOrder.size()) {
		return false;
	}

	auto tmp = std::vector<int>(tabNames.size());
	std::iota(tmp.begin(), tmp.end(), 0);

	for (auto &p : tmp) {
		auto it = std::find(tabOrder.begin(), tabOrder.end(), p);
		if (it == tabOrder.end()) {
			return false;
		}
	}
	return true;
}

static void resetTabWidgetOrder()
{
	auto &tabOrder = getTabOrderVector();
	tabOrder = std::vector<int>(tabNames.size());
	std::iota(tabOrder.begin(), tabOrder.end(), 0);
}

void SaveTabOrder(obs_data_t *obj)
{
	auto &tabOrder = getTabOrderVector();

	// Can happen when corrupting settings files
	if (!tabWidgetOrderValid()) {
		resetTabWidgetOrder();
	}

	OBSDataArrayAutoRelease tabWidgetOrder = obs_data_array_create();
	for (size_t i = 0; i < tabNames.size(); i++) {
		OBSDataAutoRelease entry = obs_data_create();
		obs_data_set_int(entry, tabNames[i].c_str(), tabOrder[i]);
		obs_data_array_push_back(tabWidgetOrder, entry);
	}
	obs_data_set_array(obj, "tabWidgetOrder", tabWidgetOrder);
}

void LoadTabOrder(obs_data_t *obj)
{
	OBSDataArrayAutoRelease defaultTabWidgetOrder = obs_data_array_create();
	for (size_t i = 0; i < tabNames.size(); i++) {
		OBSDataAutoRelease entry = obs_data_create();
		obs_data_set_default_int(entry, tabNames[i].c_str(), i);
		obs_data_array_push_back(defaultTabWidgetOrder, entry);
	}
	obs_data_set_default_array(obj, "tabWidgetOrder",
				   defaultTabWidgetOrder);

	auto &tabOrder = getTabOrderVector();
	tabOrder.clear();
	OBSDataArrayAutoRelease tabWidgetOrder =
		obs_data_get_array(obj, "tabWidgetOrder");
	for (size_t i = 0; i < tabNames.size(); i++) {
		OBSDataAutoRelease entry =
			obs_data_array_item(tabWidgetOrder, i);
		tabOrder.emplace_back(
			(int)(obs_data_get_int(entry, tabNames[i].c_str())));
	}

	if (!tabWidgetOrderValid()) {
		resetTabWidgetOrder();
	}
}

static int findTabIndex(QTabWidget *tabWidget, int pos)
{
	int at = -1;
	QString tabName = QString::fromStdString(tabNames.at(pos));
	QWidget *page = tabWidget->findChild<QWidget *>(tabName);
	if (page) {
		at = tabWidget->indexOf(page);
	}
	if (at == -1) {
		blog(LOG_INFO, "failed to find tab %s",
		     tabName.toUtf8().constData());
	}

	return at;
}

void SetTabOrder(QTabWidget *tabWidget)
{
	if (!tabWidgetOrderValid()) {
		resetTabWidgetOrder();
	}

	auto &tabOrder = getTabOrderVector();

	auto bar = tabWidget->tabBar();
	for (int i = 0; i < bar->count(); ++i) {
		int curPos = findTabIndex(tabWidget, tabOrder[i]);

		if (i != curPos && curPos != -1) {
			bar->moveTab(curPos, i);
		}
	}

	QWidget::connect(bar, &QTabBar::tabMoved,
			 [&tabOrder](int from, int to) {
				 std::swap(tabOrder[from], tabOrder[to]);
			 });
}

void SetCurrentTab(QTabWidget *tabWidget)
{
	if (lastOpenedTab >= 0 && tabWidget->isTabVisible(lastOpenedTab)) {
		tabWidget->setCurrentIndex(lastOpenedTab);
	}
}

void SetupOtherTabs(QTabWidget *tabWidget)
{
	for (const auto &[name, callbacks] : createTabCallbacks) {
		auto widget = callbacks.createWidget();
		widget->setObjectName(name);
		auto tabText = obs_module_text(
			(std::string("AdvSceneSwitcher.") + name + ".title")
				.c_str());
		tabWidget->insertTab(0, widget, tabText);
		callbacks.setupTab(tabWidget);
	}
}

} // namespace advss
