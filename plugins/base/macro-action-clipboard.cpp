#include "macro-action-clipboard.hpp"
#include "curl-helper.hpp"

#include <obs.hpp>
#include <QApplication>
#include <QClipboard>
#include <QImage>
#include <QMimeData>

namespace advss {

const std::string MacroActionClipboard::id = "clipboard";

bool MacroActionClipboard::_registered = MacroActionFactory::Register(
	MacroActionClipboard::id,
	{MacroActionClipboard::Create, MacroActionClipboardEdit::Create,
	 "AdvSceneSwitcher.action.clipboard"});

const static std::map<MacroActionClipboard::Action, std::string> actionTypes = {
	{MacroActionClipboard::Action::COPY_TEXT,
	 "AdvSceneSwitcher.action.clipboard.type.copy.text"},
	{MacroActionClipboard::Action::COPY_IMAGE,
	 "AdvSceneSwitcher.action.clipboard.type.copy.image"},
};

static size_t writeCallback(void *ptr, size_t size, size_t nmemb,
			    QByteArray *buffer)
{
	buffer->append((char *)ptr, nmemb);
	return size * nmemb;
}

static std::optional<QImage> getImageFromUrl(const char *url)
{
	QByteArray response;

	CurlHelper::SetOpt(CURLOPT_URL, url);
	CurlHelper::SetOpt(CURLOPT_HTTPGET, 1L);
	CurlHelper::SetOpt(CURLOPT_TIMEOUT_MS, 30000);
	CurlHelper::SetOpt(CURLOPT_WRITEFUNCTION, writeCallback);
	CurlHelper::SetOpt(CURLOPT_WRITEDATA, &response);
	auto code = CurlHelper::Perform();

	if (code != CURLE_OK) {
		blog(LOG_WARNING,
		     "Retrieving image failed in %s with error: %s", __func__,
		     CurlHelper::GetError(code));
		return {};
	}

	return QImage::fromData(response);
}

static void setMimeTypeParams(ClipboardQueueParams *params,
			      QClipboard *clipboard)
{
	auto mimeData = clipboard->mimeData();
	if (!mimeData) {
		return;
	}
	auto mimeTypeList = mimeData->formats();
	if (mimeTypeList.empty()) {
		return;
	}
	params->mimeTypePrimary = mimeTypeList.first().toStdString();
	params->mimeTypeAll = mimeTypeList.join(" ").toStdString();
}

static void copyText(void *param)
{
	auto params = static_cast<ClipboardTextQueueParams *>(param);
	QClipboard *clipboard = QApplication::clipboard();

	clipboard->setText(params->text.c_str());

	setMimeTypeParams(params, clipboard);
}

static void copyImageFromUrl(void *param)
{
	auto params = static_cast<ClipboardImageQueueParams *>(param);
	auto url = params->url.c_str();
	auto image = getImageFromUrl(url);

	if (!image || (*image).isNull()) {
		blog(LOG_WARNING, "Failed to convert %s URL to image!", url);
		return;
	}

	QClipboard *clipboard = QApplication::clipboard();
	clipboard->setImage(*image);

	setMimeTypeParams(params, clipboard);
}

std::shared_ptr<MacroAction> MacroActionClipboard::Create(Macro *m)
{
	return std::make_shared<MacroActionClipboard>(m);
}

std::shared_ptr<MacroAction> MacroActionClipboard::Copy() const
{
	return std::make_shared<MacroActionClipboard>(*this);
}

bool MacroActionClipboard::PerformAction()
{
	switch (_action) {
	case Action::COPY_TEXT: {
		ClipboardTextQueueParams params{"", "", _text};
		obs_queue_task(OBS_TASK_UI, copyText, &params, true);
		SetTempVarValues(params);
		break;
	}
	case Action::COPY_IMAGE: {
		ClipboardImageQueueParams params{"", "", _url};
		obs_queue_task(OBS_TASK_UI, copyImageFromUrl, &params, true);
		SetTempVarValues(params);
		break;
	}
	default:
		break;
	}

	return true;
}

void MacroActionClipboard::SetupTempVars()
{
	MacroAction::SetupTempVars();

	AddTempvar(
		"mimeType.primary",
		obs_module_text(
			"AdvSceneSwitcher.tempVar.clipboard.mimeType.primary"),
		obs_module_text(
			"AdvSceneSwitcher.tempVar.clipboard.mimeType.primary.description"));
	AddTempvar(
		"mimeType.all",
		obs_module_text(
			"AdvSceneSwitcher.tempVar.clipboard.mimeType.all"),
		obs_module_text(
			"AdvSceneSwitcher.tempVar.clipboard.mimeType.all.description"));
}

void MacroActionClipboard::SetTempVarValues(const ClipboardQueueParams &params)
{
	SetTempVarValue("mimeType.primary", params.mimeTypePrimary);
	SetTempVarValue("mimeType.all", params.mimeTypeAll);
}

bool MacroActionClipboard::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	_text.Save(obj, "text");
	_url.Save(obj, "url");

	return true;
}

bool MacroActionClipboard::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_action = static_cast<Action>(obs_data_get_int(obj, "action"));
	_text.Load(obj, "text");
	_url.Load(obj, "url");

	return true;
}

void MacroActionClipboard::ResolveVariablesToFixedValues()
{
	_text.ResolveVariables();
	_url.ResolveVariables();
}

static inline void populateActionSelection(QComboBox *list)
{
	for (const auto &[action, name] : actionTypes) {
		list->addItem(obs_module_text(name.c_str()),
			      static_cast<int>(action));
	}
}

MacroActionClipboardEdit::MacroActionClipboardEdit(
	QWidget *parent, std::shared_ptr<MacroActionClipboard> entryData)
	: QWidget(parent),
	  _actions(new FilterComboBox(this)),
	  _text(new VariableLineEdit(this)),
	  _url(new VariableLineEdit(this))
{
	populateActionSelection(_actions);
	_url->setToolTip(obs_module_text(
		"AdvSceneSwitcher.action.clipboard.copy.image.url.tooltip"));

	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_text, SIGNAL(editingFinished()), this,
			 SLOT(TextChanged()));
	QWidget::connect(_url, SIGNAL(editingFinished()), this,
			 SLOT(UrlChanged()));

	auto mainLayout = new QHBoxLayout();
	mainLayout->addWidget(_actions);
	mainLayout->addWidget(_text);
	mainLayout->addWidget(_url);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionClipboardEdit::ActionChanged(int index)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_action = static_cast<MacroActionClipboard::Action>(
		_actions->itemData(index).toInt());

	SetWidgetVisibility();
}

void MacroActionClipboardEdit::TextChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_text = _text->text().toStdString();
}

void MacroActionClipboardEdit::UrlChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_url = _url->text().toStdString();
}

void MacroActionClipboardEdit::SetWidgetVisibility()
{
	_text->setVisible(_entryData->_action ==
			  MacroActionClipboard::Action::COPY_TEXT);
	_url->setVisible(_entryData->_action ==
			 MacroActionClipboard::Action::COPY_IMAGE);

	adjustSize();
	updateGeometry();
}

void MacroActionClipboardEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_actions->setCurrentIndex(
		_actions->findData(static_cast<int>(_entryData->_action)));
	_text->setText(_entryData->_text);
	_url->setText(_entryData->_url);

	SetWidgetVisibility();
}

} // namespace advss
