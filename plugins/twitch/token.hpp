#pragma once
#include "event-sub.hpp"

#include <item-selection-helpers.hpp>
#include <httplib.h>
#include <set>
#include <QCheckBox>
#include <QThread>
#include <QLayout>
#include <QTimer>
#include <optional>

namespace advss {

class TwitchConnectionSelection;
class TwitchTokenSettingsDialog;

class TokenOption {
public:
	void Load(obs_data_t *obj);
	void Save(obs_data_t *obj) const;
	std::string GetLocale() const;

	static const std::unordered_map<std::string, std::string> &
	GetTokenOptionMap();
	static std::set<TokenOption> GetAllTokenOptions();
	bool operator<(const TokenOption &other) const;
	std::string apiId = "";

private:
	const static std::unordered_map<std::string, std::string> _apiIdToLocale;
};

class TwitchToken : public Item {
public:
	TwitchToken() = default;
	TwitchToken(const TwitchToken &);
	TwitchToken &operator=(const TwitchToken &);

	static std::shared_ptr<Item> Create()
	{
		return std::make_shared<TwitchToken>();
	}

	void Load(obs_data_t *obj);
	void Save(obs_data_t *obj) const;
	std::string GetName() { return _name; }
	bool OptionIsActive(const TokenOption &option) const;
	bool OptionIsEnabled(const TokenOption &option) const;
	bool AnyOptionIsEnabled(const std::vector<TokenOption> &options) const;
	void SetToken(const std::string &);
	bool IsEmpty() const { return _token.empty(); }
	std::optional<std::string> GetToken() const;
	std::string GetUserID() const { return _userID; }
	std::shared_ptr<EventSub> GetEventSub();
	bool ValidateTimestamps() const { return _validateEventSubTimestamps; }
	bool IsValid(bool forceUpdate = false) const;
	bool WarnIfInvalid() const { return _warnIfInvalid; }
	void SetWarnIfInvalid(bool value) { _warnIfInvalid = value; }
	size_t PermissionCount() const { return _tokenOptions.size(); }

private:
	std::string _token;
	mutable std::mutex _cacheMutex;
	mutable std::string _lastValidityCheckValue;
	mutable bool _lastValidityCheckResult = false;
	mutable std::chrono::system_clock::time_point _lastValidityCheckTime;
	std::string _userID;
	std::set<TokenOption> _tokenOptions = TokenOption::GetAllTokenOptions();
	std::shared_ptr<EventSub> _eventSub;
	bool _validateEventSubTimestamps = false;
	bool _warnIfInvalid = true;

	static bool _setup;

	friend TwitchConnectionSelection;
	friend TwitchTokenSettingsDialog;
};

class TokenGrabberThread : public QThread {
	Q_OBJECT

public:
	~TokenGrabberThread();
	void SetTokenScope(const QString &value) { _scope = value; }

signals:
	void GotToken(const std::optional<QString> &);

protected:
	void run() override;

private:
	void Stop();

	QString _scope;
	std::optional<QString> _tokenString;

	static int _timeout;
	std::mutex _mutex;
	std::atomic_bool _stopWaiting = {false};
	std::condition_variable _cv;
	std::thread _serverThread;
	httplib::Server _server;
};

class TwitchTokenSettingsDialog : public ItemSettingsDialog {
	Q_OBJECT

public:
	TwitchTokenSettingsDialog(QWidget *parent, const TwitchToken &);
	static bool AskForSettings(QWidget *parent, TwitchToken &settings);

private slots:
	void ShowToken();
	void HideToken();
	void TokenOptionChanged(int);
	void RequestToken();
	void GotToken(const std::optional<QString> &);
	void CheckIfTokenValid();

private:
	std::set<TokenOption> GetEnabledOptions();
	void SetTokenInfoVisible(bool);

	QPushButton *_requestToken;
	QPushButton *_showToken;
	QLineEdit *_currentTokenValue;
	QLabel *_tokenStatus;
	QGridLayout *_generalSettingsGrid;
	int _nameRow = -1;
	int _tokenValueRow = -1;
	TokenGrabberThread _tokenGrabber;
	TwitchToken _currentToken;
	std::unordered_map<std::string, QCheckBox *> _optionWidgets;
	QCheckBox *_validateTimestamps;
	QCheckBox *_warnIfInvalid;
	QTimer _validationTimer;
};

class TwitchConnectionSelection : public ItemSelection {
	Q_OBJECT

public:
	TwitchConnectionSelection(QWidget *parent = 0);
	void SetToken(const std::string &);
	void SetToken(const std::weak_ptr<TwitchToken> &);
};

// Helper class so that it is not required to add signals to the
// AdvSceneSwitcher class for handling adding and removing Twitch connections
class TwitchConnectionSignalManager : public QObject {
	Q_OBJECT
public:
	static TwitchConnectionSignalManager *Instance();

private:
signals:
	// Rename signal not required as name is based on Twitch account name
	// and item name cannot be manually changed
	void Add(const QString &);
	void Remove(const QString &);
};

TwitchToken *GetTwitchTokenByName(const QString &);
TwitchToken *GetTwitchTokenByName(const std::string &);
std::weak_ptr<TwitchToken> GetWeakTwitchTokenByName(const std::string &name);
std::weak_ptr<TwitchToken> GetWeakTwitchTokenByQString(const QString &name);
std::string GetWeakTwitchTokenName(std::weak_ptr<TwitchToken>);
bool TokenIsValid(const std::weak_ptr<TwitchToken> &token);
std::deque<std::shared_ptr<Item>> &GetTwitchTokens();

} // namespace advss

Q_DECLARE_METATYPE(advss::TwitchToken *);
