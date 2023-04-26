#include "macro-action-filter.hpp"
#include "utility.hpp"

namespace advss {

const std::string MacroActionFilter::id = "filter";

bool MacroActionFilter::_registered = MacroActionFactory::Register(
	MacroActionFilter::id,
	{MacroActionFilter::Create, MacroActionFilterEdit::Create,
	 "AdvSceneSwitcher.action.filter"});

const static std::map<MacroActionFilter::Action, std::string> actionTypes = {
	{MacroActionFilter::Action::ENABLE,
	 "AdvSceneSwitcher.action.filter.type.enable"},
	{MacroActionFilter::Action::DISABLE,
	 "AdvSceneSwitcher.action.filter.type.disable"},
	{MacroActionFilter::Action::SETTINGS,
	 "AdvSceneSwitcher.action.filter.type.settings"},
};

bool MacroActionFilter::PerformAction()
{
	auto s = obs_weak_source_get_source(_filter.GetFilter(_source));
	switch (_action) {
	case Action::ENABLE:
		obs_source_set_enabled(s, true);
		break;
	case Action::DISABLE:
		obs_source_set_enabled(s, false);
		break;
	case Action::SETTINGS:
		SetSourceSettings(s, _settings);
		break;
	default:
		break;
	}
	obs_source_release(s);
	return true;
}

void MacroActionFilter::LogAction() const
{
	auto it = actionTypes.find(_action);
	if (it != actionTypes.end()) {
		vblog(LOG_INFO,
		      "performed action \"%s\" for filter \"%s\" on source \"%s\"",
		      it->second.c_str(), _filter.ToString().c_str(),
		      _source.ToString(true).c_str());
	} else {
		blog(LOG_WARNING, "ignored unknown filter action %d",
		     static_cast<int>(_action));
	}
}

bool MacroActionFilter::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	_source.Save(obj);
	_filter.Save(obj, "filter");
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	_settings.Save(obj, "settings");
	return true;
}

bool MacroActionFilter::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_source.Load(obj);
	_filter.Load(obj, _source, "filter");
	_action = static_cast<Action>(obs_data_get_int(obj, "action"));
	_settings.Load(obj, "settings");
	return true;
}

std::string MacroActionFilter::GetShortDesc() const
{
	if (!_filter.ToString().empty() && !_source.ToString().empty()) {
		return _source.ToString() + " - " + _filter.ToString();
	}
	return "";
}

static inline void populateActionSelection(QComboBox *list)
{
	for (auto entry : actionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroActionFilterEdit::MacroActionFilterEdit(
	QWidget *parent, std::shared_ptr<MacroActionFilter> entryData)
	: QWidget(parent),
	  _sources(new SourceSelectionWidget(this, QStringList(), true)),
	  _filters(new FilterSelectionWidget(this, _sources, true)),
	  _actions(new QComboBox()),
	  _getSettings(new QPushButton(obs_module_text(
		  "AdvSceneSwitcher.action.filter.getSettings"))),
	  _settings(new VariableTextEdit(this))
{
	_filters->setSizeAdjustPolicy(QComboBox::AdjustToContents);

	populateActionSelection(_actions);
	auto sources = GetSourcesWithFilterNames();
	sources.sort();
	_sources->SetSourceNameList(sources);

	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_sources,
			 SIGNAL(SourceChanged(const SourceSelection &)), this,
			 SLOT(SourceChanged(const SourceSelection &)));
	QWidget::connect(_filters,
			 SIGNAL(FilterChanged(const FilterSelection &)), this,
			 SLOT(FilterChanged(const FilterSelection &)));
	QWidget::connect(_getSettings, SIGNAL(clicked()), this,
			 SLOT(GetSettingsClicked()));
	QWidget::connect(_settings, SIGNAL(textChanged()), this,
			 SLOT(SettingsChanged()));

	QHBoxLayout *entryLayout = new QHBoxLayout;

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{sources}}", _sources},         {"{{filters}}", _filters},
		{"{{actions}}", _actions},         {"{{settings}}", _settings},
		{"{{getSettings}}", _getSettings},
	};
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.action.filter.entry"),
		     entryLayout, widgetPlaceholders);

	QHBoxLayout *buttonLayout = new QHBoxLayout;
	buttonLayout->addWidget(_getSettings);
	buttonLayout->addStretch();
	buttonLayout->setContentsMargins(0, 0, 0, 0);

	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(entryLayout);
	mainLayout->addWidget(_settings);
	mainLayout->addLayout(buttonLayout);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionFilterEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_actions->setCurrentIndex(static_cast<int>(_entryData->_action));
	_sources->SetSource(_entryData->_source);
	_filters->SetFilter(_entryData->_source, _entryData->_filter);
	_settings->setPlainText(_entryData->_settings);
	SetWidgetVisibility(_entryData->_action ==
			    MacroActionFilter::Action::SETTINGS);

	adjustSize();
	updateGeometry();
}

void MacroActionFilterEdit::SourceChanged(const SourceSelection &source)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_source = source;
}

void MacroActionFilterEdit::FilterChanged(const FilterSelection &filter)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_filter = filter;
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionFilterEdit::ActionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_action = static_cast<MacroActionFilter::Action>(value);
	SetWidgetVisibility(_entryData->_action ==
			    MacroActionFilter::Action::SETTINGS);
}

void MacroActionFilterEdit::GetSettingsClicked()
{
	if (_loading || !_entryData ||
	    !_entryData->_filter.GetFilter(_entryData->_source)) {
		return;
	}

	_settings->setPlainText(FormatJsonString(GetSourceSettings(
		_entryData->_filter.GetFilter(_entryData->_source))));
}

void MacroActionFilterEdit::SettingsChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_settings = _settings->toPlainText().toStdString();

	adjustSize();
	updateGeometry();
}

void MacroActionFilterEdit::SetWidgetVisibility(bool showSettings)
{
	_settings->setVisible(showSettings);
	_getSettings->setVisible(showSettings);
	adjustSize();
	updateGeometry();
}

} // namespace advss
