#pragma once
#include "item-selection-helpers.hpp"
#include "macro-action.hpp"

#include <deque>
#include <obs-data.h>
#include <QCheckBox>
#include <thread>
#include <condition_variable>

namespace advss {

class ActionQueueSelection;
class ActionQueueSettingsDialog;

class ActionQueue : public Item {
public:
	ActionQueue();
	~ActionQueue();

	static std::shared_ptr<Item> Create();

	void Save(obs_data_t *obj) const;
	void Load(obs_data_t *obj);

	void Start();
	void Stop();
	bool IsRunning() const;
	bool RunsOnStartup() const;

	void Clear();
	bool IsEmpty();

	void Add(const std::shared_ptr<MacroAction> &);
	size_t Size();

private:
	void RunActions();

	bool _runOnStartup = true;
	bool _resolveVariablesOnAdd = true;
	std::atomic_bool _stop = {false};
	std::mutex _mutex;
	std::condition_variable _cv;
	std::thread _thread;
	std::deque<std::shared_ptr<MacroAction>> _actions;

	friend ActionQueueSelection;
	friend ActionQueueSettingsDialog;
};

class ActionQueueSettingsDialog : public ItemSettingsDialog {
	Q_OBJECT

public:
	ActionQueueSettingsDialog(QWidget *parent, ActionQueue &);
	static bool AskForSettings(QWidget *parent, ActionQueue &settings);

private slots:
	void StartStopClicked();
	void ClearClicked();
	void UpdateLabels();

private:
	QLabel *_queueRunStatus;
	QPushButton *_startStopToggle;
	QLabel *_queueSize;
	QPushButton *_clear;
	QCheckBox *_runOnStartup;
	QCheckBox *_resolveVariablesOnAdd;

	ActionQueue &_queue;
};

class ActionQueueSelection : public ItemSelection {
	Q_OBJECT

public:
	ActionQueueSelection(QWidget *parent = 0);
	void SetActionQueue(const std::weak_ptr<ActionQueue> &);
};

class ActionQueueSignalManager : public QObject {
	Q_OBJECT
public:
	ActionQueueSignalManager(QObject *parent = nullptr);
	static ActionQueueSignalManager *Instance();

private slots:
	void StartNewQueue(const QString &);

signals:
	void Rename(const QString &, const QString &);
	void Add(const QString &);
	void Remove(const QString &);
};

void SetupActionQueues();
void SaveActionQueues(obs_data_t *);
void LoadActionQueues(obs_data_t *);
void ImportQueues(obs_data_t *);
std::weak_ptr<ActionQueue> GetWeakActionQueueByName(const std::string &name);
std::weak_ptr<ActionQueue> GetWeakActionQueueByQString(const QString &name);
std::string GetActionQueueName(const std::weak_ptr<ActionQueue> &);
void AutoStartActionQueues();
void StopAndClearAllActionQueues();

} // namespace advss
