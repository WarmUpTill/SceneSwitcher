#include "macro-action-scene-transform.hpp"
#include "advanced-scene-switcher.hpp"
#include "utility.hpp"

namespace advss {

const std::string MacroActionSceneTransform::id = "scene_transform";

bool MacroActionSceneTransform::_registered = MacroActionFactory::Register(
	MacroActionSceneTransform::id,
	{MacroActionSceneTransform::Create,
	 MacroActionSceneTransformEdit::Create,
	 "AdvSceneSwitcher.action.sceneTransform"});

bool MacroActionSceneTransform::PerformAction()
{
	ApplySettings(_settings); // Resolves variables

	auto items = _source.GetSceneItems(_scene);
	for (auto &item : items) {
		obs_sceneitem_defer_update_begin(item);
		obs_sceneitem_set_info(item, &_info);
		obs_sceneitem_set_crop(item, &_crop);
		obs_sceneitem_defer_update_end(item);

		obs_sceneitem_release(item);
	}
	return true;
}

void MacroActionSceneTransform::LogAction() const
{
	vblog(LOG_INFO,
	      "performed transform action for source \"%s\" on scene \"%s\"",
	      _source.ToString(true).c_str(), _scene.ToString(true).c_str());
}

bool MacroActionSceneTransform::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	_scene.Save(obj);
	_source.Save(obj);
	_settings.Save(obj, "settings");
	return true;
}

bool MacroActionSceneTransform::Load(obs_data_t *obj)
{
	// Convert old data format
	// TODO: Remove in future version
	if (obs_data_has_user_value(obj, "source")) {
		auto sourceName = obs_data_get_string(obj, "source");
		obs_data_set_string(obj, "sceneItem", sourceName);
	}

	MacroAction::Load(obj);
	_scene.Load(obj);
	_source.Load(obj);
	_settings.Load(obj, "settings");

	// Convert old data format
	// TODO: Remove in future version
	if (!obs_data_has_user_value(obj, "settings")) {
		loadTransformState(obj, _info, _crop);
		_settings = ConvertSettings();
	}
	return true;
}

std::string MacroActionSceneTransform::GetShortDesc() const
{
	if (_source.ToString().empty()) {
		return "";
	}
	return _scene.ToString() + " - " + _source.ToString();
}

std::string MacroActionSceneTransform::ConvertSettings()
{
	auto data = obs_data_create();
	saveTransformState(data, _info, _crop);
	std::string json = obs_data_get_json(data);
	obs_data_release(data);
	return json;
}

void MacroActionSceneTransform::ApplySettings(const std::string &settings)
{
	auto data = obs_data_create_from_json(settings.c_str());
	if (!data) {
		return;
	}
	loadTransformState(data, _info, _crop);
	auto items = _source.GetSceneItems(_scene);
	if (items.empty()) {
		return;
	}
	if (obs_data_has_user_value(data, "size")) {
		auto obj = obs_data_get_obj(data, "size");
		auto source = obs_sceneitem_get_source(items[0]);
		if (double h = obs_data_get_double(obj, "height")) {
			_info.scale.y =
				h / double(obs_source_get_height(source));
		}
		if (double w = obs_data_get_double(obj, "width")) {
			_info.scale.x =
				w / double(obs_source_get_width(source));
		}
		obs_data_release(obj);
	}
	for (auto item : items) {
		obs_sceneitem_release(item);
	}
	obs_data_release(data);
}

MacroActionSceneTransformEdit::MacroActionSceneTransformEdit(
	QWidget *parent, std::shared_ptr<MacroActionSceneTransform> entryData)
	: QWidget(parent)
{
	_scenes = new SceneSelectionWidget(window(), true, false, false, true);
	_sources = new SceneItemSelectionWidget(parent);
	_getSettings = new QPushButton(obs_module_text(
		"AdvSceneSwitcher.action.sceneTransform.getTransform"));
	_settings = new VariableTextEdit(this);

	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 this, SLOT(SceneChanged(const SceneSelection &)));
	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 _sources, SLOT(SceneChanged(const SceneSelection &)));
	QWidget::connect(_sources,
			 SIGNAL(SceneItemChanged(const SceneItemSelection &)),
			 this, SLOT(SourceChanged(const SceneItemSelection &)));
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
	_sources->SetSceneItem(_entryData->_source);
	_settings->setPlainText(_entryData->_settings);

	adjustSize();
	updateGeometry();
}

void MacroActionSceneTransformEdit::SceneChanged(const SceneSelection &s)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_scene = s;
}

void MacroActionSceneTransformEdit::SourceChanged(const SceneItemSelection &item)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_source = item;
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionSceneTransformEdit::GetSettingsClicked()
{
	if (_loading || !_entryData || !_entryData->_scene.GetScene(false)) {
		return;
	}

	auto items = _entryData->_source.GetSceneItems(_entryData->_scene);
	if (items.empty()) {
		return;
	}

	auto settings = getSceneItemTransform(items[0]);
	_settings->setPlainText(formatJsonString(settings));
	for (auto item : items) {
		obs_sceneitem_release(item);
	}
}

void MacroActionSceneTransformEdit::SettingsChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_settings = _settings->toPlainText().toStdString();

	adjustSize();
	updateGeometry();
}

} // namespace advss
