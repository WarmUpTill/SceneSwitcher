#include "http-server.hpp"
#include "layout-helpers.hpp"
#include "log-helper.hpp"
#include "macro-export-extensions.hpp"
#include "obs-module-helper.hpp"
#include "plugin-state-helpers.hpp"

#include <obs-data.h>
#include <httplib.h>

#undef DispatchMessage

Q_DECLARE_METATYPE(advss::HttpServer *);

namespace advss {

static std::deque<std::shared_ptr<Item>> httpServers;
static void saveHttpServers(obs_data_t *obj);
static void loadHttpServers(obs_data_t *obj);
static bool setup();
static bool setupDone = setup();

bool setup()
{
	AddSaveStep(saveHttpServers);
	AddLoadStep(loadHttpServers);
	AddPluginCleanupStep([]() {
		for (auto &s : httpServers) {
			auto server = std::dynamic_pointer_cast<HttpServer>(s);
			if (server) {
				server->Stop();
			}
		}
		httpServers.clear();
	});
	AddMacroExportExtension(
		{"AdvSceneSwitcher.macroTab.export.httpServers", "httpServers",
		 [](obs_data_t *data, const QStringList &selectedIds) {
			 OBSDataArrayAutoRelease array =
				 obs_data_array_create();
			 for (const auto &s : httpServers) {
				 if (!selectedIds.isEmpty() &&
				     !selectedIds.contains(
					     QString::fromStdString(s->Name())))
					 continue;
				 OBSDataAutoRelease item = obs_data_create();
				 s->Save(item);
				 obs_data_array_push_back(array, item);
			 }
			 obs_data_set_array(data, "httpServers", array);
		 },
		 [](obs_data_t *data, const QStringList &) {
			 OBSDataArrayAutoRelease array =
				 obs_data_get_array(data, "httpServers");
			 const size_t count = obs_data_array_count(array);
			 for (size_t i = 0; i < count; ++i) {
				 OBSDataAutoRelease item =
					 obs_data_array_item(array, i);
				 auto server = HttpServer::Create();
				 server->Load(item);
				 if (!GetWeakHttpServerByName(server->Name())
					      .expired())
					 continue;
				 httpServers.emplace_back(server);
				 HttpServerSignalManager::Instance()->Add(
					 QString::fromStdString(
						 server->Name()));
			 }
		 },
		 []() -> QList<QPair<QString, QString>> {
			 QList<QPair<QString, QString>> items;
			 for (const auto &s : httpServers) {
				 const QString name =
					 QString::fromStdString(s->Name());
				 items.append({name, name});
			 }
			 return items;
		 }});
	return true;
}

static void saveHttpServers(obs_data_t *obj)
{
	auto array = obs_data_array_create();
	for (const auto &s : httpServers) {
		auto item = obs_data_create();
		s->Save(item);
		obs_data_array_push_back(array, item);
		obs_data_release(item);
	}
	obs_data_set_array(obj, "httpServers", array);
	obs_data_array_release(array);
}

static void loadHttpServers(obs_data_t *obj)
{
	httpServers.clear();
	auto array = obs_data_get_array(obj, "httpServers");
	const size_t count = obs_data_array_count(array);
	for (size_t i = 0; i < count; ++i) {
		auto item = obs_data_array_item(array, i);
		auto server = HttpServer::Create();
		httpServers.emplace_back(server);
		httpServers.back()->Load(item);
		obs_data_release(item);
	}
	obs_data_array_release(array);
}

struct HttpServer::Impl {
	std::unique_ptr<httplib::Server> server;
	std::thread thread;
	std::mutex mutex;
	std::atomic_bool listening{false};
	MessageDispatcher<HttpRequest> dispatcher;
};

HttpServer::HttpServer() : _impl(std::make_unique<Impl>()) {}

HttpServer::HttpServer(const HttpServer &other)
	: Item(other),
	  _impl(std::make_unique<Impl>())
{
	_port = other._port;
	_startOnLoad = other._startOnLoad;
}

HttpServer &HttpServer::operator=(const HttpServer &other)
{
	if (this != &other) {
		Stop();
		_name = other._name;
		_port = other._port;
		_startOnLoad = other._startOnLoad;
	}
	return *this;
}

HttpServer::~HttpServer()
{
	Stop();
}

bool HttpServer::IsListening() const
{
	return _impl->listening.load();
}

void HttpServer::Start()
{
	std::lock_guard<std::mutex> lock(_impl->mutex);

	if (_impl->server) {
		_impl->server->stop();
	}
	if (_impl->thread.joinable()) {
		_impl->thread.join();
	}

	_impl->server = std::make_unique<httplib::Server>();
	_impl->listening.store(false);

	auto handleRequest = [this](const httplib::Request &req,
				    httplib::Response &res) {
		HttpRequest request;
		request.method = req.method;
		request.path = req.path;
		request.body = req.body;
		for (const auto &[name, value] : req.headers) {
			request.headers.emplace(name, value);
		}
		_impl->dispatcher.DispatchMessage(request);
		res.status = 200;
	};

	_impl->server->Get(".*", handleRequest);
	_impl->server->Post(".*", handleRequest);
	_impl->server->Put(".*", handleRequest);
	_impl->server->Patch(".*", handleRequest);
	_impl->server->Delete(".*", handleRequest);

	const int port = _port;
	const std::string name = _name;
	auto serverPtr = _impl->server.get();
	auto listeningFlag = &_impl->listening;

	_impl->thread = std::thread([serverPtr, listeningFlag, port, name]() {
		if (!serverPtr->bind_to_port("0.0.0.0", port)) {
			blog(LOG_WARNING,
			     "HTTP server \"%s\" failed to bind to port %d",
			     name.c_str(), port);
			return;
		}
		listeningFlag->store(true);
		blog(LOG_INFO, "HTTP server \"%s\" listening on port %d",
		     name.c_str(), port);
		serverPtr->listen_after_bind();
		listeningFlag->store(false);
		blog(LOG_INFO, "HTTP server \"%s\" stopped", name.c_str());
	});
}

void HttpServer::Stop()
{
	if (!_impl) {
		return;
	}
	std::lock_guard<std::mutex> lock(_impl->mutex);
	if (_impl->server) {
		_impl->server->stop();
	}
	if (_impl->thread.joinable()) {
		_impl->thread.join();
	}
	_impl->server.reset();
	_impl->listening.store(false);
}

HttpRequestBuffer HttpServer::RegisterForRequests()
{
	return _impl->dispatcher.RegisterClient();
}

void HttpServer::Load(obs_data_t *obj)
{
	Item::Load(obj);
	_port = (int)obs_data_get_int(obj, "port");
	_startOnLoad = obs_data_get_bool(obj, "startOnLoad");
	if (_startOnLoad) {
		Start();
	}
}

void HttpServer::Save(obs_data_t *obj) const
{
	Item::Save(obj);
	obs_data_set_int(obj, "port", _port);
	obs_data_set_bool(obj, "startOnLoad", _startOnLoad);
}

HttpServer *GetHttpServerByName(const std::string &name)
{
	for (auto &s : httpServers) {
		if (s->Name() == name) {
			return dynamic_cast<HttpServer *>(s.get());
		}
	}
	return nullptr;
}

std::weak_ptr<HttpServer> GetWeakHttpServerByName(const std::string &name)
{
	for (const auto &s : httpServers) {
		if (s->Name() == name) {
			return std::dynamic_pointer_cast<HttpServer>(s);
		}
	}
	return {};
}

std::weak_ptr<HttpServer> GetWeakHttpServerByQString(const QString &name)
{
	return GetWeakHttpServerByName(name.toStdString());
}

std::string GetWeakHttpServerName(const std::weak_ptr<HttpServer> &server)
{
	auto s = server.lock();
	if (!s) {
		return obs_module_text("AdvSceneSwitcher.httpServer.invalid");
	}
	return s->Name();
}

std::deque<std::shared_ptr<Item>> &GetHttpServers()
{
	return httpServers;
}

// --- HttpServerSettingsDialog ---

static bool ServerNameAvailable(const QString &name)
{
	return !GetHttpServerByName(name.toStdString());
}

static bool AskForSettingsWrapper(QWidget *parent, Item &settings)
{
	HttpServer &server = dynamic_cast<HttpServer &>(settings);
	return HttpServerSettingsDialog::AskForSettings(parent, server);
}

HttpServerSettingsDialog::HttpServerSettingsDialog(QWidget *parent,
						   const HttpServer &settings)
	: ItemSettingsDialog(settings, httpServers,
			     "AdvSceneSwitcher.httpServer.select",
			     "AdvSceneSwitcher.httpServer.add",
			     "AdvSceneSwitcher.item.nameNotAvailable", true,
			     parent),
	  _port(new QSpinBox()),
	  _startOnLoad(new QCheckBox()),
	  _layout(new QGridLayout())
{
	_port->setMinimum(1);
	_port->setMaximum(65535);
	_port->setValue(settings._port);
	_startOnLoad->setChecked(settings._startOnLoad);

	int row = 0;
	_layout->addWidget(
		new QLabel(obs_module_text("AdvSceneSwitcher.httpServer.name")),
		row, 0);
	auto nameLayout = new QHBoxLayout;
	nameLayout->addWidget(_name);
	nameLayout->addWidget(_nameHint);
	_layout->addLayout(nameLayout, row, 1);
	++row;
	_layout->addWidget(
		new QLabel(obs_module_text("AdvSceneSwitcher.httpServer.port")),
		row, 0);
	_layout->addWidget(_port, row, 1);
	++row;
	_layout->addWidget(new QLabel(obs_module_text(
				   "AdvSceneSwitcher.httpServer.startOnLoad")),
			   row, 0);
	_layout->addWidget(_startOnLoad, row, 1);
	++row;
	_layout->addWidget(_buttonbox, row, 0, 1, -1);
	setLayout(_layout);

	MinimizeSizeOfColumn(_layout, 0);
}

bool HttpServerSettingsDialog::AskForSettings(QWidget *parent,
					      HttpServer &settings)
{
	HttpServerSettingsDialog dialog(parent, settings);
	dialog.setWindowTitle(obs_module_text("AdvSceneSwitcher.windowTitle"));
	if (dialog.exec() != DialogCode::Accepted) {
		return false;
	}
	settings._name = dialog._name->text().toStdString();
	settings._port = dialog._port->value();
	settings._startOnLoad = dialog._startOnLoad->isChecked();
	settings.Start();
	return true;
}

HttpServerSelection::HttpServerSelection(QWidget *parent)
	: ItemSelection(httpServers, HttpServer::Create, AskForSettingsWrapper,
			"AdvSceneSwitcher.httpServer.select",
			"AdvSceneSwitcher.httpServer.add",
			"AdvSceneSwitcher.item.nameNotAvailable",
			"AdvSceneSwitcher.httpServer.configure", parent)
{
	QWidget::connect(HttpServerSignalManager::Instance(),
			 SIGNAL(Rename(const QString &, const QString &)), this,
			 SLOT(RenameItem(const QString &, const QString &)));
	QWidget::connect(HttpServerSignalManager::Instance(),
			 SIGNAL(Add(const QString &)), this,
			 SLOT(AddItem(const QString &)));
	QWidget::connect(HttpServerSignalManager::Instance(),
			 SIGNAL(Remove(const QString &)), this,
			 SLOT(RemoveItem(const QString &)));

	QWidget::connect(this,
			 SIGNAL(ItemRenamed(const QString &, const QString &)),
			 HttpServerSignalManager::Instance(),
			 SIGNAL(Rename(const QString &, const QString &)));
	QWidget::connect(this, SIGNAL(ItemAdded(const QString &)),
			 HttpServerSignalManager::Instance(),
			 SIGNAL(Add(const QString &)));
	QWidget::connect(this, SIGNAL(ItemRemoved(const QString &)),
			 HttpServerSignalManager::Instance(),
			 SIGNAL(Remove(const QString &)));
}

void HttpServerSelection::SetServer(const std::string &name)
{
	if (GetHttpServerByName(name)) {
		SetItem(name);
	} else {
		SetItem("");
	}
}

void HttpServerSelection::SetServer(const std::weak_ptr<HttpServer> &server)
{
	auto s = server.lock();
	if (s) {
		SetItem(s->Name());
	} else {
		SetItem("");
	}
}

HttpServerSignalManager::HttpServerSignalManager(QObject *parent)
	: QObject(parent)
{
}

HttpServerSignalManager *HttpServerSignalManager::Instance()
{
	static HttpServerSignalManager manager;
	return &manager;
}

} // namespace advss
