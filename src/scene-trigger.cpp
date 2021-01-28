#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

bool SceneTrigger::pause = false;
static QMetaObject::Connection addPulse;

//void AdvSceneSwitcher::on_triggerAdd_clicked()
//{
//	std::lock_guard<std::mutex> lock(switcher->m);
//	switcher->sceneTriggerSwitches.emplace_back();
//
//	listAddClicked(ui->sceneTriggerSwitches,
//		       new TimeSwitchWidget(this,
//					    &switcher->timeSwitches.back()),
//		       ui->timeAdd, &addPulse);
//
//	ui->tiggerHelp->setVisible(false);
//}
//
//void AdvSceneSwitcher::on_triggerRemove_clicked()
//{
//	QListWidgetItem *item = ui->sceneTriggerSwitches->currentItem();
//	if (!item)
//		return;
//
//	{
//		std::lock_guard<std::mutex> lock(switcher->m);
//		int idx = ui->timeSwitches->currentRow();
//		auto &switches = switcher->timeSwitches;
//		switches.erase(switches.begin() + idx);
//	}
//
//	delete item;
//}
//
//void AdvSceneSwitcher::on_triggerUp_clicked()
//{
//	int index = ui->timeSwitches->currentRow();
//	if (!listMoveUp(ui->timeSwitches))
//		return;
//
//	TimeSwitchWidget *s1 = (TimeSwitchWidget *)ui->timeSwitches->itemWidget(
//		ui->timeSwitches->item(index));
//	TimeSwitchWidget *s2 = (TimeSwitchWidget *)ui->timeSwitches->itemWidget(
//		ui->timeSwitches->item(index - 1));
//	TimeSwitchWidget::swapSwitchData(s1, s2);
//
//	std::lock_guard<std::mutex> lock(switcher->m);
//
//	std::swap(switcher->timeSwitches[index],
//		  switcher->timeSwitches[index - 1]);
//}
//
//void AdvSceneSwitcher::on_triggerDown_clicked()
//{
//	int index = ui->timeSwitches->currentRow();
//
//	if (!listMoveDown(ui->timeSwitches))
//		return;
//
//	TimeSwitchWidget *s1 = (TimeSwitchWidget *)ui->timeSwitches->itemWidget(
//		ui->timeSwitches->item(index));
//	TimeSwitchWidget *s2 = (TimeSwitchWidget *)ui->timeSwitches->itemWidget(
//		ui->timeSwitches->item(index + 1));
//	TimeSwitchWidget::swapSwitchData(s1, s2);
//
//	std::lock_guard<std::mutex> lock(switcher->m);
//
//	std::swap(switcher->timeSwitches[index],
//		  switcher->timeSwitches[index + 1]);
//}

//void SwitcherData::checkTriggers()
//{
//	if (TimeSwitch::pause)
//		return;
//	}
//}
//
//void SwitcherData::saveSceneTriggers(obs_data_t *obj)
//{
//	obs_data_array_t *timeArray = obs_data_array_create();
//	for (TimeSwitch &s : switcher->timeSwitches) {
//		obs_data_t *array_obj = obs_data_create();
//
//		s.save(array_obj);
//		obs_data_array_push_back(timeArray, array_obj);
//
//		obs_data_release(array_obj);
//	}
//	obs_data_set_array(obj, "timeSwitches", timeArray);
//	obs_data_array_release(timeArray);
//}
//
//void SwitcherData::loadSceneTriggers(obs_data_t *obj)
//{
//	switcher->timeSwitches.clear();
//
//	obs_data_array_t *timeArray = obs_data_get_array(obj, "timeSwitches");
//	size_t count = obs_data_array_count(timeArray);
//
//	for (size_t i = 0; i < count; i++) {
//		obs_data_t *array_obj = obs_data_array_item(timeArray, i);
//
//		switcher->timeSwitches.emplace_back();
//		timeSwitches.back().load(array_obj);
//
//		obs_data_release(array_obj);
//	}
//	obs_data_array_release(timeArray);
//}
//
//void AdvSceneSwitcher::setupTriggerTab()
//{
//	for (auto &s : switcher->timeSwitches) {
//		QListWidgetItem *item;
//		item = new QListWidgetItem(ui->timeSwitches);
//		ui->timeSwitches->addItem(item);
//		TimeSwitchWidget *sw = new TimeSwitchWidget(this, &s);
//		item->setSizeHint(sw->minimumSizeHint());
//		ui->timeSwitches->setItemWidget(item, sw);
//	}
//
//	if (switcher->timeSwitches.size() == 0) {
//		addPulse = PulseWidget(ui->timeAdd, QColor(Qt::green));
//		ui->timeHelp->setVisible(true);
//	} else {
//		ui->timeHelp->setVisible(false);
//	}
//}

void SceneTrigger::save(obs_data_t *obj)
{
	SceneSwitcherEntry::save(obj, "unused", "scene", "unused");

	//obs_data_set_int(obj, "trigger", trigger);
	//obs_data_set_string(obj, "time", time.toString().toStdString().c_str());
}

void SceneTrigger::load(obs_data_t *obj)
{
	SceneSwitcherEntry::load(obj, "unused", "scene", "unused");

	//trigger = (timeTrigger)obs_data_get_int(obj, "trigger");
	//time = QTime::fromString(obs_data_get_string(obj, "time"));
}

inline void populateTriggers(QComboBox *list)
{
	//list->addItem(obs_module_text("AdvSceneSwitcher.timeTab.anyDay"));
}

SceneTriggerWidget::SceneTriggerWidget(QWidget *parent, SceneTrigger *s)
	: SwitchWidget(parent, s, false, false)
{
	triggers = new QComboBox();
	actions = new QComboBox();
	duration = new QDoubleSpinBox();

	//QWidget::connect(triggers, SIGNAL(currentIndexChanged(int)), this,
	//		 SLOT(TriggerChanged(int)));
	//QWidget::connect(time, SIGNAL(timeChanged(const QTime &)), this,
	//		 SLOT(TimeChanged(const QTime &)));
	//
	//populateTriggers(triggers);
	//time->setDisplayFormat("HH:mm:ss");
	//
	//if (s) {
	//	triggers->setCurrentIndex(s->trigger);
	//	time->setTime(s->time);
	//}
	//
	//QHBoxLayout *mainLayout = new QHBoxLayout;
	//std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
	//	{"{{triggers}}", triggers},
	//	{"{{time}}", time},
	//	{"{{scenes}}", scenes},
	//	{"{{transitions}}", transitions}};
	//placeWidgets(obs_module_text("AdvSceneSwitcher.timeTab.entry"),
	//	     mainLayout, widgetPlaceholders);
	//setLayout(mainLayout);
	//
	//switchData = s;
	//
	//loading = false;
}

SceneTrigger *SceneTriggerWidget::getSwitchData()
{
	return switchData;
}

void SceneTriggerWidget::setSwitchData(SceneTrigger *s)
{
	switchData = s;
}

void SceneTriggerWidget::swapSwitchData(SceneTriggerWidget *s1,
					SceneTriggerWidget *s2)
{
	SwitchWidget::swapSwitchData(s1, s2);

	SceneTrigger *t = s1->getSwitchData();
	s1->setSwitchData(s2->getSwitchData());
	s2->setSwitchData(t);
}

void SceneTriggerWidget::TriggerTypeChanged(int index) {}

void SceneTriggerWidget::TriggerActionChanged(int index) {}

void SceneTriggerWidget::DurationChanged(double dur) {}
