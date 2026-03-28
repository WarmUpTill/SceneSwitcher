#include "macro-condition-http.hpp"
#include "layout-helpers.hpp"
#include "macro-helpers.hpp"

#undef DELETE

namespace advss {

const std::string MacroConditionHttp::id = "http";

bool MacroConditionHttp::_registered = MacroConditionFactory::Register(
	MacroConditionHttp::id,
	{MacroConditionHttp::Create, MacroConditionHttpEdit::Create,
	 "AdvSceneSwitcher.condition.http"});

static const std::map<MacroConditionHttp::Method, std::string> methodLabels = {
	{MacroConditionHttp::Method::ANY,
	 "AdvSceneSwitcher.condition.http.method.any"},
	{MacroConditionHttp::Method::GET,
	 "AdvSceneSwitcher.condition.http.method.get"},
	{MacroConditionHttp::Method::POST,
	 "AdvSceneSwitcher.condition.http.method.post"},
	{MacroConditionHttp::Method::PUT,
	 "AdvSceneSwitcher.condition.http.method.put"},
	{MacroConditionHttp::Method::PATCH,
	 "AdvSceneSwitcher.condition.http.method.patch"},
	{MacroConditionHttp::Method::DELETE,
	 "AdvSceneSwitcher.condition.http.method.delete"},
};

static std::string_view methodToString(MacroConditionHttp::Method method)
{
	switch (method) {
	case MacroConditionHttp::Method::GET:
		return "GET";
	case MacroConditionHttp::Method::POST:
		return "POST";
	case MacroConditionHttp::Method::PUT:
		return "PUT";
	case MacroConditionHttp::Method::PATCH:
		return "PATCH";
	case MacroConditionHttp::Method::DELETE:
		return "DELETE";
	default:
		break;
	}
	return "";
}

void MacroConditionHttp::SetServer(const std::string &name)
{
	_server = GetWeakHttpServerByName(name);
	auto server = _server.lock();
	if (!server) {
		_requestBuffer.reset();
		return;
	}
	_requestBuffer = server->RegisterForRequests();
}

bool MacroConditionHttp::CheckCondition()
{
	if (!_requestBuffer) {
		return false;
	}

	const bool macroWasPausedSinceLastCheck =
		MacroWasPausedSince(GetMacro(), _lastCheck);
	_lastCheck = std::chrono::high_resolution_clock::now();
	if (macroWasPausedSinceLastCheck) {
		_requestBuffer->Clear();
		return false;
	}

	while (!_requestBuffer->Empty()) {
		auto request = _requestBuffer->ConsumeMessage();
		if (!request) {
			continue;
		}

		SetTempVarValue("method", request->method);
		SetTempVarValue("path", request->path);
		SetTempVarValue("body", request->body);

		if (_method != Method::ANY &&
		    request->method != methodToString(_method)) {
			continue;
		}

		const std::string pathPattern = std::string(_path);
		if (_pathRegex.Enabled()) {
			if (!_pathRegex.Matches(request->path, _path)) {
				continue;
			}
		} else {
			if (request->path != pathPattern) {
				continue;
			}
		}

		const std::string bodyPattern = std::string(_body);
		if (_bodyRegex.Enabled()) {
			if (!_bodyRegex.Matches(request->body, _body)) {
				continue;
			}
		} else {
			if (request->body != bodyPattern) {
				continue;
			}
		}

		if (_clearBufferOnMatch) {
			_requestBuffer->Clear();
		}
		return true;
	}

	return false;
}

bool MacroConditionHttp::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	obs_data_set_int(obj, "method", static_cast<int>(_method));
	_path.Save(obj, "path");
	_pathRegex.Save(obj, "pathRegex");
	_body.Save(obj, "body");
	_bodyRegex.Save(obj, "bodyRegex");
	obs_data_set_bool(obj, "clearBufferOnMatch", _clearBufferOnMatch);
	auto server = _server.lock();
	obs_data_set_string(obj, "server",
			    server ? server->Name().c_str() : "");
	return true;
}

bool MacroConditionHttp::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_method = static_cast<Method>(obs_data_get_int(obj, "method"));
	_path.Load(obj, "path");
	_pathRegex.Load(obj, "pathRegex");
	_body.Load(obj, "body");
	_bodyRegex.Load(obj, "bodyRegex");
	_clearBufferOnMatch = obs_data_get_bool(obj, "clearBufferOnMatch");
	if (!obs_data_has_user_value(obj, "clearBufferOnMatch")) {
		_clearBufferOnMatch = true;
	}
	SetServer(obs_data_get_string(obj, "server"));
	return true;
}

std::string MacroConditionHttp::GetShortDesc() const
{
	auto server = _server.lock();
	return server ? server->Name() : "";
}

