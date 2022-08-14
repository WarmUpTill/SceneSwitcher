#include "macro-action-screenshot.hpp"
#include "advanced-scene-switcher.hpp"
#include "utility.hpp"

const std::string MacroActionScreenshot::id = "screenshot";

bool MacroActionScreenshot::_registered = MacroActionFactory::Register(
	MacroActionScreenshot::id,
	{MacroActionScreenshot::Create, MacroActionScreenshotEdit::Create,
	 "AdvSceneSwitcher.action.screenshot"});

bool MacroActionScreenshot::PerformAction()
{
	if (_source) {
		auto s = obs_weak_source_get_source(_source);
		obs_frontend_take_source_screenshot(s);
		obs_source_release(s);
	} else {
		obs_frontend_take_screenshot();
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
	return true;
}

bool MacroActionScreenshot::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	const char *sourceName = obs_data_get_string(obj, "source");
	_source = GetWeakSourceByName(sourceName);
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

MacroActionScreenshotEdit::MacroActionScreenshotEdit(
	QWidget *parent, std::shared_ptr<MacroActionScreenshot> entryData)
	: QWidget(parent)
{
	_sources = new QComboBox();
	populateVideoSelection(_sources, true, false);
	addOBSMainOutputEntry(_sources);

	QWidget::connect(_sources, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(SourceChanged(const QString &)));

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{sources}}", _sources},
	};
	placeWidgets(
		obs_module_text("AdvSceneSwitcher.action.screenshot.entry"),
		mainLayout, widgetPlaceholders);

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
