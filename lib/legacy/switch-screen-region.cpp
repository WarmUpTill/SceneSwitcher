#include "advanced-scene-switcher.hpp"
#include "layout-helpers.hpp"
#include "selection-helpers.hpp"
#include "source-helpers.hpp"
#include "switcher-data.hpp"
#include "ui-helpers.hpp"
#include "utility.hpp"

#include <QTimer>

namespace advss {

bool ScreenRegionSwitch::pause = false;
static QObject *addPulse = nullptr;

void AdvSceneSwitcher::ClearFrames(QListWidget *list)
{
	for (int i = 0; i < list->count(); ++i) {
		ScreenRegionWidget *sw =
			(ScreenRegionWidget *)list->itemWidget(list->item(i));
		sw->hideFrame();
	}
}

void showCurrentFrame(QListWidget *list)
{
	QListWidgetItem *item = list->currentItem();

	if (!item) {
		return;
	}

	ScreenRegionWidget *sw = (ScreenRegionWidget *)list->itemWidget(item);
	sw->showFrame();
}

void AdvSceneSwitcher::SetShowFrames()
{
	ui->showFrame->setText(obs_module_text(
		"AdvSceneSwitcher.screenRegionTab.showGuideFrames"));
}

void AdvSceneSwitcher::SetHideFrames()
{
	ui->showFrame->setText(obs_module_text(
		"AdvSceneSwitcher.screenRegionTab.hideGuideFrames"));
}

void AdvSceneSwitcher::on_showFrame_clicked()
{
	switcher->showFrame = !switcher->showFrame;

	if (switcher->showFrame) {
		SetHideFrames();
		showCurrentFrame(ui->screenRegionSwitches);
	} else {
		SetShowFrames();
		ClearFrames(ui->screenRegionSwitches);
	}
}

void AdvSceneSwitcher::on_screenRegionSwitches_currentRowChanged(int idx)
{
	if (loading || idx == -1) {
		return;
	}

	if (switcher->showFrame) {
		ClearFrames(ui->screenRegionSwitches);
		showCurrentFrame(ui->screenRegionSwitches);
	}
}

void AdvSceneSwitcher::on_screenRegionAdd_clicked()
{
	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->screenRegionSwitches.emplace_back();

	listAddClicked(ui->screenRegionSwitches,
		       new ScreenRegionWidget(
			       this, &switcher->screenRegionSwitches.back()),
		       ui->screenRegionAdd, addPulse);

	ui->regionHelp->setVisible(false);
}

void AdvSceneSwitcher::on_screenRegionRemove_clicked()
{
	QListWidgetItem *item = ui->screenRegionSwitches->currentItem();
	if (!item) {
		return;
	}

	{
		std::lock_guard<std::mutex> lock(switcher->m);
		int idx = ui->screenRegionSwitches->currentRow();
		auto &switches = switcher->screenRegionSwitches;
		switches.erase(switches.begin() + idx);
	}

	delete item;
}

void AdvSceneSwitcher::on_screenRegionUp_clicked()
{
	int index = ui->screenRegionSwitches->currentRow();
	if (!listMoveUp(ui->screenRegionSwitches)) {
		return;
	}

	ScreenRegionWidget *s1 =
		(ScreenRegionWidget *)ui->screenRegionSwitches->itemWidget(
			ui->screenRegionSwitches->item(index));
	ScreenRegionWidget *s2 =
		(ScreenRegionWidget *)ui->screenRegionSwitches->itemWidget(
			ui->screenRegionSwitches->item(index - 1));
	ScreenRegionWidget::swapSwitchData(s1, s2);

	std::lock_guard<std::mutex> lock(switcher->m);

	std::swap(switcher->screenRegionSwitches[index],
		  switcher->screenRegionSwitches[index - 1]);
}

void AdvSceneSwitcher::on_screenRegionDown_clicked()
{
	int index = ui->screenRegionSwitches->currentRow();

	if (!listMoveDown(ui->screenRegionSwitches)) {
		return;
	}

	ScreenRegionWidget *s1 =
		(ScreenRegionWidget *)ui->screenRegionSwitches->itemWidget(
			ui->screenRegionSwitches->item(index));
	ScreenRegionWidget *s2 =
		(ScreenRegionWidget *)ui->screenRegionSwitches->itemWidget(
			ui->screenRegionSwitches->item(index + 1));
	ScreenRegionWidget::swapSwitchData(s1, s2);

	std::lock_guard<std::mutex> lock(switcher->m);

	std::swap(switcher->screenRegionSwitches[index],
		  switcher->screenRegionSwitches[index + 1]);
}

bool shouldIgnoreSceneSwitch(ScreenRegionSwitch &matchingRegion)
{
	if (!matchingRegion.excludeScene) {
		return false;
	}

	return matchingRegion.excludeScene == switcher->currentScene;
}

bool SwitcherData::checkScreenRegionSwitch(OBSWeakSource &scene,
					   OBSWeakSource &transition)
{
	if (ScreenRegionSwitch::pause) {
		return false;
	}

	std::pair<int, int> cursorPos = GetCursorPos();
	int minRegionSize = 99999;
	bool match = false;

	for (auto &s : screenRegionSwitches) {
		if (!s.initialized()) {
			continue;
		}

		if (cursorPos.first >= s.minX && cursorPos.second >= s.minY &&
		    cursorPos.first <= s.maxX && cursorPos.second <= s.maxY) {
			int regionSize = (s.maxX - s.minX) + (s.maxY - s.minY);
			if (regionSize < minRegionSize) {
				if (shouldIgnoreSceneSwitch(s)) {
					// We technically have a match.
					// But just ignore it.
					return false;
				}
				match = true;
				scene = s.getScene();
				transition = s.transition;
				minRegionSize = regionSize;

				if (VerboseLoggingEnabled()) {
					s.logMatch();
				}
				break;
			}
		}
	}
	return match;
}

void AdvSceneSwitcher::updateScreenRegionCursorPos()
{
	std::pair<int, int> position = GetCursorPos();
	ui->cursorXPosition->setText(QString::number(position.first));
	ui->cursorYPosition->setText(QString::number(position.second));
}

void SwitcherData::saveScreenRegionSwitches(obs_data_t *obj)
{
	obs_data_array_t *screenRegionArray = obs_data_array_create();
	for (ScreenRegionSwitch &s : screenRegionSwitches) {
		obs_data_t *array_obj = obs_data_create();

		s.save(array_obj);
		obs_data_array_push_back(screenRegionArray, array_obj);

		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "screenRegion", screenRegionArray);
	obs_data_array_release(screenRegionArray);
}

void SwitcherData::loadScreenRegionSwitches(obs_data_t *obj)
{
	screenRegionSwitches.clear();

	obs_data_array_t *screenRegionArray =
		obs_data_get_array(obj, "screenRegion");
	size_t count = obs_data_array_count(screenRegionArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj =
			obs_data_array_item(screenRegionArray, i);

		screenRegionSwitches.emplace_back();
		screenRegionSwitches.back().load(array_obj);

		obs_data_release(array_obj);
	}
	obs_data_array_release(screenRegionArray);
}

void AdvSceneSwitcher::SetupRegionTab()
{
	for (auto &s : switcher->screenRegionSwitches) {
		QListWidgetItem *item;
		item = new QListWidgetItem(ui->screenRegionSwitches);
		ui->screenRegionSwitches->addItem(item);
		ScreenRegionWidget *sw = new ScreenRegionWidget(this, &s);
		item->setSizeHint(sw->minimumSizeHint());
		ui->screenRegionSwitches->setItemWidget(item, sw);
	}

	if (switcher->screenRegionSwitches.size() == 0) {
		if (!switcher->disableHints) {
			addPulse = HighlightWidget(ui->screenRegionAdd,
						   QColor(Qt::green));
		}
		ui->regionHelp->setVisible(true);
	} else {
		ui->regionHelp->setVisible(false);
	}

	// screen region cursor position
	QTimer *screenRegionTimer = new QTimer(this);
	connect(screenRegionTimer, SIGNAL(timeout()), this,
		SLOT(updateScreenRegionCursorPos()));
	screenRegionTimer->start(1000);
}

void ScreenRegionSwitch::save(obs_data_t *obj)
{
	SceneSwitcherEntry::save(obj);

	obs_data_set_string(obj, "excludeScene",
			    GetWeakSourceName(excludeScene).c_str());

	obs_data_set_int(obj, "minX", minX);
	obs_data_set_int(obj, "minY", minY);
	obs_data_set_int(obj, "maxX", maxX);
	obs_data_set_int(obj, "maxY", maxY);
}

void ScreenRegionSwitch::load(obs_data_t *obj)
{
	SceneSwitcherEntry::load(obj);

	const char *excludeSceneName = obs_data_get_string(obj, "excludeScene");
	excludeScene = GetWeakSourceByName(excludeSceneName);

	minX = obs_data_get_int(obj, "minX");
	minY = obs_data_get_int(obj, "minY");
	maxX = obs_data_get_int(obj, "maxX");
	maxY = obs_data_get_int(obj, "maxY");
}

ScreenRegionWidget::ScreenRegionWidget(QWidget *parent, ScreenRegionSwitch *s)
	: SwitchWidget(parent, s, true, true)
{
	excludeScenes = new QComboBox();
	minX = new QSpinBox();
	minY = new QSpinBox();
	maxX = new QSpinBox();
	maxY = new QSpinBox();

	minX->setPrefix("Min X: ");
	minY->setPrefix("Min Y: ");
	maxX->setPrefix("Max X: ");
	maxY->setPrefix("Max Y: ");

	minX->setMinimum(-1000000);
	minY->setMinimum(-1000000);
	maxX->setMinimum(-1000000);
	maxY->setMinimum(-1000000);

	minX->setMaximum(1000000);
	minY->setMaximum(1000000);
	maxX->setMaximum(1000000);
	maxY->setMaximum(1000000);

	QWidget::connect(excludeScenes,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(ExcludeSceneChanged(const QString &)));
	QWidget::connect(minX, SIGNAL(valueChanged(int)), this,
			 SLOT(MinXChanged(int)));
	QWidget::connect(minY, SIGNAL(valueChanged(int)), this,
			 SLOT(MinYChanged(int)));
	QWidget::connect(maxX, SIGNAL(valueChanged(int)), this,
			 SLOT(MaxXChanged(int)));
	QWidget::connect(maxY, SIGNAL(valueChanged(int)), this,
			 SLOT(MaxYChanged(int)));

	PopulateSceneSelection(
		excludeScenes, false, false, false, false, nullptr, true,
		obs_module_text(
			"AdvSceneSwitcher.screenRegionTab.excludeScenes.None"),
		true);

	if (s) {
		excludeScenes->setCurrentText(
			GetWeakSourceName(s->excludeScene).c_str());
		minX->setValue(s->minX);
		minY->setValue(s->minY);
		maxX->setValue(s->maxX);
		maxY->setValue(s->maxY);
	}

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{minX}}", minX},
		{"{{minY}}", minY},
		{"{{maxX}}", maxX},
		{"{{maxY}}", maxY},
		{"{{scenes}}", scenes},
		{"{{transitions}}", transitions},
		{"{{excludeScenes}}", excludeScenes}};
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.screenRegionTab.entry"),
		     mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	switchData = s;

	loading = false;
}

