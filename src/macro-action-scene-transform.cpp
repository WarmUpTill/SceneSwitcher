#include "headers/macro-action-scene-transform.hpp"
#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

const std::string MacroActionSceneTransform::id = "scene_transform";

bool MacroActionSceneTransform::_registered = MacroActionFactory::Register(
	MacroActionSceneTransform::id,
	{MacroActionSceneTransform::Create,
	 MacroActionSceneTransformEdit::Create,
	 "AdvSceneSwitcher.action.sceneTransform"});

bool MacroActionSceneTransform::PerformAction()
{
	auto s = obs_weak_source_get_source(_scene.GetScene(false));
	auto scene = obs_scene_from_source(s);
	auto name = GetWeakSourceName(_source);
	auto items = getSceneItemsWithName(scene, name);

	for (auto &item : items) {
		// Merge new settings with current settings
		struct obs_transform_info info;
		struct obs_sceneitem_crop crop;
		obs_sceneitem_get_info(item, &info);
		obs_sceneitem_get_crop(item, &crop);
		auto temp = obs_data_create();
		saveTransformState(temp, info, crop);
		auto temp2 = obs_data_create();
		saveTransformState(temp, _info, _crop);
		obs_data_apply(temp, temp2);
		loadTransformState(temp, info, crop);
		obs_data_release(temp);
		obs_data_release(temp2);

		// Apply settings
		obs_sceneitem_defer_update_begin(item);
		obs_sceneitem_set_info(item, &info);
		obs_sceneitem_set_crop(item, &crop);
		obs_sceneitem_defer_update_end(item);

		obs_sceneitem_release(item);
	}

	obs_source_release(s);
	return true;
}

void MacroActionSceneTransform::LogAction()
{
	vblog(LOG_INFO,
	      "performed transform action for source \"%s\" on scene \"%s\"",
	      GetWeakSourceName(_source).c_str(), _scene.ToString().c_str());
}

bool MacroActionSceneTransform::Save(obs_data_t *obj)
{
	MacroAction::Save(obj);
	_scene.Save(obj);
	obs_data_set_string(obj, "source", GetWeakSourceName(_source).c_str());
	saveTransformState(obj, _info, _crop);
	return true;
}

bool MacroActionSceneTransform::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_scene.Load(obj);
	const char *sourceName = obs_data_get_string(obj, "source");
	_source = GetWeakSourceByName(sourceName);
	loadTransformState(obj, _info, _crop);
	return true;
}

std::string MacroActionSceneTransform::GetShortDesc()
{
	if (_source) {
		return _scene.ToString() + " - " + GetWeakSourceName(_source);
	}
	return "";
}

std::string MacroActionSceneTransform::GetSettings()
{
	auto data = obs_data_create();
	saveTransformState(data, _info, _crop);
	std::string json = obs_data_get_json(data);
	obs_data_release(data);
	return json;
}

void MacroActionSceneTransform::SetSettings(std::string &settings)
{
	auto data = obs_data_create_from_json(settings.c_str());
	if (!data) {
		return;
	}
	loadTransformState(data, _info, _crop);
	obs_data_release(data);
}

MacroActionSceneTransformEdit::MacroActionSceneTransformEdit(
	QWidget *parent, std::shared_ptr<MacroActionSceneTransform> entryData)
	: QWidget(parent)
{
	_scenes = new SceneSelectionWidget(window(), false, false, true);
	_sources = new QComboBox();
	_getSettings = new QPushButton(obs_module_text(
		"AdvSceneSwitcher.action.sceneTransform.getTransform"));
	_settings = new QPlainTextEdit();

	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 this, SLOT(SceneChanged(const SceneSelection &)));
	QWidget::connect(_sources, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(SourceChanged(const QString &)));
	QWidget::connect(_getSettings, SIGNAL(clicked()), this,
			 SLOT(GetSettingsClicked()));
	QWidget::connect(_settings, SIGNAL(textChanged()), this,
			 SLOT(SettingsChanged()));

	QHBoxLayout *entryLayout = new QHBoxLayout;

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{scenes}}", _scenes},
		{"{{sources}}", _sources},
		{"{{settings}}", _settings},
		{"{{getSettings}}", _getSettings},
	};
	placeWidgets(
		obs_module_text("AdvSceneSwitcher.action.sceneTransform.entry"),
		entryLayout, widgetPlaceholders);

	QHBoxLayout *buttonLayout = new QHBoxLayout;
	buttonLayout->addWidget(_getSettings);
	buttonLayout->addStretch();

	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(entryLayout);
	mainLayout->addWidget(_settings);
	mainLayout->addLayout(buttonLayout);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionSceneTransformEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_scenes->SetScene(_entryData->_scene);
	populateSceneItemSelection(_sources, _entryData->_scene);
	_sources->setCurrentText(
		GetWeakSourceName(_entryData->_source).c_str());
	if (_entryData->_source) {
		_settings->setPlainText(
			formatJsonString(_entryData->GetSettings()));
	}
}

void MacroActionSceneTransformEdit::SceneChanged(const SceneSelection &s)
{
	if (_loading || !_entryData) {
		return;
	}
	{
		std::lock_guard<std::mutex> lock(switcher->m);
		_entryData->_scene = s;
	}
	_sources->clear();
	populateSceneItemSelection(_sources, _entryData->_scene);
}

void MacroActionSceneTransformEdit::SourceChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_source = GetWeakSourceByQString(text);
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionSceneTransformEdit::GetSettingsClicked()
{
	if (_loading || !_entryData || !_entryData->_scene.GetScene(false) ||
	    !_entryData->_source) {
		return;
	}

	auto s = obs_weak_source_get_source(_entryData->_scene.GetScene(false));
	auto scene = obs_scene_from_source(s);
	auto name = GetWeakSourceName(_entryData->_source);
	auto item = obs_scene_find_source_recursive(scene, name.c_str());
	obs_source_release(s);

	if (!item) {
		return;
	}

	struct obs_transform_info info;
	struct obs_sceneitem_crop crop;
	obs_sceneitem_get_info(item, &info);
	obs_sceneitem_get_crop(item, &crop);

	auto data = obs_data_create();
	saveTransformState(data, info, crop);
	auto json = obs_data_get_json(data);
	_settings->setPlainText(formatJsonString(json));
	obs_data_release(data);
}

void MacroActionSceneTransformEdit::SettingsChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	auto json = _settings->toPlainText().toStdString();
	_entryData->SetSettings(json);
}
