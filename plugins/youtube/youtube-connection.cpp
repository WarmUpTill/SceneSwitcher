#include "youtube-connection.hpp"
#include "youtube-helpers.hpp"
#include "youtube-tab.hpp"

#include <help-icon.hpp>
#include <layout-helpers.hpp>
#include <log-helper.hpp>
#include <macro-export-extensions.hpp>
#include <obs-module-helper.hpp>
#include <plugin-state-helpers.hpp>
#include <ui-helpers.hpp>

#include <QDesktopServices>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>
#include <thread>
#include <unordered_map>

namespace advss {

static const int authTimeoutSeconds = 120;
static const int refreshRetryDelaySeconds = 30;

static std::deque<std::shared_ptr<Item>> youtubeConnections;

static void saveConnections(obs_data_t *obj);
static void loadConnections(obs_data_t *obj);

static void exportConnections(obs_data_t *data, const QStringList &selectedIds)
{
	OBSDataArrayAutoRelease array = obs_data_array_create();
	for (const auto &item : youtubeConnections) {
		if (!selectedIds.isEmpty() &&
		    !selectedIds.contains(
			    QString::fromStdString(item->Name()))) {
			continue;
		}
		auto connection =
			std::dynamic_pointer_cast<YouTubeConnection>(item);
		OBSDataAutoRelease obj = obs_data_create();
		connection->SaveForExport(obj);
		obs_data_array_push_back(array, obj);
	}
	obs_data_set_array(data, "youtubeConnections", array);
}

static void importConnections(obs_data_t *data, const QStringList &)
{
	OBSDataArrayAutoRelease array =
		obs_data_get_array(data, "youtubeConnections");
	const size_t count = obs_data_array_count(array);
	for (size_t i = 0; i < count; ++i) {
		OBSDataAutoRelease obj = obs_data_array_item(array, i);
		auto connection = YouTubeConnection::Create();
		connection->Load(obj);
		if (!GetWeakYouTubeConnectionByName(connection->Name())
			     .expired()) {
			continue;
		}
		youtubeConnections.emplace_back(connection);
		YouTubeConnectionSignalManager::Instance()->Add(
			QString::fromStdString(connection->Name()));
	}
}

static QList<QPair<QString, QString>> getExportItems()
{
	QList<QPair<QString, QString>> items;
	for (const auto &connection : youtubeConnections) {
		const auto name = QString::fromStdString(connection->Name());
		items.append({name, name});
	}
	return items;
}

static bool setup()
{
	AddSaveStep(saveConnections);
	AddLoadStep(loadConnections);
	AddPluginCleanupStep([]() { youtubeConnections.clear(); });
	AddMacroExportExtension(
		{"AdvSceneSwitcher.macroTab.export.youtubeConnections",
		 "youtubeConnections", exportConnections, importConnections,
		 getExportItems});
	return true;
}

static bool setupDone = setup();

static void saveConnections(obs_data_t *obj)
{
	OBSDataArrayAutoRelease array = obs_data_array_create();
	for (const auto &connection : youtubeConnections) {
		OBSDataAutoRelease arrayObj = obs_data_create();
		connection->Save(arrayObj);
		obs_data_array_push_back(array, arrayObj);
	}
	obs_data_set_array(obj, "youtubeConnections", array);
}

static void loadConnections(obs_data_t *obj)
{
	youtubeConnections.clear();

	OBSDataArrayAutoRelease array =
		obs_data_get_array(obj, "youtubeConnections");
	const size_t count = obs_data_array_count(array);
	for (size_t i = 0; i < count; i++) {
		OBSDataAutoRelease arrayObj = obs_data_array_item(array, i);
		auto connection = YouTubeConnection::Create();
		connection->Load(arrayObj);
		youtubeConnections.emplace_back(connection);
	}
}

YouTubeConnection::YouTubeConnection(const YouTubeConnection &other)
	: Item(other._name),
	  _customClientId(other._customClientId),
	  _customClientSecret(other._customClientSecret),
	  _refreshToken(other._refreshToken),
	  _channelId(other._channelId),
	  _warnIfInvalid(other._warnIfInvalid.load())
{
	std::lock_guard<std::mutex> lock(other._mutex);
	_accessToken = other._accessToken;
	_accessTokenExpiry = other._accessTokenExpiry;
	_refreshTokenRevoked = other._refreshTokenRevoked;
}

YouTubeConnection &YouTubeConnection::operator=(const YouTubeConnection &other)
{
	if (this == &other) {
		return *this;
	}

	std::scoped_lock lock(_mutex, other._mutex);
	_name = other._name;
	_customClientId = other._customClientId;
	_customClientSecret = other._customClientSecret;
	_refreshToken = other._refreshToken;
	_channelId = other._channelId;
	_warnIfInvalid = other._warnIfInvalid.load();
	_accessToken = other._accessToken;
	_accessTokenExpiry = other._accessTokenExpiry;
	_refreshTokenRevoked = other._refreshTokenRevoked;
	return *this;
}

void YouTubeConnection::Load(obs_data_t *obj)
{
	std::lock_guard<std::mutex> lock(_mutex);
	Item::Load(obj);
	_customClientId = obs_data_get_string(obj, "customClientID");
	_customClientSecret = obs_data_get_string(obj, "customClientSecret");
	_refreshToken = obs_data_get_string(obj, "refreshToken");
	_channelId = obs_data_get_string(obj, "channelID");
	obs_data_set_default_bool(obj, "warnIfInvalid", true);
	_warnIfInvalid = obs_data_get_bool(obj, "warnIfInvalid");
	_accessToken.clear();
	_refreshTokenRevoked = false;
}

void YouTubeConnection::Save(obs_data_t *obj) const
{
	SaveForExport(obj);
	std::lock_guard<std::mutex> lock(_mutex);
	obs_data_set_string(obj, "customClientSecret",
			    _customClientSecret.c_str());
	obs_data_set_string(obj, "refreshToken", _refreshToken.c_str());
	obs_data_set_bool(obj, "warnIfInvalid", _warnIfInvalid);
}

void YouTubeConnection::SaveForExport(obs_data_t *obj) const
{
	std::lock_guard<std::mutex> lock(_mutex);
	Item::Save(obj);
	obs_data_set_string(obj, "customClientID", _customClientId.c_str());
	obs_data_set_string(obj, "channelID", _channelId.c_str());
}

std::optional<OAuthClientConfig> YouTubeConnection::GetClientConfig() const
{
	if (_customClientId.empty()) {
		return GetBuiltInClientConfig();
	}
	return OAuthClientConfig{_customClientId, _customClientSecret};
}

std::optional<std::string> YouTubeConnection::GetAccessToken(bool forceRefresh)
{
	std::lock_guard<std::mutex> lock(_mutex);

	const auto timeLeft =
		_accessTokenExpiry - std::chrono::system_clock::now();
	if (!forceRefresh && !_accessToken.empty() &&
	    timeLeft > std::chrono::seconds(60)) {
		return _accessToken;
	}

	if (_refreshToken.empty() || _refreshTokenRevoked) {
		return {};
	}

	const auto config = GetClientConfig();
	if (!config) {
		return {};
	}

	const auto now = std::chrono::steady_clock::now();
	if (now - _lastRefreshFailure <
	    std::chrono::seconds(refreshRetryDelaySeconds)) {
		return {};
	}

	const auto response = RefreshAccessToken(*config, _refreshToken);
	if (response.error == "invalid_grant") {
		blog(LOG_WARNING,
		     "YouTube connection \"%s\" was revoked or expired!",
		     _name.c_str());
		_refreshTokenRevoked = true;
		_accessToken.clear();
		if (_warnIfInvalid) {
			const auto name = QString::fromStdString(_name);
			QueueUITask([name]() {
				InvalidConnectionDialog::ShowWarning(name);
			});
		}
		return {};
	}
	if (!response.error.empty()) {
		_lastRefreshFailure = now;
		return {};
	}

	vblog(LOG_INFO, "refreshed access token of YouTube connection \"%s\"",
	      _name.c_str());
	_accessToken = response.accessToken;
	_accessTokenExpiry = response.expiresAt;
	if (!response.refreshToken.empty()) {
		_refreshToken = response.refreshToken;
	}
	return _accessToken;
}

bool YouTubeConnection::SetAuthResult(const TokenResponse &response,
				      std::string &errorReason)
{
	if (!response.error.empty()) {
		errorReason = response.error;
		return false;
	}
	if (response.refreshToken.empty()) {
		errorReason = "noRefreshToken";
		return false;
	}

	{
		std::lock_guard<std::mutex> lock(_mutex);
		_accessToken = response.accessToken;
		_accessTokenExpiry = response.expiresAt;
		_refreshToken = response.refreshToken;
		_refreshTokenRevoked = false;
		_lastRefreshFailure = {};
	}

	const auto result = SendYouTubeRequest(
		*this, YouTubeRequestMethod::GET, "/channels",
		{{"part", "snippet"}, {"mine", "true"}});
	if (result.status != 200) {
		errorReason = result.errorReason.empty() ? "requestFailed"
							 : result.errorReason;
		return false;
	}

	OBSDataArrayAutoRelease items =
		obs_data_get_array(result.data, "items");
	OBSDataAutoRelease channel = obs_data_array_item(items, 0);
	if (!channel) {
		errorReason = "noChannel";
		return false;
	}

	OBSDataAutoRelease snippet = obs_data_get_obj(channel, "snippet");
	std::lock_guard<std::mutex> lock(_mutex);
	_channelId = obs_data_get_string(channel, "id");
	_name = obs_data_get_string(snippet, "title");
	return true;
}

bool YouTubeConnection::IsValid() const
{
	std::lock_guard<std::mutex> lock(_mutex);
	return !_refreshToken.empty() && !_refreshTokenRevoked &&
	       GetClientConfig();
}

void YouTubeConnection::PauseRequests(std::chrono::seconds duration)
{
	std::lock_guard<std::mutex> lock(_mutex);
	_requestsPausedUntil = std::chrono::steady_clock::now() + duration;
}

bool YouTubeConnection::RequestsArePaused() const
{
	std::lock_guard<std::mutex> lock(_mutex);
	return std::chrono::steady_clock::now() < _requestsPausedUntil;
}

void YouTubeConnection::Revoke()
{
	std::lock_guard<std::mutex> lock(_mutex);
	if (!_refreshToken.empty()) {
		RevokeToken(_refreshToken);
	}
	_refreshToken.clear();
	_accessToken.clear();
}

std::string YouTubeConnection::GetChannelID() const
{
	std::lock_guard<std::mutex> lock(_mutex);
	return _channelId;
}

std::weak_ptr<YouTubeConnection>
GetWeakYouTubeConnectionByName(const std::string &name)
{
	for (const auto &connection : youtubeConnections) {
		if (connection->Name() == name) {
			return std::dynamic_pointer_cast<YouTubeConnection>(
				connection);
		}
	}
	return {};
}

std::string
GetWeakYouTubeConnectionName(const std::weak_ptr<YouTubeConnection> &weak)
{
	auto connection = weak.lock();
	if (!connection) {
		return obs_module_text(
			"AdvSceneSwitcher.youtubeConnection.invalid");
	}
	return connection->Name();
}

std::deque<std::shared_ptr<Item>> &GetYouTubeConnections()
{
	return youtubeConnections;
}

static std::string getBrowserFailText(const std::string &reason)
{
	return QString(obs_module_text(
			       "AdvSceneSwitcher.youtubeConnection.connect.fail.browser"))
		.arg(QString::fromStdString(reason))
		.toStdString();
}

AuthCodeGrabberThread::~AuthCodeGrabberThread()
{
	_stopWaiting = true;
	_cv.notify_all();
	wait();
}

void AuthCodeGrabberThread::run()
{
	_result = {};
	_stopWaiting = false;

	httplib::Server server;
	const int port = server.bind_to_any_port("127.0.0.1");
	if (port < 0) {
		blog(LOG_WARNING,
		     "failed to bind YouTube auth server to localhost!");
		_result.error = "serverError";
		return;
	}

	const auto redirectUri = "http://127.0.0.1:" + std::to_string(port);
	const auto codeVerifier = GenerateCodeVerifier();
	const auto state = GenerateState();
	std::string code;

	server.Get("/", [&](const httplib::Request &req,
			    httplib::Response &res) {
		std::lock_guard<std::mutex> lock(_mutex);
		if (req.get_param_value("state") != state) {
			res.set_content(
				getBrowserFailText(obs_module_text(
					"AdvSceneSwitcher.youtubeConnection.connect.fail.stateMismatch")),
				"text/plain");
			return;
		}

		if (req.has_param("error")) {
			_result.error = req.get_param_value("error");
			res.set_content(getBrowserFailText(_result.error),
					"text/plain");
		} else {
			code = req.get_param_value("code");
			res.set_content(
				obs_module_text(
					"AdvSceneSwitcher.youtubeConnection.connect.success.browser"),
				"text/plain");
		}
		_stopWaiting = true;
		_cv.notify_all();
	});

	std::thread serverThread([&server]() { server.listen_after_bind(); });

	const auto authUrl = BuildAuthUrl(
		_config, redirectUri, GetCodeChallenge(codeVerifier), state);
	QDesktopServices::openUrl(QUrl(QString::fromStdString(authUrl)));

	{
		std::unique_lock<std::mutex> lock(_mutex);
		_cv.wait_for(lock, std::chrono::seconds(authTimeoutSeconds),
			     [this]() { return _stopWaiting.load(); });
	}
	server.stop();
	serverThread.join();

	if (!_result.error.empty()) {
		return;
	}
	if (code.empty()) {
		_result.error = "timeout";
		return;
	}
	_result = ExchangeAuthCode(_config, code, codeVerifier, redirectUri);
}

YouTubeConnectionSettingsDialog::YouTubeConnectionSettingsDialog(
	QWidget *parent, const YouTubeConnection &settings)
	: ItemSettingsDialog(
		  settings, youtubeConnections,
		  "AdvSceneSwitcher.youtubeConnection.select",
		  "AdvSceneSwitcher.youtubeConnection.add",
		  "AdvSceneSwitcher.youtubeConnection.nameNotAvailable", false,
		  parent),
	  _useOwnClient(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.youtubeConnection.useOwnClient"))),
	  _layout(new QGridLayout()),
	  _clientId(new QLineEdit(
		  QString::fromStdString(settings._customClientId))),
	  _clientSecret(new QLineEdit(
		  QString::fromStdString(settings._customClientSecret))),
	  _connect(new QPushButton(obs_module_text(
		  "AdvSceneSwitcher.youtubeConnection.connect"))),
	  _status(new QLabel()),
	  _warnIfInvalid(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.youtubeConnection.warnIfInvalid"))),
	  _currentConnection(settings)
{
	_warnIfInvalid->setChecked(settings.WarnIfInvalid());
	_name->setReadOnly(true);
	_clientSecret->setEchoMode(QLineEdit::PasswordEchoOnEdit);
	_status->setWordWrap(true);
	_useOwnClient->setToolTip(obs_module_text(
		"AdvSceneSwitcher.youtubeConnection.useOwnClient.tooltip"));
	_useOwnClient->setChecked(!settings._customClientId.empty());
	_useOwnClient->setVisible(GetBuiltInClientConfig().has_value());

	QWidget::connect(_useOwnClient, &QCheckBox::toggled, this,
			 &YouTubeConnectionSettingsDialog::UseOwnClientChanged);
	QWidget::connect(_clientId, &QLineEdit::textEdited, this,
			 &YouTubeConnectionSettingsDialog::ClientConfigChanged);
	QWidget::connect(_clientSecret, &QLineEdit::textEdited, this,
			 &YouTubeConnectionSettingsDialog::ClientConfigChanged);
	QWidget::connect(_connect, &QPushButton::clicked, this,
			 &YouTubeConnectionSettingsDialog::Connect);
	QWidget::connect(&_authGrabber, &QThread::finished, this,
			 &YouTubeConnectionSettingsDialog::AuthFinished);
	QWidget::connect(&_countdownTimer, &QTimer::timeout, this,
			 &YouTubeConnectionSettingsDialog::UpdateWaitingStatus);

	int row = 0;
	_layout->addWidget(new QLabel(obs_module_text(
				   "AdvSceneSwitcher.youtubeConnection.name")),
			   row, 0);
	auto nameLayout = new QHBoxLayout();
	nameLayout->addWidget(_name);
	nameLayout->addWidget(_nameHint);
	_layout->addLayout(nameLayout, row, 1);
	++row;
	_layout->addWidget(_useOwnClient, row, 0, 1, 2);
	++row;
	auto clientIdLabelLayout = new QHBoxLayout();
	clientIdLabelLayout->addWidget(new QLabel(obs_module_text(
		"AdvSceneSwitcher.youtubeConnection.clientId")));
	clientIdLabelLayout->addWidget(new HelpIcon(obs_module_text(
		"AdvSceneSwitcher.youtubeConnection.clientConfig.help")));
	clientIdLabelLayout->addStretch();
	_layout->addLayout(clientIdLabelLayout, row, 0);
	_layout->addWidget(_clientId, row, 1);
	_clientIdRow = row;
	++row;
	_layout->addWidget(
		new QLabel(obs_module_text(
			"AdvSceneSwitcher.youtubeConnection.clientSecret")),
		row, 0);
	_layout->addWidget(_clientSecret, row, 1);
	_clientSecretRow = row;
	++row;
	_layout->addWidget(_connect, row, 0);
	_layout->addWidget(_status, row, 1);

	auto setupGuide = new QLabel(obs_module_text(
		"AdvSceneSwitcher.youtubeConnection.setupGuide"));
	setupGuide->setOpenExternalLinks(true);

	auto layout = new QVBoxLayout();
	layout->addLayout(_layout);
	layout->addWidget(setupGuide);
	layout->addWidget(_warnIfInvalid);
	layout->addWidget(_buttonbox);
	setLayout(layout);

	_status->setText(obs_module_text(
		settings.IsValid()
			? "AdvSceneSwitcher.youtubeConnection.connect.success"
			: "AdvSceneSwitcher.youtubeConnection.connect.notConnected"));

	if (_name->text().isEmpty()) {
		HighlightWidget(_connect, Qt::green, QColor(0, 0, 0, 0), true);
	}

	SetClientConfigVisibility();
}

bool YouTubeConnectionSettingsDialog::UseOwnClient() const
{
	return !GetBuiltInClientConfig() || _useOwnClient->isChecked();
}

void YouTubeConnectionSettingsDialog::SetClientConfigVisibility()
{
	const bool visible = UseOwnClient();
	SetGridLayoutRowVisible(_layout, _clientIdRow, visible);
	SetGridLayoutRowVisible(_layout, _clientSecretRow, visible);
	adjustSize();
}

void YouTubeConnectionSettingsDialog::UseOwnClientChanged()
{
	SetClientConfigVisibility();
	ClientConfigChanged();
}

void YouTubeConnectionSettingsDialog::ClientConfigChanged()
{
	// An empty custom client ID selects the built-in client
	const bool useOwnClient = UseOwnClient();
	_currentConnection._customClientId =
		useOwnClient ? _clientId->text().trimmed().toStdString() : "";
	_currentConnection._customClientSecret =
		useOwnClient ? _clientSecret->text().trimmed().toStdString()
			     : "";
	_currentConnection._refreshToken.clear();
	_currentConnection._accessToken.clear();

	_name->setText("");
	QMetaObject::invokeMethod(this, "NameChanged",
				  Q_ARG(const QString &, ""));
	_status->setText(obs_module_text(
		"AdvSceneSwitcher.youtubeConnection.connect.notConnected"));
}

void YouTubeConnectionSettingsDialog::Connect()
{
	const auto config = _currentConnection.GetClientConfig();
	if (!config || config->clientId.empty() ||
	    config->clientSecret.empty()) {
		SetErrorStatus("missingClientConfig");
		return;
	}

	// Don't allow parallel auth requests
	_connect->setDisabled(true);
	_authGrabber.SetClientConfig(*config);
	_authGrabber.start();
	_authDeadline = std::chrono::steady_clock::now() +
			std::chrono::seconds(authTimeoutSeconds);
	UpdateWaitingStatus();
	_countdownTimer.start(1000);
}

void YouTubeConnectionSettingsDialog::UpdateWaitingStatus()
{
	const auto remaining = std::chrono::ceil<std::chrono::seconds>(
		_authDeadline - std::chrono::steady_clock::now());
	_status->setText(
		QString(obs_module_text(
				"AdvSceneSwitcher.youtubeConnection.connect.waiting"))
			.arg(std::max<long long>(remaining.count(), 0)));
}

void YouTubeConnectionSettingsDialog::AuthFinished()
{
	_countdownTimer.stop();
	_connect->setEnabled(true);

	std::string errorReason;
	if (!_currentConnection.SetAuthResult(_authGrabber.Result(),
					      errorReason)) {
		_name->setText("");
		QMetaObject::invokeMethod(this, "NameChanged",
					  Q_ARG(const QString &, ""));
		SetErrorStatus(errorReason);
		return;
	}

	const auto name = QString::fromStdString(_currentConnection.Name());
	_name->setText(name);
	QMetaObject::invokeMethod(this, "NameChanged",
				  Q_ARG(const QString &, name));
	_status->setText(obs_module_text(
		"AdvSceneSwitcher.youtubeConnection.connect.success"));
}

void YouTubeConnectionSettingsDialog::SetErrorStatus(
	const std::string &errorReason)
{
	static const std::unordered_map<std::string, const char *> errorTexts{
		{"missingClientConfig",
		 "AdvSceneSwitcher.youtubeConnection.error.missingClientConfig"},
		{"access_denied",
		 "AdvSceneSwitcher.youtubeConnection.error.accessDenied"},
		{"invalid_client",
		 "AdvSceneSwitcher.youtubeConnection.error.invalidClient"},
		{"invalid_grant",
		 "AdvSceneSwitcher.youtubeConnection.error.invalidGrant"},
		{"accessNotConfigured",
		 "AdvSceneSwitcher.youtubeConnection.error.apiNotEnabled"},
		{"noChannel",
		 "AdvSceneSwitcher.youtubeConnection.error.noChannel"},
		{"timeout", "AdvSceneSwitcher.youtubeConnection.error.timeout"},
		{"network_error",
		 "AdvSceneSwitcher.youtubeConnection.error.network"},
		{"serverError",
		 "AdvSceneSwitcher.youtubeConnection.error.server"},
	};

	const auto it = errorTexts.find(errorReason);
	if (it != errorTexts.end()) {
		_status->setText(obs_module_text(it->second));
		return;
	}

	_status->setText(
		QString(obs_module_text(
				"AdvSceneSwitcher.youtubeConnection.error.unknown"))
			.arg(QString::fromStdString(errorReason)));
}

bool YouTubeConnectionSettingsDialog::AskForSettings(
	QWidget *parent, YouTubeConnection &settings)
{
	YouTubeConnectionSettingsDialog dialog(parent, settings);
	dialog.setWindowTitle(obs_module_text("AdvSceneSwitcher.windowTitle"));
	if (dialog.exec() != DialogCode::Accepted) {
		return false;
	}

	settings = dialog._currentConnection;
	settings.SetWarnIfInvalid(dialog._warnIfInvalid->isChecked());
	return true;
}

static bool askForSettingsWrapper(QWidget *parent, Item &settings)
{
	auto &connection = dynamic_cast<YouTubeConnection &>(settings);
	return YouTubeConnectionSettingsDialog::AskForSettings(parent,
							       connection);
}

YouTubeConnectionSelection::YouTubeConnectionSelection(QWidget *parent)
	: ItemSelection(youtubeConnections, YouTubeConnection::Create,
			askForSettingsWrapper,
			"AdvSceneSwitcher.youtubeConnection.select",
			"AdvSceneSwitcher.youtubeConnection.add",
			"AdvSceneSwitcher.youtubeConnection.nameNotAvailable",
			"AdvSceneSwitcher.youtubeConnection.configure", parent)
{
	ShowRenameContextMenu(false);

	QWidget::connect(YouTubeConnectionSignalManager::Instance(),
			 SIGNAL(Add(const QString &)), this,
			 SLOT(AddItem(const QString &)));
	QWidget::connect(YouTubeConnectionSignalManager::Instance(),
			 SIGNAL(Remove(const QString &)), this,
			 SLOT(RemoveItem(const QString &)));

	QWidget::connect(this, SIGNAL(ItemAdded(const QString &)),
			 YouTubeConnectionSignalManager::Instance(),
			 SIGNAL(Add(const QString &)));
	QWidget::connect(this, SIGNAL(ItemRemoved(const QString &)),
			 YouTubeConnectionSignalManager::Instance(),
			 SIGNAL(Remove(const QString &)));
}

void YouTubeConnectionSelection::SetConnection(
	const std::weak_ptr<YouTubeConnection> &weak)
{
	auto connection = weak.lock();
	SetItem(connection ? connection->Name() : "");
}

YouTubeConnectionSignalManager *YouTubeConnectionSignalManager::Instance()
{
	static YouTubeConnectionSignalManager manager;
	return &manager;
}

} // namespace advss
