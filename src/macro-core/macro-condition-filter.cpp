#include "macro-condition-edit.hpp"
#include "macro-condition-filter.hpp"
#include "utility.hpp"
#include "advanced-scene-switcher.hpp"

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

void MacroConditionFilter::ResolveVariables()
{
	if (_source.GetType() == SourceSelection::Type::SOURCE) {
		return;
	}

	std::string name = GetWeakSourceName(_filter);
	if (!name.empty()) {
		_filterName = name;
	}
	_filter = GetWeakFilterByName(_source.GetSource(), _filterName.c_str());
}

bool MacroConditionFilter::CheckCondition()
{
	ResolveVariables();
	if (!_source.GetSource()) {
		return false;
	}

	bool ret = false;
	auto s = obs_weak_source_get_source(_filter);

	switch (_condition) {
	case Condition::ENABLED:
		ret = obs_source_enabled(s);
		break;
	case Condition::DISABLED:
		ret = !obs_source_enabled(s);
		break;
	case Condition::SETTINGS:
		ret = CompareSourceSettings(_filter, _settings, _regex);
		if (IsReferencedInVars()) {
			SetVariableValue(GetSourceSettings(_filter));
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

bool MacroConditionFilter::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	_source.Save(obj);
	obs_data_set_string(obj, "filter", _filterName.c_str());
	obs_data_set_int(obj, "condition", static_cast<int>(_condition));
	_settings.Save(obj, "settings");
	_regex.Save(obj);
	return true;
}

bool MacroConditionFilter::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_source.Load(obj);
	_filterName = obs_data_get_string(obj, "filter");
	_filter = GetWeakFilterByQString(_source.GetSource(),
					 _filterName.c_str());
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
	if (_filter && !_source.ToString().empty()) {
		return _source.ToString() + " - " + GetWeakSourceName(_filter);
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
	  _filters(new QComboBox()),
	  _conditions(new QComboBox()),
	  _getSettings(new QPushButton(obs_module_text(
		  "AdvSceneSwitcher.condition.filter.getSettings"))),
	  _settings(new VariableTextEdit(this)),
	  _regex(new RegexConfigWidget(parent))
{
	_filters->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	populateConditionSelection(_conditions);
	auto sources = GetSourcesWithFilterNames();
	sources.sort();
	_sources->SetSourceNameList(sources);

	QWidget::connect(_sources,
			 SIGNAL(SourceChanged(const SourceSelection &)), this,
			 SLOT(SourceChanged(const SourceSelection &)));
	QWidget::connect(_filters, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(FilterChanged(const QString &)));
	QWidget::connect(_conditions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(_getSettings, SIGNAL(clicked()), this,
			 SLOT(GetSettingsClicked()));
	QWidget::connect(_settings, SIGNAL(textChanged()), this,
			 SLOT(SettingsChanged()));
	QWidget::connect(_regex, SIGNAL(RegexConfigChanged(RegexConfig)), this,
			 SLOT(RegexChanged(RegexConfig)));

	QHBoxLayout *line1Layout = new QHBoxLayout;
	QHBoxLayout *line2Layout = new QHBoxLayout;
	QHBoxLayout *line3Layout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{sources}}", _sources},         {"{{filters}}", _filters},
		{"{{conditions}}", _conditions},   {"{{settings}}", _settings},
		{"{{getSettings}}", _getSettings}, {"{{regex}}", _regex},
	};
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.filter.entry.line1"),
		     line1Layout, widgetPlaceholders);
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.filter.entry.line2"),
		     line2Layout, widgetPlaceholders, false);
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
	{
		std::lock_guard<std::mutex> lock(switcher->m);
		_entryData->_source = source;
	}
	_filters->clear();
	PopulateFilterSelection(_filters, _entryData->_source.GetSource());
	_filters->adjustSize();
}

void MacroConditionFilterEdit::FilterChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_filterName = text.toStdString();
	_entryData->_filter =
		GetWeakFilterByQString(_entryData->_source.GetSource(), text);
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionFilterEdit::ConditionChanged(int index)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_condition =
		static_cast<MacroConditionFilter::Condition>(index);
	SetSettingsSelectionVisible(_entryData->_condition ==
				    MacroConditionFilter::Condition::SETTINGS);
}

void MacroConditionFilterEdit::GetSettingsClicked()
{
	if (_loading || !_entryData || !_entryData->_source.GetSource()) {
		return;
	}

	QString json = FormatJsonString(GetSourceSettings(_entryData->_filter));
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

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_settings = _settings->toPlainText().toStdString();

	adjustSize();
	updateGeometry();
}

void MacroConditionFilterEdit::RegexChanged(RegexConfig conf)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
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
}

void MacroConditionFilterEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_sources->SetSource(_entryData->_source);
	PopulateFilterSelection(_filters, _entryData->_source.GetSource());
	_filters->setCurrentText(
		GetWeakSourceName(_entryData->_filter).c_str());
	_conditions->setCurrentIndex(static_cast<int>(_entryData->_condition));
	_settings->setPlainText(_entryData->_settings);
	_regex->SetRegexConfig(_entryData->_regex);
	SetSettingsSelectionVisible(_entryData->_condition ==
				    MacroConditionFilter::Condition::SETTINGS);

	adjustSize();
	updateGeometry();
}

} // namespace advss
