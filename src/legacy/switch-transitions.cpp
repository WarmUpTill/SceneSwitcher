#include <thread>

#include "advanced-scene-switcher.hpp"
#include "utility.hpp"

namespace advss {

constexpr auto default_def_transition_dealy = 300;
bool DefaultSceneTransition::pause = false;
unsigned int DefaultSceneTransition::delay = default_def_transition_dealy;

void AdvSceneSwitcher::on_transitionsAdd_clicked()
{
	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->sceneTransitions.emplace_back();

	listAddClicked(ui->sceneTransitions,
		       new TransitionSwitchWidget(
			       this, &switcher->sceneTransitions.back()));

	ui->transitionHelp->setVisible(false);
}

void AdvSceneSwitcher::on_transitionsRemove_clicked()
{
	QListWidgetItem *item = ui->sceneTransitions->currentItem();
	if (!item) {
		return;
	}

	{
		std::lock_guard<std::mutex> lock(switcher->m);
		int idx = ui->sceneTransitions->currentRow();
		auto &switches = switcher->sceneTransitions;
		switches.erase(switches.begin() + idx);
	}

	delete item;
}

void AdvSceneSwitcher::on_transitionsUp_clicked()
{
	int index = ui->sceneTransitions->currentRow();
	if (!listMoveUp(ui->sceneTransitions)) {
		return;
	}

	TransitionSwitchWidget *s1 =
		(TransitionSwitchWidget *)ui->sceneTransitions->itemWidget(
			ui->sceneTransitions->item(index));
	TransitionSwitchWidget *s2 =
		(TransitionSwitchWidget *)ui->sceneTransitions->itemWidget(
			ui->sceneTransitions->item(index - 1));
	TransitionSwitchWidget::swapSwitchData(s1, s2);

	std::lock_guard<std::mutex> lock(switcher->m);

	std::swap(switcher->sceneTransitions[index],
		  switcher->sceneTransitions[index - 1]);
}

void AdvSceneSwitcher::on_transitionsDown_clicked()
{
	int index = ui->sceneTransitions->currentRow();

	if (!listMoveDown(ui->sceneTransitions)) {
		return;
	}

	TransitionSwitchWidget *s1 =
		(TransitionSwitchWidget *)ui->sceneTransitions->itemWidget(
			ui->sceneTransitions->item(index));
	TransitionSwitchWidget *s2 =
		(TransitionSwitchWidget *)ui->sceneTransitions->itemWidget(
			ui->sceneTransitions->item(index + 1));
	TransitionSwitchWidget::swapSwitchData(s1, s2);

	std::lock_guard<std::mutex> lock(switcher->m);

	std::swap(switcher->sceneTransitions[index],
		  switcher->sceneTransitions[index + 1]);
}

void AdvSceneSwitcher::on_defaultTransitionsAdd_clicked()
{
	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->defaultSceneTransitions.emplace_back();

	listAddClicked(ui->defaultTransitions,
		       new DefTransitionSwitchWidget(
			       this,
			       &switcher->defaultSceneTransitions.back()));

	ui->defaultTransitionHelp->setVisible(false);
}

void AdvSceneSwitcher::on_defaultTransitionsRemove_clicked()
{
	QListWidgetItem *item = ui->defaultTransitions->currentItem();
	if (!item) {
		return;
	}

	{
		std::lock_guard<std::mutex> lock(switcher->m);
		int idx = ui->defaultTransitions->currentRow();
		auto &switches = switcher->defaultSceneTransitions;
		switches.erase(switches.begin() + idx);
	}

	delete item;
}

void AdvSceneSwitcher::on_defaultTransitionsUp_clicked()
{
	int index = ui->defaultTransitions->currentRow();
	if (!listMoveUp(ui->defaultTransitions)) {
		return;
	}

	TransitionSwitchWidget *s1 =
		(TransitionSwitchWidget *)ui->defaultTransitions->itemWidget(
			ui->defaultTransitions->item(index));
	TransitionSwitchWidget *s2 =
		(TransitionSwitchWidget *)ui->defaultTransitions->itemWidget(
			ui->defaultTransitions->item(index - 1));
	TransitionSwitchWidget::swapSwitchData(s1, s2);

	std::lock_guard<std::mutex> lock(switcher->m);

	std::swap(switcher->defaultSceneTransitions[index],
		  switcher->defaultSceneTransitions[index - 1]);
}

void AdvSceneSwitcher::on_defaultTransitionsDown_clicked()
{
	int index = ui->defaultTransitions->currentRow();

	if (!listMoveDown(ui->defaultTransitions)) {
		return;
	}

	DefTransitionSwitchWidget *s1 =
		(DefTransitionSwitchWidget *)ui->defaultTransitions->itemWidget(
			ui->defaultTransitions->item(index));
	DefTransitionSwitchWidget *s2 =
		(DefTransitionSwitchWidget *)ui->defaultTransitions->itemWidget(
			ui->defaultTransitions->item(index + 1));
	DefTransitionSwitchWidget::swapSwitchData(s1, s2);

	std::lock_guard<std::mutex> lock(switcher->m);

	std::swap(switcher->defaultSceneTransitions[index],
		  switcher->defaultSceneTransitions[index + 1]);
}

void SwitcherData::checkDefaultSceneTransitions()
{
	if (DefaultSceneTransition::pause || stop) {
		return;
	}

	for (auto &t : defaultSceneTransitions) {
		if (t.checkMatch(currentScene)) {
			if (verbose) {
				t.logMatch();
			}
			t.setTransition();
			break;
		}
	}
}

void AdvSceneSwitcher::DefTransitionDelayValueChanged(int value)
{
	if (loading) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	DefaultSceneTransition::delay = value;
}

std::pair<obs_weak_source_t *, int> getNextTransition(obs_weak_source_t *scene1,
						      obs_weak_source_t *scene2)
{
	obs_weak_source_t *ws = nullptr;
	int duration = 0;
	if (scene1 && scene2) {
		for (SceneTransition &t : switcher->sceneTransitions) {
			if (!t.initialized()) {
				continue;
			}

			if (t.scene == scene1 && t.scene2 == scene2) {
				ws = t.transition;
				duration = t.duration * 1000;
				break;
			}
		}
	}
	return std::make_pair(ws, duration);
}

void overwriteTransitionOverride(const sceneSwitchInfo &ssi, transitionData &td)
{
	obs_source_t *scene = obs_weak_source_get_source(ssi.scene);
	obs_data_t *data = obs_source_get_private_settings(scene);

	td.name = obs_data_get_string(data, "transition");
	td.duration = obs_data_get_int(data, "transition_duration");

	std::string name = GetWeakSourceName(ssi.transition);

	obs_data_set_string(data, "transition", name.c_str());
	obs_data_set_int(data, "transition_duration", ssi.duration);

	obs_data_release(data);
	obs_source_release(scene);
}

void restoreTransitionOverride(obs_source_t *scene, const transitionData &td)
{
	obs_data_t *data = obs_source_get_private_settings(scene);

	obs_data_set_string(data, "transition", td.name.c_str());
	obs_data_set_int(data, "transition_duration", td.duration);

	obs_data_release(data);
}

void setNextTransition(const sceneSwitchInfo &sceneSwitch,
		       obs_source_t *currentSource, transitionData &td)
{
	// Priority:
	// 1. Transition tab
	// 2. Individual switcher entry
	// 3. Current transition settings

	// Transition Tab
	obs_weak_source_t *currentScene =
		obs_source_get_weak_source(currentSource);
	auto tinfo = getNextTransition(currentScene, sceneSwitch.scene);
	obs_weak_source_release(currentScene);

	OBSWeakSource nextTransition = tinfo.first;
	int nextTransitionDuration = tinfo.second;

	// Individual switcher entry
	if (!nextTransition) {
		nextTransition = sceneSwitch.transition;
	}
	if (!nextTransitionDuration) {
		nextTransitionDuration = sceneSwitch.duration;
	}

	// Current transition settings
	if (!nextTransition) {
		auto ct = obs_frontend_get_current_transition();
		nextTransition = obs_source_get_weak_source(ct);
		obs_weak_source_release(nextTransition);
		obs_source_release(ct);
	}
	if (!nextTransitionDuration) {
		nextTransitionDuration = obs_frontend_get_transition_duration();
	}

	if (switcher->adjustActiveTransitionType) {
		obs_frontend_set_transition_duration(nextTransitionDuration);
		auto t = obs_weak_source_get_source(nextTransition);
		obs_frontend_set_current_transition(t);
		obs_source_release(t);
	}

	if (switcher->transitionOverrideOverride) {
		overwriteTransitionOverride({sceneSwitch.scene, nextTransition,
					     nextTransitionDuration},
					    td);
	}
}

void SwitcherData::saveSceneTransitions(obs_data_t *obj)
{
	obs_data_array_t *sceneTransitionsArray = obs_data_array_create();
	for (SceneTransition &s : sceneTransitions) {
		obs_data_t *array_obj = obs_data_create();

		s.save(array_obj);
		obs_data_array_push_back(sceneTransitionsArray, array_obj);

		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "sceneTransitions", sceneTransitionsArray);
	obs_data_array_release(sceneTransitionsArray);

	obs_data_array_t *defaultTransitionsArray = obs_data_array_create();
	for (DefaultSceneTransition &s : defaultSceneTransitions) {
		obs_data_t *array_obj = obs_data_create();

		s.save(array_obj);
		obs_data_array_push_back(defaultTransitionsArray, array_obj);

		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "defaultTransitions", defaultTransitionsArray);
	obs_data_array_release(defaultTransitionsArray);
	obs_data_set_default_int(obj, "defTransitionDelay",
				 default_def_transition_dealy);
	obs_data_set_int(obj, "defTransitionDelay",
			 DefaultSceneTransition::delay);
}

void SwitcherData::loadSceneTransitions(obs_data_t *obj)
{
	sceneTransitions.clear();

	obs_data_array_t *sceneTransitionsArray =
		obs_data_get_array(obj, "sceneTransitions");
	size_t count = obs_data_array_count(sceneTransitionsArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj =
			obs_data_array_item(sceneTransitionsArray, i);
		sceneTransitions.emplace_back();
		sceneTransitions.back().load(array_obj);
		obs_data_release(array_obj);
	}
	obs_data_array_release(sceneTransitionsArray);

	defaultSceneTransitions.clear();

	obs_data_array_t *defaultTransitionsArray =
		obs_data_get_array(obj, "defaultTransitions");
	count = obs_data_array_count(defaultTransitionsArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj =
			obs_data_array_item(defaultTransitionsArray, i);
		defaultSceneTransitions.emplace_back();
		defaultSceneTransitions.back().load(array_obj);
		obs_data_release(array_obj);
	}
	obs_data_array_release(defaultTransitionsArray);

	// Check for invalid config
	if (!transitionOverrideOverride && !adjustActiveTransitionType) {
		adjustActiveTransitionType = true;
	}

	DefaultSceneTransition::delay =
		obs_data_get_int(obj, "defTransitionDelay");
}

void AdvSceneSwitcher::SetupTransitionsTab()
{
	for (auto &s : switcher->sceneTransitions) {
		QListWidgetItem *item;
		item = new QListWidgetItem(ui->sceneTransitions);
		ui->sceneTransitions->addItem(item);
		TransitionSwitchWidget *sw =
			new TransitionSwitchWidget(this, &s);
		item->setSizeHint(sw->minimumSizeHint());
		ui->sceneTransitions->setItemWidget(item, sw);
	}

	if (switcher->sceneTransitions.size() == 0) {
		ui->transitionHelp->setVisible(true);
	} else {
		ui->transitionHelp->setVisible(false);
	}

	for (auto &s : switcher->defaultSceneTransitions) {
		QListWidgetItem *item;
		item = new QListWidgetItem(ui->defaultTransitions);
		ui->defaultTransitions->addItem(item);
		DefTransitionSwitchWidget *sw =
			new DefTransitionSwitchWidget(this, &s);
		item->setSizeHint(sw->minimumSizeHint());
		ui->defaultTransitions->setItemWidget(item, sw);
	}

	if (switcher->defaultSceneTransitions.size() == 0) {
		ui->defaultTransitionHelp->setVisible(true);
	} else {
		ui->defaultTransitionHelp->setVisible(false);
	}

	ui->transitionOverridecheckBox->setChecked(
		switcher->transitionOverrideOverride);
	ui->adjustActiveTransitionType->setChecked(
		switcher->adjustActiveTransitionType);

	QSpinBox *defTransitionDelay = new QSpinBox();
	defTransitionDelay->setSuffix("ms");
	defTransitionDelay->setMinimum(50);
	defTransitionDelay->setMaximum(10000);
	defTransitionDelay->setValue(DefaultSceneTransition::delay);
	defTransitionDelay->setToolTip(obs_module_text(
		"AdvSceneSwitcher.transitionTab.defaultTransition.delay.help"));

	QWidget::connect(defTransitionDelay, SIGNAL(valueChanged(int)), this,
			 SLOT(DefTransitionDelayValueChanged(int)));

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{defTransitionDelay}}", defTransitionDelay}};
	placeWidgets(
		obs_module_text(
			"AdvSceneSwitcher.transitionTab.defaultTransition.delay"),
		ui->defTransitionDelayLayout, widgetPlaceholders);
}

