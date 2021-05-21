#include "headers/macro-action-filter.hpp"
#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

const std::string MacroActionFilter::id = "filter";

bool MacroActionFilter::_registered = MacroActionFactory::Register(
	MacroActionFilter::id,
	{MacroActionFilter::Create, MacroActionFilterEdit::Create,
	 "AdvSceneSwitcher.action.filter"});

const static std::map<FilterAction, std::string> actionTypes = {
	{FilterAction::ENABLE, "AdvSceneSwitcher.action.filter.type.enable"},
	{FilterAction::DISABLE, "AdvSceneSwitcher.action.filter.type.disable"},
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
	default:
		break;
	}
	obs_source_release(s);
	return true;
}

void MacroActionFilter::LogAction()
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

bool MacroActionFilter::Save(obs_data_t *obj)
{
	MacroAction::Save(obj);
	obs_data_set_string(obj, "source", GetWeakSourceName(_source).c_str());
	obs_data_set_string(obj, "filter", GetWeakSourceName(_filter).c_str());
	obs_data_set_int(obj, "action", static_cast<int>(_action));
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
	return true;
}

static inline void populateActionSelection(QComboBox *list)
{
	for (auto entry : actionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

static inline void populateFilters(QComboBox *list,
				   OBSWeakSource weakSource = nullptr)
{
	auto enumFilters = [](obs_source_t *, obs_source_t *filter, void *ptr) {
		QComboBox *list = reinterpret_cast<QComboBox *>(ptr);
		auto name = obs_source_get_name(filter);
		list->addItem(name);
	};

	list->clear();
	list->addItem(obs_module_text("AdvSceneSwitcher.selectFilter"));
	auto s = obs_weak_source_get_source(weakSource);
	obs_source_enum_filters(s, enumFilters, list);
	obs_source_release(s);
}

static inline void hasFilterEnum(obs_source_t *, obs_source_t *filter,
				 void *ptr)
{
	if (!filter) {
		return;
	}
	bool *hasFilter = reinterpret_cast<bool *>(ptr);
	*hasFilter = true;
}

static inline void populateSourcesWithFilter(QComboBox *list)
{
	auto enumSourcesWithFilters = [](void *param, obs_source_t *source) {
		if (!source) {
			return true;
		}
		QComboBox *list = reinterpret_cast<QComboBox *>(param);
		bool hasFilter = false;
		obs_source_enum_filters(source, hasFilterEnum, &hasFilter);
		if (hasFilter) {
			list->addItem(obs_source_get_name(source));
		}
		return true;
	};

	list->clear();
	list->addItem(obs_module_text("AdvSceneSwitcher.selectSource"));
	obs_enum_sources(enumSourcesWithFilters, list);
}

MacroActionFilterEdit::MacroActionFilterEdit(
	QWidget *parent, std::shared_ptr<MacroActionFilter> entryData)
	: QWidget(parent)
{
	_sources = new QComboBox();
	_filters = new QComboBox();
	_actions = new QComboBox();

	populateActionSelection(_actions);
	populateSourcesWithFilter(_sources);

	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_sources, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(SourceChanged(const QString &)));
	QWidget::connect(_filters, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(FilterChanged(const QString &)));

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{sources}}", _sources},
		{"{{filters}}", _filters},
		{"{{actions}}", _actions},
	};
	placeWidgets(obs_module_text("AdvSceneSwitcher.action.filter.entry"),
		     mainLayout, widgetPlaceholders);
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
	populateFilters(_filters, _entryData->_source);
	_filters->setCurrentText(
		GetWeakSourceName(_entryData->_filter).c_str());
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
	populateFilters(_filters, _entryData->_source);
}

void MacroActionFilterEdit::FilterChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_filter = GetWeakFilterByQString(_entryData->_source, text);
}

void MacroActionFilterEdit::ActionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_action = static_cast<FilterAction>(value);
}
