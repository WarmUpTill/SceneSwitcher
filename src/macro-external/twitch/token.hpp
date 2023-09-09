#pragma once
#include <item-selection-helpers.hpp>
#include <httplib.h>
#include <set>
#include <QCheckBox>
#include <QThread>
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
	bool operator<(const TokenOption &other) const;
	std::string apiId = "";

private:
	const static std::unordered_map<std::string, std::string> apiIdToLocale;
};

class TwitchToken : public Item {
public:
	static std::shared_ptr<Item> Create()
	{
		return std::make_shared<TwitchToken>();
	}

	void Load(obs_data_t *obj);
	void Save(obs_data_t *obj) const;
	std::string GetName() { return _name; }
	bool OptionIsEnabled(const TokenOption &) const;
	void SetToken(const std::string &);
	bool IsEmpty() const { return _token.empty(); }
	std::string GetToken() const { return _token; }
	std::string GetUserID() const { return _userID; }

private:
	std::string _token;
	std::string _userID;
	std::set<TokenOption> _tokenOptions = {{"channel:manage:broadcast"}};

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

private:
	std::set<TokenOption> GetEnabledOptions();

	QPushButton *_requestToken;
	QPushButton *_showToken;
	QLineEdit *_currentTokenValue;
	QLabel *_tokenStatus;
	TokenGrabberThread _tokenGrabber;
	TwitchToken _currentToken;
	std::unordered_map<std::string, QCheckBox *> _optionWidgets;
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

} // namespace advss

Q_DECLARE_METATYPE(advss::TwitchToken *);
