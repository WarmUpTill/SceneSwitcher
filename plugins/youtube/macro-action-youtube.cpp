#include "macro-action-youtube.hpp"
#include "youtube-helpers.hpp"

#include <layout-helpers.hpp>
#include <log-helper.hpp>
#include <macro-helpers.hpp>
#include <sync-helpers.hpp>

#include <nlohmann/json.hpp>
#include <QDateTime>
#include <QFile>
#include <QHBoxLayout>
#include <QLocale>
#include <QMimeDatabase>
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

const static std::vector<std::pair<std::string, std::string>> latencyTypes = {
	{"normal", "AdvSceneSwitcher.action.youtube.create.latency.normal"},
	{"low", "AdvSceneSwitcher.action.youtube.create.latency.low"},
	{"ultraLow", "AdvSceneSwitcher.action.youtube.create.latency.ultraLow"},
};

// Limit enforced by YouTube for custom thumbnails
static const qint64 maxThumbnailSize = 2 * 1024 * 1024;

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

static void setVideoCategory(YouTubeConnection &connection,
			     const std::string &videoId,
			     const std::string &title,
			     const std::string &description,
			     const std::string &categoryId)
{
	// The snippet is replaced as a whole, so title and description have to
	// be sent again
	const nlohmann::json body = {
		{"id", videoId},
		{"snippet",
		 {{"title", title},
		  {"description", description},
		  {"categoryId", categoryId}}},
	};
	SendYouTubeRequest(connection, YouTubeRequestMethod::PUT, "/videos",
			   {{"part", "snippet"}},
			   body.dump(-1, ' ', false,
				     nlohmann::json::error_handler_t::replace));
}

static void uploadThumbnail(YouTubeConnection &connection,
			    const std::string &videoId, const QString &path)
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly)) {
		blog(LOG_WARNING, "could not open YouTube thumbnail \"%s\"",
		     path.toUtf8().constData());
		return;
	}
	if (file.size() > maxThumbnailSize) {
		blog(LOG_WARNING,
		     "YouTube thumbnail \"%s\" is larger than 2 MB",
		     path.toUtf8().constData());
		return;
	}

	const auto data = file.readAll();
	const auto mimeType = QMimeDatabase().mimeTypeForData(data).name();
	SendYouTubeUploadRequest(connection, "/thumbnails/set",
				 {{"videoId", videoId}},
				 std::string(data.constData(), data.size()),
				 mimeType.toStdString());
}

static std::string getScheduledStartTime(bool scheduleForLater,
					 const Duration &startIn)
{
	auto startTime = QDateTime::currentDateTimeUtc();
	if (scheduleForLater) {
		startTime = startTime.addMSecs(
			static_cast<qint64>(startIn.Milliseconds()));
	}
	return startTime.toString(Qt::ISODate).toStdString();
}

