#pragma once
#include "item-selection-helpers.hpp"
#include "message-buffer.hpp"
#include "message-dispatcher.hpp"

#include <map>
#include <memory>
#include <string>

#include <QCheckBox>
#include <QGridLayout>
#include <QSpinBox>

namespace advss {

struct HttpRequest {
	std::string method;
	std::string path;
	std::string body;
	std::map<std::string, std::string> headers;
};

using HttpRequestBuffer = std::shared_ptr<MessageBuffer<HttpRequest>>;

class HttpServerSelection;
class HttpServerSettingsDialog;

class HttpServer : public Item {
public:
	HttpServer();
	HttpServer(const HttpServer &);
	HttpServer &operator=(const HttpServer &);
	~HttpServer();

	static std::shared_ptr<Item> Create()
	{
		return std::make_shared<HttpServer>();
	}

	void Load(obs_data_t *obj);
	void Save(obs_data_t *obj) const;

	HttpRequestBuffer RegisterForRequests();
	int GetPort() const { return _port; }
	bool IsListening() const;

	void Start();
	void Stop();

private:
	int _port = 16384;
	bool _startOnLoad = true;

	struct Impl;
	std::unique_ptr<Impl> _impl;

	friend HttpServerSelection;
	friend HttpServerSettingsDialog;
};

class HttpServerSettingsDialog : public ItemSettingsDialog {
	Q_OBJECT

public:
	HttpServerSettingsDialog(QWidget *parent, const HttpServer &);
	static bool AskForSettings(QWidget *parent, HttpServer &settings);

private:
	QSpinBox *_port;
	QCheckBox *_startOnLoad;
	QGridLayout *_layout;
};

class HttpServerSelection : public ItemSelection {
	Q_OBJECT

public:
	HttpServerSelection(QWidget *parent = nullptr);
	void SetServer(const std::string &);
	void SetServer(const std::weak_ptr<HttpServer> &);
};

class HttpServerSignalManager : public QObject {
	Q_OBJECT
public:
	HttpServerSignalManager(QObject *parent = nullptr);
	static HttpServerSignalManager *Instance();

signals:
	void Rename(const QString &, const QString &);
	void Add(const QString &);
	void Remove(const QString &);
};

HttpServer *GetHttpServerByName(const std::string &);
std::weak_ptr<HttpServer> GetWeakHttpServerByName(const std::string &);
std::weak_ptr<HttpServer> GetWeakHttpServerByQString(const QString &);
std::string GetWeakHttpServerName(const std::weak_ptr<HttpServer> &);
std::deque<std::shared_ptr<Item>> &GetHttpServers();

} // namespace advss
