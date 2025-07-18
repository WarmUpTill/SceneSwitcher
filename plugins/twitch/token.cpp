#include "token.hpp"
#include "twitch-helpers.hpp"

#include <layout-helpers.hpp>
#include <log-helper.hpp>
#include <obs-module-helper.hpp>
#include <plugin-state-helpers.hpp>
#include <QDesktopServices>
#include <QGroupBox>
#include <QScrollArea>
#include <QUrl>
#include <ui-helpers.hpp>

namespace advss {

static const int tokenGrabberPort = 42171;

static std::deque<std::shared_ptr<Item>> twitchTokens;

const std::unordered_map<std::string, std::string> TokenOption::_apiIdToLocale{
	{"channel:manage:broadcast",
	 "AdvSceneSwitcher.twitchToken.channel.broadcast.manage"},
	{"moderator:read:followers",
	 "AdvSceneSwitcher.twitchToken.moderator.followers.read"},
	{"channel:read:subscriptions",
	 "AdvSceneSwitcher.twitchToken.channel.subscriptions.read"},
	{"bits:read", "AdvSceneSwitcher.twitchToken.bits.read"},
	{"moderator:read:shoutouts",
	 "AdvSceneSwitcher.twitchToken.moderator.shoutouts.read"},
	{"channel:manage:raids",
	 "AdvSceneSwitcher.twitchToken.channel.raids.manage"},
	{"moderator:manage:shoutouts",
	 "AdvSceneSwitcher.twitchToken.moderator.shoutouts.manage"},
	{"channel:read:polls",
	 "AdvSceneSwitcher.twitchToken.channel.polls.read"},
	{"channel:manage:polls",
	 "AdvSceneSwitcher.twitchToken.channel.polls.manage"},
	{"channel:read:predictions",
	 "AdvSceneSwitcher.twitchToken.channel.predictions.read"},
	{"channel:manage:predictions",
	 "AdvSceneSwitcher.twitchToken.channel.predictions.manage"},
	{"channel:read:goals",
	 "AdvSceneSwitcher.twitchToken.channel.goals.read"},
	{"channel:read:hype_train",
	 "AdvSceneSwitcher.twitchToken.channel.hypeTrain.read"},
	{"channel:read:charity",
	 "AdvSceneSwitcher.twitchToken.channel.charity.read"},
	{"moderator:read:shield_mode",
	 "AdvSceneSwitcher.twitchToken.moderator.shieldMode.read"},
	{"moderator:manage:shield_mode",
	 "AdvSceneSwitcher.twitchToken.moderator.shieldMode.manage"},
	{"channel:read:redemptions",
	 "AdvSceneSwitcher.twitchToken.channel.redemptions.read"},
	{"channel:manage:redemptions",
	 "AdvSceneSwitcher.twitchToken.channel.redemptions.manage"},
	{"channel:moderate", "AdvSceneSwitcher.twitchToken.channel.moderate"},
	{"moderator:manage:banned_users",
	 "AdvSceneSwitcher.twitchToken.moderator.bannedUsers.manage"},
	{"user:manage:blocked_users",
	 "AdvSceneSwitcher.twitchToken.user.blockedUsers.manage"},
	{"moderation:read", "AdvSceneSwitcher.twitchToken.moderation.read"},
	{"channel:manage:moderators",
	 "AdvSceneSwitcher.twitchToken.channel.moderators.manage"},
	{"channel:manage:vips",
	 "AdvSceneSwitcher.twitchToken.channel.vips.manage"},
	{"channel:edit:commercial",
	 "AdvSceneSwitcher.twitchToken.channel.commercial.edit"},
	{"channel:manage:ads",
	 "AdvSceneSwitcher.twitchToken.channel.ads.manage"},
	{"clips:edit", "AdvSceneSwitcher.twitchToken.clips.edit"},
	{"moderator:manage:announcements",
	 "AdvSceneSwitcher.twitchToken.moderator.announcements.manage"},
	{"moderator:manage:chat_settings",
	 "AdvSceneSwitcher.twitchToken.moderator.chat.settings.manage"},
	{"user:manage:whispers",
	 "AdvSceneSwitcher.twitchToken.user.whispers.manage"},
	{"chat:read", "AdvSceneSwitcher.twitchToken.chat.read"},
	{"chat:edit", "AdvSceneSwitcher.twitchToken.chat.edit"},
};

static void saveConnections(obs_data_t *obj);
static void loadConnections(obs_data_t *obj);

static bool setupTwitchTokenSupport()
{
	AddSaveStep(saveConnections);
	AddLoadStep(loadConnections);
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
	return _apiIdToLocale.at(apiId);
}

const std::unordered_map<std::string, std::string> &
TokenOption::GetTokenOptionMap()
{
	return _apiIdToLocale;
}

std::set<TokenOption> TokenOption::GetAllTokenOptions()
{
	std::set<TokenOption> result;
	for (const auto &[optionStr, _] : _apiIdToLocale) {
		TokenOption option = {optionStr};
		result.emplace(option);
	}
	return result;
}

bool TokenOption::operator<(const TokenOption &other) const
{
	return apiId < other.apiId;
}

TwitchToken::TwitchToken(const TwitchToken &other)
	: Item(other._name),
	  _token(other._token),
	  _lastValidityCheckTime(),
	  _lastValidityCheckValue(),
	  _lastValidityCheckResult(false),
	  _userID(other._userID),
	  _tokenOptions(other._tokenOptions),
	  _eventSub(),
	  _validateEventSubTimestamps(other._validateEventSubTimestamps)
{
}

TwitchToken &TwitchToken::operator=(const TwitchToken &other)
{
	_name = other._name;
	_token = other._token;
	_userID = other._userID;
	_tokenOptions = other._tokenOptions;
	_validateEventSubTimestamps = other._validateEventSubTimestamps;
	return *this;
}

void TwitchToken::Load(obs_data_t *obj)
{
	Item::Load(obj);
	_token = obs_data_get_string(obj, "token");
	_userID = obs_data_get_string(obj, "userID");
	obs_data_set_default_bool(obj, "validateEventSubTimestamps", true);
	_validateEventSubTimestamps =
		obs_data_get_bool(obj, "validateEventSubTimestamps");
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
	obs_data_set_bool(obj, "validateEventSubTimestamps",
			  _validateEventSubTimestamps);
	OBSDataArrayAutoRelease options = obs_data_array_create();
	for (auto &option : _tokenOptions) {
		OBSDataAutoRelease arrayObj = obs_data_create();
		option.Save(arrayObj);
		obs_data_array_push_back(options, arrayObj);
	}
	obs_data_set_array(obj, "options", options);
}

bool TwitchToken::OptionIsActive(const TokenOption &option) const
{
	for (const auto &activeOption : _tokenOptions) {
		if (activeOption.apiId == option.apiId) {
			return true;
		}
	}

	return false;
}

bool TwitchToken::OptionIsEnabled(const TokenOption &option) const
{
	if (!IsValid()) {
		return false;
	}

	return OptionIsActive(option);
}

bool TwitchToken::AnyOptionIsEnabled(
	const std::vector<TokenOption> &options) const
{
	if (!IsValid()) {
		return false;
	}

	if (options.empty()) {
		return true;
	}

	for (const auto &tokenOption : options) {
		if (OptionIsActive(tokenOption)) {
			return true;
		}
	}

	return false;
}

void TwitchToken::SetToken(const std::string &value)
{
	_token = value;
	auto res =
		SendGetRequest(*this, "https://api.twitch.tv", "/helix/users");
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

	// Trigger resubscribes with new token
	if (_eventSub) {
		_eventSub->ClearActiveSubscriptions();
	}
}

std::optional<std::string> TwitchToken::GetToken() const
{
	if (!IsValid()) {
		return {};
	}
	return _token;
}

std::shared_ptr<EventSub> TwitchToken::GetEventSub()
{
	if (!_eventSub) {
		_eventSub = std::make_shared<EventSub>();
		_eventSub->EnableTimestampValidation(
			_validateEventSubTimestamps);
	}
	return _eventSub;
}

bool TwitchToken::IsValid(bool forceUpdate) const
{
	static httplib::Client cli("https://id.twitch.tv");
	httplib::Headers headers{{"Authorization", "OAuth " + _token}};

	std::scoped_lock<std::mutex> lock(_cacheMutex);

	auto currentTime = std::chrono::system_clock::now();
	auto diff = currentTime - _lastValidityCheckTime;
	const bool cacheIsTooOld = diff >= std::chrono::hours(1);

	const auto checkToken = [&]() -> bool {
		const auto response =
			cli.Get("/oauth2/validate", httplib::Params{}, headers);
		_lastValidityCheckTime = std::chrono::system_clock::now();
		_lastValidityCheckValue = _token;
		_lastValidityCheckResult = response && response->status == 200;
		if (!_lastValidityCheckResult) {
			blog(LOG_INFO, "Twitch token %s is not valid!",
			     _name.c_str());
		}
		return _lastValidityCheckResult;
	};

	const bool tokenChanged = _lastValidityCheckValue != _token;
	if (tokenChanged) {
		return checkToken();
	}

	if (!forceUpdate && !cacheIsTooOld) {
		return _lastValidityCheckResult;
	}

	return checkToken();
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

bool TokenIsValid(const std::weak_ptr<TwitchToken> &token_)
{
	auto token = token_.lock();
	if (!token) {
		return false;
	}
	return token->IsValid();
}

std::deque<std::shared_ptr<Item>> &GetTwitchTokens()
{
	return twitchTokens;
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
	if (!!GetTwitchTokenByName(token)) {
		SetItem(token);
	} else {
		SetItem("");
	}
}

void TwitchConnectionSelection::SetToken(
	const std::weak_ptr<TwitchToken> &token_)
{
	auto token = token_.lock();
	if (token) {
		SetItem(token->Name());
	} else {
		SetItem("");
	}
}

static QCheckBox *addOption(const TokenOption &option, const TwitchToken &token,
			    QGridLayout *layout, int &row)
{
	auto label = new QLabel(obs_module_text(option.GetLocale().c_str()));
	label->setWordWrap(true);
	layout->addWidget(label, row, 1);
	auto checkBox = new QCheckBox();
	checkBox->setChecked(token.OptionIsActive(option));
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
			     false, parent),
	  _requestToken(new QPushButton(
		  obs_module_text("AdvSceneSwitcher.twitchToken.request"))),
	  _showToken(new QPushButton()),
	  _currentTokenValue(new QLineEdit()),
	  _tokenStatus(new QLabel()),
	  _generalSettingsGrid(new QGridLayout()),
	  _validateTimestamps(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.twitchToken.validateTimestamps")))
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

	int row = 0;
	_generalSettingsGrid->addWidget(
		new QLabel(
			obs_module_text("AdvSceneSwitcher.twitchToken.name")),
		row, 0);
	auto nameLayout = new QHBoxLayout();
	nameLayout->addWidget(_name);
	nameLayout->addWidget(_nameHint);
	_generalSettingsGrid->addLayout(nameLayout, row, 1);
	_nameRow = row;
	++row;
	_generalSettingsGrid->addWidget(
		new QLabel(
			obs_module_text("AdvSceneSwitcher.twitchToken.value")),
		row, 0);
	auto tokenValueLayout = new QHBoxLayout();
	tokenValueLayout->addWidget(_currentTokenValue);
	tokenValueLayout->addWidget(_showToken);
	_generalSettingsGrid->addLayout(tokenValueLayout, row, 1);
	_tokenValueRow = row;
	++row;
	_generalSettingsGrid->addWidget(_requestToken, row, 0);
	_generalSettingsGrid->addWidget(_tokenStatus, row, 1);

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
	layout->addLayout(_generalSettingsGrid);
	layout->addWidget(optionsBox);
	layout->addWidget(_validateTimestamps);
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
		SetTokenInfoVisible(false);
	}
	HideToken();

	if (_name->text().isEmpty()) {
		HighlightWidget(_requestToken, Qt::green, QColor(0, 0, 0, 0),
				true);
	}

	_validateTimestamps->setChecked(settings._validateEventSubTimestamps);
#ifndef VERIFY_TIMESTAMPS
	_validateTimestamps->hide();
#endif

	_currentToken = settings;

	QWidget::connect(&_validationTimer, &QTimer::timeout, this,
			 &TwitchTokenSettingsDialog::CheckIfTokenValid);
	_validationTimer.start(10000);
	CheckIfTokenValid();
}

void TwitchTokenSettingsDialog::SetTokenInfoVisible(bool visible)
{
	SetGridLayoutRowVisible(_generalSettingsGrid, _nameRow, visible);
	SetGridLayoutRowVisible(_generalSettingsGrid, _tokenValueRow, visible);
}

void TwitchTokenSettingsDialog::CheckIfTokenValid()
{
	if (_currentToken._token.empty()) {
		return;
	}
	if (_currentToken.IsValid(true)) {
		return;
	}
	_tokenStatus->setText(
		obs_module_text("AdvSceneSwitcher.twitchToken.request.notSet"));
}

void TwitchTokenSettingsDialog::ShowToken()
{
	SetButtonIcon(_showToken, GetThemeTypeName() == "Light"
					  ? ":res/images/visible.svg"
					  : "theme:Dark/visible.svg");
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
		HighlightWidget(_requestToken, Qt::green, QColor(0, 0, 0, 0),
				true);
	}
	_name->setText("");
	SetTokenInfoVisible(false);
	QMetaObject::invokeMethod(this, "NameChanged",
				  Q_ARG(const QString &, ""));
	_tokenStatus->setText(
		obs_module_text("AdvSceneSwitcher.twitchToken.request.notSet"));
	_currentTokenValue->setText("");
}

