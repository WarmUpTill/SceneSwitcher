#include "macro-action-http.hpp"
#include "curl-helper.hpp"
#include "layout-helpers.hpp"

namespace advss {

const std::string MacroActionHttp::id = "http";

bool MacroActionHttp::_registered = MacroActionFactory::Register(
	MacroActionHttp::id,
	{MacroActionHttp::Create, MacroActionHttpEdit::Create,
	 "AdvSceneSwitcher.action.http"});

const static std::map<MacroActionHttp::Method, std::string> methods = {
	{MacroActionHttp::Method::GET, "AdvSceneSwitcher.action.http.type.get"},
	{MacroActionHttp::Method::POST,
	 "AdvSceneSwitcher.action.http.type.post"},
	{MacroActionHttp::Method::PUT, "AdvSceneSwitcher.action.http.type.put"},
	{MacroActionHttp::Method::PATCH,
	 "AdvSceneSwitcher.action.http.type.patch"},
	{MacroActionHttp::Method::DELETION,
	 "AdvSceneSwitcher.action.http.type.delete"},
	{MacroActionHttp::Method::HEAD,
	 "AdvSceneSwitcher.action.http.type.head"},
};

static size_t WriteCB(void *ptr, size_t size, size_t nmemb, std::string *buffer)
{
	buffer->append((char *)ptr, nmemb);
	return size * nmemb;
}

void MacroActionHttp::SetCommonOpts(bool useRequestBody = true,
				    bool useResponseBody = true)
{
	CurlHelper::SetOpt(CURLOPT_URL, _url.c_str());
	CurlHelper::SetOpt(CURLOPT_TIMEOUT_MS, _timeout.Milliseconds());
	SetHeaders();
	CurlHelper::SetOpt(CURLOPT_HEADERFUNCTION, WriteCB);
	CurlHelper::SetOpt(CURLOPT_HEADERDATA, &_responseHeaders);

	if (useResponseBody) {
		CurlHelper::SetOpt(CURLOPT_WRITEFUNCTION, WriteCB);
		CurlHelper::SetOpt(CURLOPT_WRITEDATA, &_responseBody);
	} else {
		CurlHelper::SetOpt(CURLOPT_NOBODY, 1L);
	}

	if (useRequestBody) {
		CurlHelper::SetOpt(CURLOPT_POSTFIELDS, _data.c_str());
	}

	if (_errorForBadStatusCode) {
		CurlHelper::SetOpt(CURLOPT_FAILONERROR, 1L);
	}
}

void MacroActionHttp::SetHeaders()
{
	if (!_setHeaders) {
		return;
	}

	if (!_headers.empty()) {
		struct curl_slist *headers = nullptr;
		for (auto &header : _headers) {
			headers = CurlHelper::SlistAppend(headers,
							  header.c_str());
		}

		CurlHelper::SetOpt(CURLOPT_HTTPHEADER, headers);
	}
}

void MacroActionHttp::SendRequest()
{
	int maxAttempts = _retryCount + 1;

	for (int i = 1; i <= maxAttempts; ++i) {
		auto code = CurlHelper::Perform();

		if (code == CURLE_OK) {
			SetVariableValue(_responseBody);
			SetTempVarValues();
			return;
		}

		vblog(LOG_WARNING,
		      "Performing HTTP request failed on attempt %d in %s with error: %s",
		      i, __func__, CurlHelper::GetError(code));

		if (i != maxAttempts) {
			std::this_thread::sleep_for(
				std::chrono::duration<double>(
					_retryDelay.Seconds()));
		}
	}
}

void MacroActionHttp::Reset()
{
	CurlHelper::Reset();
	_responseBody = "";
	_responseHeaders = "";
}

void MacroActionHttp::SendGetRequest()
{
	SetCommonOpts(false);
	CurlHelper::SetOpt(CURLOPT_HTTPGET, 1L);

	SendRequest();
}

void MacroActionHttp::SendPostRequest()
{
	SetCommonOpts();

	SendRequest();
}

void MacroActionHttp::SendPutRequest()
{
	SetCommonOpts();
	CurlHelper::SetOpt(CURLOPT_CUSTOMREQUEST, "PUT");

	SendRequest();
}

void MacroActionHttp::SendPatchRequest()
{
	SetCommonOpts();
	CurlHelper::SetOpt(CURLOPT_CUSTOMREQUEST, "PATCH");

	SendRequest();
}

void MacroActionHttp::SendDeleteRequest()
{
	SetCommonOpts();
	CurlHelper::SetOpt(CURLOPT_CUSTOMREQUEST, "DELETE");

	SendRequest();
}

void MacroActionHttp::SendHeadRequest()
{
	SetCommonOpts(false, false);

	SendRequest();
}

bool MacroActionHttp::PerformAction()
{
	if (!CurlHelper::Initialized()) {
		blog(LOG_WARNING,
		     "cannot perform http action (curl not found)");
		return true;
	}

	Reset();

	switch (_method) {
	case MacroActionHttp::Method::GET:
		SendGetRequest();
		break;
	case MacroActionHttp::Method::POST:
		SendPostRequest();
		break;
	case MacroActionHttp::Method::PUT:
		SendPutRequest();
		break;
	case MacroActionHttp::Method::PATCH:
		SendPatchRequest();
		break;
	case MacroActionHttp::Method::DELETION:
		SendDeleteRequest();
		break;
	case MacroActionHttp::Method::HEAD:
		SendHeadRequest();
		break;
	default:
		break;
	}

	return true;
}

void MacroActionHttp::SetupTempVars()
{
	MacroAction::SetupTempVars();

	AddTempvar(
		"redirects.count",
		obs_module_text(
			"AdvSceneSwitcher.tempVar.http.redirects.count"),
		obs_module_text(
			"AdvSceneSwitcher.tempVar.http.redirects.count.description"));
	AddTempvar(
		"response.body",
		obs_module_text("AdvSceneSwitcher.tempVar.http.response.body"),
		obs_module_text(
			"AdvSceneSwitcher.tempVar.http.response.body.description"));
	AddTempvar(
		"response.header.contentType",
		obs_module_text(
			"AdvSceneSwitcher.tempVar.http.response.header.contentType"),
		obs_module_text(
			"AdvSceneSwitcher.tempVar.http.response.header.contentType.description"));
	AddTempvar(
		"response.header.retryAfter",
		obs_module_text(
			"AdvSceneSwitcher.tempVar.http.response.header.retryAfter"),
		obs_module_text(
			"AdvSceneSwitcher.tempVar.http.response.header.retryAfter.description"));
	AddTempvar(
		"response.headers",
		obs_module_text(
			"AdvSceneSwitcher.tempVar.http.response.headers"),
		obs_module_text(
			"AdvSceneSwitcher.tempVar.http.response.headers.description"));
	AddTempvar(
		"response.code",
		obs_module_text(
			"AdvSceneSwitcher.tempVar.http.response.statusCode"),
		obs_module_text(
			"AdvSceneSwitcher.tempVar.http.response.statusCode.description"));
	AddTempvar(
		"transfer.speed.download.average",
		obs_module_text(
			"AdvSceneSwitcher.tempVar.http.transfer.speed.download.average"),
		obs_module_text(
			"AdvSceneSwitcher.tempVar.http.transfer.speed.download.average.description"));
	AddTempvar(
		"transfer.speed.upload.average",
		obs_module_text(
			"AdvSceneSwitcher.tempVar.http.transfer.speed.upload.average"),
		obs_module_text(
			"AdvSceneSwitcher.tempVar.http.transfer.speed.upload.average.description"));
	AddTempvar(
		"transfer.time.total",
		obs_module_text(
			"AdvSceneSwitcher.tempVar.http.transfer.time.total"),
		obs_module_text(
			"AdvSceneSwitcher.tempVar.http.transfer.time.total.description"));
}

void MacroActionHttp::SetTempVarValues()
{
	SetTempVarValue("response.body", _responseBody);
	SetTempVarValue("response.headers", _responseHeaders);

	long redirectCount;
	CurlHelper::GetInfo(CURLINFO_REDIRECT_COUNT, &redirectCount);
	SetTempVarValue("redirects.count", std::to_string(redirectCount));

	char *responseContentTypeHeader;
	CurlHelper::GetInfo(CURLINFO_CONTENT_TYPE, &responseContentTypeHeader);
	SetTempVarValue("response.header.contentType",
			responseContentTypeHeader);

	curl_off_t responseRetryAfterHeader;
	CurlHelper::GetInfo(CURLINFO_RETRY_AFTER, &responseRetryAfterHeader);
	SetTempVarValue("response.header.retryAfter",
			std::to_string(responseRetryAfterHeader));

	long responseCode;
	CurlHelper::GetInfo(CURLINFO_RESPONSE_CODE, &responseCode);
	SetTempVarValue("response.statusCode", std::to_string(responseCode));

	curl_off_t averageDownloadSpeed;
	CurlHelper::GetInfo(CURLINFO_SPEED_DOWNLOAD_T, &averageDownloadSpeed);
	SetTempVarValue("transfer.speed.download.average",
			std::to_string(averageDownloadSpeed));

	curl_off_t averageUploadSpeed;
	CurlHelper::GetInfo(CURLINFO_SPEED_UPLOAD_T, &averageUploadSpeed);
	SetTempVarValue("transfer.speed.upload.average",
			std::to_string(averageUploadSpeed));

	double totalTime;
	CurlHelper::GetInfo(CURLINFO_TOTAL_TIME, &totalTime);
	SetTempVarValue("transfer.time.total", std::to_string(totalTime));
}

void MacroActionHttp::LogAction() const
{
	auto it = methods.find(_method);
	if (it != methods.end()) {
		vblog(LOG_INFO,
		      "Sent http request \"%s\" to \"%s\" with data \"%s\"",
		      it->second.c_str(), _url.c_str(), _data.c_str());
	} else {
		blog(LOG_WARNING, "Ignored unknown http action %d",
		     static_cast<int>(_method));
	}
}

bool MacroActionHttp::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	obs_data_set_int(obj, "method", static_cast<int>(_method));
	_url.Save(obj, "url");
	_data.Save(obj, "data");
	obs_data_set_bool(obj, "setHeaders", _setHeaders);
	_headers.Save(obj, "headers", "header");
	_timeout.Save(obj);
	_retryCount.Save(obj, "retryCount");
	_retryDelay.Save(obj);
	obs_data_set_bool(obj, "errorForBadStatusCode", _errorForBadStatusCode);

