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
	auto items = _source.GetSceneItems(_scene);

	for (auto &item : items) {
		auto json = getSceneItemTransform(item);
		if (matchJson(json, _settings, _regex)) {
			ret = true;
		}
		obs_sceneitem_release(item);
	}
	return ret;
}

bool MacroConditionSceneTransform::Save(obs_data_t *obj)
{
	MacroCondition::Save(obj);
	_scene.Save(obj);
	_source.Save(obj);
	obs_data_set_string(obj, "settings", _settings.c_str());
	obs_data_set_bool(obj, "regex", _regex);
	return true;
}

bool MacroConditionSceneTransform::Load(obs_data_t *obj)
{
	// Convert old data format
	// TODO: Remove in future version
	if (obs_data_has_user_value(obj, "source")) {
		auto sourceName = obs_data_get_string(obj, "source");
		obs_data_set_string(obj, "sceneItem", sourceName);
	}

	MacroCondition::Load(obj);
	_scene.Load(obj);
	_source.Load(obj);
	_settings = obs_data_get_string(obj, "settings");
	_regex = obs_data_get_bool(obj, "regex");
	return true;
}

std::string MacroConditionSceneTransform::GetShortDesc()
{
	if (_source.ToString().empty()) {
		return "";
	}

	return _scene.ToString() + " - " + _source.ToString();
}

MacroConditionSceneTransformEdit::MacroConditionSceneTransformEdit(
	QWidget *parent,
	std::shared_ptr<MacroConditionSceneTransform> entryData)
	: QWidget(parent)
{
	_scenes = new SceneSelectionWidget(window(), false, false, true);
	_sources = new SceneItemSelectionWidget(
		parent, true, SceneItemSelectionWidget::AllSelectionType::ANY);
	_getSettings = new QPushButton(obs_module_text(
		"AdvSceneSwitcher.condition.sceneTransform.getTransform"));
	_settings = new QPlainTextEdit();
	_regex = new QCheckBox(obs_module_text(
		"AdvSceneSwitcher.condition.sceneTransform.regex"));

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
	_sources->SetSceneItem(_entryData->_source);
	_regex->setChecked(_entryData->_regex);
	_settings->setPlainText(QString::fromStdString(_entryData->_settings));
}

void MacroConditionSceneTransformEdit::SceneChanged(const SceneSelection &s)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_scene = s;
}

void MacroConditionSceneTransformEdit::SourceChanged(
	const SceneItemSelection &item)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_source = item;
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionSceneTransformEdit::GetSettingsClicked()
{

	if (_loading || !_entryData || !_entryData->_scene.GetScene(false)) {
		return;
	}

	auto items = _entryData->_source.GetSceneItems(_entryData->_scene);
	if (items.empty()) {
		return;
	}

	auto settings = formatJsonString(getSceneItemTransform(items[0]));
	if (_entryData->_regex) {
		settings = escapeForRegex(settings);
	}
	_settings->setPlainText(settings);

	for (auto item : items) {
		obs_sceneitem_release(item);
	}
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