void MacroConditionHttp::SetupTempVars()
{
	MacroCondition::SetupTempVars();
	AddTempvar("method",
		   obs_module_text("AdvSceneSwitcher.tempVar.http.method"),
		   obs_module_text(
			   "AdvSceneSwitcher.tempVar.http.method.description"));
	AddTempvar("path",
		   obs_module_text("AdvSceneSwitcher.tempVar.http.path"),
		   obs_module_text(
			   "AdvSceneSwitcher.tempVar.http.path.description"));
	AddTempvar("body",
		   obs_module_text("AdvSceneSwitcher.tempVar.http.body"),
		   obs_module_text(
			   "AdvSceneSwitcher.tempVar.http.body.description"));
}

static void populateMethodSelection(QComboBox *list)
{
	for (const auto &[value, name] : methodLabels) {
		list->addItem(obs_module_text(name.c_str()),
			      static_cast<int>(value));
	}
}

MacroConditionHttpEdit::MacroConditionHttpEdit(
	QWidget *parent, std::shared_ptr<MacroConditionHttp> entryData)
	: QWidget(parent),
	  _method(new QComboBox(this)),
	  _server(new HttpServerSelection(this)),
	  _path(new VariableLineEdit(this)),
	  _pathRegex(new RegexConfigWidget(this)),
	  _body(new VariableTextEdit(this)),
	  _bodyRegex(new RegexConfigWidget(this)),
	  _clearBufferOnMatch(new QCheckBox(
		  obs_module_text("AdvSceneSwitcher.clearBufferOnMatch")))
{
	populateMethodSelection(_method);

	QWidget::connect(_method, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(MethodChanged(int)));
	QWidget::connect(_server, SIGNAL(SelectionChanged(const QString &)),
			 this, SLOT(ServerChanged(const QString &)));
	QWidget::connect(_path, SIGNAL(editingFinished()), this,
			 SLOT(PathChanged()));
	QWidget::connect(_pathRegex,
			 SIGNAL(RegexConfigChanged(const RegexConfig &)), this,
			 SLOT(PathRegexChanged(const RegexConfig &)));
	QWidget::connect(_body, SIGNAL(textChanged()), this,
			 SLOT(BodyChanged()));
	QWidget::connect(_bodyRegex,
			 SIGNAL(RegexConfigChanged(const RegexConfig &)), this,
			 SLOT(BodyRegexChanged(const RegexConfig &)));
	QWidget::connect(_clearBufferOnMatch, SIGNAL(stateChanged(int)), this,
			 SLOT(ClearBufferOnMatchChanged(int)));

	auto topLayout = new QHBoxLayout;
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.condition.http.layout"),
		     topLayout,
		     {{"{{method}}", _method}, {"{{server}}", _server}});

	auto pathLayout = new QHBoxLayout;
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.condition.http.layout.path"),
		pathLayout, {{"{{path}}", _path}, {"{{regex}}", _pathRegex}},
		false);

	auto bodyLayout = new QHBoxLayout;
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.condition.http.layout.body"),
		bodyLayout, {{"{{body}}", _body}, {"{{regex}}", _bodyRegex}},
		false);

	auto mainLayout = new QVBoxLayout;
	mainLayout->addLayout(topLayout);
	mainLayout->addLayout(pathLayout);
	mainLayout->addLayout(bodyLayout);
	mainLayout->addWidget(_clearBufferOnMatch);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionHttpEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_method->setCurrentIndex(
		_method->findData(static_cast<int>(_entryData->_method)));
	_server->SetServer(_entryData->GetServer());
	_path->setText(_entryData->_path);
	_pathRegex->SetRegexConfig(_entryData->_pathRegex);
	_body->setPlainText(_entryData->_body);
	_bodyRegex->SetRegexConfig(_entryData->_bodyRegex);
	_clearBufferOnMatch->setChecked(_entryData->_clearBufferOnMatch);

	adjustSize();
	updateGeometry();
}

void MacroConditionHttpEdit::MethodChanged(int idx)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_method = static_cast<MacroConditionHttp::Method>(
		_method->itemData(idx).toInt());
}

void MacroConditionHttpEdit::ServerChanged(const QString &name)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->SetServer(name.toStdString());
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionHttpEdit::PathChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_path = _path->text().toStdString();
}

void MacroConditionHttpEdit::PathRegexChanged(const RegexConfig &conf)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_pathRegex = conf;
	adjustSize();
	updateGeometry();
}

void MacroConditionHttpEdit::BodyChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_body = _body->toPlainText().toUtf8().constData();
	adjustSize();
	updateGeometry();
}

void MacroConditionHttpEdit::BodyRegexChanged(const RegexConfig &conf)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_bodyRegex = conf;
	adjustSize();
	updateGeometry();
}

void MacroConditionHttpEdit::ClearBufferOnMatchChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_clearBufferOnMatch = value;
}

} // namespace advss
