#include "token.hpp"
#include "twitch-helpers.hpp"

#include <switcher-data.hpp>
#include <utility.hpp>
#include <QScrollArea>
#include <QDesktopServices>

namespace advss {

static std::deque<std::shared_ptr<Item>> twitchTokens;

const std::unordered_map<std::string, std::string> TokenOption::apiIdToLocale{
	// Add necessary token permissions here
	/*
	{"analytics:read:extensions",
	 "AdvSceneSwitcher.twitchToken.analytics.readExtensions"},
	{"analytics:read:games",
	 "AdvSceneSwitcher.twitchToken.analytics.readGames"},
	{"bits:read", "AdvSceneSwitcher.twitchToken.bits.read"},
	*/

	{"channel:manage:broadcast",
	 "AdvSceneSwitcher.twitchToken.channel.manageBroadcast"},
	{"clips:edit", "AdvSceneSwitcher.twitchToken.clips.edit"},
	{"channel:edit:commercial",
	 "AdvSceneSwitcher.twitchToken.channel.startCommercial"},
	{"moderator:manage:announcements",
	 "AdvSceneSwitcher.twitchToken.moderator.manageAnnouncements"},
	{"moderator:manage:chat_settings",
	 "AdvSceneSwitcher.twitchToken.moderator.manageChatSettings"}};

static void saveConnections(obs_data_t *obj);
static void loadConnections(obs_data_t *obj);

bool setupTwitchTokenSupport()
{
	GetSwitcher()->AddSaveStep(saveConnections);
	GetSwitcher()->AddLoadStep(loadConnections);
	return true;
}

bool TwitchToken::_setup = setupTwitchTokenSupport();

static void saveConnections(obs_data_t *obj)
{
	OBSDataArrayAutoRelease connectionArray = obs_data_array_create();
	for (const auto &c : twitchTokens) {
		OBSDataAutoRelease arrayObj = obs_data_create();
		c->Save(arrayObj);
		obs_data_array_push_back(connectionArray, arrayObj);
	}
	obs_data_set_array(obj, "twitchConnections", connectionArray);
}

static void loadConnections(obs_data_t *obj)
{
	twitchTokens.clear();

	OBSDataArrayAutoRelease connectionArray =
		obs_data_get_array(obj, "twitchConnections");
	size_t count = obs_data_array_count(connectionArray);
	for (size_t i = 0; i < count; i++) {
		OBSDataAutoRelease arrayObj =
			obs_data_array_item(connectionArray, i);
		auto con = TwitchToken::Create();
		twitchTokens.emplace_back(con);
		twitchTokens.back()->Load(arrayObj);
	}
}

void TokenOption::Load(obs_data_t *obj)
{
	apiId = obs_data_get_string(obj, "apiID");
}

void TokenOption::Save(obs_data_t *obj) const
{
	obs_data_set_string(obj, "apiID", apiId.c_str());
}

std::string TokenOption::GetLocale() const
{
	return apiIdToLocale.at(apiId);
}

const std::unordered_map<std::string, std::string> &
TokenOption::GetTokenOptionMap()
{
	return apiIdToLocale;
}

bool TokenOption::operator<(const TokenOption &other) const
{
	return apiId < other.apiId;
}

void TwitchToken::Load(obs_data_t *obj)
{
	Item::Load(obj);
	_token = obs_data_get_string(obj, "token");
	_userID = obs_data_get_string(obj, "userID");
	_tokenOptions.clear();
	OBSDataArrayAutoRelease options = obs_data_get_array(obj, "options");
	size_t count = obs_data_array_count(options);
	for (size_t i = 0; i < count; i++) {
		OBSDataAutoRelease arrayObj = obs_data_array_item(options, i);
		TokenOption tokenOption;
		tokenOption.Load(arrayObj);
		_tokenOptions.insert(tokenOption);
	}
}

void TwitchToken::Save(obs_data_t *obj) const
{
	Item::Save(obj);
	obs_data_set_string(obj, "token", _token.c_str());
	obs_data_set_string(obj, "userID", _userID.c_str());
	OBSDataArrayAutoRelease options = obs_data_array_create();
	for (auto &option : _tokenOptions) {
		OBSDataAutoRelease arrayObj = obs_data_create();
		option.Save(arrayObj);
		obs_data_array_push_back(options, arrayObj);
	}
	obs_data_set_array(obj, "options", options);
}

bool TwitchToken::OptionIsEnabled(const TokenOption &option) const
{
	for (const auto &activeOption : _tokenOptions) {
		if (activeOption.apiId == option.apiId) {
			return true;
		}
	}
	return false;
}

void TwitchToken::SetToken(const std::string &value)
{
	_token = value;
	auto res =
		SendGetRequest("https://api.twitch.tv", "/helix/users", *this);
	if (res.status != 200) {
		blog(LOG_WARNING, "failed to get Twitch user id from token!");
		_userID = -1;
		return;
	}

	OBSDataArrayAutoRelease array = obs_data_get_array(res.data, "data");
	size_t count = obs_data_array_count(array);
	for (size_t i = 0; i < count; i++) {
		OBSDataAutoRelease arrayObj = obs_data_array_item(array, i);
		_userID = obs_data_get_string(arrayObj, "id");
		_name = obs_data_get_string(arrayObj, "display_name");
	}
}

TwitchToken *GetTwitchTokenByName(const QString &name)
{
	return GetTwitchTokenByName(name.toStdString());
}

TwitchToken *GetTwitchTokenByName(const std::string &name)
{
	for (auto &t : twitchTokens) {
		if (t->Name() == name) {
			return dynamic_cast<TwitchToken *>(t.get());
		}
	}
	return nullptr;
}

std::weak_ptr<TwitchToken> GetWeakTwitchTokenByName(const std::string &name)
{
	for (const auto &t : twitchTokens) {
		if (t->Name() == name) {
			std::weak_ptr<TwitchToken> wp =
				std::dynamic_pointer_cast<TwitchToken>(t);
			return wp;
		}
	}
	return std::weak_ptr<TwitchToken>();
}

std::weak_ptr<TwitchToken> GetWeakTwitchTokenByQString(const QString &name)
{
	return GetWeakTwitchTokenByName(name.toStdString());
}

std::string GetWeakTwitchTokenName(std::weak_ptr<TwitchToken> token)
{
	auto con = token.lock();
	if (!con) {
		return obs_module_text("AdvSceneSwitcher.twitchToken.invalid");
	}
	return con->Name();
}

static bool ConnectionNameAvailable(const QString &name)
{
	return !GetTwitchTokenByName(name);
}

static bool ConnectionNameAvailable(const std::string &name)
{
	return ConnectionNameAvailable(QString::fromStdString(name));
}

static bool AskForSettingsWrapper(QWidget *parent, Item &settings)
{
	TwitchToken &ConnectionSettings = dynamic_cast<TwitchToken &>(settings);
	return TwitchTokenSettingsDialog::AskForSettings(parent,
							 ConnectionSettings);
}

TwitchConnectionSelection::TwitchConnectionSelection(QWidget *parent)
	: ItemSelection(twitchTokens, TwitchToken::Create,
			AskForSettingsWrapper,
			"AdvSceneSwitcher.twitchToken.select",
			"AdvSceneSwitcher.twitchToken.add",
			"AdvSceneSwitcher.twitchToken.nameNotAvailable",
			"AdvSceneSwitcher.twitchToken.configure", parent)
{
	ShowRenameContextMenu(false);

	// Connect to slots
	QWidget::connect(TwitchConnectionSignalManager::Instance(),
			 SIGNAL(Add(const QString &)), this,
			 SLOT(AddItem(const QString &)));
	QWidget::connect(TwitchConnectionSignalManager::Instance(),
			 SIGNAL(Remove(const QString &)), this,
			 SLOT(RemoveItem(const QString &)));

	// Forward signals
	QWidget::connect(this, SIGNAL(ItemAdded(const QString &)),
			 TwitchConnectionSignalManager::Instance(),
			 SIGNAL(Add(const QString &)));
	QWidget::connect(this, SIGNAL(ItemRemoved(const QString &)),
			 TwitchConnectionSignalManager::Instance(),
			 SIGNAL(Remove(const QString &)));
}

void TwitchConnectionSelection::SetToken(const std::string &token)
{
	const QSignalBlocker blocker(_selection);
	if (!!GetTwitchTokenByName(token)) {
		_selection->setCurrentText(QString::fromStdString(token));
	} else {
		_selection->setCurrentIndex(-1);
	}
}

void TwitchConnectionSelection::SetToken(
	const std::weak_ptr<TwitchToken> &token_)
{
	const QSignalBlocker blocker(_selection);
	auto token = token_.lock();
	if (token) {
		SetToken(token->Name());
	} else {
		_selection->setCurrentIndex(-1);
	}
}

static QCheckBox *addOption(const TokenOption &option, const TwitchToken &token,
			    QGridLayout *layout, int &row)
{
	auto label = new QLabel(obs_module_text(option.GetLocale().c_str()));
	label->setWordWrap(true);
	layout->addWidget(label, row, 1);
	auto checkBox = new QCheckBox();
	checkBox->setChecked(token.OptionIsEnabled(option));
	layout->addWidget(checkBox, row, 0);
	row++;
	return checkBox;
}

TwitchTokenSettingsDialog::TwitchTokenSettingsDialog(
	QWidget *parent, const TwitchToken &settings)
	: ItemSettingsDialog(settings, twitchTokens,
			     "AdvSceneSwitcher.twitchToken.select",
			     "AdvSceneSwitcher.twitchToken.add",
			     "AdvSceneSwitcher.twitchToken.nameNotAvailable",
			     parent),
	  _requestToken(new QPushButton(
		  obs_module_text("AdvSceneSwitcher.twitchToken.request"))),
	  _showToken(new QPushButton()),
	  _currentTokenValue(new QLineEdit()),
	  _tokenStatus(new QLabel())
{
	_showToken->setMaximumWidth(22);
	_showToken->setFlat(true);
	_showToken->setStyleSheet(
		"QPushButton { background-color: transparent; border: 0px }");

	_currentTokenValue->setReadOnly(true);
	_currentTokenValue->setText(QString::fromStdString(settings._token));

	_name->setReadOnly(true);

	QWidget::connect(_requestToken, SIGNAL(clicked()), this,
			 SLOT(RequestToken()));
	QWidget::connect(_showToken, SIGNAL(pressed()), this,
			 SLOT(ShowToken()));
	QWidget::connect(_showToken, SIGNAL(released()), this,
			 SLOT(HideToken()));
	QWidget::connect(&_tokenGrabber, &TokenGrabberThread::GotToken, this,
			 &TwitchTokenSettingsDialog::GotToken);

	auto generalSettingsGrid = new QGridLayout();
	int row = 0;
	generalSettingsGrid->addWidget(
		new QLabel(
			obs_module_text("AdvSceneSwitcher.twitchToken.name")),
		row, 0);
	auto nameLayout = new QHBoxLayout;
	nameLayout->addWidget(_name);
	nameLayout->addWidget(_nameHint);
	generalSettingsGrid->addLayout(nameLayout, row, 1);
	++row;
	generalSettingsGrid->addWidget(
		new QLabel(
			obs_module_text("AdvSceneSwitcher.twitchToken.value")),
		row, 0);
	auto tokenValueLayout = new QHBoxLayout;
	tokenValueLayout->addWidget(_currentTokenValue);
	tokenValueLayout->addWidget(_showToken);
	generalSettingsGrid->addLayout(tokenValueLayout, row, 1);
	++row;
	generalSettingsGrid->addWidget(_requestToken, row, 0);
	generalSettingsGrid->addWidget(_tokenStatus, row, 1);

	auto optionsGrid = new QGridLayout();
	row = 0;
	auto optionsBox = new QGroupBox(
		obs_module_text("AdvSceneSwitcher.twitchToken.permissions"));
	for (const auto &[id, _] : TokenOption::GetTokenOptionMap()) {
		auto checkBox = addOption({id}, settings, optionsGrid, row);
		QWidget::connect(checkBox, SIGNAL(stateChanged(int)), this,
				 SLOT(TokenOptionChanged(int)));
		_optionWidgets[id] = checkBox;
	}
	MinimizeSizeOfColumn(optionsGrid, 0);
	optionsBox->setLayout(optionsGrid);

	auto scrollArea = new QScrollArea(this);
	scrollArea->setWidgetResizable(true);
	scrollArea->setFrameShape(QFrame::NoFrame);

	auto contentWidget = new QWidget(scrollArea);
	auto layout = new QVBoxLayout(contentWidget);
	layout->addLayout(generalSettingsGrid);
	layout->addWidget(optionsBox);
	layout->setContentsMargins(0, 0, 0, 0);
	scrollArea->setWidget(contentWidget);

	auto dialogLayout = new QVBoxLayout();
	dialogLayout->addWidget(scrollArea);
	dialogLayout->addWidget(_buttonbox);
	setLayout(dialogLayout);

	_currentTokenValue->setText(QString::fromStdString(settings._token));
	if (settings._token.empty()) {
		_tokenStatus->setText(obs_module_text(
			"AdvSceneSwitcher.twitchToken.request.notSet"));
	}
	HideToken();

	if (_name->text().isEmpty()) {
		PulseWidget(_requestToken, Qt::green, QColor(0, 0, 0, 0), true);
	}

	_currentToken = settings;
}

void TwitchTokenSettingsDialog::ShowToken()
{
	SetButtonIcon(_showToken, ":res/images/visible.svg");
	_currentTokenValue->setEchoMode(QLineEdit::Normal);
}

void TwitchTokenSettingsDialog::HideToken()
{
	SetButtonIcon(_showToken, ":res/images/invisible.svg");
	_currentTokenValue->setEchoMode(QLineEdit::PasswordEchoOnEdit);
}

void TwitchTokenSettingsDialog::TokenOptionChanged(int)
{
	if (!_name->text().isEmpty()) {
		PulseWidget(_requestToken, Qt::green, QColor(0, 0, 0, 0), true);
	}
	_name->setText("");
	QMetaObject::invokeMethod(this, "NameChanged",
				  Q_ARG(const QString &, ""));
	_currentTokenValue->setText("");
}

static std::string generateStateString()
{
	const char *chars =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
	const size_t stateStringLen = 32;

	static std::random_device rd;
	static std::mt19937 gen(rd());
	static std::uniform_int_distribution<size_t> dis(0, sizeof(chars) - 2);

	std::string state;
	state.reserve(stateStringLen);

	for (size_t i = 0; i < stateStringLen; ++i) {
		state += chars[dis(gen)];
	}

	return state;
}

static std::string generateScopeString(const std::set<TokenOption> &options)
{
	if (options.empty()) {
		return "";
	}

	std::string scope;
	for (const auto &option : options) {
		scope += option.apiId + "+";
	}
	scope.pop_back(); // Remove trailing +
	return scope;
}

static std::string getHtml(const QString &redirect)
{
	const char *html = R"(
	<html>
	<head>
	    <title>Advanced scene switcher</title>
	</head>
	<body>
	    <div id="output">Please click this link to continue if not automatically redirected</div>
	    <p><a href="%1">Login with Twitch</a></a></p>
	    <script type="text/javascript">
	        if (document.location.hash && document.location.hash != '') {
	            var parsedHash = new URLSearchParams(window.location.hash.slice(1));
	            if (parsedHash.get('access_token')) {
	                window.location.replace(`http://localhost:8080/token?access_token=${parsedHash.get('access_token')}&state=${parsedHash.get('state')}`);
	                output.textContent = 'It is safe to close this window';
	            }
	        } else {
	            window.location.replace('%1');
	        }
	    </script>
	</body>
	</html>)";

