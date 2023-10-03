#include "macro-condition-scene-transform.hpp"
#include "utility.hpp"

namespace advss {

const std::string MacroConditionSceneTransform::id = "scene_transform";

bool MacroConditionSceneTransform::_registered =
	MacroConditionFactory::Register(
		MacroConditionSceneTransform::id,
		{MacroConditionSceneTransform::Create,
		 MacroConditionSceneTransformEdit::Create,
		 "AdvSceneSwitcher.condition.sceneTransform"});

const static std::map<MacroConditionSceneTransform::CondType, std::string>
	conditionTypes = {
		{MacroConditionSceneTransform::CondType::MATCHES,
		 "AdvSceneSwitcher.condition.sceneTransform.entry.type.matches"},
		{MacroConditionSceneTransform::CondType::CHANGED,
		 "AdvSceneSwitcher.condition.sceneTransform.entry.type.changed"},
};

static bool doesTransformOfAnySceneItemMatch(
	const std::vector<OBSSceneItem> &items, const std::string &jsonCompare,
	const RegexConfig &regex, std::string &newVariable)
{
	bool ret = false;
	std::string json;
	for (const auto &item : items) {
		json = GetSceneItemTransform(item);
		if (MatchJson(json, jsonCompare, regex)) {
			ret = true;
		}
	}
	newVariable = json;
	return ret;
}

static bool
didTransformOfAnySceneItemChange(const std::vector<OBSSceneItem> &items,
				 std::vector<std::string> &previousTransform,
				 std::string &newVariable)
{
	bool ret = false;
	std::string json;
	RegexConfig regex;
	int n_items = items.size();
	if (previousTransform.size() < n_items) {
		ret = true;
		previousTransform.resize(n_items);
	}
	for (int idx = 0; idx < n_items; ++idx) {
		auto const &item = items[idx];
		json = GetSceneItemTransform(item);
		if (!MatchJson(json, previousTransform[idx], regex)) {
			ret = true;
			previousTransform[idx] = json;
		}
	}
	newVariable = json;
	return ret;
}

bool MacroConditionSceneTransform::CheckCondition()
{
	auto items = _source.GetSceneItems(_scene);
	if (items.empty()) {
		return false;
	}

	std::string newVariable = "";
	bool ret = false;
	switch (_type) {
	case CondType::MATCHES:
		ret = doesTransformOfAnySceneItemMatch(items, _settings, _regex,
						       newVariable);
		break;
	case CondType::CHANGED:
		ret = didTransformOfAnySceneItemChange(
			items, _previousTransform, newVariable);
		break;

	default:
		return false;
	}

	SetVariableValue(newVariable);
	return ret;
}

bool MacroConditionSceneTransform::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	_scene.Save(obj);
	_source.Save(obj);
	_settings.Save(obj, "settings");
	_regex.Save(obj);
	obs_data_set_int(obj, "type", static_cast<int>(_type));
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
	_settings.Load(obj, "settings");
	_regex.Load(obj);
	_type = static_cast<CondType>(obs_data_get_int(obj, "type"));
	// TOOD: remove in future version
	if (obs_data_has_user_value(obj, "regex")) {
		_regex.CreateBackwardsCompatibleRegex(
			obs_data_get_bool(obj, "regex"));
	}
	return true;
}

std::string MacroConditionSceneTransform::GetShortDesc() const
{
	if (_source.ToString().empty()) {
		return "";
	}

	return _scene.ToString() + " - " + _source.ToString();
}

static inline void populateTypesSelection(QComboBox *list)
{
	for (const auto &entry : conditionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroConditionSceneTransformEdit::MacroConditionSceneTransformEdit(
	QWidget *parent,
	std::shared_ptr<MacroConditionSceneTransform> entryData)
	: QWidget(parent),
	  _scenes(new SceneSelectionWidget(window(), true, false, false, true)),
	  _sources(new SceneItemSelectionWidget(
		  parent, true, SceneItemSelectionWidget::Placeholder::ANY)),
	  _types(new QComboBox()),
	  _getSettings(new QPushButton(obs_module_text(
		  "AdvSceneSwitcher.condition.sceneTransform.getTransform"))),
	  _settings(new VariableTextEdit(this)),
	  _regex(new RegexConfigWidget(parent))
{
	populateTypesSelection(_types);

	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 this, SLOT(SceneChanged(const SceneSelection &)));
	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 _sources, SLOT(SceneChanged(const SceneSelection &)));
	QWidget::connect(_sources,
			 SIGNAL(SceneItemChanged(const SceneItemSelection &)),
			 this, SLOT(SourceChanged(const SceneItemSelection &)));
	QWidget::connect(_types, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(TypeChanged(int)));
	QWidget::connect(_getSettings, SIGNAL(clicked()), this,
			 SLOT(GetSettingsClicked()));
	QWidget::connect(_settings, SIGNAL(textChanged()), this,
			 SLOT(SettingsChanged()));
	QWidget::connect(_regex, SIGNAL(RegexConfigChanged(RegexConfig)), this,
			 SLOT(RegexChanged(RegexConfig)));

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{scenes}}", _scenes},
		{"{{sources}}", _sources},
		{"{{types}}", _types},
		{"{{settings}}", _settings},
		{"{{getSettings}}", _getSettings},
		{"{{regex}}", _regex},
	};

	QHBoxLayout *line1Layout = new QHBoxLayout;
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.sceneTransform.entry.line1"),
		line1Layout, widgetPlaceholders);
	QHBoxLayout *line2Layout = new QHBoxLayout;
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.sceneTransform.entry.line2"),
		line2Layout, widgetPlaceholders, false);
	QHBoxLayout *line3Layout = new QHBoxLayout;
	PlaceWidgets(
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
	_types->setCurrentIndex(static_cast<int>(_entryData->_type));
	_regex->SetRegexConfig(_entryData->_regex);
	_settings->setPlainText(_entryData->_settings);
	SetWidgetVisibility();
}

void MacroConditionSceneTransformEdit::SceneChanged(const SceneSelection &s)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_scene = s;
}

void MacroConditionSceneTransformEdit::SourceChanged(
	const SceneItemSelection &item)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_source = item;
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
	adjustSize();
	updateGeometry();
}

void MacroConditionSceneTransformEdit::TypeChanged(int index)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_type =
		static_cast<MacroConditionSceneTransform::CondType>(index);
	SetWidgetVisibility();
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

	auto settings = FormatJsonString(GetSceneItemTransform(items[0]));
	if (_entryData->_regex.Enabled()) {
		settings = EscapeForRegex(settings);
	}
	_settings->setPlainText(settings);
}

void MacroConditionSceneTransformEdit::SettingsChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_settings = _settings->toPlainText().toStdString();

	adjustSize();
	updateGeometry();
}

void MacroConditionSceneTransformEdit::RegexChanged(RegexConfig conf)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_regex = conf;

	adjustSize();
	updateGeometry();
}

void MacroConditionSceneTransformEdit::SetWidgetVisibility()
{
	const bool showSettingsAndRegex =
		_entryData->_type ==
		MacroConditionSceneTransform::CondType::MATCHES;
	_settings->setVisible(showSettingsAndRegex);
	_getSettings->setVisible(showSettingsAndRegex);
	_regex->setVisible(showSettingsAndRegex);
	adjustSize();
	updateGeometry();
}

} // namespace advss
