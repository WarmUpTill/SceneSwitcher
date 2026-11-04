#include "macro-action-youtube.hpp"
#include "youtube-helpers.hpp"

#include <layout-helpers.hpp>
#include <log-helper.hpp>
#include <macro-helpers.hpp>
#include <sync-helpers.hpp>

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <thread>

namespace advss {

const std::string MacroActionYouTube::id = "youtube";

bool MacroActionYouTube::_registered = MacroActionFactory::Register(
	MacroActionYouTube::id,
	{MacroActionYouTube::Create, MacroActionYouTubeEdit::Create,
	 "AdvSceneSwitcher.action.youtube"});

static const int transitionTimeoutSeconds = 30;

const static std::map<MacroActionYouTube::Action, std::string> actionTypes = {
	{MacroActionYouTube::Action::START_BROADCAST,
	 "AdvSceneSwitcher.action.youtube.type.startBroadcast"},
	{MacroActionYouTube::Action::END_BROADCAST,
	 "AdvSceneSwitcher.action.youtube.type.endBroadcast"},
};

static bool transitionBroadcast(YouTubeConnection &connection,
				const std::string &id,
				const std::string &status)
{
	const auto result = SendYouTubeRequest(
		connection, YouTubeRequestMethod::POST,
		"/liveBroadcasts/transition",
		{{"broadcastStatus", status}, {"id", id}, {"part", "status"}});
	return result.status == 200;
}

static std::string getLifeCycleStatus(YouTubeConnection &connection,
				      const std::string &id)
{
	const auto result = SendYouTubeRequest(
		connection, YouTubeRequestMethod::GET, "/liveBroadcasts",
		{{"part", "status"}, {"id", id}});
	OBSDataArrayAutoRelease items =
		obs_data_get_array(result.data, "items");
	OBSDataAutoRelease item = obs_data_array_item(items, 0);
	OBSDataAutoRelease status = obs_data_get_obj(item, "status");
	return obs_data_get_string(status, "lifeCycleStatus");
}

static bool waitForLifeCycleStatus(YouTubeConnection &connection,
				   const std::string &id,
				   const std::string &targetStatus,
				   const Macro *macro)
{
	const auto deadline = std::chrono::steady_clock::now() +
			      std::chrono::seconds(transitionTimeoutSeconds);
	while (!MacroWaitShouldAbort() && !MacroIsStopped(macro)) {
		if (getLifeCycleStatus(connection, id) == targetStatus) {
			return true;
		}
		if (std::chrono::steady_clock::now() >= deadline) {
			blog(LOG_WARNING,
			     "timed out waiting for YouTube broadcast %s to reach status \"%s\"",
			     id.c_str(), targetStatus.c_str());
			return false;
		}
		std::this_thread::sleep_for(std::chrono::seconds(2));
	}
	return false;
}

static void startBroadcast(YouTubeConnection &connection,
			   const YouTubeBroadcastInfo &broadcast,
			   const Macro *macro)
{
	auto status = broadcast.lifeCycleStatus;
	if (status == "live" || status == "liveStarting") {
		vblog(LOG_INFO, "YouTube broadcast %s is already live",
		      broadcast.id.c_str());
		return;
	}

	// Broadcasts with an enabled monitor stream have to be in the testing
	// state before they can be transitioned to live
	if (broadcast.monitorStreamEnabled && status == "ready") {
		if (!transitionBroadcast(connection, broadcast.id, "testing")) {
			return;
		}
		status = "testStarting";
	}
	if (status == "testStarting" &&
	    !waitForLifeCycleStatus(connection, broadcast.id, "testing",
				    macro)) {
		return;
	}

	transitionBroadcast(connection, broadcast.id, "live");
}

bool MacroActionYouTube::PerformAction()
{
	auto connection = _connection.lock();
	if (!connection) {
		return true;
	}

	const auto action = _action;
	const auto broadcastSelection = _broadcast;
	SuspendLock suspendLock(*this);

	const auto broadcast = broadcastSelection.GetInfo(*connection);
	if (!broadcast) {
		blog(LOG_WARNING, "no matching YouTube broadcast found");
		return true;
	}

	switch (action) {
	case Action::START_BROADCAST:
		startBroadcast(*connection, *broadcast, GetMacro());
		break;
	case Action::END_BROADCAST:
		transitionBroadcast(*connection, broadcast->id, "complete");
		break;
	}
	return true;
}

void MacroActionYouTube::LogAction() const
{
	auto it = actionTypes.find(_action);
	if (it != actionTypes.end()) {
		ablog(LOG_INFO,
		      "performed action \"%s\" with YouTube connection \"%s\"",
		      it->second.c_str(),
		      GetWeakYouTubeConnectionName(_connection).c_str());
	} else {
		blog(LOG_WARNING, "ignored unknown YouTube action %d",
		     static_cast<int>(_action));
	}
}

bool MacroActionYouTube::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	obs_data_set_string(obj, "connection",
			    GetWeakYouTubeConnectionName(_connection).c_str());
	_broadcast.Save(obj);
	return true;
}