	return true;
}

bool MacroActionHttp::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_method = static_cast<Method>(obs_data_get_int(obj, "method"));
	_url.Load(obj, "url");
	_data.Load(obj, "data");
	_setHeaders = obs_data_get_bool(obj, "setHeaders");
	_headers.Load(obj, "headers", "header");
	_timeout.Load(obj);
	_retryCount.Load(obj, "retryCount");
	_retryDelay.Load(obj);
	_errorForBadStatusCode =
		obs_data_get_bool(obj, "errorForBadStatusCode");

	return true;
}

std::string MacroActionHttp::GetShortDesc() const
{
	return _url.UnresolvedValue();
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
	  _methods(new QComboBox()),
	  _url(new VariableLineEdit(this)),
	  _data(new VariableTextEdit(this)),
	  _setHeaders(new QCheckBox(
		  obs_module_text("AdvSceneSwitcher.action.http.setHeaders"))),
	  _headerListLayout(new QVBoxLayout()),
	  _headerList(new StringListEdit(
		  this, obs_module_text("AdvSceneSwitcher.action.http.headers"),
		  obs_module_text("AdvSceneSwitcher.action.http.addHeader"))),
	  _timeout(new DurationSelection(this, false)),
	  _retryCount(new VariableSpinBox(this)),
	  _retryDelay(new DurationSelection(this, false)),
	  _errorForBadStatusCode(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.action.http.errorForBadStatusCode.label")))
{
	populateMethodSelection(_methods);
	_headerList->SetMaxStringSize(4096);
	_retryCount->setMinimum(0);

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
	QWidget::connect(
		_retryCount,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(RetryCountChanged(const NumberVariable<int> &)));
	QWidget::connect(_retryDelay, SIGNAL(DurationChanged(const Duration &)),
			 this, SLOT(RetryDelayChanged(const Duration &)));
	QWidget::connect(_errorForBadStatusCode, SIGNAL(stateChanged(int)),
			 this, SLOT(ErrorForBadStatusCodeChanged(int)));

	auto *actionLayout = new QHBoxLayout;
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.action.http.entry.line1"),
		actionLayout,
		{
			{"{{url}}", _url},
			{"{{method}}", _methods},
		});
	auto *timeoutLayout = new QHBoxLayout;
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.action.http.entry.line2"),
		timeoutLayout,
		{
			{"{{timeout}}", _timeout},
		});
	auto *retryLayout = new QHBoxLayout;
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.action.http.entry.line3"),
		retryLayout,
		{
			{"{{count}}", _retryCount},
			{"{{delay}}", _retryDelay},
		});

	_headerListLayout->addWidget(new QLabel(
		obs_module_text("AdvSceneSwitcher.action.http.headers")));
	_headerListLayout->addWidget(_headerList);

	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(actionLayout);
	mainLayout->addWidget(_setHeaders);
	mainLayout->addLayout(_headerListLayout);
	mainLayout->addWidget(_data);
	mainLayout->addLayout(timeoutLayout);
	mainLayout->addLayout(retryLayout);
	mainLayout->addWidget(_errorForBadStatusCode);
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
	_retryCount->SetValue(_entryData->_retryCount);
	_retryDelay->SetDuration(_entryData->_retryDelay);
	_errorForBadStatusCode->setChecked(_entryData->_errorForBadStatusCode);

	SetWidgetVisibility();
}

