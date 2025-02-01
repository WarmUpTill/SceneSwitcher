#include "action-queue.hpp"
#include "obs-module-helper.hpp"
#include "plugin-state-helpers.hpp"
#include "ui-helpers.hpp"

namespace advss {

static std::deque<std::shared_ptr<Item>> queues;

std::deque<std::shared_ptr<Item>> &GetActionQueues()
{
	return queues;
}

void RegisterActionQueueTab();

void SetupActionQueues()
{
	static bool done = false;
	if (done) {
		return;
	}
	AddSaveStep(SaveActionQueues);
	AddLoadStep(LoadActionQueues);
	RegisterActionQueueTab();
	done = true;
}

ActionQueue::ActionQueue() : Item()
{
	_lastEmpty = std::chrono::high_resolution_clock::now();
}

ActionQueue::~ActionQueue()
{
	Stop();
}

std::shared_ptr<Item> ActionQueue::Create()
{
	return std::make_shared<ActionQueue>();
}

void ActionQueue::Save(obs_data_t *obj) const
{
	obs_data_set_string(obj, "name", _name.c_str());
	obs_data_set_bool(obj, "runOnStartup", _runOnStartup);
	obs_data_set_bool(obj, "resolveVariablesOnAdd", _resolveVariablesOnAdd);
}

void ActionQueue::Load(obs_data_t *obj)
{
	std::lock_guard<std::mutex> lock(_mutex);
	_name = obs_data_get_string(obj, "name");
	_runOnStartup = obs_data_get_bool(obj, "runOnStartup");
	_resolveVariablesOnAdd =
		obs_data_get_bool(obj, "resolveVariablesOnAdd");

	if (_runOnStartup) {
		Start();
	}
}

void ActionQueue::Start()
{
	if (!_stop) {
		return;
	}

	if (_thread.joinable()) {
		_thread.join();
	}
	_stop = false;
	_thread = std::thread(&ActionQueue::RunActions, this);
}

void ActionQueue::Stop()
{
	_stop = true;
	_cv.notify_all();
	if (std::this_thread::get_id() == _thread.get_id()) {
		return;
	}

	if (_thread.joinable()) {
		_thread.join();
	}
}

bool ActionQueue::IsRunning() const
{
	return !_stop;
}

bool ActionQueue::RunsOnStartup() const
{
	return _runOnStartup;
}

void ActionQueue::Clear()
{
	std::lock_guard<std::mutex> lock(_mutex);
	_actions.clear();
}

void ActionQueue::Add(const std::shared_ptr<MacroAction> &action)
{
	std::lock_guard<std::mutex> lock(_mutex);
	if (_resolveVariablesOnAdd) {
		auto copy = action->Copy();
		copy->GetMacro();
		OBSDataAutoRelease data = obs_data_create();
		action->Save(data);
		copy->Load(data);
		copy->PostLoad();
		RunPostLoadSteps();
		copy->ResolveVariablesToFixedValues();
		_actions.emplace_back(copy);
	} else {
		_actions.emplace_back(action);
	}
	_cv.notify_all();
}

bool ActionQueue::IsEmpty()
{
	std::lock_guard<std::mutex> lock(_mutex);
	return _actions.empty();
}

ActionQueue::TimePoint ActionQueue::GetLastEmptyTime()
{
	if (IsEmpty()) {
		_lastEmpty = std::chrono::high_resolution_clock::now();
	}
	return _lastEmpty;
}

size_t ActionQueue::Size()
{
	std::lock_guard<std::mutex> lock(_mutex);
	return _actions.size();
}

void ActionQueue::RunActions()
{
	std::shared_ptr<MacroAction> action;
	while (true) {
		{ // Grab next action to run
			std::unique_lock<std::mutex> lock(_mutex);
			while (_actions.empty() && !_stop) {
				_lastEmpty =
					std::chrono::high_resolution_clock::now();
				_cv.wait(lock);
			}

			if (_stop) {
				return;
			}
			action = _actions.front();
			_actions.pop_front();
		}

		if (!action) {
			continue;
		}

		if (ActionLoggingEnabled()) {
			blog(LOG_INFO, "Performing action '%s' in queue '%s'",
			     action->GetId().c_str(), _name.c_str());
			action->LogAction();
		}
		action->PerformAction();
	}
}

ActionQueueSettingsDialog::ActionQueueSettingsDialog(QWidget *parent,
						     ActionQueue &settings)
	: ItemSettingsDialog(settings, queues,
			     "AdvSceneSwitcher.actionQueues.select",
			     "AdvSceneSwitcher.actionQueues.add",
			     "AdvSceneSwitcher.actionQueues.nameNotAvailable",
			     parent),
	  _queueRunStatus(new QLabel()),
	  _startStopToggle(new QPushButton()),
	  _queueSize(new QLabel()),
	  _clear(new QPushButton(
		  obs_module_text("AdvSceneSwitcher.actionQueues.clear"))),
	  _runOnStartup(new QCheckBox()),
	  _resolveVariablesOnAdd(new QCheckBox()),
	  _queue(settings)
{
	QWidget::connect(_startStopToggle, SIGNAL(clicked()), this,
			 SLOT(StartStopClicked()));
	QWidget::connect(_clear, SIGNAL(clicked()), this, SLOT(ClearClicked()));

	_runOnStartup->setChecked(settings._runOnStartup);
	_resolveVariablesOnAdd->setChecked(settings._resolveVariablesOnAdd);
	UpdateLabels();

	auto layout = new QGridLayout();
	int row = 0;
	layout->addWidget(new QLabel(obs_module_text(
				  "AdvSceneSwitcher.actionQueues.name")),
			  row, 0);
	auto nameLayout = new QHBoxLayout();
	nameLayout->setContentsMargins(0, 0, 0, 0);
	nameLayout->addWidget(_name);
	nameLayout->addWidget(_nameHint);
	layout->addLayout(nameLayout, row, 1);
	++row;
	layout->addWidget(
		new QLabel(obs_module_text(
			"AdvSceneSwitcher.actionQueues.runOnStartup")),
		row, 0);
	layout->addWidget(_runOnStartup, row, 1);
	_runOnStartup->setToolTip(
		obs_module_text("AdvSceneSwitcher.actionQueues.runOnStartup"));
	++row;
	layout->addWidget(
		new QLabel(obs_module_text(
			"AdvSceneSwitcher.actionQueues.resolveVariablesOnAdd")),
		row, 0);
	layout->addWidget(_resolveVariablesOnAdd, row, 1);
	_resolveVariablesOnAdd->setToolTip(obs_module_text(
		"AdvSceneSwitcher.actionQueues.resolveVariablesOnAdd"));
	++row;
	layout->addWidget(_queueRunStatus, row, 0);
	layout->addWidget(_startStopToggle, row, 1);
	++row;
	layout->addWidget(_queueSize, row, 0);
	layout->addWidget(_clear, row, 1);
	++row;
	layout->addWidget(_buttonbox, row, 0, 1, -1);
	layout->setSizeConstraint(QLayout::SetFixedSize);
	setLayout(layout);

	auto timer = new QTimer(this);
	QWidget::connect(timer, &QTimer::timeout, this,
			 &ActionQueueSettingsDialog::UpdateLabels);
	timer->start(300);
}

bool ActionQueueSettingsDialog::AskForSettings(QWidget *parent,
					       ActionQueue &settings)
{
	ActionQueueSettingsDialog dialog(parent, settings);
	dialog.setWindowTitle(obs_module_text("AdvSceneSwitcher.windowTitle"));
	if (dialog.exec() != DialogCode::Accepted) {
		return false;
	}

	settings._name = dialog._name->text().toStdString();
	settings._runOnStartup = dialog._runOnStartup->isChecked();
	settings._resolveVariablesOnAdd =
		dialog._resolveVariablesOnAdd->isChecked();
	return true;
}

void ActionQueueSettingsDialog::StartStopClicked()
{
	if (_queue.IsRunning()) {
		_queue.Stop();
	} else {
		_queue.Start();
	}
	UpdateLabels();
}

void ActionQueueSettingsDialog::ClearClicked()
{
	_queue.Clear();
}

void ActionQueueSettingsDialog::UpdateLabels()
{
	_queueRunStatus->setText(obs_module_text(
		_queue.IsRunning() ? "AdvSceneSwitcher.actionQueues.running"
				   : "AdvSceneSwitcher.actionQueues.stopped"));
	_startStopToggle->setText(
		_queue.IsRunning()
			? obs_module_text("AdvSceneSwitcher.actionQueues.stop")
			: obs_module_text(
				  "AdvSceneSwitcher.actionQueues.start"));
	_queueSize->setText(
		QString(obs_module_text("AdvSceneSwitcher.actionQueues.size"))
			.arg(QString::number(_queue.Size())));
}

static bool AskForSettingsWrapper(QWidget *parent, Item &settings)
{
	ActionQueue &queue = dynamic_cast<ActionQueue &>(settings);
	if (ActionQueueSettingsDialog::AskForSettings(parent, queue)) {
		return true;
	}
	return false;
}

void ActionQueueSelection::SetActionQueue(
	const std::weak_ptr<ActionQueue> &queue_)
{
	auto queue = queue_.lock();
	if (queue) {
		SetItem(queue->Name());
	} else {
		SetItem("");
	}
}

ActionQueueSelection::ActionQueueSelection(QWidget *parent)
	: ItemSelection(queues, ActionQueue::Create, AskForSettingsWrapper,
			"AdvSceneSwitcher.actionQueues.select",
			"AdvSceneSwitcher.actionQueues.add",
			"AdvSceneSwitcher.item.nameNotAvailable",
			"AdvSceneSwitcher.actionQueues.configure", parent)
{
	// Connect to slots
	QWidget::connect(ActionQueueSignalManager::Instance(),
			 SIGNAL(Rename(const QString &, const QString &)), this,
			 SLOT(RenameItem(const QString &, const QString &)));
	QWidget::connect(ActionQueueSignalManager::Instance(),
			 SIGNAL(Add(const QString &)), this,
			 SLOT(AddItem(const QString &)));
	QWidget::connect(ActionQueueSignalManager::Instance(),
			 SIGNAL(Remove(const QString &)), this,
			 SLOT(RemoveItem(const QString &)));

	// Forward signals
	QWidget::connect(this,
			 SIGNAL(ItemRenamed(const QString &, const QString &)),
			 ActionQueueSignalManager::Instance(),
			 SIGNAL(Rename(const QString &, const QString &)));
	QWidget::connect(this, SIGNAL(ItemAdded(const QString &)),
			 ActionQueueSignalManager::Instance(),
			 SIGNAL(Add(const QString &)));
	QWidget::connect(this, SIGNAL(ItemRemoved(const QString &)),
			 ActionQueueSignalManager::Instance(),
			 SIGNAL(Remove(const QString &)));
}

ActionQueueSignalManager::ActionQueueSignalManager(QObject *parent)
	: QObject(parent)
{
	QWidget::connect(this, SIGNAL(Add(const QString &)), this,
			 SLOT(StartNewQueue(const QString &)));
}

ActionQueueSignalManager *ActionQueueSignalManager::Instance()
{
	static ActionQueueSignalManager manager;
	return &manager;
}

void ActionQueueSignalManager::StartNewQueue(const QString &name)
{
	auto weakQueue = GetWeakActionQueueByQString(name);
	auto queue = weakQueue.lock();
	if (!queue) {
		return;
	}
	if (queue->RunsOnStartup()) {
		queue->Start();
	}
}

void SaveActionQueues(obs_data_t *obj)
{
	OBSDataArrayAutoRelease array = obs_data_array_create();
	for (const auto &queue : queues) {
		OBSDataAutoRelease obj = obs_data_create();
		queue->Save(obj);
		obs_data_array_push_back(array, obj);
	}
	obs_data_set_array(obj, "actionQueues", array);
}

void LoadActionQueues(obs_data_t *obj)
{
	queues.clear();

	OBSDataArrayAutoRelease array = obs_data_get_array(obj, "actionQueues");
	size_t count = obs_data_array_count(array);

	for (size_t i = 0; i < count; i++) {
		OBSDataAutoRelease obj = obs_data_array_item(array, i);
		auto queue = ActionQueue::Create();
		queues.emplace_back(queue);
		queues.back()->Load(obj);
	}
}

static bool queueWithNameExists(const std::string &name)
{
	return !GetWeakActionQueueByName(name).expired();
}

static void signalImportedQueues(void *varsPtr)
{
	auto queues = std::unique_ptr<std::vector<std::shared_ptr<Item>>>(
		static_cast<std::vector<std::shared_ptr<Item>> *>(varsPtr));
	for (const auto &queue : *queues) {
		ActionQueueSignalManager::Instance()->Add(
			QString::fromStdString(queue->Name()));
	}
}

void ImportQueues(obs_data_t *data)
{
	OBSDataArrayAutoRelease array =
		obs_data_get_array(data, "actionQueues");
	size_t count = obs_data_array_count(array);

	auto importedQueues = new std::vector<std::shared_ptr<Item>>;

	for (size_t i = 0; i < count; i++) {
		OBSDataAutoRelease arrayElement = obs_data_array_item(array, i);
		auto queue = ActionQueue::Create();
		queue->Load(arrayElement);
		if (queueWithNameExists(queue->Name())) {
			continue;
		}
		queues.emplace_back(queue);
		importedQueues->emplace_back(queue);
	}

	QeueUITask(signalImportedQueues, importedQueues);
}

std::weak_ptr<ActionQueue> GetWeakActionQueueByName(const std::string &name)
{
	for (const auto &queue : queues) {
		if (queue->Name() == name) {
			std::weak_ptr<ActionQueue> wp =
				std::dynamic_pointer_cast<ActionQueue>(queue);
			return wp;
		}
	}
	return std::weak_ptr<ActionQueue>();
}

std::weak_ptr<ActionQueue> GetWeakActionQueueByQString(const QString &name)
{
	return GetWeakActionQueueByName(name.toStdString());
}

std::string GetActionQueueName(const std::weak_ptr<ActionQueue> &queue_)
{
	auto queue = queue_.lock();
	if (!queue) {
		return obs_module_text("AdvSceneSwitcher.actionQueues.invalid");
	}
	return queue->Name();
}

void AutoStartActionQueues()
{
	for (auto &queue : queues) {
		auto actionQueue =
			std::dynamic_pointer_cast<ActionQueue>(queue);
		if (actionQueue->RunsOnStartup()) {
			actionQueue->Start();
		}
	}
}

void StopAndClearAllActionQueues()
{
	for (auto &queue : queues) {
		auto actionQueue =
			std::dynamic_pointer_cast<ActionQueue>(queue);
		actionQueue->Stop();
		actionQueue->Clear();
	}
}

} // namespace advss
