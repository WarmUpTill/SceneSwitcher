#include "macro-action-screenshot.hpp"
#include "advanced-scene-switcher.hpp"
#include "utility.hpp"

const std::string MacroActionScreenshot::id = "screenshot";

bool MacroActionScreenshot::_registered = MacroActionFactory::Register(
	MacroActionScreenshot::id,
	{MacroActionScreenshot::Create, MacroActionScreenshotEdit::Create,
	 "AdvSceneSwitcher.action.screenshot"});

void MacroActionScreenshot::FrontendScreenshot()
{
	if (_source) {
		auto s = obs_weak_source_get_source(_source);
		obs_frontend_take_source_screenshot(s);
		obs_source_release(s);
	} else {
		obs_frontend_take_screenshot();
	}
}

void MacroActionScreenshot::CustomScreenshot()
{
	auto s = obs_weak_source_get_source(_source);
	_screenshot.~ScreenshotHelper();
	new (&_screenshot) ScreenshotHelper(s, false, 0, true, _path);
	obs_source_release(s);
}

bool MacroActionScreenshot::PerformAction()
{
	switch (_saveType) {
	case MacroActionScreenshot::SaveType::OBS_DEFAULT:
		FrontendScreenshot();
		break;
	case MacroActionScreenshot::SaveType::CUSTOM:
		CustomScreenshot();
		break;
	default:
		break;
	}

	return true;
}

void MacroActionScreenshot::LogAction()
{
	vblog(LOG_INFO, "trigger screenshot for source \"%s\"",
	      GetWeakSourceName(_source).c_str());
}

bool MacroActionScreenshot::Save(obs_data_t *obj)
{
	MacroAction::Save(obj);
	obs_data_set_string(obj, "source", GetWeakSourceName(_source).c_str());
	obs_data_set_int(obj, "saveType", static_cast<int>(_saveType));
	obs_data_set_string(obj, "savePath", _path.c_str());
	return true;
}

bool MacroActionScreenshot::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	const char *sourceName = obs_data_get_string(obj, "source");
	_source = GetWeakSourceByName(sourceName);
	_saveType = static_cast<SaveType>(obs_data_get_int(obj, "saveType"));
	_path = obs_data_get_string(obj, "savePath");
	return true;
}

std::string MacroActionScreenshot::GetShortDesc()
{
	if (_source) {
		return GetWeakSourceName(_source);
	}
	return "";
}

void addOBSMainOutputEntry(QComboBox *cb)
{
	cb->insertItem(
		0, obs_module_text(
			   "AdvSceneSwitcher.action.screenshot.mainOutput"));
}

void populateSaveTypeSelection(QComboBox *list)
{
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.action.screenshot.save.default"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.action.screenshot.save.custom"));
}

MacroActionScreenshotEdit::MacroActionScreenshotEdit(
	QWidget *parent, std::shared_ptr<MacroActionScreenshot> entryData)
	: QWidget(parent),
	  _sources(new QComboBox()),
	  _saveType(new QComboBox()),
	  _savePath(new FileSelection(FileSelection::Type::WRITE, this))
{
	populateVideoSelection(_sources, true, false);
	addOBSMainOutputEntry(_sources);
	populateSaveTypeSelection(_saveType);

	QWidget::connect(_sources, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(SourceChanged(const QString &)));
	QWidget::connect(_saveType, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(SaveTypeChanged(int)));
	QWidget::connect(_savePath, SIGNAL(PathChanged(const QString &)), this,
			 SLOT(PathChanged(const QString &)));

	QHBoxLayout *layout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{sources}}", _sources},
		{"{{saveType}}", _saveType},
	};
	placeWidgets(
		obs_module_text("AdvSceneSwitcher.action.screenshot.entry"),
		layout, widgetPlaceholders);

	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(layout);
	mainLayout->addWidget(_savePath);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionScreenshotEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	if (_entryData->_source) {
		_sources->setCurrentText(
			GetWeakSourceName(_entryData->_source).c_str());
	} else {
		_sources->setCurrentIndex(0);
	}
	_saveType->setCurrentIndex(static_cast<int>(_entryData->_saveType));
	_savePath->SetPath(QString::fromStdString(_entryData->_path));
	SetWidgetVisibility();
}

void MacroActionScreenshotEdit::SaveTypeChanged(int index)
{
	if (_loading || !_entryData) {
	}

	_entryData->_saveType =
		static_cast<MacroActionScreenshot::SaveType>(index);
	SetWidgetVisibility();
}

void MacroActionScreenshotEdit::PathChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_path = text.toUtf8().constData();
}

void MacroActionScreenshotEdit::SourceChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_source = GetWeakSourceByQString(text);
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionScreenshotEdit::SetWidgetVisibility()
{
	if (!_entryData) {
		return;
	}
	_savePath->setVisible(_entryData->_saveType ==
			      MacroActionScreenshot::SaveType::CUSTOM);
	adjustSize();
}