bool SceneTransition::initialized()
{
	return SceneSwitcherEntry::initialized() && scene2;
}

bool SceneTransition::valid()
{
	return !initialized() ||
	       (SceneSwitcherEntry::valid() && WeakSourceValid(scene2));
}

void SceneTransition::save(obs_data_t *obj)
{
	SceneSwitcherEntry::save(obj, "targetType", "Scene1");
	obs_data_set_string(obj, "Scene2", GetWeakSourceName(scene2).c_str());
	obs_data_set_double(obj, "duration", duration);
}

void SceneTransition::load(obs_data_t *obj)
{
	SceneSwitcherEntry::load(obj, "targetType", "Scene1");
	const char *sourceName = obs_data_get_string(obj, "Scene2");
	scene2 = GetWeakSourceByName(sourceName);
	duration = obs_data_get_double(obj, "duration");
}

TransitionSwitchWidget::TransitionSwitchWidget(QWidget *parent,
					       SceneTransition *s)
	: SwitchWidget(parent, s, false, false, false)
{
	scenes2 = new QComboBox();
	duration = new QDoubleSpinBox();

	duration->setMinimum(0.0);
	duration->setMaximum(99.000000);
	duration->setSuffix("s");

	QWidget::connect(scenes2, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(Scene2Changed(const QString &)));
	QWidget::connect(duration, SIGNAL(valueChanged(double)), this,
			 SLOT(DurationChanged(double)));

	populateSceneSelection(scenes2);

	if (s) {
		scenes2->setCurrentText(GetWeakSourceName(s->scene2).c_str());
		duration->setValue(s->duration);
	}

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{scenes}}", scenes},
		{"{{scenes2}}", scenes2},
		{"{{duration}}", duration},
		{"{{transitions}}", transitions}};
	placeWidgets(obs_module_text("AdvSceneSwitcher.transitionTab.entry"),
		     mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	switchData = s;

	loading = false;
}

