#include "advanced-scene-switcher.hpp"
#include "layout-helpers.hpp"
#include "switcher-data.hpp"
#include "ui-helpers.hpp"
#include "utility.hpp"

namespace advss {

bool TimeSwitch::pause = false;
static QMetaObject::Connection addPulse;

void AdvSceneSwitcher::on_timeAdd_clicked()
{
	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->timeSwitches.emplace_back();

	listAddClicked(ui->timeSwitches,
		       new TimeSwitchWidget(this,
					    &switcher->timeSwitches.back()),
		       ui->timeAdd, &addPulse);

	ui->timeHelp->setVisible(false);
}

void AdvSceneSwitcher::on_timeRemove_clicked()
{
	QListWidgetItem *item = ui->timeSwitches->currentItem();
	if (!item) {
		return;
	}

	{
		std::lock_guard<std::mutex> lock(switcher->m);
		int idx = ui->timeSwitches->currentRow();
		auto &switches = switcher->timeSwitches;
		switches.erase(switches.begin() + idx);
	}

	delete item;
}

void AdvSceneSwitcher::on_timeUp_clicked()
{
	int index = ui->timeSwitches->currentRow();
	if (!listMoveUp(ui->timeSwitches)) {
		return;
	}

	TimeSwitchWidget *s1 = (TimeSwitchWidget *)ui->timeSwitches->itemWidget(
		ui->timeSwitches->item(index));
	TimeSwitchWidget *s2 = (TimeSwitchWidget *)ui->timeSwitches->itemWidget(
		ui->timeSwitches->item(index - 1));
	TimeSwitchWidget::swapSwitchData(s1, s2);

	std::lock_guard<std::mutex> lock(switcher->m);

	std::swap(switcher->timeSwitches[index],
		  switcher->timeSwitches[index - 1]);
}

void AdvSceneSwitcher::on_timeDown_clicked()
{
	int index = ui->timeSwitches->currentRow();

	if (!listMoveDown(ui->timeSwitches)) {
		return;
	}

	TimeSwitchWidget *s1 = (TimeSwitchWidget *)ui->timeSwitches->itemWidget(
		ui->timeSwitches->item(index));
	TimeSwitchWidget *s2 = (TimeSwitchWidget *)ui->timeSwitches->itemWidget(
		ui->timeSwitches->item(index + 1));
	TimeSwitchWidget::swapSwitchData(s1, s2);

	std::lock_guard<std::mutex> lock(switcher->m);

	std::swap(switcher->timeSwitches[index],
		  switcher->timeSwitches[index + 1]);
}

bool timesAreInInterval(QTime &time1, QTime &time2, int &interval)
{
	if (time1.isNull() || time2.isNull()) {
		return false;
	}

	bool ret = false;
	QTime validSwitchTimeWindow = time1.addMSecs(interval);

	ret = time1 <= time2 && time2 <= validSwitchTimeWindow;
	// Check for overflow
	if (!ret && validSwitchTimeWindow.msecsSinceStartOfDay() < interval) {
		ret = time2 >= time1 || time2 <= validSwitchTimeWindow;
	}
	return ret;
}

bool checkLiveTime(TimeSwitch &s, QDateTime &start, int &interval)
{
	if (start.isNull()) {
		return false;
	}

	QDateTime now = QDateTime::currentDateTime();
	QTime timePassed = QTime(0, 0).addMSecs(start.msecsTo(now));

	return timesAreInInterval(s.time, timePassed, interval);
}

bool checkRegularTime(TimeSwitch &s, int &interval)
{
	if (s.trigger != ANY_DAY &&
	    s.trigger != QDate::currentDate().dayOfWeek())
		return false;

	QTime now = QTime::currentTime();

	return timesAreInInterval(s.time, now, interval);
}

bool SwitcherData::checkTimeSwitch(OBSWeakSource &scene,
				   OBSWeakSource &transition)
{
	if (TimeSwitch::pause) {
		return false;
	}

	bool match = false;
	for (TimeSwitch &s : timeSwitches) {
		if (!s.initialized()) {
			continue;
		}

		if (s.trigger == LIVE) {
			match = checkLiveTime(s, liveTime, interval);
		} else {
			match = checkRegularTime(s, interval);
		}

		if (match) {
			scene = s.getScene();
			transition = s.transition;
			match = true;

			if (VerboseLoggingEnabled()) {
				s.logMatch();
			}
			break;
		}
	}
	return match;
}

void SwitcherData::saveTimeSwitches(obs_data_t *obj)
{
	obs_data_array_t *timeArray = obs_data_array_create();
	for (TimeSwitch &s : timeSwitches) {
		obs_data_t *array_obj = obs_data_create();

		s.save(array_obj);
		obs_data_array_push_back(timeArray, array_obj);

		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "timeSwitches", timeArray);
	obs_data_array_release(timeArray);
}

void SwitcherData::loadTimeSwitches(obs_data_t *obj)
{
	timeSwitches.clear();

	obs_data_array_t *timeArray = obs_data_get_array(obj, "timeSwitches");
	size_t count = obs_data_array_count(timeArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj = obs_data_array_item(timeArray, i);

		timeSwitches.emplace_back();
		timeSwitches.back().load(array_obj);

		obs_data_release(array_obj);
	}
	obs_data_array_release(timeArray);
}

void AdvSceneSwitcher::SetupTimeTab()
{
	for (auto &s : switcher->timeSwitches) {
		QListWidgetItem *item;
		item = new QListWidgetItem(ui->timeSwitches);
		ui->timeSwitches->addItem(item);
		TimeSwitchWidget *sw = new TimeSwitchWidget(this, &s);
		item->setSizeHint(sw->minimumSizeHint());
		ui->timeSwitches->setItemWidget(item, sw);
	}

	if (switcher->timeSwitches.size() == 0) {
		if (!switcher->disableHints) {
			addPulse = PulseWidget(ui->timeAdd, QColor(Qt::green));
		}
		ui->timeHelp->setVisible(true);
	} else {
		ui->timeHelp->setVisible(false);
	}
}

void TimeSwitch::save(obs_data_t *obj)
{
	SceneSwitcherEntry::save(obj);

	obs_data_set_int(obj, "trigger", trigger);
	obs_data_set_string(obj, "time", time.toString().toStdString().c_str());
}

void TimeSwitch::load(obs_data_t *obj)
{
	SceneSwitcherEntry::load(obj);

	trigger = (timeTrigger)obs_data_get_int(obj, "trigger");
	time = QTime::fromString(obs_data_get_string(obj, "time"));
}

static inline void populateTriggers(QComboBox *list)
{
	list->addItem(obs_module_text("AdvSceneSwitcher.timeTab.anyDay"));
	list->addItem(obs_module_text("AdvSceneSwitcher.timeTab.mondays"));
	list->addItem(obs_module_text("AdvSceneSwitcher.timeTab.tuesdays"));
	list->addItem(obs_module_text("AdvSceneSwitcher.timeTab.wednesdays"));
	list->addItem(obs_module_text("AdvSceneSwitcher.timeTab.thursdays"));
	list->addItem(obs_module_text("AdvSceneSwitcher.timeTab.fridays"));
	list->addItem(obs_module_text("AdvSceneSwitcher.timeTab.saturdays"));
	list->addItem(obs_module_text("AdvSceneSwitcher.timeTab.sundays"));
	list->addItem(obs_module_text("AdvSceneSwitcher.timeTab.afterstart"));

	list->setItemData(
		8, obs_module_text("AdvSceneSwitcher.timeTab.afterstart.tip"),
		Qt::ToolTipRole);
}

TimeSwitchWidget::TimeSwitchWidget(QWidget *parent, TimeSwitch *s)
	: SwitchWidget(parent, s, true, true)
{
	triggers = new QComboBox();
	time = new QTimeEdit();

	QWidget::connect(triggers, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(TriggerChanged(int)));
	QWidget::connect(time, SIGNAL(timeChanged(const QTime &)), this,
			 SLOT(TimeChanged(const QTime &)));

	populateTriggers(triggers);
	time->setDisplayFormat("HH:mm:ss");

	if (s) {
		triggers->setCurrentIndex(s->trigger);
		time->setTime(s->time);
	}

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{triggers}}", triggers},
		{"{{time}}", time},
		{"{{scenes}}", scenes},
		{"{{transitions}}", transitions}};
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.timeTab.entry"),
		     mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	switchData = s;

	loading = false;
}

TimeSwitch *TimeSwitchWidget::getSwitchData()
{
	return switchData;
}

void TimeSwitchWidget::setSwitchData(TimeSwitch *s)
{
	switchData = s;
}

void TimeSwitchWidget::swapSwitchData(TimeSwitchWidget *s1,
				      TimeSwitchWidget *s2)
{
	SwitchWidget::swapSwitchData(s1, s2);

	TimeSwitch *t = s1->getSwitchData();
	s1->setSwitchData(s2->getSwitchData());
	s2->setSwitchData(t);
}

void TimeSwitchWidget::TriggerChanged(int index)
{
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->trigger = (timeTrigger)index;
}

void TimeSwitchWidget::TimeChanged(const QTime &time)
{
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->time = time;
}

} // namespace advss
