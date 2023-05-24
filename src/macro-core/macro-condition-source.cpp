#include "macro-condition-source.hpp"
#include "utility.hpp"

namespace advss {

const std::string MacroConditionSource::id = "source";

bool MacroConditionSource::_registered = MacroConditionFactory::Register(
	MacroConditionSource::id,
	{MacroConditionSource::Create, MacroConditionSourceEdit::Create,
	 "AdvSceneSwitcher.condition.source"});

static std::map<SourceCondition, std::string> sourceConditionTypes = {
	{SourceCondition::ACTIVE,
	 "AdvSceneSwitcher.condition.source.type.active"},
	{SourceCondition::SHOWING,
	 "AdvSceneSwitcher.condition.source.type.showing"},
	{SourceCondition::SETTINGS,
	 "AdvSceneSwitcher.condition.source.type.settings"},
};

bool MacroConditionSource::CheckCondition()
{
	if (!_source.GetSource()) {
		return false;
	}

	bool ret = false;
	auto s = obs_weak_source_get_source(_source.GetSource());

	switch (_condition) {
	case SourceCondition::ACTIVE:
		ret = obs_source_active(s);
		break;
	case SourceCondition::SHOWING:
		ret = obs_source_showing(s);
		break;
	case SourceCondition::SETTINGS:
		ret = CompareSourceSettings(_source.GetSource(), _settings,
					    _regex);
		if (IsReferencedInVars()) {
			SetVariableValue(
				GetSourceSettings(_source.GetSource()));
		}
		break;
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
	_condition = static_cast<SourceCondition>(
		obs_data_get_int(obj, "condition"));
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
	for (auto entry : sourceConditionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
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
	_sources->SetSourceNameList(sources);

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
	_entryData->_condition = static_cast<SourceCondition>(index);
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
			      SourceCondition::SETTINGS);
	_getSettings->setVisible(_entryData->_condition ==
				 SourceCondition::SETTINGS);
	_regex->setVisible(_entryData->_condition == SourceCondition::SETTINGS);

	setToolTip(
		(_entryData->_condition == SourceCondition::ACTIVE ||
		 _entryData->_condition == SourceCondition::SHOWING)
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