SceneTransition *TransitionSwitchWidget::getSwitchData()
{
	return switchData;
}

void TransitionSwitchWidget::setSwitchData(SceneTransition *s)
{
	switchData = s;
}

void TransitionSwitchWidget::swapSwitchData(TransitionSwitchWidget *s1,
					    TransitionSwitchWidget *s2)
{
	SwitchWidget::swapSwitchData(s1, s2);

	SceneTransition *t = s1->getSwitchData();
	s1->setSwitchData(s2->getSwitchData());
	s2->setSwitchData(t);
}

void TransitionSwitchWidget::Scene2Changed(const QString &text)
{
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->scene2 = GetWeakSourceByQString(text);
}

void TransitionSwitchWidget::DurationChanged(double dur)
{
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->duration = dur;
}

DefTransitionSwitchWidget::DefTransitionSwitchWidget(QWidget *parent,
						     DefaultSceneTransition *s)
	: SwitchWidget(parent, s, false, false, false)
{
	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{scenes}}", scenes}, {"{{transitions}}", transitions}};
	placeWidgets(
		obs_module_text(
			"AdvSceneSwitcher.transitionTab.defaultTransitionEntry"),
		mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	switchData = s;

	loading = false;
}

DefaultSceneTransition *DefTransitionSwitchWidget::getSwitchData()
{
	return switchData;
}

