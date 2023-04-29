#include "advanced-scene-switcher.hpp"
#include "switcher-data.hpp"
#include "platform-funcs.hpp"
#include "utility.hpp"

#include <regex>

namespace advss {

bool IdleData::pause = false;
IdleWidget *idleWidget = nullptr;

bool SwitcherData::checkIdleSwitch(OBSWeakSource &scene,
				   OBSWeakSource &transition)
{
	if (!idleData.idleEnable || IdleData::pause) {
		return false;
	}

	std::string title = switcher->currentTitle;
	bool ignoreIdle = false;
	bool match = false;

	for (std::string &window : ignoreIdleWindows) {
		if (window == title) {
			ignoreIdle = true;
			break;
		}
	}

	if (!ignoreIdle) {
		for (std::string &window : ignoreIdleWindows) {
			try {
				bool matches = std::regex_match(
					title, std::regex(window));
				if (matches) {
					ignoreIdle = true;
					break;
				}
			} catch (const std::regex_error &) {
			}
		}
	}

	if (!ignoreIdle && SecondsSinceLastInput() > idleData.time) {
		if (idleData.alreadySwitched) {
			return false;
		}
		scene = idleData.getScene();
		transition = idleData.transition;
		match = true;
		idleData.alreadySwitched = true;

		if (verbose) {
			idleData.logMatch();
		}
	} else {
		idleData.alreadySwitched = false;
	}

	return match;
}

void AdvSceneSwitcher::on_idleCheckBox_stateChanged(int state)
{
	if (loading) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	if (!state) {
		switcher->idleData.idleEnable = false;
		idleWidget->setDisabled(true);
	} else {
		switcher->idleData.idleEnable = true;
		idleWidget->setDisabled(false);
	}
}

void AdvSceneSwitcher::on_ignoreIdleWindows_currentRowChanged(int idx)
{
	if (loading) {
		return;
	}
	if (idx == -1) {
		return;
	}

	QListWidgetItem *item = ui->ignoreIdleWindows->item(idx);

	QString window = item->data(Qt::UserRole).toString();

	std::lock_guard<std::mutex> lock(switcher->m);
	for (auto &w : switcher->ignoreIdleWindows) {
		if (window.compare(w.c_str()) == 0) {
			ui->ignoreIdleWindowsWindows->setCurrentText(w.c_str());
			break;
		}
	}
}

void AdvSceneSwitcher::on_ignoreIdleAdd_clicked()
{
	QString windowName = ui->ignoreIdleWindowsWindows->currentText();

	if (windowName.isEmpty()) {
		return;
	}

	QVariant v = QVariant::fromValue(windowName);

	QList<QListWidgetItem *> items =
		ui->ignoreIdleWindows->findItems(windowName, Qt::MatchExactly);

	if (items.size() == 0) {
		QListWidgetItem *item =
			new QListWidgetItem(windowName, ui->ignoreIdleWindows);
		item->setData(Qt::UserRole, v);

		std::lock_guard<std::mutex> lock(switcher->m);
		switcher->ignoreIdleWindows.emplace_back(
			windowName.toUtf8().constData());
		ui->ignoreIdleWindows->sortItems();
	}
}

void AdvSceneSwitcher::on_ignoreIdleRemove_clicked()
{
	QListWidgetItem *item = ui->ignoreIdleWindows->currentItem();
	if (!item) {
		return;
	}

	QString windowName = item->data(Qt::UserRole).toString();

	{
		std::lock_guard<std::mutex> lock(switcher->m);
		auto &windows = switcher->ignoreIdleWindows;

		for (auto it = windows.begin(); it != windows.end(); ++it) {
			auto &s = *it;

			if (s == windowName.toUtf8().constData()) {
				windows.erase(it);
				break;
			}
		}
	}

	delete item;
}

int AdvSceneSwitcher::IgnoreIdleWindowsFindByData(const QString &window)
{
	int count = ui->ignoreIdleWindows->count();
	int idx = -1;

	for (int i = 0; i < count; i++) {
		QListWidgetItem *item = ui->ignoreIdleWindows->item(i);
		QString itemRegion = item->data(Qt::UserRole).toString();

		if (itemRegion == window) {
			idx = i;
			break;
		}
	}

	return idx;
}

void SwitcherData::saveIdleSwitches(obs_data_t *obj)
{
	obs_data_array_t *ignoreIdleWindowsArray = obs_data_array_create();
	for (std::string &window : ignoreIdleWindows) {
		obs_data_t *array_obj = obs_data_create();
		obs_data_set_string(array_obj, "window", window.c_str());
		obs_data_array_push_back(ignoreIdleWindowsArray, array_obj);
		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "ignoreIdleWindows", ignoreIdleWindowsArray);
	obs_data_array_release(ignoreIdleWindowsArray);

	idleData.save(obj);
}

void SwitcherData::loadIdleSwitches(obs_data_t *obj)
{
	ignoreIdleWindows.clear();

	obs_data_array_t *ignoreIdleWindowsArray =
		obs_data_get_array(obj, "ignoreIdleWindows");
	size_t count = obs_data_array_count(ignoreIdleWindowsArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj =
			obs_data_array_item(ignoreIdleWindowsArray, i);

		const char *window = obs_data_get_string(array_obj, "window");

		ignoreIdleWindows.emplace_back(window);

		obs_data_release(array_obj);
	}
	obs_data_array_release(ignoreIdleWindowsArray);

	obs_data_set_default_bool(obj, "idleEnable", false);
	obs_data_set_default_int(obj, "idleTime", default_idle_time);

	idleData.load(obj);
}

void AdvSceneSwitcher::SetupIdleTab()
{
	PopulateWindowSelection(ui->ignoreIdleWindowsWindows);

	for (auto &window : switcher->ignoreIdleWindows) {
		QString text = QString::fromStdString(window);

		QListWidgetItem *item =
			new QListWidgetItem(text, ui->ignoreIdleWindows);
		item->setData(Qt::UserRole, text);
	}

	idleWidget = new IdleWidget(this, &switcher->idleData);
	ui->idleWidgetLayout->addWidget(idleWidget);
	ui->idleCheckBox->setChecked(switcher->idleData.idleEnable);

	if (ui->idleCheckBox->checkState()) {
		idleWidget->setDisabled(false);
	} else {
		idleWidget->setDisabled(true);
	}
}

void IdleData::save(obs_data_t *obj)
{
	SceneSwitcherEntry::save(obj, "idleTargetType", "idleSceneName",
				 "idleTransitionName");

	obs_data_set_bool(obj, "idleEnable", idleEnable);
	obs_data_set_int(obj, "idleTime", time);
}

void IdleData::load(obs_data_t *obj)
{
	SceneSwitcherEntry::load(obj, "idleTargetType", "idleSceneName",
				 "idleTransitionName");

	idleEnable = obs_data_get_bool(obj, "idleEnable");
	time = obs_data_get_int(obj, "idleTime");
}

IdleWidget::IdleWidget(QWidget *parent, IdleData *s)
	: SwitchWidget(parent, s, true, true)
{
	duration = new QSpinBox();

	duration->setMinimum(0);
	duration->setMaximum(1000000);
	duration->setSuffix("s");

	QWidget::connect(duration, SIGNAL(valueChanged(int)), this,
			 SLOT(DurationChanged(int)));

	if (s) {
		duration->setValue(s->time);
	}

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{duration}}", duration},
		{"{{scenes}}", scenes},
		{"{{transitions}}", transitions}};
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.idleTab.idleswitch"),
		     mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	switchData = s;

	loading = false;
}

void IdleWidget::DurationChanged(int dur)
{
	if (loading) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->idleData.time = dur;
}

} // namespace advss
