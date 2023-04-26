#include "macro-condition-filter.hpp"
#include "utility.hpp"

#include <regex>

namespace advss {

const std::string MacroConditionFilter::id = "filter";

bool MacroConditionFilter::_registered = MacroConditionFactory::Register(
	MacroConditionFilter::id,
	{MacroConditionFilter::Create, MacroConditionFilterEdit::Create,
	 "AdvSceneSwitcher.condition.filter"});

static std::map<MacroConditionFilter::Condition, std::string>
	filterConditionTypes = {
		{MacroConditionFilter::Condition::ENABLED,
		 "AdvSceneSwitcher.condition.filter.type.active"},
		{MacroConditionFilter::Condition::DISABLED,
		 "AdvSceneSwitcher.condition.filter.type.showing"},
		{MacroConditionFilter::Condition::SETTINGS,
		 "AdvSceneSwitcher.condition.filter.type.settings"},
};

bool MacroConditionFilter::CheckCondition()
{
	auto filterWeakSource = _filter.GetFilter(_source);
	if (!filterWeakSource) {
		return false;
	}
	auto filterSource = obs_weak_source_get_source(filterWeakSource);

	bool ret = false;
	switch (_condition) {
	case Condition::ENABLED:
		ret = obs_source_enabled(filterSource);
		break;
	case Condition::DISABLED:
		ret = !obs_source_enabled(filterSource);
		break;
	case Condition::SETTINGS:
		ret = CompareSourceSettings(filterWeakSource, _settings,
					    _regex);
		if (IsReferencedInVars()) {
			SetVariableValue(GetSourceSettings(filterWeakSource));
		}
		break;
	default:
		break;
	}

	obs_source_release(filterSource);

	if (GetVariableValue().empty()) {
		SetVariableValue(ret ? "true" : "false");
	}

	return ret;
}

bool MacroConditionFilter::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	_source.Save(obj);
	_filter.Save(obj, "filter");
	obs_data_set_int(obj, "condition", static_cast<int>(_condition));
	_settings.Save(obj, "settings");
	_regex.Save(obj);
	return true;
}

bool MacroConditionFilter::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_source.Load(obj);
	_filter.Load(obj, _source, "filter");
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

std::string MacroConditionFilter::GetShortDesc() const
{
	if (!_filter.ToString().empty() && !_source.ToString().empty()) {
		return _source.ToString() + " - " + _filter.ToString();
	}
	return "";
}

static inline void populateConditionSelection(QComboBox *list)
{
	for (auto entry : filterConditionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroConditionFilterEdit::MacroConditionFilterEdit(
	QWidget *parent, std::shared_ptr<MacroConditionFilter> entryData)
	: QWidget(parent),
	  _sources(new SourceSelectionWidget(this, QStringList(), true)),
	  _filters(new FilterSelectionWidget(this, _sources, true)),
	  _conditions(new QComboBox()),
	  _getSettings(new QPushButton(obs_module_text(
		  "AdvSceneSwitcher.condition.filter.getSettings"))),
	  _settings(new VariableTextEdit(this)),
	  _regex(new RegexConfigWidget(parent))
{
	populateConditionSelection(_conditions);
	auto sources = GetSourcesWithFilterNames();
	sources.sort();
	_sources->SetSourceNameList(sources);

	QWidget::connect(_sources,
			 SIGNAL(SourceChanged(const SourceSelection &)), this,
			 SLOT(SourceChanged(const SourceSelection &)));
	QWidget::connect(_filters,
			 SIGNAL(FilterChanged(const FilterSelection &)), this,
			 SLOT(FilterChanged(const FilterSelection &)));
	QWidget::connect(_conditions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(_getSettings, SIGNAL(clicked()), this,
			 SLOT(GetSettingsClicked()));
	QWidget::connect(_settings, SIGNAL(textChanged()), this,
			 SLOT(SettingsChanged()));
	QWidget::connect(_regex, SIGNAL(RegexConfigChanged(RegexConfig)), this,
			 SLOT(RegexChanged(RegexConfig)));

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{sources}}", _sources},         {"{{filters}}", _filters},
		{"{{conditions}}", _conditions},   {"{{settings}}", _settings},
		{"{{getSettings}}", _getSettings}, {"{{regex}}", _regex},
	};
	auto line1Layout = new QHBoxLayout;
	line1Layout->setContentsMargins(0, 0, 0, 0);
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.filter.entry.line1"),
		     line1Layout, widgetPlaceholders);
	auto line2Layout = new QHBoxLayout;
	line2Layout->setContentsMargins(0, 0, 0, 0);
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.filter.entry.line2"),
		     line2Layout, widgetPlaceholders, false);
	auto line3Layout = new QHBoxLayout;
	line3Layout->setContentsMargins(0, 0, 0, 0);
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.filter.entry.line3"),
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

void MacroConditionFilterEdit::SourceChanged(const SourceSelection &source)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_source = source;
}

void MacroConditionFilterEdit::FilterChanged(const FilterSelection &filter)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_filter = filter;
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionFilterEdit::ConditionChanged(int index)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_condition =
		static_cast<MacroConditionFilter::Condition>(index);
	SetSettingsSelectionVisible(_entryData->_condition ==
				    MacroConditionFilter::Condition::SETTINGS);
}

void MacroConditionFilterEdit::GetSettingsClicked()
{
	if (_loading || !_entryData ||
	    !_entryData->_filter.GetFilter(_entryData->_source)) {
		return;
	}

	QString json = FormatJsonString(GetSourceSettings(
		_entryData->_filter.GetFilter(_entryData->_source)));
	if (_entryData->_regex.Enabled()) {
		json = EscapeForRegex(json);
	}
	_settings->setPlainText(json);
}

void MacroConditionFilterEdit::SettingsChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_settings = _settings->toPlainText().toStdString();

	adjustSize();
	updateGeometry();
}

void MacroConditionFilterEdit::RegexChanged(RegexConfig conf)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_regex = conf;

	adjustSize();
	updateGeometry();
}

void MacroConditionFilterEdit::SetSettingsSelectionVisible(bool visible)
{
	_settings->setVisible(visible);
	_getSettings->setVisible(visible);
	_regex->setVisible(visible);
	adjustSize();
	updateGeometry();
}

void MacroConditionFilterEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_sources->SetSource(_entryData->_source);
	_filters->SetFilter(_entryData->_source, _entryData->_filter);
	_conditions->setCurrentIndex(static_cast<int>(_entryData->_condition));
	_settings->setPlainText(_entryData->_settings);
	_regex->SetRegexConfig(_entryData->_regex);
	SetSettingsSelectionVisible(_entryData->_condition ==
				    MacroConditionFilter::Condition::SETTINGS);

	adjustSize();
	updateGeometry();
}

} // namespace advss
