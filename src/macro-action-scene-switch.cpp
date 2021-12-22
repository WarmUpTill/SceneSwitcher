#include "headers/macro-action-scene-switch.hpp"
#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

const std::string MacroActionSwitchScene::id = "scene_switch";

bool MacroActionSwitchScene::_registered = MacroActionFactory::Register(
	MacroActionSwitchScene::id,
	{MacroActionSwitchScene::Create, MacroActionSwitchSceneEdit::Create,
	 "AdvSceneSwitcher.action.switchScene"});

void waitForTransitionChange(OBSWeakSource &target)
{
	const int maxNrChecks = 100;
	int curCheckNr = 0;
	bool transitionEnded = false;

	auto sceneCheckStartedOn = obs_frontend_get_current_scene();
	obs_source_release(sceneCheckStartedOn);
	switcher->abortMacroWait = false;
	std::unique_lock<std::mutex> lock(switcher->m);
	while (!transitionEnded && curCheckNr < maxNrChecks &&
	       !switcher->abortMacroWait) {
		switcher->abortMacroWait = false;
		switcher->macroTransitionCv.wait_for(
			lock,
			std::chrono::milliseconds(
				(long long)switcher->interval),
			[] { return switcher->abortMacroWait.load(); });
		auto curScene = obs_frontend_get_current_scene();
		obs_source_release(curScene);
		transitionEnded = curScene != sceneCheckStartedOn ||
				  switcher->currentScene == target;
		++curCheckNr;
	}
}

bool MacroActionSwitchScene::PerformAction()
{
	OBSWeakSource scene = getScene();
	switchScene({scene, transition, (int)(_duration.seconds * 1000)});
	if (_blockUntilTransitionDone) {
		waitForTransitionChange(scene);
		return !switcher->abortMacroWait;
	}
	return true;
}

void MacroActionSwitchScene::LogAction()
{
	if (switcher->verbose) {
		logMatch();
	}
}

bool MacroActionSwitchScene::Save(obs_data_t *obj)
{
	MacroAction::Save(obj);
	SceneSwitcherEntry::save(obj);
	_duration.Save(obj);
	obs_data_set_bool(obj, "blockUntilTransitionDone",
			  _blockUntilTransitionDone);
	return true;
}

bool MacroActionSwitchScene::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	SceneSwitcherEntry::load(obj);
	_duration.Load(obj);
	_blockUntilTransitionDone =
		obs_data_get_bool(obj, "blockUntilTransitionDone");
	return true;
}

std::string MacroActionSwitchScene::GetShortDesc()
{
	if (targetType == SwitchTargetType::Scene && scene) {
		return GetWeakSourceName(scene);
	}
	if (targetType == SwitchTargetType::SceneGroup && group) {
		return group->name;
	}
	return "";
}

MacroActionSwitchSceneEdit::MacroActionSwitchSceneEdit(
	QWidget *parent, std::shared_ptr<MacroActionSwitchScene> entryData)
	: SwitchWidget(parent, entryData.get(), true, true)
{
	_duration = new DurationSelection(parent, false);
	_blockUntilTransitionDone = new QCheckBox(obs_module_text(
		"AdvSceneSwitcher.action.scene.blockUntilTransitionDone"));

	QWidget::connect(_duration, SIGNAL(DurationChanged(double)), this,
			 SLOT(DurationChanged(double)));
	QWidget::connect(_blockUntilTransitionDone, SIGNAL(stateChanged(int)),
			 this, SLOT(BlockUntilTransitionDoneChanged(int)));
	QWidget::connect(scenes, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(ChangeHeaderInfo(const QString &)));

	QHBoxLayout *entryLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{scenes}}", scenes},
		{"{{transitions}}", transitions},
		{"{{duration}}", _duration},
		{"{{blockUntilTransitionDone}}", _blockUntilTransitionDone},
	};
	placeWidgets(obs_module_text("AdvSceneSwitcher.action.scene.entry"),
		     entryLayout, widgetPlaceholders);

	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(entryLayout);
	mainLayout->addWidget(_blockUntilTransitionDone);
	setLayout(mainLayout);

	_entryData = entryData;
	_duration->SetDuration(_entryData->_duration);
	_blockUntilTransitionDone->setChecked(
		_entryData->_blockUntilTransitionDone);
	SwitchWidget::loading = false;
}

void MacroActionSwitchSceneEdit::DurationChanged(double seconds)
{
	if (SwitchWidget::loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_duration.seconds = seconds;
}

void MacroActionSwitchSceneEdit::BlockUntilTransitionDoneChanged(int state)
{
	if (SwitchWidget::loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_blockUntilTransitionDone = state;
}

void MacroActionSwitchSceneEdit::ChangeHeaderInfo(const QString &)
{
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}
