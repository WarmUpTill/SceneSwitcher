#include "headers/macro-action-source.hpp"
#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

const std::string MacroActionSource::id = "source";

bool MacroActionSource::_registered = MacroActionFactory::Register(
	MacroActionSource::id,
	{MacroActionSource::Create, MacroActionSourceEdit::Create,
	 "AdvSceneSwitcher.action.source"});

const static std::map<SourceAction, std::string> actionTypes = {
	{SourceAction::ENABLE, "AdvSceneSwitcher.action.source.type.enable"},
	{SourceAction::DISABLE, "AdvSceneSwitcher.action.source.type.disable"},
};

bool MacroActionSource::PerformAction()
{
	auto s = obs_weak_source_get_source(_source);
	switch (_action) {
	case SourceAction::ENABLE:
		obs_source_set_enabled(s, true);
		break;
	case SourceAction::DISABLE:
		obs_source_set_enabled(s, false);
		break;
	default:
		break;
	}
	obs_source_release(s);
	return true;
}

void MacroActionSource::LogAction()
{
	auto it = actionTypes.find(_action);
	if (it != actionTypes.end()) {
		vblog(LOG_INFO, "performed action \"%s\" for Source \"%s\"",
		      it->second.c_str(), GetWeakSourceName(_source).c_str());
	} else {
		blog(LOG_WARNING, "ignored unknown source action %d",
		     static_cast<int>(_action));
	}
}

bool MacroActionSource::Save(obs_data_t *obj)
{
	MacroAction::Save(obj);
	obs_data_set_string(obj, "source", GetWeakSourceName(_source).c_str());
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	return true;
}

bool MacroActionSource::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	const char *sourceName = obs_data_get_string(obj, "source");
	_source = GetWeakSourceByName(sourceName);
	_action = static_cast<SourceAction>(obs_data_get_int(obj, "action"));
	return true;
}

static inline void populateActionSelection(QComboBox *list)
{
	for (auto entry : actionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroActionSourceEdit::MacroActionSourceEdit(
	QWidget *parent, std::shared_ptr<MacroActionSource> entryData)
	: QWidget(parent)
{
	_sources = new QComboBox();
	_actions = new QComboBox();

	populateActionSelection(_actions);
	populateSourceSelection(_sources);

	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_sources, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(SourceChanged(const QString &)));

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{sources}}", _sources},
		{"{{actions}}", _actions},
	};
	placeWidgets(obs_module_text("AdvSceneSwitcher.action.source.entry"),
		     mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionSourceEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_actions->setCurrentIndex(static_cast<int>(_entryData->_action));
	_sources->setCurrentText(
		GetWeakSourceName(_entryData->_source).c_str());
}

void MacroActionSourceEdit::SourceChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_source = GetWeakSourceByQString(text);
}

void MacroActionSourceEdit::ActionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_action = static_cast<SourceAction>(value);
}
