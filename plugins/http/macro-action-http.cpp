#include "macro-action-http.hpp"
#include "layout-helpers.hpp"

#include <httplib.h>

#undef DELETE

namespace advss {

const std::string MacroActionHttp::id = "http_v2";

bool MacroActionHttp::_registered = MacroActionFactory::Register(
	MacroActionHttp::id,
	{MacroActionHttp::Create, MacroActionHttpEdit::Create,
	 "AdvSceneSwitcher.action.http"});

static httplib::Headers getHeaders(const StringList &strings)
{
	httplib::Headers headers;
	for (int i = 0; i < strings.size(); i = i + 2) {
		const auto pair =
			i + 1 >= strings.size()
				? std::make_pair(std::string(strings.at(i)), "")
				: std::make_pair(
					  std::string(strings.at(i)),
					  std::string(strings.at(i + 1)));
		headers.emplace(pair);
	}
	return headers;
}

static httplib::Params getParams(const StringList &strings)
{
	httplib::Params params;
	for (int i = 0; i < strings.size(); i = i + 2) {
		const auto pair =
			i + 1 >= strings.size()
				? std::make_pair(std::string(strings.at(i)), "")
				: std::make_pair(
					  std::string(strings.at(i)),
					  std::string(strings.at(i + 1)));
		params.emplace(pair);
	}
	return params;
}

static void setTimeout(httplib::Client &client, const Duration &timeout)
{
	const time_t seconds = timeout.Seconds();
	const time_t usecs = timeout.Milliseconds() * 1000;
	client.set_read_timeout(seconds, usecs);
	client.set_write_timeout(seconds, usecs);
}

void MacroActionHttp::SetupTempVars()
{
	MacroAction::SetupTempVars();
	AddTempvar("status",
		   obs_module_text("AdvSceneSwitcher.tempVar.http.status"));
	AddTempvar("body",
		   obs_module_text("AdvSceneSwitcher.tempVar.http.body"));
	AddTempvar("error",
		   obs_module_text("AdvSceneSwitcher.tempVar.http.error"),
		   obs_module_text(
			   "AdvSceneSwitcher.tempVar.http.error.description"));
}

bool MacroActionHttp::PerformAction()
{
	httplib::Client cli(_url);
	setTimeout(cli, _timeout);
	const auto params = _setParams ? getParams(_params) : httplib::Params();
	const auto headers = _setHeaders ? getHeaders(_headers)
					 : httplib::Headers();

	httplib::Result response;
	switch (_method) {
	case MacroActionHttp::Method::GET:
		response = cli.Get(_path, params, headers);
		break;
	case MacroActionHttp::Method::POST: {
		const auto path = httplib::append_query_params(_path, params);
		response = cli.Post(path, headers, _body, _contentType);
		break;
	}
	case MacroActionHttp::Method::PUT: {
		const auto path = httplib::append_query_params(_path, params);
		response = cli.Put(path, headers, _body, _contentType);
		break;
	}
	case MacroActionHttp::Method::PATCH: {
		const auto path = httplib::append_query_params(_path, params);
		response = cli.Patch(path, headers, _body, _contentType);
		break;
	}
	case MacroActionHttp::Method::DELETE: {
		const auto path = httplib::append_query_params(_path, params);
		response = cli.Delete(path, headers, _body, _contentType);
		break;
	}
	default:
		break;
	}

	if (VerboseLoggingEnabled() && !response) {
		blog(LOG_INFO, "HTTP action error: %s",
		     httplib::to_string(response.error()).c_str());
	}

	SetTempVarValue("status",
			response ? std::to_string(response->status) : "");
	SetTempVarValue("body", response ? response->body : "");
	SetTempVarValue("error",
			response ? "" : httplib::to_string(response.error()));

	return true;
}

static constexpr std::string_view methodToString(MacroActionHttp::Method method)
{
	switch (method) {
	case MacroActionHttp::Method::GET:
		return "GET";
	case MacroActionHttp::Method::POST:
		return "POST";
	case MacroActionHttp::Method::PUT:
		return "PUT";
	case MacroActionHttp::Method::PATCH:
		return "PATCH";
	case MacroActionHttp::Method::DELETE:
		return "DELETE";
	default:
		break;
	}
	return "unknown";
}

static std::string stringListToString(const StringList &list)
{
	if (list.empty()) {
		return "[]";
	}
	std::string result = "[";
	for (const auto &string : list) {
		result += std::string(string) + ", ";
	}
	result.pop_back();
	result.pop_back();
	return result + "]";
}

void MacroActionHttp::LogAction() const
{
	ablog(LOG_INFO,
	      "sent HTTP request (%s) "
	      "to URL \"%s\" "
	      "to path \"%s\" "
	      "with content type \"%s\" "
	      "with body \"%s\" "
	      "with headers \"%s\" "
	      "with parameters \"%s\" "
	      "with timeout \"%s\"",
	      methodToString(_method).data(), _url.c_str(), _path.c_str(),
	      _contentType.c_str(), _body.c_str(),
	      _setHeaders ? stringListToString(_headers).c_str() : "-",
	      _setParams ? stringListToString(_params).c_str() : "-",
	      _timeout.ToString().c_str());
}

bool MacroActionHttp::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	_url.Save(obj, "url");
	_path.Save(obj, "path");
	_contentType.Save(obj, "contentType");
	_body.Save(obj, "body");
	obs_data_set_bool(obj, "setHeaders", _setHeaders);
	_headers.Save(obj, "headers", "header");
	obs_data_set_bool(obj, "setParams", _setParams);
	_params.Save(obj, "params", "param");
	obs_data_set_int(obj, "method", static_cast<int>(_method));
	_timeout.Save(obj);
	return true;
}