void DefTransitionSwitchWidget::setSwitchData(DefaultSceneTransition *s)
{
	switchData = s;
}

void DefTransitionSwitchWidget::swapSwitchData(DefTransitionSwitchWidget *s1,
					       DefTransitionSwitchWidget *s2)
{
	SwitchWidget::swapSwitchData(s1, s2);

	DefaultSceneTransition *t = s1->getSwitchData();
	s1->setSwitchData(s2->getSwitchData());
	s2->setSwitchData(t);
}

void DefaultSceneTransition::save(obs_data_t *obj)
{
	SceneSwitcherEntry::save(obj, "targetType", "Scene");
}

void DefaultSceneTransition::load(obs_data_t *obj)
{
	SceneSwitcherEntry::load(obj, "targetType", "Scene");
}

bool DefaultSceneTransition::checkMatch(OBSWeakSource currentScene)
{
	return scene == currentScene;
}

void setTransitionDelayed(OBSWeakSource transition, unsigned int delay)
{
	// A hardcoded delay of 50 ms before switching transition type is
	// necessary due to OBS_FRONTEND_EVENT_SCENE_CHANGED seemingly firing a
	// bit too early and thus leading to canceled transitions.
	//
	// The same is to be the case for OBS_FRONTEND_EVENT_TRANSITION_STOPPED.
	//
	// 50 ms was chosen as a default value as it seems to avoid the problem
	// mentioned above and becuase that is the minimum value which can be
	// chosen for the scene switcher's check interval.
	// Thus it can be made sure that the delayed setting of the transition
	// does not interfere with any new scene changes triggered by the scene
	// switcher

	std::this_thread::sleep_for(std::chrono::milliseconds(delay));

	obs_source_t *transitionSource = obs_weak_source_get_source(transition);
	obs_frontend_set_current_transition(transitionSource);
	obs_source_release(transitionSource);
}

void DefaultSceneTransition::setTransition()
{
	std::thread t;
	t = std::thread(setTransitionDelayed, transition, delay);
	t.detach();
}

} // namespace advss
