#include "macro-condition-source.hpp"
#include "utility.hpp"

namespace advss {

const std::string MacroConditionSource::id = "source";

bool MacroConditionSource::_registered = MacroConditionFactory::Register(
	MacroConditionSource::id,
	{MacroConditionSource::Create, MacroConditionSourceEdit::Create,
	 "AdvSceneSwitcher.condition.source"});

const static std::map<MacroConditionSource::Condition, std::string>
	sourceCnditionTypes = {
		{MacroConditionSource::Condition::ACTIVE,
		 "AdvSceneSwitcher.condition.source.type.active"},
		{MacroConditionSource::Condition::SHOWING,
		 "AdvSceneSwitcher.condition.source.type.showing"},
		{MacroConditionSource::Condition::SETTINGS_MATCH,
		 "AdvSceneSwitcher.condition.source.type.settings"},
		{MacroConditionSource::Condition::SETTINGS_CHANGED,
		 "AdvSceneSwitcher.condition.source.type.settingsChanged"},
};

bool MacroConditionSource::CheckCondition()
{
	if (!_source.GetSource()) {
		return false;
	}

	bool ret = false;
	auto s = obs_weak_source_get_source(_source.GetSource());

	switch (_condition) {
	case Condition::ACTIVE:
		ret = obs_source_active(s);
		break;
	case Condition::SHOWING:
		ret = obs_source_showing(s);
		break;
	case Condition::SETTINGS_MATCH:
		ret = CompareSourceSettings(_source.GetSource(), _settings,
					    _regex);
		if (IsReferencedInVars()) {
			SetVariableValue(
				GetSourceSettings(_source.GetSource()));
		}
		break;
	case Condition::SETTINGS_CHANGED: {
		std::string settings = GetSourceSettings(_source.GetSource());
		ret = !_currentSettings.empty() && settings != _currentSettings;
		_currentSettings = settings;
		SetVariableValue(settings);
		break;
	}
	default:
		break;
	}

	obs_source_release(s);

	if (GetVariableValue().empty()) {
		SetVariableValue(ret ? "true" : "false");
	}

	return ret;
}

bool MacroConditionSource::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	_source.Save(obj);
	obs_data_set_int(obj, "condition", static_cast<int>(_condition));
	_settings.Save(obj, "settings");
	_regex.Save(obj);
	return true;
}

bool MacroConditionSource::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_source.Load(obj);
	_condition = static_cast<Condition>(obs_data_get_int(obj, "condition"));
	_settings.Load(obj, "settings");
	_regex.Load(obj);
	// TOOD: remove in future version
	if (obs_data_has_user_value(obj, "regex")) {
		_regex.CreateBackwardsCompatibleRegex(
			obs_data_get_bool(obj, "regex"));
	}
	return true;
}

std::string MacroConditionSource::GetShortDesc() const
{
	return _source.ToString();
}

static inline void populateConditionSelection(QComboBox *list)
{
	for (const auto &[_, name] : sourceCnditionTypes) {
		list->addItem(obs_module_text(name.c_str()));
	}
}

MacroConditionSourceEdit::MacroConditionSourceEdit(
	QWidget *parent, std::shared_ptr<MacroConditionSource> entryData)
	: QWidget(parent),
	  _sources(new SourceSelectionWidget(this, QStringList(), true)),
	  _conditions(new QComboBox()),
	  _getSettings(new QPushButton(obs_module_text(
		  "AdvSceneSwitcher.condition.filter.getSettings"))),
	  _settings(new VariableTextEdit(this)),
	  _regex(new RegexConfigWidget(parent))
{
	populateConditionSelection(_conditions);
	auto sources = GetSourceNames();
	sources.sort();
	auto scenes = GetSceneNames();
	scenes.sort();
	_sources->SetSourceNameList(sources + scenes);

	QWidget::connect(_sources,
			 SIGNAL(SourceChanged(const SourceSelection &)), this,
			 SLOT(SourceChanged(const SourceSelection &)));
	QWidget::connect(_conditions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(_getSettings, SIGNAL(clicked()), this,
			 SLOT(GetSettingsClicked()));
	QWidget::connect(_settings, SIGNAL(textChanged()), this,
			 SLOT(SettingsChanged()));
	QWidget::connect(_regex, SIGNAL(RegexConfigChanged(RegexConfig)), this,
			 SLOT(RegexChanged(RegexConfig)));

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{sources}}", _sources},   {"{{conditions}}", _conditions},
		{"{{settings}}", _settings}, {"{{getSettings}}", _getSettings},
		{"{{regex}}", _regex},
	};
	auto line1Layout = new QHBoxLayout;
	line1Layout->setContentsMargins(0, 0, 0, 0);
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.source.entry.line1"),
		     line1Layout, widgetPlaceholders);
	auto line2Layout = new QHBoxLayout;
	line2Layout->setContentsMargins(0, 0, 0, 0);
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.source.entry.line2"),
		     line2Layout, widgetPlaceholders, false);
	auto line3Layout = new QHBoxLayout;
	line3Layout->setContentsMargins(0, 0, 0, 0);
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.source.entry.line3"),
		     line3Layout, widgetPlaceholders);
	auto mainLayout = new QVBoxLayout;
	mainLayout->addLayout(line1Layout);
	mainLayout->addLayout(line2Layout);
	mainLayout->addLayout(line3Layout);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionSourceEdit::SourceChanged(const SourceSelection &source)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_source = source;
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionSourceEdit::ConditionChanged(int index)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_condition =
		static_cast<MacroConditionSource::Condition>(index);
	SetWidgetVisibility();
}

void MacroConditionSourceEdit::GetSettingsClicked()
{
	if (_loading || !_entryData || !_entryData->_source.GetSource()) {
		return;
	}

	QString json = FormatJsonString(
		GetSourceSettings(_entryData->_source.GetSource()));
	if (_entryData->_regex.Enabled()) {
		json = EscapeForRegex(json);
	}
	_settings->setPlainText(json);
}

void MacroConditionSourceEdit::SettingsChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_settings = _settings->toPlainText().toStdString();

	adjustSize();
	updateGeometry();
}

void MacroConditionSourceEdit::RegexChanged(RegexConfig conf)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_regex = conf;

	adjustSize();
	updateGeometry();
}

void MacroConditionSourceEdit::SetWidgetVisibility()
{
	_settings->setVisible(_entryData->_condition ==
			      MacroConditionSource::Condition::SETTINGS_MATCH);
	_getSettings->setVisible(
		_entryData->_condition ==
		MacroConditionSource::Condition::SETTINGS_MATCH);
	_regex->setVisible(_entryData->_condition ==
			   MacroConditionSource::Condition::SETTINGS_MATCH);

	setToolTip(
		(_entryData->_condition ==
			 MacroConditionSource::Condition::ACTIVE ||
		 _entryData->_condition ==
			 MacroConditionSource::Condition::SHOWING)
			? obs_module_text(
				  "AdvSceneSwitcher.condition.source.sceneVisibilityHint")
			: "");

	adjustSize();
	updateGeometry();
}

void MacroConditionSourceEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_sources->SetSource(_entryData->_source);
	_conditions->setCurrentIndex(static_cast<int>(_entryData->_condition));
	_settings->setPlainText(_entryData->_settings);
	_regex->SetRegexConfig(_entryData->_regex);
	SetWidgetVisibility();
}

} // namespace advss
