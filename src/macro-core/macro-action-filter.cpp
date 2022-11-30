#include "macro-action-filter.hpp"
#include "advanced-scene-switcher.hpp"
#include "utility.hpp"

const std::string MacroActionFilter::id = "filter";

bool MacroActionFilter::_registered = MacroActionFactory::Register(
	MacroActionFilter::id,
	{MacroActionFilter::Create, MacroActionFilterEdit::Create,
	 "AdvSceneSwitcher.action.filter"});

const static std::map<FilterAction, std::string> actionTypes = {
	{FilterAction::ENABLE, "AdvSceneSwitcher.action.filter.type.enable"},
	{FilterAction::DISABLE, "AdvSceneSwitcher.action.filter.type.disable"},
	{FilterAction::SETTINGS,
	 "AdvSceneSwitcher.action.filter.type.settings"},
};

bool MacroActionFilter::PerformAction()
{
	auto s = obs_weak_source_get_source(_filter);
	switch (_action) {
	case FilterAction::ENABLE:
		obs_source_set_enabled(s, true);
		break;
	case FilterAction::DISABLE:
		obs_source_set_enabled(s, false);
		break;
	case FilterAction::SETTINGS:
		setSourceSettings(s, _settings);
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
		      it->second.c_str(), GetWeakSourceName(_filter).c_str(),
		      GetWeakSourceName(_source).c_str());
	} else {
		blog(LOG_WARNING, "ignored unknown filter action %d",
		     static_cast<int>(_action));
	}
}

bool MacroActionFilter::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	obs_data_set_string(obj, "source", GetWeakSourceName(_source).c_str());
	obs_data_set_string(obj, "filter", GetWeakSourceName(_filter).c_str());
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	_settings.Save(obj, "settings");
	return true;
}

bool MacroActionFilter::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	const char *sourceName = obs_data_get_string(obj, "source");
	_source = GetWeakSourceByName(sourceName);
	const char *filterName = obs_data_get_string(obj, "filter");
	_filter = GetWeakFilterByQString(_source, filterName);
	_action = static_cast<FilterAction>(obs_data_get_int(obj, "action"));
	_settings.Load(obj, "settings");
	return true;
}

std::string MacroActionFilter::GetShortDesc() const
{
	if (_filter && _source) {
		return GetWeakSourceName(_source) + " - " +
		       GetWeakSourceName(_filter);
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
	: QWidget(parent)
{
	_sources = new QComboBox();
	_filters = new QComboBox();
	_filters->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	_actions = new QComboBox();
	_getSettings = new QPushButton(
		obs_module_text("AdvSceneSwitcher.action.filter.getSettings"));
	_settings = new VariableTextEdit(this);

	populateActionSelection(_actions);
	populateSourcesWithFilterSelection(_sources);

	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_sources, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(SourceChanged(const QString &)));
	QWidget::connect(_filters, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(FilterChanged(const QString &)));
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
	placeWidgets(obs_module_text("AdvSceneSwitcher.action.filter.entry"),
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

void MacroActionFilterEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_actions->setCurrentIndex(static_cast<int>(_entryData->_action));
	_sources->setCurrentText(
		GetWeakSourceName(_entryData->_source).c_str());
	populateFilterSelection(_filters, _entryData->_source);
	_filters->setCurrentText(
		GetWeakSourceName(_entryData->_filter).c_str());
	_settings->setPlainText(_entryData->_settings);
	SetWidgetVisibility(_entryData->_action == FilterAction::SETTINGS);

	adjustSize();
	updateGeometry();
}

void MacroActionFilterEdit::SourceChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}
	{
		std::lock_guard<std::mutex> lock(switcher->m);
		_entryData->_source = GetWeakSourceByQString(text);
	}
	_filters->clear();
	populateFilterSelection(_filters, _entryData->_source);
	_filters->adjustSize();
}

void MacroActionFilterEdit::FilterChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_filter = GetWeakFilterByQString(_entryData->_source, text);
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionFilterEdit::ActionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_action = static_cast<FilterAction>(value);
	SetWidgetVisibility(_entryData->_action == FilterAction::SETTINGS);
}

void MacroActionFilterEdit::GetSettingsClicked()
{
	if (_loading || !_entryData || !_entryData->_source ||
	    !_entryData->_filter) {
		return;
	}

	_settings->setPlainText(
		formatJsonString(getSourceSettings(_entryData->_filter)));
}

void MacroActionFilterEdit::SettingsChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_settings = _settings->toPlainText().toStdString();

	adjustSize();
	updateGeometry();
}

void MacroActionFilterEdit::SetWidgetVisibility(bool showSettings)
{
	_settings->setVisible(showSettings);
	_getSettings->setVisible(showSettings);
	adjustSize();
}
