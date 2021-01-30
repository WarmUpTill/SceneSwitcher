#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

bool SceneTrigger::pause = false;
static QMetaObject::Connection addPulse;

void AdvSceneSwitcher::on_triggerAdd_clicked()
{
	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->sceneTriggers.emplace_back();

	listAddClicked(ui->sceneTriggers,
		       new SceneTriggerWidget(this,
					      &switcher->sceneTriggers.back()),
		       ui->triggerAdd, &addPulse);

	ui->triggerHelp->setVisible(false);
}

void AdvSceneSwitcher::on_triggerRemove_clicked()
{
	QListWidgetItem *item = ui->sceneTriggers->currentItem();
	if (!item)
		return;

	{
		std::lock_guard<std::mutex> lock(switcher->m);
		int idx = ui->sceneTriggers->currentRow();
		auto &switches = switcher->sceneTriggers;
		switches.erase(switches.begin() + idx);
	}

	delete item;
}

void AdvSceneSwitcher::on_triggerUp_clicked()
{
	int index = ui->sceneTriggers->currentRow();
	if (!listMoveUp(ui->sceneTriggers))
		return;

	SceneTriggerWidget *s1 =
		(SceneTriggerWidget *)ui->sceneTriggers->itemWidget(
			ui->sceneTriggers->item(index));
	SceneTriggerWidget *s2 =
		(SceneTriggerWidget *)ui->sceneTriggers->itemWidget(
			ui->sceneTriggers->item(index - 1));
	SceneTriggerWidget::swapSwitchData(s1, s2);

	std::lock_guard<std::mutex> lock(switcher->m);

	std::swap(switcher->sceneTriggers[index],
		  switcher->sceneTriggers[index - 1]);
}

void AdvSceneSwitcher::on_triggerDown_clicked()
{
	int index = ui->sceneTriggers->currentRow();

	if (!listMoveDown(ui->sceneTriggers))
		return;

	SceneTriggerWidget *s1 =
		(SceneTriggerWidget *)ui->sceneTriggers->itemWidget(
			ui->sceneTriggers->item(index));
	SceneTriggerWidget *s2 =
		(SceneTriggerWidget *)ui->sceneTriggers->itemWidget(
			ui->sceneTriggers->item(index + 1));
	SceneTriggerWidget::swapSwitchData(s1, s2);

	std::lock_guard<std::mutex> lock(switcher->m);

	std::swap(switcher->sceneTriggers[index],
		  switcher->sceneTriggers[index + 1]);
}

//void SwitcherData::checkTriggers()
//{
//	if (TimeSwitch::pause)
//		return;
//	}
//}
//
void SwitcherData::saveSceneTriggers(obs_data_t *obj)
{
	obs_data_array_t *triggerArray = obs_data_array_create();
	for (auto &s : switcher->sceneTriggers) {
		obs_data_t *array_obj = obs_data_create();

		s.save(array_obj);
		obs_data_array_push_back(triggerArray, array_obj);

		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "triggers", triggerArray);
	obs_data_array_release(triggerArray);
}

void SwitcherData::loadSceneTriggers(obs_data_t *obj)
{
	switcher->sceneTriggers.clear();

	obs_data_array_t *triggerArray = obs_data_get_array(obj, "triggers");
	size_t count = obs_data_array_count(triggerArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj = obs_data_array_item(triggerArray, i);

		switcher->sceneTriggers.emplace_back();
		sceneTriggers.back().load(array_obj);

		obs_data_release(array_obj);
	}
	obs_data_array_release(triggerArray);
}

void AdvSceneSwitcher::setupTriggerTab()
{
	for (auto &s : switcher->sceneTriggers) {
		QListWidgetItem *item;
		item = new QListWidgetItem(ui->sceneTriggers);
		ui->sceneTriggers->addItem(item);
		SceneTriggerWidget *sw = new SceneTriggerWidget(this, &s);
		item->setSizeHint(sw->minimumSizeHint());
		ui->sceneTriggers->setItemWidget(item, sw);
	}

	if (switcher->sceneTriggers.size() == 0) {
		addPulse = PulseWidget(ui->triggerAdd, QColor(Qt::green));
		ui->triggerHelp->setVisible(true);
	} else {
		ui->triggerHelp->setVisible(false);
	}
}

void SceneTrigger::save(obs_data_t *obj)
{
	SceneSwitcherEntry::save(obj, "unused", "scene", "unused");

	obs_data_set_int(obj, "triggerType", static_cast<int>(triggerType));
	obs_data_set_int(obj, "triggerAction", static_cast<int>(triggerAction));
}

void SceneTrigger::load(obs_data_t *obj)
{
	SceneSwitcherEntry::load(obj, "unused", "scene", "unused2");

	triggerType = static_cast<sceneTriggerType>(
		obs_data_get_int(obj, "triggerType"));
	triggerAction = static_cast<sceneTriggerAction>(
		obs_data_get_int(obj, "triggerAction"));
}

inline void populateTriggers(QComboBox *list)
{
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.sceneTriggerTab.sceneTriggerType.none"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.sceneTriggerTab.sceneTriggerType.sceneActive"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.sceneTriggerTab.sceneTriggerType.sceneInactive"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.sceneTriggerTab.sceneTriggerType.sceneLeave"));
}

inline void populateActions(QComboBox *list)
{
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.sceneTriggerTab.sceneTriggerAction.none"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.sceneTriggerTab.sceneTriggerAction.stopRecording"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.sceneTriggerTab.sceneTriggerAction.stopStreaming"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.sceneTriggerTab.sceneTriggerAction.startRecording"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.sceneTriggerTab.sceneTriggerAction.startStreaming"));
}

SceneTriggerWidget::SceneTriggerWidget(QWidget *parent, SceneTrigger *s)
	: SwitchWidget(parent, s, false, false)
{
	triggers = new QComboBox();
	actions = new QComboBox();
	duration = new QDoubleSpinBox();

	duration->setMinimum(0.0);
	duration->setMaximum(99.000000);
	duration->setSuffix("s");

	QWidget::connect(triggers, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(TriggerChanged(int)));
	QWidget::connect(actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(duration, SIGNAL(valueChanged(double)), this,
			 SLOT(DurationChanged(double)));

	populateTriggers(triggers);
	populateActions(actions);

	if (s) {
		triggers->setCurrentIndex(static_cast<int>(s->triggerType));
		actions->setCurrentIndex(static_cast<int>(s->triggerAction));
		duration->setValue(s->duration);
	}

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{triggers}}", triggers},
		{"{{actions}}", actions},
		{"{{duration}}", duration},
		{"{{scenes}}", scenes}};
	placeWidgets(obs_module_text("AdvSceneSwitcher.sceneTriggerTab.entry"),
		     mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	switchData = s;

	loading = false;
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

void SceneTriggerWidget::TriggerTypeChanged(int index)
{
	if (loading || !switchData)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->triggerType = static_cast<sceneTriggerType>(index);
}

void SceneTriggerWidget::TriggerActionChanged(int index)
{
	if (loading || !switchData)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->triggerAction = static_cast<sceneTriggerAction>(index);
}

void SceneTriggerWidget::DurationChanged(double dur)
{
	if (loading || !switchData)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->duration = dur;
}