	return QString(html).arg(redirect).toStdString();
}

void TwitchTokenSettingsDialog::RequestToken()
{
	// Don't allow parallel RequestToken() calls
	_requestToken->setDisabled(true);

	auto scope = QString::fromStdString(
		generateScopeString(GetEnabledOptions()));
	_tokenGrabber.SetTokenScope(scope);
	_tokenGrabber.start();
	_tokenStatus->setText(obs_module_text(
		"AdvSceneSwitcher.twitchToken.request.waiting"));
}

void TwitchTokenSettingsDialog::GotToken(const std::optional<QString> &value)
{
	_currentTokenValue->setText(value.value_or(""));
	if (value.has_value()) {
		_tokenStatus->setText(obs_module_text(
			"AdvSceneSwitcher.twitchToken.request.success"));
		_currentToken.SetToken(value.value().toStdString());
		auto name = QString::fromStdString(_currentToken._name);
		_name->setText(name);
		_name->textEdited(name);
	} else {
		_tokenStatus->setText(obs_module_text(
			"AdvSceneSwitcher.twitchToken.request.fail"));
		_name->setText("");
	}
	_requestToken->setEnabled(true);
}

std::set<TokenOption> TwitchTokenSettingsDialog::GetEnabledOptions()
{
	std::set<TokenOption> result;
	for (const auto &[id, checkBox] : _optionWidgets) {
		if (checkBox->isChecked()) {
			TokenOption option;
			option.apiId = id;
			result.emplace(option);
		}
	}
	return result;
}