ScreenRegionSwitch *ScreenRegionWidget::getSwitchData()
{
	return switchData;
}

void ScreenRegionWidget::setSwitchData(ScreenRegionSwitch *s)
{
	switchData = s;
}

void ScreenRegionWidget::swapSwitchData(ScreenRegionWidget *s1,
					ScreenRegionWidget *s2)
{
	SwitchWidget::swapSwitchData(s1, s2);

	ScreenRegionSwitch *t = s1->getSwitchData();
	s1->setSwitchData(s2->getSwitchData());
	s2->setSwitchData(t);
}

void ScreenRegionWidget::showFrame()
{
	drawFrame();
	helperFrame.show();
}

void ScreenRegionWidget::hideFrame()
{
	helperFrame.hide();
}

void ScreenRegionWidget::ExcludeSceneChanged(const QString &text)
{
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->excludeScene = GetWeakSourceByQString(text);
}

void ScreenRegionWidget::MinXChanged(int pos)
{
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->minX = pos;

	drawFrame();
}

void ScreenRegionWidget::MinYChanged(int pos)
{
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->minY = pos;

	drawFrame();
}

void ScreenRegionWidget::MaxXChanged(int pos)
{
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->maxX = pos;

	drawFrame();
}

void ScreenRegionWidget::MaxYChanged(int pos)
{
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->maxY = pos;

	drawFrame();
}

void ScreenRegionWidget::drawFrame()
{
	helperFrame.setFrameStyle(QFrame::Box | QFrame::Plain);
	helperFrame.setWindowFlags(Qt::FramelessWindowHint | Qt::Tool |
				   Qt::WindowTransparentForInput |
				   Qt::WindowDoesNotAcceptFocus |
				   Qt::WindowStaysOnTopHint);
	helperFrame.setAttribute(Qt::WA_TranslucentBackground, true);

	if (switchData) {
		helperFrame.setGeometry(switchData->minX, switchData->minY,
					switchData->maxX - switchData->minX,
					switchData->maxY - switchData->minY);
	}
}

} // namespace advss
