#include "macro-action-youtube.hpp"
#include "youtube-helpers.hpp"

#include <layout-helpers.hpp>
#include <log-helper.hpp>
#include <macro-helpers.hpp>
#include <sync-helpers.hpp>

#include <nlohmann/json.hpp>
#include <QDateTime>
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
	{MacroActionYouTube::Action::CREATE_BROADCAST,
	 "AdvSceneSwitcher.action.youtube.type.createBroadcast"},
};

// The values are the ones used by the YouTube Data API
const static std::vector<std::pair<std::string, std::string>> privacyTypes = {
	{"private", "AdvSceneSwitcher.action.youtube.create.privacy.private"},
	{"unlisted", "AdvSceneSwitcher.action.youtube.create.privacy.unlisted"},
	{"public", "AdvSceneSwitcher.action.youtube.create.privacy.public"},
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

static std::string createBroadcast(YouTubeConnection &connection,
				   const nlohmann::json &body)
{
	const auto result = SendYouTubeRequest(
		connection, YouTubeRequestMethod::POST, "/liveBroadcasts",
		{{"part", "snippet,status,contentDetails"}},
		body.dump(-1, ' ', false,
			  nlohmann::json::error_handler_t::replace));
	if (result.status != 200) {
		return "";
	}
	return obs_data_get_string(result.data, "id");
}

static void bindBroadcastToObsStreamKey(YouTubeConnection &connection,
					const std::string &broadcastId)
{
	const auto streamId = GetObsStreamId(connection);
	if (streamId.empty()) {
		blog(LOG_WARNING,
		     "could not bind YouTube broadcast %s as the OBS stream key does not belong to channel \"%s\"",
		     broadcastId.c_str(), connection.Name().c_str());
		return;
	}

	SendYouTubeRequest(connection, YouTubeRequestMethod::POST,
			   "/liveBroadcasts/bind",
			   {{"id", broadcastId},
			    {"part", "id,contentDetails"},
			    {"streamId", streamId}});
}

void MacroActionYouTube::CreateBroadcast(YouTubeConnection &connection)
{
	const std::string title = _title;
	const nlohmann::json body = {
		{"snippet",
		 {{"title", title},
		  {"description", std::string(_description)},
		  {"scheduledStartTime", QDateTime::currentDateTimeUtc()
						 .toString(Qt::ISODate)
						 .toStdString()}}},
		{"status",
		 {{"privacyStatus", _privacy},
		  {"selfDeclaredMadeForKids", _madeForKids}}},
		{"contentDetails",
		 {{"enableAutoStart", _autoStart},
		  {"enableAutoStop", _autoStop}}},
	};
	const bool bindToObsStreamKey = _bindToObsStreamKey;

	std::string id;
	{
		SuspendLock suspendLock(*this);
		id = createBroadcast(connection, body);
		if (!id.empty() && bindToObsStreamKey) {
			bindBroadcastToObsStreamKey(connection, id);
		}
	}

	SetTempVarValue("id", id);
	SetTempVarValue("title", title);
}

bool MacroActionYouTube::PerformAction()
{
	auto connection = _connection.lock();
	if (!connection) {
		return true;
	}

	if (_action == Action::CREATE_BROADCAST) {
		CreateBroadcast(*connection);
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
	case Action::CREATE_BROADCAST:
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
	_title.Save(obj, "title");
	_description.Save(obj, "description");
	obs_data_set_string(obj, "privacy", _privacy.c_str());
	obs_data_set_bool(obj, "autoStart", _autoStart);
	obs_data_set_bool(obj, "autoStop", _autoStop);
	obs_data_set_bool(obj, "madeForKids", _madeForKids);
	obs_data_set_bool(obj, "bindToObsStreamKey", _bindToObsStreamKey);
	return true;
}

bool MacroActionYouTube::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	SetAction(static_cast<Action>(obs_data_get_int(obj, "action")));
	_connection = GetWeakYouTubeConnectionByName(
		obs_data_get_string(obj, "connection"));
	_broadcast.Load(obj);
	_title.Load(obj, "title");
	_description.Load(obj, "description");
	obs_data_set_default_string(obj, "privacy", "private");
	obs_data_set_default_bool(obj, "autoStart", true);
	obs_data_set_default_bool(obj, "autoStop", true);
	obs_data_set_default_bool(obj, "bindToObsStreamKey", true);
	_privacy = obs_data_get_string(obj, "privacy");
	_autoStart = obs_data_get_bool(obj, "autoStart");
	_autoStop = obs_data_get_bool(obj, "autoStop");
	_madeForKids = obs_data_get_bool(obj, "madeForKids");
	_bindToObsStreamKey = obs_data_get_bool(obj, "bindToObsStreamKey");
	return true;
}

void MacroActionYouTube::SetAction(Action action)
{
	_action = action;
	SetupTempVars();
}

void MacroActionYouTube::SetupTempVars()
{
	MacroAction::SetupTempVars();
	if (_action != Action::CREATE_BROADCAST) {
		return;
	}
	AddTempvar("id",
		   obs_module_text("AdvSceneSwitcher.tempVar.youtube.id"));
	AddTempvar("title",
		   obs_module_text("AdvSceneSwitcher.tempVar.youtube.title"));
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
	_title.ResolveVariables();
	_description.ResolveVariables();
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
	  _createSettings(new QWidget(this)),
	  _title(new VariableLineEdit(this)),
	  _description(new VariableTextEdit(this, 5, 1, 1)),
	  _privacy(new QComboBox(this)),
	  _autoStart(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.action.youtube.create.autoStart"))),
	  _autoStop(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.action.youtube.create.autoStop"))),
	  _madeForKids(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.action.youtube.create.madeForKids"))),
	  _bindToObsStreamKey(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.action.youtube.create.bindToObsStreamKey"))),
	  _connectionWarning(new QLabel(this))
{
	populateActionSelection(_actions);
	for (const auto &[value, name] : privacyTypes) {
		_privacy->addItem(obs_module_text(name.c_str()),
				  QString::fromStdString(value));
	}

	QWidget::connect(_connections,
			 SIGNAL(SelectionChanged(const QString &)), this,
			 SLOT(ConnectionChanged(const QString &)));
	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_broadcast,
			 SIGNAL(BroadcastChanged(const YouTubeBroadcast &)),
			 this,
			 SLOT(BroadcastChanged(const YouTubeBroadcast &)));
	QWidget::connect(_title, SIGNAL(editingFinished()), this,
			 SLOT(TitleChanged()));
	QWidget::connect(_description, SIGNAL(textChanged()), this,
			 SLOT(DescriptionChanged()));
	QWidget::connect(_privacy, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(PrivacyChanged(int)));
	for (const auto checkBox :
	     {_autoStart, _autoStop, _madeForKids, _bindToObsStreamKey}) {
		QWidget::connect(checkBox, SIGNAL(stateChanged(int)), this,
				 SLOT(CreateOptionsChanged()));
	}
	QWidget::connect(&_connectionCheckTimer, SIGNAL(timeout()), this,
			 SLOT(CheckConnection()));

	auto entryLayout = new QHBoxLayout();
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.action.youtube.layout"),
		     entryLayout,
		     {{"{{connection}}", _connections},
		      {"{{actions}}", _actions}});

	auto titleLayout = new QHBoxLayout();
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.action.youtube.create.layout.title"),
		titleLayout, {{"{{title}}", _title}}, false);
	auto descriptionLayout = new QHBoxLayout();
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.action.youtube.create.layout.description"),
		descriptionLayout, {{"{{description}}", _description}}, false);
	auto privacyLayout = new QHBoxLayout();
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.action.youtube.create.layout.privacy"),
		privacyLayout, {{"{{privacy}}", _privacy}});

	auto createLayout = new QVBoxLayout();
	createLayout->setContentsMargins(0, 0, 0, 0);
	createLayout->addLayout(titleLayout);
	createLayout->addLayout(descriptionLayout);
	createLayout->addLayout(privacyLayout);
	createLayout->addWidget(_autoStart);
	createLayout->addWidget(_autoStop);
	createLayout->addWidget(_madeForKids);
	createLayout->addWidget(_bindToObsStreamKey);
	_createSettings->setLayout(createLayout);

	auto layout = new QVBoxLayout();
	layout->addLayout(entryLayout);
	layout->addWidget(_broadcast);
	layout->addWidget(_createSettings);
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
		_actions->findData(static_cast<int>(_entryData->GetAction())));
	_broadcast->SetConnection(_entryData->_connection);
	_broadcast->SetBroadcast(_entryData->_broadcast);
	_title->setText(_entryData->_title);
	_description->setPlainText(_entryData->_description);
	_privacy->setCurrentIndex(_privacy->findData(
		QString::fromStdString(_entryData->_privacy)));
	_autoStart->setChecked(_entryData->_autoStart);
	_autoStop->setChecked(_entryData->_autoStop);
	_madeForKids->setChecked(_entryData->_madeForKids);
	_bindToObsStreamKey->setChecked(_entryData->_bindToObsStreamKey);
	SetWidgetVisibility();
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
	_entryData->SetAction(static_cast<MacroActionYouTube::Action>(
		_actions->itemData(index).toInt()));
	SetWidgetVisibility();
}

void MacroActionYouTubeEdit::TitleChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_title = _title->text().toStdString();
}

void MacroActionYouTubeEdit::DescriptionChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_description = _description->toPlainText().toStdString();
	adjustSize();
	updateGeometry();
}

void MacroActionYouTubeEdit::PrivacyChanged(int index)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_privacy =
		_privacy->itemData(index).toString().toStdString();
}

void MacroActionYouTubeEdit::CreateOptionsChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_autoStart = _autoStart->isChecked();
	_entryData->_autoStop = _autoStop->isChecked();
	_entryData->_madeForKids = _madeForKids->isChecked();
	_entryData->_bindToObsStreamKey = _bindToObsStreamKey->isChecked();
}

void MacroActionYouTubeEdit::SetWidgetVisibility()
{
	if (!_entryData) {
		return;
	}

	const bool isCreate = _entryData->GetAction() ==
			      MacroActionYouTube::Action::CREATE_BROADCAST;
	_broadcast->setVisible(!isCreate);
	_createSettings->setVisible(isCreate);
	adjustSize();
	updateGeometry();
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