void MacroActionYouTube::CreateBroadcast(YouTubeConnection &connection)
{
	const std::string title = _title;
	const std::string description = _description;
	const nlohmann::json body = {
		{"snippet",
		 {{"title", title},
		  {"description", description},
		  {"scheduledStartTime",
		   getScheduledStartTime(_scheduleForLater,
					 _scheduledStartIn)}}},
		{"status",
		 {{"privacyStatus", _privacy},
		  {"selfDeclaredMadeForKids", _madeForKids}}},
		{"contentDetails",
		 {{"latencyPreference", _latency},
		  {"enableAutoStart", _autoStart},
		  {"enableAutoStop", _autoStop},
		  {"enableDvr", _dvr},
		  {"projection", _projection360 ? "360" : "rectangular"},
		  // Same as broadcasts created by OBS, which allows going live
		  // without the testing state first
		  {"monitorStream", {{"enableMonitorStream", false}}}}},
	};
	const auto categoryId = _categoryId;
	const auto thumbnail = QString::fromStdString(_thumbnail);
	const bool bindToObsStreamKey = _bindToObsStreamKey;

	std::string id;
	{
		SuspendLock suspendLock(*this);
		id = createBroadcast(connection, body);
		if (!id.empty() && !categoryId.empty()) {
			setVideoCategory(connection, id, title, description,
					 categoryId);
		}
		if (!id.empty() && !thumbnail.isEmpty()) {
			uploadThumbnail(connection, id, thumbnail);
		}
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
	obs_data_set_string(obj, "categoryId", _categoryId.c_str());
	obs_data_set_string(obj, "categoryTitle", _categoryTitle.c_str());
	_thumbnail.Save(obj, "thumbnail");
	obs_data_set_string(obj, "latency", _latency.c_str());
	obs_data_set_bool(obj, "dvr", _dvr);
	obs_data_set_bool(obj, "projection360", _projection360);
	obs_data_set_bool(obj, "scheduleForLater", _scheduleForLater);
	_scheduledStartIn.Save(obj, "scheduledStartIn");
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
	_categoryId = obs_data_get_string(obj, "categoryId");
	_categoryTitle = obs_data_get_string(obj, "categoryTitle");
	_thumbnail.Load(obj, "thumbnail");
	obs_data_set_default_string(obj, "latency", "normal");
	obs_data_set_default_bool(obj, "dvr", true);
	_latency = obs_data_get_string(obj, "latency");
	_dvr = obs_data_get_bool(obj, "dvr");
	_projection360 = obs_data_get_bool(obj, "projection360");
	_scheduleForLater = obs_data_get_bool(obj, "scheduleForLater");
	_scheduledStartIn.Load(obj, "scheduledStartIn");
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
	_thumbnail.ResolveVariables();
	_scheduledStartIn.ResolveVariables();
}

static YouTubeRequestResult
requestVideoCategories(YouTubeConnection &connection,
		       const std::string &language)
{
	// Other regions lack categories which are still available to channels
	// in those regions, so only the language is localized like OBS does
	return SendYouTubeRequest(
		connection, YouTubeRequestMethod::GET, "/videoCategories",
		{{"part", "snippet"}, {"regionCode", "US"}, {"hl", language}},
		"", true);
}

static std::vector<YouTubeVideoCategory>
getVideoCategories(YouTubeConnection &connection)
{
	auto result = requestVideoCategories(connection,
					     QLocale().name().toStdString());
	if (result.errorReason == "unsupportedLanguageCode" ||
	    result.errorReason == "invalidLanguage") {
		result = requestVideoCategories(connection, "en_US");
	}
	if (result.status != 200) {
		return {};
	}

	std::vector<YouTubeVideoCategory> categories;
	OBSDataArrayAutoRelease items =
		obs_data_get_array(result.data, "items");
	const size_t count = obs_data_array_count(items);
	for (size_t i = 0; i < count; i++) {
		OBSDataAutoRelease item = obs_data_array_item(items, i);
		OBSDataAutoRelease snippet = obs_data_get_obj(item, "snippet");
		if (!obs_data_get_bool(snippet, "assignable")) {
			continue;
		}
		categories.push_back({obs_data_get_string(item, "id"),
				      obs_data_get_string(snippet, "title")});
	}
	return categories;
}

static void populateActionSelection(QComboBox *list)
{
	for (const auto &[action, name] : actionTypes) {
		list->addItem(obs_module_text(name.c_str()),
			      static_cast<int>(action));
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

MacroActionYouTubeEdit::MacroActionYouTubeEdit(
	QWidget *parent, std::shared_ptr<MacroActionYouTube> entryData)
	: QWidget(parent),
	  _connections(new YouTubeConnectionSelection(this)),
	  _actions(new QComboBox(this)),
	  _broadcast(new YouTubeBroadcastSelection(this)),
	  _help(new HelpIcon("", this)),
	  _createSettings(new QWidget(this)),
	  _title(new VariableLineEdit(this)),
	  _description(new VariableTextEdit(this, 5, 1, 1)),
	  _privacy(new QComboBox(this)),
	  _category(new LazyFilterComboBox(this)),
	  _thumbnail(new FileSelection(FileSelection::Type::READ, this)),
	  _latency(new QComboBox(this)),
	  _scheduleForLater(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.action.youtube.create.scheduleForLater"))),
	  _scheduledStartIn(new DurationSelection(this)),
	  _autoStart(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.action.youtube.create.autoStart"))),
	  _autoStop(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.action.youtube.create.autoStop"))),
	  _dvr(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.action.youtube.create.dvr"))),
	  _projection360(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.action.youtube.create.projection360"))),
	  _madeForKids(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.action.youtube.create.madeForKids"))),
	  _bindToObsStreamKey(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.action.youtube.create.bindToObsStreamKey"))),
	  _connectionWarning(new QLabel(this))
{
	populateActionSelection(_actions);
	populateValueSelection(_privacy, privacyTypes);
	populateValueSelection(_latency, latencyTypes);
	_thumbnail->setToolTip(obs_module_text(
		"AdvSceneSwitcher.action.youtube.create.thumbnail.tooltip"));

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
	QWidget::connect(_category, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(CategoryChanged(int)));
	QWidget::connect(_category, SIGNAL(ItemsNeeded()), this,
			 SLOT(UpdateCategoryList()));
	QWidget::connect(_thumbnail, SIGNAL(PathChanged(const QString &)), this,
			 SLOT(ThumbnailChanged(const QString &)));
	QWidget::connect(_latency, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(LatencyChanged(int)));
	QWidget::connect(_scheduledStartIn,
			 SIGNAL(DurationChanged(const Duration &)), this,
			 SLOT(ScheduledStartInChanged(const Duration &)));
	for (const auto checkBox :
	     {_scheduleForLater, _autoStart, _autoStop, _dvr, _projection360,
	      _madeForKids, _bindToObsStreamKey}) {
		QWidget::connect(checkBox, SIGNAL(stateChanged(int)), this,
				 SLOT(CreateOptionsChanged()));
	}
	QWidget::connect(&_connectionCheckTimer, SIGNAL(timeout()), this,
			 SLOT(CheckConnection()));

	auto entryLayout = new QHBoxLayout();
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.action.youtube.layout"),
		     entryLayout,
		     {{"{{connection}}", _connections},
		      {"{{actions}}", _actions},
		      {"{{help}}", _help}});

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
	auto categoryLayout = new QHBoxLayout();
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.action.youtube.create.layout.category"),
		categoryLayout, {{"{{category}}", _category}});
	auto thumbnailLayout = new QHBoxLayout();
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.action.youtube.create.layout.thumbnail"),
		thumbnailLayout, {{"{{thumbnail}}", _thumbnail}}, false);
	auto latencyLayout = new QHBoxLayout();
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.action.youtube.create.layout.latency"),
		latencyLayout, {{"{{latency}}", _latency}});
	auto scheduleLayout = new QHBoxLayout();
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.action.youtube.create.layout.schedule"),
		scheduleLayout,
		{{"{{scheduleForLater}}", _scheduleForLater},
		 {"{{scheduledStartIn}}", _scheduledStartIn}});

	auto createLayout = new QVBoxLayout();
	createLayout->setContentsMargins(0, 0, 0, 0);
	createLayout->addLayout(titleLayout);
	createLayout->addLayout(descriptionLayout);
	createLayout->addLayout(privacyLayout);
	createLayout->addLayout(categoryLayout);
	createLayout->addLayout(thumbnailLayout);
	createLayout->addLayout(latencyLayout);
	createLayout->addLayout(scheduleLayout);
	createLayout->addWidget(_autoStart);
	createLayout->addWidget(_autoStop);
	createLayout->addWidget(_dvr);
	createLayout->addWidget(_projection360);
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
	PopulateCategories({});
	_thumbnail->SetPath(_entryData->_thumbnail);
	_latency->setCurrentIndex(_latency->findData(
		QString::fromStdString(_entryData->_latency)));
	_scheduleForLater->setChecked(_entryData->_scheduleForLater);
	_scheduledStartIn->SetDuration(_entryData->_scheduledStartIn);
	_autoStart->setChecked(_entryData->_autoStart);
	_autoStop->setChecked(_entryData->_autoStop);
	_dvr->setChecked(_entryData->_dvr);
	_projection360->setChecked(_entryData->_projection360);
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

void MacroActionYouTubeEdit::PopulateCategories(
	const std::vector<YouTubeVideoCategory> &categories)
{
	if (!_entryData) {
		return;
	}

	const QSignalBlocker blocker(_category);
	_category->clear();
	_category->addItem(
		obs_module_text(
			"AdvSceneSwitcher.action.youtube.create.category.default"),
		QString());

	const auto &selectedId = _entryData->_categoryId;
	bool selectedCategoryListed = false;
	for (const auto &category : categories) {
		_category->addItem(QString::fromStdString(category.title),
				   QString::fromStdString(category.id));
		selectedCategoryListed |= category.id == selectedId;
	}

	// Keep showing the selection if the list was not fetched yet
	if (!selectedId.empty() && !selectedCategoryListed) {
		_category->addItem(
			QString::fromStdString(_entryData->_categoryTitle),
			QString::fromStdString(selectedId));
	}

	_category->setCurrentIndex(
		_category->findData(QString::fromStdString(selectedId)));
}

void MacroActionYouTubeEdit::UpdateCategoryList()
{
	if (!_entryData) {
		return;
	}

	auto connection = _entryData->_connection.lock();
	PopulateCategories(connection ? getVideoCategories(*connection)
				      : std::vector<YouTubeVideoCategory>());
}

void MacroActionYouTubeEdit::CategoryChanged(int index)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_categoryId =
		_category->itemData(index).toString().toStdString();
	_entryData->_categoryTitle =
		_entryData->_categoryId.empty()
			? ""
			: _category->itemText(index).toStdString();
}

void MacroActionYouTubeEdit::ThumbnailChanged(const QString &path)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_thumbnail = path.toStdString();
}

void MacroActionYouTubeEdit::LatencyChanged(int index)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_latency =
		_latency->itemData(index).toString().toStdString();
}

void MacroActionYouTubeEdit::ScheduledStartInChanged(const Duration &duration)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_scheduledStartIn = duration;
}

void MacroActionYouTubeEdit::CreateOptionsChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_scheduleForLater = _scheduleForLater->isChecked();
	_entryData->_autoStart = _autoStart->isChecked();
	_entryData->_autoStop = _autoStop->isChecked();
	_entryData->_dvr = _dvr->isChecked();
	_entryData->_projection360 = _projection360->isChecked();
	_entryData->_madeForKids = _madeForKids->isChecked();
	_entryData->_bindToObsStreamKey = _bindToObsStreamKey->isChecked();
	_scheduledStartIn->setEnabled(_entryData->_scheduleForLater);
}

void MacroActionYouTubeEdit::SetWidgetVisibility()
{
	if (!_entryData) {
		return;
	}

	const auto action = _entryData->GetAction();
	const bool isCreate = action ==
			      MacroActionYouTube::Action::CREATE_BROADCAST;
	const bool isStart = action ==
			     MacroActionYouTube::Action::START_BROADCAST;
	_broadcast->setVisible(!isCreate);
	_createSettings->setVisible(isCreate);
	_scheduledStartIn->setEnabled(_entryData->_scheduleForLater);
	_help->setVisible(isCreate || isStart);
	_help->setToolTip(obs_module_text(
		isCreate ? "AdvSceneSwitcher.action.youtube.create.help"
			 : "AdvSceneSwitcher.action.youtube.start.help"));
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
