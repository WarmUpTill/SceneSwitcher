#include "macro-condition-youtube.hpp"
#include "youtube-helpers.hpp"

#include <layout-helpers.hpp>
#include <sync-helpers.hpp>

#include <QHBoxLayout>
#include <QVBoxLayout>

namespace advss {

const std::string MacroConditionYouTube::id = "youtube";

bool MacroConditionYouTube::_registered = MacroConditionFactory::Register(
	MacroConditionYouTube::id,
	{MacroConditionYouTube::Create, MacroConditionYouTubeEdit::Create,
	 "AdvSceneSwitcher.condition.youtube"});

const static std::map<MacroConditionYouTube::Condition, std::string>
	conditionTypes = {
		{MacroConditionYouTube::Condition::BROADCAST_STATUS,
		 "AdvSceneSwitcher.condition.youtube.type.broadcastStatus"},
		{MacroConditionYouTube::Condition::STREAM_HEALTH,
		 "AdvSceneSwitcher.condition.youtube.type.streamHealth"},
};

// The values are the ones used by the YouTube Data API
const static std::vector<std::pair<std::string, std::string>>
	broadcastStatuses = {
		{"ready",
		 "AdvSceneSwitcher.condition.youtube.broadcastStatus.ready"},
		{"testing",
		 "AdvSceneSwitcher.condition.youtube.broadcastStatus.testing"},
		{"live",
		 "AdvSceneSwitcher.condition.youtube.broadcastStatus.live"},
		{"complete",
		 "AdvSceneSwitcher.condition.youtube.broadcastStatus.complete"},
};

const static std::vector<std::pair<std::string, std::string>> streamHealths = {
	{"good", "AdvSceneSwitcher.condition.youtube.streamHealth.good"},
	{"ok", "AdvSceneSwitcher.condition.youtube.streamHealth.ok"},
	{"bad", "AdvSceneSwitcher.condition.youtube.streamHealth.bad"},
	{"noData", "AdvSceneSwitcher.condition.youtube.streamHealth.noData"},
};

static std::string getStreamHealth(YouTubeConnection &connection,
				   const std::string &streamId)
{
	if (streamId.empty()) {
		return "";
	}

	const auto result = SendYouTubeRequest(
		connection, YouTubeRequestMethod::GET, "/liveStreams",
		{{"part", "status"}, {"id", streamId}}, "", true);
	OBSDataArrayAutoRelease items =
		obs_data_get_array(result.data, "items");
	OBSDataAutoRelease item = obs_data_array_item(items, 0);
	OBSDataAutoRelease status = obs_data_get_obj(item, "status");
	OBSDataAutoRelease healthStatus =
		obs_data_get_obj(status, "healthStatus");
	return obs_data_get_string(healthStatus, "status");
}

bool MacroConditionYouTube::CheckCondition()
{
	auto connection = _connection.lock();
	if (!connection) {
		return false;
	}

	const auto condition = _condition;
	const auto broadcastSelection = _broadcast;
	std::optional<YouTubeBroadcastInfo> broadcast;
	std::string streamHealth;
	{
		SuspendLock suspendLock(static_cast<MacroCondition &>(*this));
		broadcast = broadcastSelection.GetInfo(*connection, true);
		if (broadcast && condition == Condition::STREAM_HEALTH) {
			streamHealth = getStreamHealth(
				*connection, broadcast->boundStreamId);
		}
	}

	if (!broadcast) {
		return false;
	}

	SetTempVarValue("id", broadcast->id);
	SetTempVarValue("title", broadcast->title);
	SetTempVarValue("broadcastStatus", broadcast->lifeCycleStatus);
	SetTempVarValue("streamHealth", streamHealth);

	switch (condition) {
	case Condition::BROADCAST_STATUS:
		return broadcast->lifeCycleStatus == _broadcastStatus;
	case Condition::STREAM_HEALTH:
		return streamHealth == _streamHealth;
	}
	return false;
}

bool MacroConditionYouTube::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	obs_data_set_int(obj, "condition", static_cast<int>(_condition));
	obs_data_set_string(obj, "connection",
			    GetWeakYouTubeConnectionName(_connection).c_str());
	_broadcast.Save(obj);
	obs_data_set_string(obj, "broadcastStatus", _broadcastStatus.c_str());
	obs_data_set_string(obj, "streamHealth", _streamHealth.c_str());
	return true;
}

bool MacroConditionYouTube::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_condition = static_cast<Condition>(obs_data_get_int(obj, "condition"));
	_connection = GetWeakYouTubeConnectionByName(
		obs_data_get_string(obj, "connection"));
	_broadcast.Load(obj);
	obs_data_set_default_string(obj, "broadcastStatus", "live");
	obs_data_set_default_string(obj, "streamHealth", "good");
	_broadcastStatus = obs_data_get_string(obj, "broadcastStatus");
	_streamHealth = obs_data_get_string(obj, "streamHealth");
	return true;
}

std::string MacroConditionYouTube::GetShortDesc() const
{
	return GetWeakYouTubeConnectionName(_connection);
}

