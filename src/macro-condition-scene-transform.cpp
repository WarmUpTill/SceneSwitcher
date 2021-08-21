#include "headers/macro-condition-edit.hpp"
#include "headers/macro-condition-scene-transform.hpp"
#include "headers/utility.hpp"
#include "headers/advanced-scene-switcher.hpp"

const std::string MacroConditionSceneTransform::id = "scene_transform";

bool MacroConditionSceneTransform::_registered =
	MacroConditionFactory::Register(
		MacroConditionSceneTransform::id,
		{MacroConditionSceneTransform::Create,
		 MacroConditionSceneTransformEdit::Create,
		 "AdvSceneSwitcher.condition.sceneTransform"});

bool MacroConditionSceneTransform::CheckCondition()
{
	bool ret = false;
	auto s = obs_weak_source_get_source(_scene.GetScene(false));
	auto scene = obs_scene_from_source(s);
	auto name = GetWeakSourceName(_source);
	auto items = getSceneItemsWithName(scene, name);

	for (auto &item : items) {
		struct obs_transform_info info;
		struct obs_sceneitem_crop crop;
		obs_sceneitem_get_info(item, &info);
		obs_sceneitem_get_crop(item, &crop);

		auto data = obs_data_create();
		saveTransformState(data, info, crop);
		auto json = obs_data_get_json(data);

		if (matchJson(json, _settings, _regex)) {
			ret = true;
		}

		obs_data_release(data);
		obs_sceneitem_release(item);
	}

	obs_source_release(s);
	return ret;
}

bool MacroConditionSceneTransform::Save(obs_data_t *obj)
{
	MacroCondition::Save(obj);
	_scene.Save(obj);
	obs_data_set_string(obj, "source", GetWeakSourceName(_source).c_str());
	obs_data_set_string(obj, "settings", _settings.c_str());
	obs_data_set_bool(obj, "regex", _regex);
	return true;
}

bool MacroConditionSceneTransform::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_scene.Load(obj);
	const char *sourceName = obs_data_get_string(obj, "source");
	_source = GetWeakSourceByName(sourceName);
	_settings = obs_data_get_string(obj, "settings");
	_regex = obs_data_get_bool(obj, "regex");
	return true;
}

std::string MacroConditionSceneTransform::GetShortDesc()
{
	if (_source) {
		return _scene.ToString() + " - " + GetWeakSourceName(_source);
	}
	return "";
}

MacroConditionSceneTransformEdit::MacroConditionSceneTransformEdit(
	QWidget *parent,
	std::shared_ptr<MacroConditionSceneTransform> entryData)
	: QWidget(parent)
{
	_scenes = new SceneSelectionWidget(window(), false, false, true);
	_sources = new QComboBox();
	_getSettings = new QPushButton(obs_module_text(
		"AdvSceneSwitcher.condition.sceneTransform.getTransform"));
	_settings = new QPlainTextEdit();
	_regex = new QCheckBox(obs_module_text(
		"AdvSceneSwitcher.condition.sceneTransform.regex"));

	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 this, SLOT(SceneChanged(const SceneSelection &)));
	QWidget::connect(_sources, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(SourceChanged(const QString &)));
	QWidget::connect(_getSettings, SIGNAL(clicked()), this,
			 SLOT(GetSettingsClicked()));
	QWidget::connect(_settings, SIGNAL(textChanged()), this,
			 SLOT(SettingsChanged()));
	QWidget::connect(_regex, SIGNAL(stateChanged(int)), this,
			 SLOT(RegexChanged(int)));

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{scenes}}", _scenes},     {"{{sources}}", _sources},
		{"{{settings}}", _settings}, {"{{getSettings}}", _getSettings},
		{"{{regex}}", _regex},
	};

	QHBoxLayout *line1Layout = new QHBoxLayout;
	placeWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.sceneTransform.entry.line1"),
		line1Layout, widgetPlaceholders);
	QHBoxLayout *line2Layout = new QHBoxLayout;
	placeWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.sceneTransform.entry.line2"),
		line2Layout, widgetPlaceholders, false);
	QHBoxLayout *line3Layout = new QHBoxLayout;
	placeWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.sceneTransform.entry.line3"),
		line3Layout, widgetPlaceholders);

	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(line1Layout);
	mainLayout->addLayout(line2Layout);
	mainLayout->addLayout(line3Layout);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionSceneTransformEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_scenes->SetScene(_entryData->_scene);
	populateSceneItemSelection(_sources, _entryData->_scene);
	_sources->setCurrentText(
		GetWeakSourceName(_entryData->_source).c_str());
	_regex->setChecked(_entryData->_regex);
	if (_entryData->_source) {
		_settings->setPlainText(
			QString::fromStdString(_entryData->_settings));
	}
}

void MacroConditionSceneTransformEdit::SceneChanged(const SceneSelection &s)
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

void MacroConditionSceneTransformEdit::SourceChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_source = GetWeakSourceByQString(text);
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionSceneTransformEdit::GetSettingsClicked()
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

	QString json = formatJsonString(obs_data_get_json(data));
	if (_entryData->_regex) {
		json = escapeForRegex(json);
	}

	_settings->setPlainText(json);
	obs_data_release(data);
}

void MacroConditionSceneTransformEdit::SettingsChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_settings = _settings->toPlainText().toStdString();
}

void MacroConditionSceneTransformEdit::RegexChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_regex = state;
}