bool MacroActionHttp::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_url.Load(obj, "url");
	_path.Load(obj, "path");
	_contentType.Load(obj, "contentType");
	_body.Load(obj, "body");
	_setHeaders = obs_data_get_bool(obj, "setHeaders");
	_headers.Load(obj, "headers", "header");
	_setParams = obs_data_get_bool(obj, "setParams");
	_params.Load(obj, "params", "param");
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
	_path.ResolveVariables();
	_contentType.ResolveVariables();
	_body.ResolveVariables();
	_headers.ResolveVariables();
	_params.ResolveVariables();
	_timeout.ResolveVariables();
}

static inline void populateMethodSelection(QComboBox *list)
{
	const static std::map<MacroActionHttp::Method, std::string> methods = {
		{MacroActionHttp::Method::GET,
		 "AdvSceneSwitcher.action.http.type.get"},
		{MacroActionHttp::Method::POST,
		 "AdvSceneSwitcher.action.http.type.post"},
		{MacroActionHttp::Method::PUT,
		 "AdvSceneSwitcher.action.http.type.put"},
		{MacroActionHttp::Method::PATCH,
		 "AdvSceneSwitcher.action.http.type.patch"},
		{MacroActionHttp::Method::DELETE,
		 "AdvSceneSwitcher.action.http.type.delete"},
	};

	for (const auto &[value, name] : methods) {
		list->addItem(obs_module_text(name.c_str()),
			      static_cast<int>(value));
	}
}

MacroActionHttpEdit::MacroActionHttpEdit(
	QWidget *parent, std::shared_ptr<MacroActionHttp> entryData)
	: QWidget(parent),
	  _url(new VariableLineEdit(this)),
	  _path(new VariableLineEdit(this)),
	  _contentType(new VariableLineEdit(this)),
	  _contentTypeLayout(new QHBoxLayout()),
	  _methods(new QComboBox()),
	  _body(new VariableTextEdit(this)),
	  _bodyLayout(new QVBoxLayout()),
	  _setHeaders(new QCheckBox(
		  obs_module_text("AdvSceneSwitcher.action.http.setHeaders"))),
	  _headerList(new KeyValueListEdit(
		  this, obs_module_text("AdvSceneSwitcher.action.http.headers"),
		  obs_module_text("AdvSceneSwitcher.action.http.addHeader.name"),
		  obs_module_text("AdvSceneSwitcher.action.http.headers"),
		  obs_module_text(
			  "AdvSceneSwitcher.action.http.addHeader.value"))),
	  _headerListLayout(new QVBoxLayout()),
	  _setParams(new QCheckBox(
		  obs_module_text("AdvSceneSwitcher.action.http.setParams"))),
	  _paramList(new KeyValueListEdit(
		  this, obs_module_text("AdvSceneSwitcher.action.http.params"),
		  obs_module_text("AdvSceneSwitcher.action.http.addParam.name"),
		  obs_module_text("AdvSceneSwitcher.action.http.params"),
		  obs_module_text(
			  "AdvSceneSwitcher.action.http.addParam.value"))),
	  _paramListLayout(new QVBoxLayout()),
	  _timeout(new DurationSelection(this, false))
{
	populateMethodSelection(_methods);

	SetWidgetSignalConnections();
	SetWidgetLayout();

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
	_path->setText(_entryData->_path);
	_contentType->setText(_entryData->_contentType);
	_body->setPlainText(_entryData->_body);
	_setHeaders->setChecked(_entryData->_setHeaders);
	_headerList->SetStringList(_entryData->_headers);
	_setParams->setChecked(_entryData->_setParams);
	_paramList->SetStringList(_entryData->_params);
	_methods->setCurrentIndex(
		_methods->findData(static_cast<int>(_entryData->_method)));
	_timeout->SetDuration(_entryData->_timeout);
	SetWidgetVisibility();
}

