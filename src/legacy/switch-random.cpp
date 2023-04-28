#include <random>

#include "advanced-scene-switcher.hpp"
#include "utility.hpp"

namespace advss {

bool RandomSwitch::pause = false;
static QMetaObject::Connection addPulse;

void AdvSceneSwitcher::on_randomAdd_clicked()
{
	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->randomSwitches.emplace_back();

	listAddClicked(ui->randomSwitches,
		       new RandomSwitchWidget(this,
					      &switcher->randomSwitches.back()),
		       ui->randomAdd, &addPulse);

	ui->randomHelp->setVisible(false);
}

void AdvSceneSwitcher::on_randomRemove_clicked()
{
	QListWidgetItem *item = ui->randomSwitches->currentItem();
	if (!item) {
		return;
	}

	{
		std::lock_guard<std::mutex> lock(switcher->m);
		int idx = ui->randomSwitches->currentRow();
		auto &switches = switcher->randomSwitches;
		switches.erase(switches.begin() + idx);
	}

	delete item;
}

bool SwitcherData::checkRandom(OBSWeakSource &scene, OBSWeakSource &transition,
			       int &delay)
{
	if (randomSwitches.size() == 0 || RandomSwitch::pause) {
		return false;
	}

	std::deque<RandomSwitch> rs(randomSwitches);
	std::random_device rng;
	std::mt19937 urng(rng());
	std::shuffle(rs.begin(), rs.end(), urng);
	bool match = false;
	for (RandomSwitch &r : rs) {
		if (!r.initialized()) {
			continue;
		}

		if (randomSwitches.size() != 1) {
			if (r.targetType == SwitchTargetType::Scene &&
			    r.scene == lastRandomScene) {
				continue;
			} else if (r.targetType ==
					   SwitchTargetType::SceneGroup &&
				   r.group && r.group == lastRandomSceneGroup) {
				continue;
			}
		}

		scene = r.getScene();
		transition = r.transition;
		delay = (int)r.delay * 1000;
		match = true;
		lastRandomScene = r.scene;
		lastRandomSceneGroup = r.group;

		if (verbose) {
			r.logMatch();
		}
		break;
	}
	return match;
}

void SwitcherData::saveRandomSwitches(obs_data_t *obj)
{
	obs_data_array_t *randomArray = obs_data_array_create();
	for (RandomSwitch &s : randomSwitches) {
		obs_data_t *array_obj = obs_data_create();

		s.save(array_obj);
		obs_data_array_push_back(randomArray, array_obj);

		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "randomSwitches", randomArray);
	obs_data_array_release(randomArray);
}

void SwitcherData::loadRandomSwitches(obs_data_t *obj)
{
	randomSwitches.clear();

	obs_data_array_t *randomArray =
		obs_data_get_array(obj, "randomSwitches");
	size_t count = obs_data_array_count(randomArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj = obs_data_array_item(randomArray, i);

		randomSwitches.emplace_back();
		randomSwitches.back().load(array_obj);

		obs_data_release(array_obj);
	}
	obs_data_array_release(randomArray);
}

void AdvSceneSwitcher::SetupRandomTab()
{
	for (auto &s : switcher->randomSwitches) {
		QListWidgetItem *item;
		item = new QListWidgetItem(ui->randomSwitches);
		ui->randomSwitches->addItem(item);
		RandomSwitchWidget *sw = new RandomSwitchWidget(this, &s);
		item->setSizeHint(sw->minimumSizeHint());
		ui->randomSwitches->setItemWidget(item, sw);
	}

	if (switcher->randomSwitches.size() == 0) {
		if (!switcher->disableHints) {
			addPulse =
				PulseWidget(ui->randomAdd, QColor(Qt::green));
		}
		ui->randomHelp->setVisible(true);
	} else {
		ui->randomHelp->setVisible(false);
	}

	ui->randomDisabledWarning->setStyleSheet("QLabel{ \
		border-style: outset; \
		border-width: 2px; \
		border-radius: 7px; \
		border-color: rgb(0,0,0,0) \
		}");
	if (switcher->switchIfNotMatching != RANDOM_SWITCH) {
		if (!switcher->disableHints) {
			PulseWidget(ui->randomDisabledWarning, QColor(Qt::red),
				    QColor(0, 0, 0, 0));
		}
	} else {
		ui->randomDisabledWarning->setVisible(false);
	}
}

void RandomSwitch::save(obs_data_t *obj)
{
	SceneSwitcherEntry::save(obj, "targetType", "scene");
	obs_data_set_double(obj, "delay", delay);
}

void RandomSwitch::load(obs_data_t *obj)
{
	SceneSwitcherEntry::load(obj, "targetType", "scene");
	delay = obs_data_get_double(obj, "delay");
}

RandomSwitchWidget::RandomSwitchWidget(QWidget *parent, RandomSwitch *s)
	: SwitchWidget(parent, s, false, true)
{
	delay = new QDoubleSpinBox();

	QWidget::connect(delay, SIGNAL(valueChanged(double)), this,
			 SLOT(DelayChanged(double)));

	delay->setSuffix("s");
	delay->setMaximum(999999999.9);

	if (s) {
		delay->setValue(s->delay);
	}

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{scenes}}", scenes},
		{"{{transitions}}", transitions},
		{"{{delay}}", delay}};
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.randomTab.entry"),
		     mainLayout, widgetPlaceholders);
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
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->delay = d;
}

} // namespace advss
