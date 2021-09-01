#include "headers/macro-action-preview-scene.hpp"
#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

const std::string MacroActionPreviewScene::id = "preview_scene";

bool MacroActionPreviewScene::_registered = MacroActionFactory::Register(
	MacroActionPreviewScene::id,
	{MacroActionPreviewScene::Create, MacroActionPreviewSceneEdit::Create,
	 "AdvSceneSwitcher.action.previewScene"});

bool MacroActionPreviewScene::PerformAction()
{
	auto s = obs_weak_source_get_source(_scene.GetScene());
	obs_frontend_set_current_preview_scene(s);
	obs_source_release(s);
	return true;
}

void MacroActionPreviewScene::LogAction()
{
	vblog(LOG_INFO, "set preview scene to \"%s\"",
	      _scene.ToString().c_str());
}

bool MacroActionPreviewScene::Save(obs_data_t *obj)
{
	MacroAction::Save(obj);
	_scene.Save(obj);
	return true;
}

bool MacroActionPreviewScene::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_scene.Load(obj);
	const char *sourceName = obs_data_get_string(obj, "source");
	return true;
}

std::string MacroActionPreviewScene::GetShortDesc()
{
	return _scene.ToString();
}

MacroActionPreviewSceneEdit::MacroActionPreviewSceneEdit(
	QWidget *parent, std::shared_ptr<MacroActionPreviewScene> entryData)
	: QWidget(parent)
{
	_scenes = new SceneSelectionWidget(window(), false, false, false);

	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 this, SLOT(SceneChanged(const SceneSelection &)));

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{scenes}}", _scenes},
	};
	placeWidgets(
		obs_module_text("AdvSceneSwitcher.action.previewScene.entry"),
		mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionPreviewSceneEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_scenes->SetScene(_entryData->_scene);
}

void MacroActionPreviewSceneEdit::SceneChanged(const SceneSelection &s)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_scene = s;
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}