void MacroActionHttpEdit::URLChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_url = _url->text().toStdString();
	emit(HeaderInfoChanged(_url->text()));
}

void MacroActionHttpEdit::PathChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_path = _path->text().toStdString();
}

void MacroActionHttpEdit::ContentTypeChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_contentType = _contentType->text().toStdString();
}

void MacroActionHttpEdit::BodyChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_body = _body->toPlainText().toUtf8().constData();

	adjustSize();
	updateGeometry();
}

void MacroActionHttpEdit::MethodChanged(int idx)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_method = static_cast<MacroActionHttp::Method>(
		_methods->itemData(idx).toInt());
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

void MacroActionHttpEdit::SetParamsChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_setParams = value;
	SetWidgetVisibility();
}

void MacroActionHttpEdit::ParamsChanged(const StringList &params)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_params = params;
	adjustSize();
	updateGeometry();
}

void MacroActionHttpEdit::SetWidgetSignalConnections()
{
	QWidget::connect(_url, SIGNAL(editingFinished()), this,
			 SLOT(URLChanged()));
	QWidget::connect(_path, SIGNAL(editingFinished()), this,
			 SLOT(PathChanged()));
	QWidget::connect(_contentType, SIGNAL(editingFinished()), this,
			 SLOT(ContentTypeChanged()));
	QWidget::connect(_body, SIGNAL(textChanged()), this,
			 SLOT(BodyChanged()));
	QWidget::connect(_methods, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(MethodChanged(int)));
	QWidget::connect(_setHeaders, SIGNAL(stateChanged(int)), this,
			 SLOT(SetHeadersChanged(int)));
	QWidget::connect(_headerList,
			 SIGNAL(StringListChanged(const StringList &)), this,
			 SLOT(HeadersChanged(const StringList &)));
	QWidget::connect(_setParams, SIGNAL(stateChanged(int)), this,
			 SLOT(SetParamsChanged(int)));
	QWidget::connect(_paramList,
			 SIGNAL(StringListChanged(const StringList &)), this,
			 SLOT(ParamsChanged(const StringList &)));
	QWidget::connect(_timeout, SIGNAL(DurationChanged(const Duration &)),
			 this, SLOT(TimeoutChanged(const Duration &)));
}

void MacroActionHttpEdit::SetWidgetLayout()
{
	const std::unordered_map<std::string, QWidget *> widgets = {
		{"{{url}}", _url},
		{"{{path}}", _path},
		{"{{contentType}}", _contentType},
		{"{{method}}", _methods},
		{"{{body}}", _body},
		{"{{timeout}}", _timeout},
	};

	auto actionLayout = new QHBoxLayout;
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.action.http.layout.method"),
		actionLayout, widgets);

	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.action.http.layout.contentType"),
		     _contentTypeLayout, widgets);

	_bodyLayout->addWidget(new QLabel(
		obs_module_text("AdvSceneSwitcher.action.http.body")));
	_bodyLayout->addWidget(_body);

	auto timeoutLayout = new QHBoxLayout;
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.action.http.layout.timeout"),
		timeoutLayout, widgets);

	_headerListLayout->addWidget(new QLabel(
		obs_module_text("AdvSceneSwitcher.action.http.headers")));
	_headerListLayout->addWidget(_headerList);

	_paramListLayout->addWidget(new QLabel(
		obs_module_text("AdvSceneSwitcher.action.http.params")));
	_paramListLayout->addWidget(_paramList);

	auto layout = new QVBoxLayout;
	layout->addLayout(actionLayout);
	layout->addWidget(_setHeaders);
	layout->addLayout(_headerListLayout);
	layout->addWidget(_setParams);
	layout->addLayout(_paramListLayout);
	layout->addLayout(_contentTypeLayout);
	layout->addLayout(_bodyLayout);
	layout->addLayout(timeoutLayout);
	setLayout(layout);
}

void MacroActionHttpEdit::SetWidgetVisibility()
{
	SetLayoutVisible(_headerListLayout, _entryData->_setHeaders);
	SetLayoutVisible(_paramListLayout, _entryData->_setParams);
	SetLayoutVisible(_contentTypeLayout,
			 _entryData->_method != MacroActionHttp::Method::GET);
	SetLayoutVisible(_bodyLayout,
			 _entryData->_method != MacroActionHttp::Method::GET);

	adjustSize();
	updateGeometry();
}

} // namespace advss
