#include "macro-condition-queue.hpp"
#include "layout-helpers.hpp"

namespace advss {

const std::string MacroConditionQueue::id = "queue";

bool MacroConditionQueue::_registered = MacroConditionFactory::Register(
	MacroConditionQueue::id,
	{MacroConditionQueue::Create, MacroConditionQueueEdit::Create,
	 "AdvSceneSwitcher.condition.queue"});

const static std::map<MacroConditionQueue::Condition, std::string>
	conditionTypes = {
		{MacroConditionQueue::Condition::STARTED,
		 "AdvSceneSwitcher.condition.queue.type.started"},
		{MacroConditionQueue::Condition::STOPPED,
		 "AdvSceneSwitcher.condition.queue.type.stopped"},
		{MacroConditionQueue::Condition::SIZE,
		 "AdvSceneSwitcher.condition.queue.type.size"},
};

bool MacroConditionQueue::CheckCondition()
{
	auto queue = _queue.lock();
	if (!queue) {
		return false;
	}

	SetTempVarValue("size", std::to_string(queue->Size()));
	SetTempVarValue("running", queue->IsRunning());

	switch (_condition) {
	case Condition::STARTED:
		return queue->IsRunning();
	case Condition::STOPPED:
		return !queue->IsRunning();
	case Condition::SIZE:
		return (int)queue->Size() < _size;
	default:
		break;
	}
	return false;
}

bool MacroConditionQueue::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	obs_data_set_int(obj, "condition", static_cast<int>(_condition));
	obs_data_set_string(obj, "queue", GetActionQueueName(_queue).c_str());
	_size.Save(obj, "size");
	return true;
}

bool MacroConditionQueue::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_condition = static_cast<Condition>(obs_data_get_int(obj, "condition"));
	_queue = GetWeakActionQueueByName(obs_data_get_string(obj, "queue"));
	_size.Load(obj, "size");
	return true;
}

std::string MacroConditionQueue::GetShortDesc() const
{
	return GetActionQueueName(_queue);
}

void MacroConditionQueue::SetupTempVars()
{
	MacroCondition::SetupTempVars();
	AddTempvar("size",
		   obs_module_text("AdvSceneSwitcher.tempVar.queue.size"));
	AddTempvar(
		"running",
		obs_module_text("AdvSceneSwitcher.tempVar.queue.running"),
		obs_module_text(
			"AdvSceneSwitcher.tempVar.queue.running.description"));
}

static inline void populateQueueTypeSelection(QComboBox *list)
{
	for (const auto &[_, name] : conditionTypes) {
		list->addItem(obs_module_text(name.c_str()));
	}
}

MacroConditionQueueEdit::MacroConditionQueueEdit(
	QWidget *parent, std::shared_ptr<MacroConditionQueue> entryData)
	: QWidget(parent),
	  _conditions(new QComboBox()),
	  _queues(new ActionQueueSelection()),
	  _size(new VariableSpinBox()),
	  _layout(new QHBoxLayout())
{
	populateQueueTypeSelection(_conditions);

	_size->setMinimum(1);

	QWidget::connect(_conditions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(_queues, SIGNAL(SelectionChanged(const QString &)),
			 this, SLOT(QueueChanged(const QString &)));
	QWidget::connect(
		_size,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(SizeChanged(const NumberVariable<int> &)));

	setLayout(_layout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionQueueEdit::ConditionChanged(int condition)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_condition =
		static_cast<MacroConditionQueue::Condition>(condition);
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
	SetWidgetVisibility();
}

void MacroConditionQueueEdit::QueueChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_queue = GetWeakActionQueueByQString(text);
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionQueueEdit::SizeChanged(const NumberVariable<int> &value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_size = value;
}

void MacroConditionQueueEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}
	_conditions->setCurrentIndex(static_cast<int>(_entryData->_condition));
	_queues->SetActionQueue(_entryData->_queue);
	_size->SetValue(_entryData->_size);
	SetWidgetVisibility();
}

void MacroConditionQueueEdit::SetWidgetVisibility()
{
	_layout->removeWidget(_conditions);
	_layout->removeWidget(_queues);
	_layout->removeWidget(_size);

	ClearLayout(_layout);

	PlaceWidgets(
		obs_module_text(
			_entryData->_condition ==
					MacroConditionQueue::Condition::SIZE
				? "AdvSceneSwitcher.condition.queue.entry.size"
				: "AdvSceneSwitcher.condition.queue.entry.startStop"),
		_layout,
		{{"{{conditions}}", _conditions},
		 {"{{queues}}", _queues},
		 {"{{size}}", _size}});

	_size->setVisible(_entryData->_condition ==
			  MacroConditionQueue::Condition::SIZE);
}

} // namespace advss
