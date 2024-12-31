#include "macro-condition-screenshot.hpp"
#include "layout-helpers.hpp"

#include <obs-frontend-api.h>
#include <thread>

#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(29, 0, 0)

namespace advss {

const std::string MacroConditionScreenshot::id = "screenshot";

bool MacroConditionScreenshot::_registered = MacroConditionFactory::Register(
	MacroConditionScreenshot::id,
	{MacroConditionScreenshot::Create, MacroConditionScreenshotEdit::Create,
	 "AdvSceneSwitcher.condition.screenshot"});

static std::chrono::high_resolution_clock::time_point screenshotTakenTime{};
static bool setupScreenshotTakenEventHandler();
static bool setupDone = setupScreenshotTakenEventHandler();

static bool setupScreenshotTakenEventHandler()
{
	static auto handleScreenshotEvent = [](enum obs_frontend_event event,
					       void *) {
		switch (event) {
		case OBS_FRONTEND_EVENT_SCREENSHOT_TAKEN:
			screenshotTakenTime =
				std::chrono::high_resolution_clock::now();
			break;
		default:
			break;
		};
	};
	obs_frontend_add_event_callback(handleScreenshotEvent, nullptr);
	return true;
}

bool MacroConditionScreenshot::CheckCondition()
{
	if (!_screenshotTimeInitialized) {
		_screenshotTime = screenshotTakenTime;
		_screenshotTimeInitialized = true;
		return false;
	}

	auto lastScreenshotPath = obs_frontend_get_last_screenshot();
	SetTempVarValue("lastScreenshotPath", lastScreenshotPath);
	bfree(lastScreenshotPath);

	bool newSaveOccurred = _screenshotTime != screenshotTakenTime;
	_screenshotTime = screenshotTakenTime;
	return newSaveOccurred;
}

bool MacroConditionScreenshot::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	return true;
}

bool MacroConditionScreenshot::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	return true;
}

void MacroConditionScreenshot::SetupTempVars()
{
	MacroCondition::SetupTempVars();
	AddTempvar(
		"lastScreenshotPath",
		obs_module_text(
			"AdvSceneSwitcher.tempVar.screenshot.lastScreenshotPath"),
		obs_module_text(
			"AdvSceneSwitcher.tempVar.screenshot.lastScreenshotPath.description"));
}

MacroConditionScreenshotEdit::MacroConditionScreenshotEdit(
	QWidget *parent, std::shared_ptr<MacroConditionScreenshot> entryData)
	: QWidget(parent)
{
	auto layout = new QHBoxLayout;
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.condition.screenshot.entry"),
		layout, {});
	setLayout(layout);

	_entryData = entryData;
}

} // namespace advss

#endif
