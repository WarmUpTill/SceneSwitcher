#include "macro-action-http-legacy.hpp"
#include "curl-helper.hpp"
#include "layout-helpers.hpp"

namespace advss {

const std::string MacroActionHttp::id = "http";

bool MacroActionHttp::_registered = MacroActionFactory::Register(
	MacroActionHttp::id,
	{MacroActionHttp::Create, MacroActionHttpEdit::Create,
	 "AdvSceneSwitcher.action.http.legacy", true});

const static std::map<MacroActionHttp::Method, std::string> methods = {
	{MacroActionHttp::Method::GET, "AdvSceneSwitcher.action.http.type.get"},
	{MacroActionHttp::Method::POST,
	 "AdvSceneSwitcher.action.http.type.post"},
};

static size_t DropCB(void *, size_t size, size_t nmemb, std::string *)
{
	return size * nmemb;
}

static size_t WriteCB(void *ptr, size_t size, size_t nmemb, std::string *buffer)
{
	buffer->append((char *)ptr, nmemb);
	return size * nmemb;
}

void MacroActionHttp::SetupHeaders()
{
	if (!_setHeaders) {
		return;
	}
	struct curl_slist *headers = nullptr;
	for (auto &header : _headers) {
		headers = CurlHelper::SlistAppend(headers, header.c_str());
	}
	if (!_headers.empty()) {
		CurlHelper::SetOpt(CURLOPT_HTTPHEADER, headers);
	}
}

void MacroActionHttp::Get()
{
	CurlHelper::SetOpt(CURLOPT_URL, _url.c_str());
	CurlHelper::SetOpt(CURLOPT_HTTPGET, 1L);
	CurlHelper::SetOpt(CURLOPT_TIMEOUT_MS, _timeout.Milliseconds());
	SetupHeaders();

	std::string response;
	if (IsReferencedInVars()) {
		CurlHelper::SetOpt(CURLOPT_WRITEFUNCTION, WriteCB);
	} else {
		CurlHelper::SetOpt(CURLOPT_WRITEFUNCTION, DropCB);
	}
	CurlHelper::SetOpt(CURLOPT_WRITEDATA, &response);
	CurlHelper::Perform();

	SetVariableValue(response);
}

void MacroActionHttp::Post()
{
	CurlHelper::SetOpt(CURLOPT_URL, _url.c_str());
	CurlHelper::SetOpt(CURLOPT_POSTFIELDS, _data.c_str());
	CurlHelper::SetOpt(CURLOPT_TIMEOUT_MS, _timeout.Milliseconds());
	SetupHeaders();
	CurlHelper::Perform();
}

bool MacroActionHttp::PerformAction()
{
	if (!CurlHelper::Initialized()) {
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
		ablog(LOG_INFO,
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
	_url.Save(obj, "url");
	_data.Save(obj, "data");
	obs_data_set_bool(obj, "setHeaders", _setHeaders);
	_headers.Save(obj, "headers", "header");
	obs_data_set_int(obj, "method", static_cast<int>(_method));
	_timeout.Save(obj);
	return true;
}

bool MacroActionHttp::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_url.Load(obj, "url");
	_data.Load(obj, "data");
	_setHeaders = obs_data_get_bool(obj, "setHeaders");
	_headers.Load(obj, "headers", "header");
	_method = static_cast<Method>(obs_data_get_int(obj, "method"));
	_timeout.Load(obj);
	return true;
}

std::string MacroActionHttp::GetShortDesc() const
{
	return _url.UnresolvedValue();
}

std::shared_ptr<MacroAction> MacroActionHttp::Create(Macro *m)
{
	return std::make_shared<MacroActionHttp>(m);
}

std::shared_ptr<MacroAction> MacroActionHttp::Copy() const
{
	return std::make_shared<MacroActionHttp>(*this);
}

void MacroActionHttp::ResolveVariablesToFixedValues()
{
	_url.ResolveVariables();
	_data.ResolveVariables();
	_headers.ResolveVariables();
	_timeout.ResolveVariables();
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
	  _url(new VariableLineEdit(this)),
	  _methods(new QComboBox()),
	  _data(new VariableTextEdit(this)),
	  _setHeaders(new QCheckBox(
		  obs_module_text("AdvSceneSwitcher.action.http.setHeaders"))),
	  _headerListLayout(new QVBoxLayout()),
	  _headerList(new StringListEdit(
		  this, obs_module_text("AdvSceneSwitcher.action.http.headers"),
		  obs_module_text("AdvSceneSwitcher.action.http.addHeader"),
		  170, [](const std::string &input) { return input.empty(); })),
	  _timeout(new DurationSelection(this, false))
{
	populateMethodSelection(_methods);
	_headerList->SetMaxStringSize(4096);

	QWidget::connect(_url, SIGNAL(editingFinished()), this,
			 SLOT(URLChanged()));
	QWidget::connect(_data, SIGNAL(textChanged()), this,
			 SLOT(DataChanged()));
	QWidget::connect(_methods, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(MethodChanged(int)));
	QWidget::connect(_setHeaders, SIGNAL(stateChanged(int)), this,
			 SLOT(SetHeadersChanged(int)));
	QWidget::connect(_headerList,
			 SIGNAL(StringListChanged(const StringList &)), this,
			 SLOT(HeadersChanged(const StringList &)));
	QWidget::connect(_timeout, SIGNAL(DurationChanged(const Duration &)),
			 this, SLOT(TimeoutChanged(const Duration &)));

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{url}}", _url},
		{"{{method}}", _methods},
		{"{{data}}", _data},
		{"{{timeout}}", _timeout},
	};
	auto *actionLayout = new QHBoxLayout;
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.action.http.entry.line1"),
		actionLayout, widgetPlaceholders);
	auto *timeoutLayout = new QHBoxLayout;
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.action.http.entry.line2"),
		timeoutLayout, widgetPlaceholders);

	_headerListLayout->addWidget(new QLabel(
		obs_module_text("AdvSceneSwitcher.action.http.headers")));
	_headerListLayout->addWidget(_headerList);

	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(actionLayout);
	mainLayout->addWidget(_setHeaders);
	mainLayout->addLayout(_headerListLayout);
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

	_url->setText(_entryData->_url);
	_data->setPlainText(_entryData->_data);
	_setHeaders->setChecked(_entryData->_setHeaders);
	_headerList->SetStringList(_entryData->_headers);
	_methods->setCurrentIndex(static_cast<int>(_entryData->_method));
	_timeout->SetDuration(_entryData->_timeout);
	SetWidgetVisibility();
}

void MacroActionHttpEdit::URLChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_url = _url->text().toStdString();
	emit(HeaderInfoChanged(_url->text()));
}

void MacroActionHttpEdit::MethodChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_method = static_cast<MacroActionHttp::Method>(value);
	SetWidgetVisibility();
}

void MacroActionHttpEdit::TimeoutChanged(const Duration &dur)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_timeout = dur;
}

void MacroActionHttpEdit::SetHeadersChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_setHeaders = value;
	SetWidgetVisibility();
}

void MacroActionHttpEdit::HeadersChanged(const StringList &headers)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_headers = headers;
	adjustSize();
	updateGeometry();
}

void MacroActionHttpEdit::SetWidgetVisibility()
{
	_data->setVisible(_entryData->_method == MacroActionHttp::Method::POST);
	SetLayoutVisible(_headerListLayout, _entryData->_setHeaders);

	adjustSize();
	updateGeometry();
}

void MacroActionHttpEdit::DataChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_data = _data->toPlainText().toUtf8().constData();

	adjustSize();
	updateGeometry();
}

} // namespace advss