bool MacroActionYouTube::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_action = static_cast<Action>(obs_data_get_int(obj, "action"));
	_connection = GetWeakYouTubeConnectionByName(
		obs_data_get_string(obj, "connection"));
	_broadcast.Load(obj);
	return true;
}

std::shared_ptr<MacroAction> MacroActionYouTube::Create(Macro *m)
{
	return std::make_shared<MacroActionYouTube>(m);
}

std::shared_ptr<MacroAction> MacroActionYouTube::Copy() const
{
	return std::make_shared<MacroActionYouTube>(*this);
}

std::string MacroActionYouTube::GetShortDesc() const
{
	return GetWeakYouTubeConnectionName(_connection);
}

void MacroActionYouTube::ResolveVariablesToFixedValues()
{
	_broadcast.ResolveVariables();
}

static void populateActionSelection(QComboBox *list)
{
	for (const auto &[action, name] : actionTypes) {
		list->addItem(obs_module_text(name.c_str()),
			      static_cast<int>(action));
	}
}

MacroActionYouTubeEdit::MacroActionYouTubeEdit(
	QWidget *parent, std::shared_ptr<MacroActionYouTube> entryData)
	: QWidget(parent),
	  _connections(new YouTubeConnectionSelection(this)),
	  _actions(new QComboBox(this)),
	  _broadcast(new YouTubeBroadcastSelection(this)),
	  _connectionWarning(new QLabel(this))
{
	populateActionSelection(_actions);

	QWidget::connect(_connections,
			 SIGNAL(SelectionChanged(const QString &)), this,
			 SLOT(ConnectionChanged(const QString &)));
	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_broadcast,
			 SIGNAL(BroadcastChanged(const YouTubeBroadcast &)),
			 this,
			 SLOT(BroadcastChanged(const YouTubeBroadcast &)));
	QWidget::connect(&_connectionCheckTimer, SIGNAL(timeout()), this,
			 SLOT(CheckConnection()));

	auto entryLayout = new QHBoxLayout();
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.action.youtube.layout"),
		     entryLayout,
		     {{"{{connection}}", _connections},
		      {"{{actions}}", _actions}});

	auto layout = new QVBoxLayout();
	layout->addLayout(entryLayout);
	layout->addWidget(_broadcast);
	layout->addWidget(_connectionWarning);
	setLayout(layout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
	_connectionCheckTimer.start(1000);
}

void MacroActionYouTubeEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_connections->SetConnection(_entryData->_connection);
	_actions->setCurrentIndex(
		_actions->findData(static_cast<int>(_entryData->_action)));
	_broadcast->SetConnection(_entryData->_connection);
	_broadcast->SetBroadcast(_entryData->_broadcast);
	CheckConnection();
}

void MacroActionYouTubeEdit::ConnectionChanged(const QString &name)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_connection =
		GetWeakYouTubeConnectionByName(name.toStdString());
	_broadcast->SetConnection(_entryData->_connection);
	CheckConnection();
	emit HeaderInfoChanged(name);
}

void MacroActionYouTubeEdit::ActionChanged(int index)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_action = static_cast<MacroActionYouTube::Action>(
		_actions->itemData(index).toInt());
}

void MacroActionYouTubeEdit::BroadcastChanged(const YouTubeBroadcast &broadcast)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_broadcast = broadcast;
}

void MacroActionYouTubeEdit::CheckConnection()
{
	if (!_entryData) {
		return;
	}

	const auto connection = _entryData->_connection.lock();
	const char *warning = nullptr;
	if (!connection) {
		warning = "AdvSceneSwitcher.youtubeConnection.noSelection";
	} else if (!connection->IsValid()) {
		warning = "AdvSceneSwitcher.youtubeConnection.notValid";
	}

	const bool wasHidden = _connectionWarning->isHidden();
	if (warning) {
		_connectionWarning->setText(obs_module_text(warning));
	}
	_connectionWarning->setHidden(!warning);
	if (wasHidden != _connectionWarning->isHidden()) {
		adjustSize();
		updateGeometry();
	}
}

} // namespace advss
