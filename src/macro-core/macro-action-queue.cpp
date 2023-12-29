#include "macro-action-queue.hpp"
#include "macro-helpers.hpp"
#include "utility.hpp"

namespace advss {

const std::string MacroActionQueue::id = "queue";

bool MacroActionQueue::_registered = MacroActionFactory::Register(
	MacroActionQueue::id,
	{MacroActionQueue::Create, MacroActionQueueEdit::Create,
	 "AdvSceneSwitcher.action.queue"});

const static std::map<MacroActionQueue::Action, std::string> actionTypes = {
	{MacroActionQueue::Action::ADD_TO_QUEUE,
	 "AdvSceneSwitcher.action.queue.type.add"},
	{MacroActionQueue::Action::START_QUEUE,
	 "AdvSceneSwitcher.action.queue.type.start"},
	{MacroActionQueue::Action::STOP_QUEUE,
	 "AdvSceneSwitcher.action.queue.type.stop"},
	{MacroActionQueue::Action::CLEAR_QUEUE,
	 "AdvSceneSwitcher.action.queue.type.clear"},
};

void MacroActionQueue::AddActions(ActionQueue *queue)
{
	auto macro = _macro.GetMacro();
	if (!macro) {
		return;
	}

	auto actions = *GetMacroActions(macro.get());
	for (const auto &action : actions) {
		queue->Add(action);
	}
}

bool MacroActionQueue::PerformAction()
{
	auto queue = _queue.lock();
	if (!queue) {
		return true;
	}

	switch (_action) {
	case Action::ADD_TO_QUEUE:
		AddActions(queue.get());
		break;
	case Action::CLEAR_QUEUE:
		queue->Clear();
		break;
	case Action::START_QUEUE:
		queue->Start();
		break;
	case Action::STOP_QUEUE:
		queue->Stop();
		break;
	default:
		break;
	}
	return true;
}

void MacroActionQueue::LogAction() const
{
	auto macro = _macro.GetMacro();
	if (!macro) {
		return;
	}
	switch (_action) {
	case Action::ADD_TO_QUEUE:
		vblog(LOG_INFO, "queued actions of \"%s\" to \"%s\"",
		      GetMacroName(macro.get()).c_str(),
		      GetActionQueueName(_queue).c_str());
		break;
	case Action::START_QUEUE:
		vblog(LOG_INFO, "start queue \"%s\"",
		      GetActionQueueName(_queue).c_str());
		break;
	case Action::STOP_QUEUE:
		vblog(LOG_INFO, "stop queue \"%s\"",
		      GetActionQueueName(_queue).c_str());
		break;
	case Action::CLEAR_QUEUE:
		vblog(LOG_INFO, "cleared queue \"%s\"",
		      GetActionQueueName(_queue).c_str());
		break;
	default:
		break;
	}
}

bool MacroActionQueue::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	_macro.Save(obj);
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	obs_data_set_string(obj, "queue", GetActionQueueName(_queue).c_str());
	return true;
}

bool MacroActionQueue::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_macro.Load(obj);
	_action = static_cast<MacroActionQueue::Action>(
		obs_data_get_int(obj, "action"));
	_queue = GetWeakActionQueueByName(obs_data_get_string(obj, "queue"));
	return true;
}

std::string MacroActionQueue::GetShortDesc() const
{
	return GetActionQueueName(_queue);
}

static inline void populateActionSelection(QComboBox *list)
{
	for (const auto &[_, name] : actionTypes) {
		list->addItem(obs_module_text(name.c_str()));
	}
}

MacroActionQueueEdit::MacroActionQueueEdit(
	QWidget *parent, std::shared_ptr<MacroActionQueue> entryData)
	: QWidget(parent),
	  _macros(new MacroSelection(parent)),
	  _queues(new ActionQueueSelection()),
	  _actions(new QComboBox()),
	  _layout(new QHBoxLayout())
{
	populateActionSelection(_actions);

	QWidget::connect(_macros, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(MacroChanged(const QString &)));
	QWidget::connect(_queues, SIGNAL(SelectionChanged(const QString &)),
			 this, SLOT(QueueChanged(const QString &)));
	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));

	setLayout(_layout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionQueueEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}
	_actions->setCurrentIndex(static_cast<int>(_entryData->_action));
	_macros->SetCurrentMacro(_entryData->_macro);
	_queues->SetActionQueue(_entryData->_queue);
	SetWidgetVisibility();
}

void MacroActionQueueEdit::MacroChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_macro = text;
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionQueueEdit::QueueChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_queue = GetWeakActionQueueByQString(text);
}

void MacroActionQueueEdit::ActionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_action = static_cast<MacroActionQueue::Action>(value);
	SetWidgetVisibility();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionQueueEdit::SetWidgetVisibility()
{
	_layout->removeWidget(_actions);
	_layout->removeWidget(_queues);
	_layout->removeWidget(_macros);

	ClearLayout(_layout);

	PlaceWidgets(
		obs_module_text(
			_entryData->_action ==
					MacroActionQueue::Action::ADD_TO_QUEUE
				? "AdvSceneSwitcher.action.queue.entry.add"
				: "AdvSceneSwitcher.action.queue.entry.other"),
		_layout,
		{{"{{actions}}", _actions},
		 {"{{queues}}", _queues},
		 {"{{macros}}", _macros}});

	_macros->setVisible(_entryData->_action ==
			    MacroActionQueue::Action::ADD_TO_QUEUE);
	_macros->HideSelectedMacro();
}

} // namespace advss
