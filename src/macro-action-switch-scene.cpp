#include "headers/macro-action-switch-scene.hpp"
#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

const int MacroActionSwitchScene::id = 0;

bool MacroActionSwitchScene::_registered = MacroActionFactory::Register(
	MacroActionSwitchScene::id,
	{MacroActionSwitchScene::Create, MacroActionSwitchSceneEdit::Create,
	 "AdvSceneSwitcher.action.switchScene"});

bool MacroActionSwitchScene::PerformAction()
{
	OBSWeakSource scene = getScene();
	switchScene({scene, transition, (int)(_duration.seconds * 1000)});
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
	return true;
}

bool MacroActionSwitchScene::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	SceneSwitcherEntry::load(obj);
	_duration.Load(obj);
	return true;
}

MacroActionSwitchSceneEdit::MacroActionSwitchSceneEdit(
	QWidget *parent, std::shared_ptr<MacroActionSwitchScene> entryData)
	: SwitchWidget(parent, entryData.get(), true, true)
{
	_duration = new DurationSelection(parent, false);
	QWidget::connect(_duration, SIGNAL(DurationChanged(double)), this,
			 SLOT(DurationChanged(double)));

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{scenes}}", scenes},
		{"{{transitions}}", transitions},
		{"{{duration}}", _duration},
	};
	placeWidgets(obs_module_text("AdvSceneSwitcher.action.scene.entry"),
		     mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	_entryData = entryData;
	SwitchWidget::loading = false;
	_duration->SetDuration(_entryData->_duration);
}

void MacroActionSwitchSceneEdit::DurationChanged(double seconds)
{
	if (SwitchWidget::loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_duration.seconds = seconds;
}
