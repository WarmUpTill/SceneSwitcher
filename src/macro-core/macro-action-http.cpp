#include "macro-action-http.hpp"
#include "advanced-scene-switcher.hpp"
#include "utility.hpp"
#include "curl-helper.hpp"

const std::string MacroActionHttp::id = "http";

bool MacroActionHttp::_registered = MacroActionFactory::Register(
	MacroActionHttp::id,
	{MacroActionHttp::Create, MacroActionHttpEdit::Create,
	 "AdvSceneSwitcher.action.http"});

const static std::map<MacroActionHttp::Method, std::string> methods = {
	{MacroActionHttp::Method::GET, "AdvSceneSwitcher.action.http.type.get"},
	{MacroActionHttp::Method::POST,
	 "AdvSceneSwitcher.action.http.type.post"},
};

size_t WriteCB(void *, size_t size, size_t nmemb, std::string *)
{
	// Just drop the data
	return size * nmemb;
}

void MacroActionHttp::Get()
{
	switcher->curl.SetOpt(CURLOPT_URL, _url.c_str());
	switcher->curl.SetOpt(CURLOPT_HTTPGET, 1L);
	switcher->curl.SetOpt(CURLOPT_TIMEOUT_MS, _timeout.seconds * 1000);

	std::string response;
	switcher->curl.SetOpt(CURLOPT_WRITEFUNCTION, WriteCB);
	switcher->curl.SetOpt(CURLOPT_WRITEDATA, &response);

	switcher->curl.Perform();
}

void MacroActionHttp::Post()
{
	switcher->curl.SetOpt(CURLOPT_URL, _url.c_str());
	switcher->curl.SetOpt(CURLOPT_POSTFIELDS, _data.c_str());
	switcher->curl.SetOpt(CURLOPT_TIMEOUT_MS, _timeout.seconds * 1000);
	switcher->curl.Perform();
}

bool MacroActionHttp::PerformAction()
{
	if (!switcher->curl.Initialized()) {
		blog(LOG_WARNING,
		     "cannot perform http action (curl not found)");
		return true;
	}

	switch (_method) {
	case MacroActionHttp::Method::GET:
		Get();
		break;
	case MacroActionHttp::Method::POST:
		Post();
		break;
	default:
		break;
	}
	return true;
}

void MacroActionHttp::LogAction() const
{
	auto it = methods.find(_method);
	if (it != methods.end()) {
		vblog(LOG_INFO,
		      "sent http request \"%s\" to \"%s\" with data \"%s\"",
		      it->second.c_str(), _url.c_str(), _data.c_str());
	} else {
		blog(LOG_WARNING, "ignored unknown http action %d",
		     static_cast<int>(_method));
	}
}

bool MacroActionHttp::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	obs_data_set_string(obj, "url", _url.c_str());
	obs_data_set_string(obj, "data", _data.c_str());
	obs_data_set_int(obj, "method", static_cast<int>(_method));
	_timeout.Save(obj);
	return true;
}

bool MacroActionHttp::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_url = obs_data_get_string(obj, "url");
	_data = obs_data_get_string(obj, "data");
	_method = static_cast<Method>(obs_data_get_int(obj, "method"));
	_timeout.Load(obj);
	return true;
}

std::string MacroActionHttp::GetShortDesc() const
{
	return _url;
}

static inline void populateMethodSelection(QComboBox *list)
{
	for (auto entry : methods) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroActionHttpEdit::MacroActionHttpEdit(
	QWidget *parent, std::shared_ptr<MacroActionHttp> entryData)
	: QWidget(parent),
	  _url(new QLineEdit()),
	  _methods(new QComboBox()),
	  _data(new ResizingPlainTextEdit(this)),
	  _timeout(new DurationSelection(this, false))
{
	populateMethodSelection(_methods);

	QWidget::connect(_url, SIGNAL(editingFinished()), this,
			 SLOT(URLChanged()));
	QWidget::connect(_data, SIGNAL(textChanged()), this,
			 SLOT(DataChanged()));
	QWidget::connect(_methods, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(MethodChanged(int)));
	QWidget::connect(_timeout, SIGNAL(DurationChanged(double)), this,
			 SLOT(TimeoutChanged(double)));

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{url}}", _url},
		{"{{method}}", _methods},
		{"{{data}}", _data},
		{"{{timeout}}", _timeout},
	};
	auto *actionLayout = new QHBoxLayout;
	placeWidgets(
		obs_module_text("AdvSceneSwitcher.action.http.entry.line1"),
		actionLayout, widgetPlaceholders);
	auto *timeoutLayout = new QHBoxLayout;
	placeWidgets(
		obs_module_text("AdvSceneSwitcher.action.http.entry.line2"),
		timeoutLayout, widgetPlaceholders);

	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(actionLayout);
	mainLayout->addWidget(_data);
	mainLayout->addLayout(timeoutLayout);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionHttpEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_url->setText(QString::fromStdString(_entryData->_url));
	_data->setPlainText(QString::fromStdString(_entryData->_data));
	_methods->setCurrentIndex(static_cast<int>(_entryData->_method));
	_timeout->SetDuration(_entryData->_timeout);
	SetWidgetVisibility();
}

void MacroActionHttpEdit::URLChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_url = _url->text().toStdString();
	emit(HeaderInfoChanged(_url->text()));
}

void MacroActionHttpEdit::MethodChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_method = static_cast<MacroActionHttp::Method>(value);
	SetWidgetVisibility();
}

void MacroActionHttpEdit::TimeoutChanged(double seconds)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_timeout.seconds = seconds;
}

void MacroActionHttpEdit::SetWidgetVisibility()
{
	switch (_entryData->_method) {
	case MacroActionHttp::Method::GET:
		_data->hide();
		break;
	case MacroActionHttp::Method::POST:
		_data->show();
		break;
	default:
		break;
	}

	adjustSize();
	updateGeometry();
}

void MacroActionHttpEdit::DataChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_data = _data->toPlainText().toUtf8().constData();

	adjustSize();
	updateGeometry();
}