void MacroActionHttpEdit::URLChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_url = _url->text().toStdString();
	emit(HeaderInfoChanged(_url->text()));
}

void MacroActionHttpEdit::MethodChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_method = static_cast<MacroActionHttp::Method>(value);
	SetWidgetVisibility();
}

void MacroActionHttpEdit::TimeoutChanged(const Duration &dur)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_timeout = dur;
}

void MacroActionHttpEdit::SetHeadersChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_setHeaders = value;
	SetWidgetVisibility();
}

void MacroActionHttpEdit::HeadersChanged(const StringList &headers)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_headers = headers;
	adjustSize();
	updateGeometry();
}

void MacroActionHttpEdit::RetryCountChanged(const NumberVariable<int> &number)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_retryCount = number;
}

void MacroActionHttpEdit::RetryDelayChanged(const Duration &duration)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_retryDelay = duration;
}

void MacroActionHttpEdit::ErrorForBadStatusCodeChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_errorForBadStatusCode = state;
}

void MacroActionHttpEdit::SetWidgetVisibility()
{
	_data->setVisible(_entryData->_method != MacroActionHttp::Method::GET &&
			  _entryData->_method != MacroActionHttp::Method::HEAD);
	SetLayoutVisible(_headerListLayout, _entryData->_setHeaders);

	adjustSize();
	updateGeometry();
}

void MacroActionHttpEdit::DataChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_data = _data->toPlainText().toUtf8().constData();

	adjustSize();
	updateGeometry();
}

} // namespace advss