static void revokeToken(const std::string &token)
{
	httplib::Client cli("https://id.twitch.tv");
	auto response = cli.Post("/oauth2/revoke",
				 std::string("client_id=") + GetClientID() +
					 "&token=" + token,
				 "application/x-www-form-urlencoded");
	if (!response) {
		auto err = response.error();
		blog(LOG_INFO, "Failed to revoke token: %s",
		     httplib::to_string(err).c_str());
		return;
	}
	if (response->status != 200) {
		blog(LOG_INFO, "Failed to revoke token: %d", response->status);
	}
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
	QString html = R"(
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
	                window.location.replace(`http://localhost:%1/token?access_token=${parsedHash.get('access_token')}&state=${parsedHash.get('state')}`);
	                output.textContent = 'It is safe to close this window';
	            }
	        } else {
	            window.location.replace('%2');
	        }
	    </script>
	</body>
	</html>)";

	return html.arg(QString::number(tokenGrabberPort), redirect)
		.toStdString();
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
	if (!value) {
		_tokenStatus->setText(obs_module_text(
			"AdvSceneSwitcher.twitchToken.request.fail"));
		_name->setText("");
		SetTokenInfoVisible(false);
		_requestToken->setEnabled(true);
		return;
	}

	_currentToken.SetToken(value.value().toStdString());
	auto name = QString::fromStdString(_currentToken._name);
	if (name.isEmpty()) {
		_tokenStatus->setText(obs_module_text(
			"AdvSceneSwitcher.twitchToken.request.fail"));
		_name->setText("");
		SetTokenInfoVisible(false);
		_requestToken->setEnabled(true);
		return;
	}

	_tokenStatus->setText(obs_module_text(
		"AdvSceneSwitcher.twitchToken.request.success"));
	_name->setText(name);
	_name->textEdited(name);
	QMetaObject::invokeMethod(this, "NameChanged",
				  Q_ARG(const QString &, name));
	SetTokenInfoVisible(true);
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
	settings._validateEventSubTimestamps =
		dialog._validateTimestamps->isChecked();
	if (settings._eventSub) {
		settings._eventSub->EnableTimestampValidation(
			settings._validateEventSubTimestamps);
	}
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
	QString getTokenURI =
		("https://id.twitch.tv/oauth2/authorize"
		 "?response_type=token"
		 "&client_id=" +
		 QString(GetClientID()) +
		 "&redirect_uri=http://localhost:%1"
		 "/auth"
		 "&scope=" +
		 _scope + "&state=" + QString::fromStdString(state))
			.arg(QString::number(tokenGrabberPort));

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
	_serverThread = std::thread([this]() {
		if (!_server.bind_to_port("localhost", tokenGrabberPort, 0)) {
			blog(LOG_WARNING,
			     "Failed to bind token server to localhost %d!",
			     tokenGrabberPort);
			return;
		}
		if (!_server.listen_after_bind()) {
			blog(LOG_WARNING, "Token server failed to listen()!");
			return;
		}
		vblog(LOG_INFO, "Token server stopped!");
	});
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
