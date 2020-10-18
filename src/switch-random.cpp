#include <random>

#include "headers/advanced-scene-switcher.hpp"

static QMetaObject::Connection addPulse;

void AdvSceneSwitcher::on_randomAdd_clicked()
{
	ui->randomAdd->disconnect(addPulse);

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->randomSwitches.emplace_back();

	QListWidgetItem *item;
	item = new QListWidgetItem(ui->randomSwitches);
	ui->randomSwitches->addItem(item);
	RandomSwitchWidget *sw =
		new RandomSwitchWidget(&switcher->randomSwitches.back());
	item->setSizeHint(sw->minimumSizeHint());
	ui->randomSwitches->setItemWidget(item, sw);
}

void AdvSceneSwitcher::on_randomRemove_clicked()
{
	QListWidgetItem *item = ui->randomSwitches->currentItem();
	if (!item)
		return;

	{
		std::lock_guard<std::mutex> lock(switcher->m);
		int idx = ui->randomSwitches->currentRow();
		auto &switches = switcher->randomSwitches;
		switches.erase(switches.begin() + idx);
	}

	delete item;
}

void SwitcherData::checkRandom(bool &match, OBSWeakSource &scene,
			       OBSWeakSource &transition, int &delay)
{
	if (randomSwitches.size() == 0)
		return;

	std::deque<RandomSwitch> rs(randomSwitches);
	std::random_device rng;
	std::mt19937 urng(rng());
	std::shuffle(rs.begin(), rs.end(), urng);
	for (RandomSwitch &r : rs) {
		if (!r.initialized())
			continue;

		if (r.scene == lastRandomScene)
			continue;
		scene = r.scene;
		transition = r.transition;
		delay = (int)r.delay * 1000;
		match = true;
		lastRandomScene = r.scene;

		if (verbose)
			r.logMatch();
		break;
	}
}

void SwitcherData::saveRandomSwitches(obs_data_t *obj)
{
	obs_data_array_t *randomArray = obs_data_array_create();
	for (RandomSwitch &s : switcher->randomSwitches) {
		obs_data_t *array_obj = obs_data_create();

		obs_source_t *source = obs_weak_source_get_source(s.scene);
		obs_source_t *transition =
			obs_weak_source_get_source(s.transition);

		if (source && transition) {
			const char *sceneName = obs_source_get_name(source);
			const char *transitionName =
				obs_source_get_name(transition);
			obs_data_set_string(array_obj, "scene", sceneName);
			obs_data_set_string(array_obj, "transition",
					    transitionName);
			obs_data_set_double(array_obj, "delay", s.delay);
			obs_data_array_push_back(randomArray, array_obj);
			obs_source_release(source);
			obs_source_release(transition);
		}

		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "randomSwitches", randomArray);
	obs_data_array_release(randomArray);
}

void SwitcherData::loadRandomSwitches(obs_data_t *obj)
{
	switcher->randomSwitches.clear();

	obs_data_array_t *randomArray =
		obs_data_get_array(obj, "randomSwitches");
	size_t count = obs_data_array_count(randomArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj = obs_data_array_item(randomArray, i);

		const char *scene = obs_data_get_string(array_obj, "scene");
		const char *transition =
			obs_data_get_string(array_obj, "transition");
		double delay = obs_data_get_double(array_obj, "delay");

		switcher->randomSwitches.emplace_back(
			GetWeakSourceByName(scene),
			GetWeakTransitionByName(transition), delay);

		obs_data_release(array_obj);
	}
	obs_data_array_release(randomArray);
}

void AdvSceneSwitcher::setupRandomTab()
{
	for (auto &s : switcher->randomSwitches) {
		QListWidgetItem *item;
		item = new QListWidgetItem(ui->randomSwitches);
		ui->randomSwitches->addItem(item);
		RandomSwitchWidget *sw = new RandomSwitchWidget(&s);
		item->setSizeHint(sw->minimumSizeHint());
		ui->randomSwitches->setItemWidget(item, sw);
	}

	if (switcher->randomSwitches.size() == 0)
		addPulse = PulseWidget(ui->randomAdd, QColor(Qt::green));

	if (switcher->switchIfNotMatching != RANDOM_SWITCH)
		PulseWidget(ui->randomDisabledWarning, QColor(Qt::red),
			    QColor(0, 0, 0, 0), "QLabel ");
	else
		ui->randomDisabledWarning->setVisible(false);
}

RandomSwitchWidget::RandomSwitchWidget(RandomSwitch *s) : SwitchWidget(s, false)
{
	delay = new QDoubleSpinBox();

	switchLabel = new QLabel("If no switch condition is met switch to ");
	usingLabel = new QLabel("using");
	forLabel = new QLabel("for");

	QWidget::connect(delay, SIGNAL(valueChanged(double)), this,
			 SLOT(DelayChanged(double)));

	delay->setSuffix("s");
	delay->setMaximum(999999999.9);

	if (s) {
		delay->setValue(s->delay);
	}

	setStyleSheet("* { background-color: transparent; }");

	QHBoxLayout *mainLayout = new QHBoxLayout;

	mainLayout->addWidget(switchLabel);
	mainLayout->addWidget(scenes);
	mainLayout->addWidget(usingLabel);
	mainLayout->addWidget(transitions);
	mainLayout->addWidget(forLabel);
	mainLayout->addWidget(delay);
	mainLayout->addStretch();

	setLayout(mainLayout);

	switchData = s;

	loading = false;
}

RandomSwitch *RandomSwitchWidget::getSwitchData()
{
	return switchData;
}

void RandomSwitchWidget::setSwitchData(RandomSwitch *s)
{
	switchData = s;
}

void RandomSwitchWidget::swapSwitchData(RandomSwitchWidget *s1,
					RandomSwitchWidget *s2)
{
	SwitchWidget::swapSwitchData(s1, s2);

	RandomSwitch *t = s1->getSwitchData();
	s1->setSwitchData(s2->getSwitchData());
	s2->setSwitchData(t);
}

void RandomSwitchWidget::DelayChanged(double d)
{
	if (loading || !switchData)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->delay = d;
}
