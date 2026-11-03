#pragma once
#include "youtube-oauth.hpp"

#include <item-selection-helpers.hpp>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <QThread>

namespace advss {

class YouTubeConnectionSettingsDialog;

class YouTubeConnection : public Item {
public:
	YouTubeConnection() = default;
	YouTubeConnection(const YouTubeConnection &);
	YouTubeConnection &operator=(const YouTubeConnection &);

	static std::shared_ptr<Item> Create()
	{
		return std::make_shared<YouTubeConnection>();
	}

	void Load(obs_data_t *obj) override;
	void Save(obs_data_t *obj) const override;
	void SaveForExport(obs_data_t *obj) const;

	std::optional<OAuthClientConfig> GetClientConfig() const;
	std::optional<std::string> GetAccessToken(bool forceRefresh = false);
	bool SetAuthResult(const TokenResponse &, std::string &errorReason);
	bool IsValid() const;
	std::string GetChannelID() const;

private:
	std::string _customClientId;
	std::string _customClientSecret;
	std::string _refreshToken;
	std::string _channelId;

	mutable std::mutex _mutex;
	std::string _accessToken;
	std::chrono::system_clock::time_point _accessTokenExpiry;
	bool _refreshTokenRevoked = false;

	friend YouTubeConnectionSettingsDialog;
};

class AuthCodeGrabberThread : public QThread {
	Q_OBJECT

public:
	~AuthCodeGrabberThread();
	void SetClientConfig(const OAuthClientConfig &config)
	{
		_config = config;
	}
	const TokenResponse &Result() const { return _result; }

protected:
	void run() override;

private:
	OAuthClientConfig _config;
	TokenResponse _result;

	std::mutex _mutex;
	std::condition_variable _cv;
	std::atomic_bool _stopWaiting = {false};
};

class YouTubeConnectionSettingsDialog : public ItemSettingsDialog {
	Q_OBJECT

public:
	YouTubeConnectionSettingsDialog(QWidget *parent,
					const YouTubeConnection &);
	static bool AskForSettings(QWidget *parent,
				   YouTubeConnection &settings);

private slots:
	void ClientConfigChanged();
	void Connect();
	void AuthFinished();

private:
	void SetErrorStatus(const std::string &errorReason);

	QLineEdit *_clientId;
	QLineEdit *_clientSecret;
	QPushButton *_connect;
	QLabel *_status;
	AuthCodeGrabberThread _authGrabber;
	YouTubeConnection _currentConnection;
};

class YouTubeConnectionSelection : public ItemSelection {
	Q_OBJECT

public:
	YouTubeConnectionSelection(QWidget *parent = nullptr);
	void SetConnection(const std::weak_ptr<YouTubeConnection> &);
};

class YouTubeConnectionSignalManager : public QObject {
	Q_OBJECT
public:
	static YouTubeConnectionSignalManager *Instance();

private:
signals:
	// Rename signal not required as name is based on the channel name
	// and item name cannot be manually changed
	void Add(const QString &);
	void Remove(const QString &);
};

std::weak_ptr<YouTubeConnection>
GetWeakYouTubeConnectionByName(const std::string &name);
std::string
GetWeakYouTubeConnectionName(const std::weak_ptr<YouTubeConnection> &);
std::deque<std::shared_ptr<Item>> &GetYouTubeConnections();

} // namespace advss