bool TwitchTokenSettingsDialog::AskForSettings(QWidget *parent,
					       TwitchToken &settings)
{
	TwitchTokenSettingsDialog dialog(parent, settings);
	dialog.setWindowTitle(obs_module_text("AdvSceneSwitcher.windowTitle"));
	if (dialog.exec() != DialogCode::Accepted) {
		return false;
	}

	settings = dialog._currentToken;
	settings._tokenOptions = dialog.GetEnabledOptions();
	return true;
}

int TokenGrabberThread::_timeout = 15;

TokenGrabberThread::~TokenGrabberThread()
{
	_stopWaiting = true;
	_cv.notify_all();
	Stop();
	_server.stop();
}

static std::string getAuthErrorString(const char *errDetail)
{
	QString err = obs_module_text(
		"AdvSceneSwitcher.twitchToken.request.fail.browser");
	return err.arg(obs_module_text(errDetail)).toStdString();
}

void TokenGrabberThread::run()
{
	// Reset
	_server.stop();
	_server.~Server();
	new (&_server) httplib::Server();
	if (_serverThread.joinable()) {
		_serverThread.join();
	}
	_stopWaiting = {false};

	// Generate URI to request token
	auto state = generateStateString();
	auto getTokenURI = "https://id.twitch.tv/oauth2/authorize"
			   "?response_type=token"
			   "&client_id=" +
			   QString(GetClientID()) +
			   "&redirect_uri=http://localhost:8080/auth"
			   "&scope=" +
			   _scope + "&state=" + QString::fromStdString(state);

	// Setup server receiving token string
	auto html = getHtml(getTokenURI);
	_server.Get("/auth", [html, state](const httplib::Request &req,
					   httplib::Response &res) {
		// Check for errors
		if (req.has_param("error")) {
			auto recvState = req.get_param_value("state");
			if (recvState != state) {
				blog(LOG_WARNING,
				     "state string does not match in error handling?! "
				     "Got \"%s\" - expected \"%s\"\n"
				     "ignoring error ...",
				     recvState.c_str(), state.c_str());
				return;
			}
			auto errorStr =
				req.get_param_value("error_description");
			res.set_content(getAuthErrorString(errorStr.c_str()),
					"text/plain");
			return;
		}

		// Parse fragments and redirect to /token with
		// corresponding parameters.
		res.set_content(html, "text/html");
	});
	_server.Get("/token", [&](const httplib::Request &req,
				  httplib::Response &res) {
		// Check if valid request and grab the token string
		std::lock_guard<std::mutex> lk(_mutex);
		auto recvState = req.get_param_value("state");
		if (recvState != state) {
			blog(LOG_WARNING,
			     "state string does not match! "
			     "Got \"%s\" - expected \"%s\"",
			     recvState.c_str(), state.c_str());
			res.set_content(
				getAuthErrorString(
					"AdvSceneSwitcher.twitchToken.request.fail.stateMismatch"),
				"text/plain");
		} else {
			_tokenString = QString::fromStdString(
				req.get_param_value("access_token"));
			res.set_content(
				obs_module_text(
					"AdvSceneSwitcher.twitchToken.request.success.browser"),
				"text/plain");
		}
		_stopWaiting = true;
		_cv.notify_all();
	});

	// Request user to grant token
	QDesktopServices::openUrl(getTokenURI);

	// Start the server and wait
	std::unique_lock<std::mutex> lock(_mutex);
	_serverThread =
		std::thread([this]() { _server.listen("localhost", 8080); });
	auto time = std::chrono::high_resolution_clock::now() +
		    std::chrono::seconds(_timeout);
	while (!_stopWaiting) {
		if (_cv.wait_until(lock, time) == std::cv_status::timeout) {
			break;
		}
	}
	_server.stop();
	emit GotToken(_tokenString);
}

void TokenGrabberThread::Stop()
{
	if (_server.is_running()) {
		_server.stop();
	}
	if (_serverThread.joinable()) {
		_serverThread.join();
	}
	wait();
}

TwitchConnectionSignalManager *TwitchConnectionSignalManager::Instance()
{
	static TwitchConnectionSignalManager manager;
	return &manager;
}

} // namespace advss