void MacroConditionYouTube::SetupTempVars()
{
	MacroCondition::SetupTempVars();
	AddTempvar("id",
		   obs_module_text("AdvSceneSwitcher.tempVar.youtube.id"));
	AddTempvar("title",
		   obs_module_text("AdvSceneSwitcher.tempVar.youtube.title"));
	AddTempvar("broadcastStatus",
		   obs_module_text(
			   "AdvSceneSwitcher.tempVar.youtube.broadcastStatus"));
	AddTempvar(
		"streamHealth",
		obs_module_text(
			"AdvSceneSwitcher.tempVar.youtube.streamHealth"),
		obs_module_text(
			"AdvSceneSwitcher.tempVar.youtube.streamHealth.description"));
}

static void populateConditionSelection(QComboBox *list)
{
	for (const auto &[condition, name] : conditionTypes) {
		list->addItem(obs_module_text(name.c_str()),
			      static_cast<int>(condition));
	}
}

static void populateValueSelection(
	QComboBox *list,
	const std::vector<std::pair<std::string, std::string>> &values)
{
	for (const auto &[value, name] : values) {
		list->addItem(obs_module_text(name.c_str()),
			      QString::fromStdString(value));
	}
}

MacroConditionYouTubeEdit::MacroConditionYouTubeEdit(
	QWidget *parent, std::shared_ptr<MacroConditionYouTube> entryData)
	: QWidget(parent),
	  _connections(new YouTubeConnectionSelection(this)),
	  _conditions(new QComboBox(this)),
	  _broadcastStatus(new QComboBox(this)),
	  _streamHealth(new QComboBox(this)),
	  _broadcast(new YouTubeBroadcastSelection(this)),
	  _connectionWarning(new QLabel(this))
{
	populateConditionSelection(_conditions);
	populateValueSelection(_broadcastStatus, broadcastStatuses);
	populateValueSelection(_streamHealth, streamHealths);

	QWidget::connect(_connections,
			 SIGNAL(SelectionChanged(const QString &)), this,
			 SLOT(ConnectionChanged(const QString &)));
	QWidget::connect(_conditions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(_broadcastStatus, SIGNAL(currentIndexChanged(int)),
			 this, SLOT(BroadcastStatusChanged(int)));
	QWidget::connect(_streamHealth, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(StreamHealthChanged(int)));
	QWidget::connect(_broadcast,
			 SIGNAL(BroadcastChanged(const YouTubeBroadcast &)),
			 this,
			 SLOT(BroadcastChanged(const YouTubeBroadcast &)));
	QWidget::connect(&_connectionCheckTimer, SIGNAL(timeout()), this,
			 SLOT(CheckConnection()));

	auto entryLayout = new QHBoxLayout();
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.condition.youtube.layout"),
		entryLayout,
		{{"{{connection}}", _connections},
		 {"{{conditions}}", _conditions},
		 {"{{broadcastStatus}}", _broadcastStatus},
		 {"{{streamHealth}}", _streamHealth}});

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

void MacroConditionYouTubeEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_connections->SetConnection(_entryData->_connection);
	_conditions->setCurrentIndex(_conditions->findData(
		static_cast<int>(_entryData->_condition)));
	_broadcastStatus->setCurrentIndex(_broadcastStatus->findData(
		QString::fromStdString(_entryData->_broadcastStatus)));
	_streamHealth->setCurrentIndex(_streamHealth->findData(
		QString::fromStdString(_entryData->_streamHealth)));
	_broadcast->SetBroadcast(_entryData->_broadcast);
	SetWidgetVisibility();
	CheckConnection();
}

void MacroConditionYouTubeEdit::ConnectionChanged(const QString &name)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_connection =
		GetWeakYouTubeConnectionByName(name.toStdString());
	CheckConnection();
	emit HeaderInfoChanged(name);
}

void MacroConditionYouTubeEdit::ConditionChanged(int index)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_condition = static_cast<MacroConditionYouTube::Condition>(
		_conditions->itemData(index).toInt());
	SetWidgetVisibility();
}

void MacroConditionYouTubeEdit::BroadcastStatusChanged(int index)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_broadcastStatus =
		_broadcastStatus->itemData(index).toString().toStdString();
}

void MacroConditionYouTubeEdit::StreamHealthChanged(int index)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_streamHealth =
		_streamHealth->itemData(index).toString().toStdString();
}

void MacroConditionYouTubeEdit::BroadcastChanged(
	const YouTubeBroadcast &broadcast)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_broadcast = broadcast;
}

void MacroConditionYouTubeEdit::CheckConnection()
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

void MacroConditionYouTubeEdit::SetWidgetVisibility()
{
	if (!_entryData) {
		return;
	}

	_broadcastStatus->setVisible(
		_entryData->_condition ==
		MacroConditionYouTube::Condition::BROADCAST_STATUS);
	_streamHealth->setVisible(
		_entryData->_condition ==
		MacroConditionYouTube::Condition::STREAM_HEALTH);
	adjustSize();
	updateGeometry();
}

} // namespace advss
