#include <QTimer>

#include "headers/advanced-scene-switcher.hpp"

static QMetaObject::Connection addPulse;

void clearFrames(QListWidget *list)
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

	if (!item)
		return;

	ScreenRegionWidget *sw = (ScreenRegionWidget *)list->itemWidget(item);
	sw->showFrame();
}

void SceneSwitcher::on_showFrame_clicked()
{
	switcher->showFrame = !switcher->showFrame;

	if (switcher->showFrame) {
		ui->showFrame->setText("Hide guide frames");
		showCurrentFrame(ui->screenRegionSwitches);
	} else {
		ui->showFrame->setText("Show guide frames");
		clearFrames(ui->screenRegionSwitches);
	}
}

void SceneSwitcher::on_screenRegionSwitches_currentRowChanged(int idx)
{
	UNUSED_PARAMETER(idx);
	if (loading)
		return;

	if (switcher->showFrame) {
		clearFrames(ui->screenRegionSwitches);
		showCurrentFrame(ui->screenRegionSwitches);
	}
}

void SceneSwitcher::on_screenRegionAdd_clicked()
{
	ui->screenRegionAdd->disconnect(addPulse);

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->screenRegionSwitches.emplace_back();

	QListWidgetItem *item;
	item = new QListWidgetItem(ui->screenRegionSwitches);
	ui->screenRegionSwitches->addItem(item);
	ScreenRegionWidget *sw =
		new ScreenRegionWidget(&switcher->screenRegionSwitches.back());
	item->setSizeHint(sw->minimumSizeHint());
	ui->screenRegionSwitches->setItemWidget(item, sw);
}

void SceneSwitcher::on_screenRegionRemove_clicked()
{
	QListWidgetItem *item = ui->screenRegionSwitches->currentItem();
	if (!item)
		return;

	{
		std::lock_guard<std::mutex> lock(switcher->m);
		int idx = ui->screenRegionSwitches->currentRow();
		auto &switches = switcher->screenRegionSwitches;
		switches.erase(switches.begin() + idx);
	}

	delete item;
}

void SceneSwitcher::on_screenRegionUp_clicked()
{
	int index = ui->screenRegionSwitches->currentRow();
	if (!listMoveUp(ui->screenRegionSwitches))
		return;

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

void SceneSwitcher::on_screenRegionDown_clicked()
{
	int index = ui->screenRegionSwitches->currentRow();

	if (!listMoveDown(ui->screenRegionSwitches))
		return;

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

void SwitcherData::checkScreenRegionSwitch(bool &match, OBSWeakSource &scene,
					   OBSWeakSource &transition)
{
	std::pair<int, int> cursorPos = getCursorPos();
	int minRegionSize = 99999;

	for (auto &s : screenRegionSwitches) {
		if (cursorPos.first >= s.minX && cursorPos.second >= s.minY &&
		    cursorPos.first <= s.maxX && cursorPos.second <= s.maxY) {
			int regionSize = (s.maxX - s.minX) + (s.maxY - s.minY);
			if (regionSize < minRegionSize) {
				match = true;
				scene = s.scene;
				transition = s.transition;
				minRegionSize = regionSize;

				if (verbose)
					s.logMatch();
				break;
			}
		}
	}
}

void SceneSwitcher::updateScreenRegionCursorPos()
{
	std::pair<int, int> position = getCursorPos();
	ui->cursorXPosition->setText(QString::number(position.first));
	ui->cursorYPosition->setText(QString::number(position.second));
}

void SwitcherData::saveScreenRegionSwitches(obs_data_t *obj)
{
	obs_data_array_t *screenRegionArray = obs_data_array_create();
	for (ScreenRegionSwitch &s : switcher->screenRegionSwitches) {
		obs_data_t *array_obj = obs_data_create();

		obs_source_t *source = obs_weak_source_get_source(s.scene);
		obs_source_t *transition =
			obs_weak_source_get_source(s.transition);
		if (source && transition) {
			const char *sceneName = obs_source_get_name(source);
			const char *transitionName =
				obs_source_get_name(transition);
			obs_data_set_string(array_obj, "screenRegionScene",
					    sceneName);
			obs_data_set_string(array_obj, "transition",
					    transitionName);
			obs_data_set_int(array_obj, "minX", s.minX);
			obs_data_set_int(array_obj, "minY", s.minY);
			obs_data_set_int(array_obj, "maxX", s.maxX);
			obs_data_set_int(array_obj, "maxY", s.maxY);
			obs_data_array_push_back(screenRegionArray, array_obj);
			obs_source_release(source);
			obs_source_release(transition);
		}

		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "screenRegion", screenRegionArray);
	obs_data_array_release(screenRegionArray);
}

void SwitcherData::loadScreenRegionSwitches(obs_data_t *obj)
{
	switcher->screenRegionSwitches.clear();

	obs_data_array_t *screenRegionArray =
		obs_data_get_array(obj, "screenRegion");
	size_t count = obs_data_array_count(screenRegionArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj =
			obs_data_array_item(screenRegionArray, i);

		const char *scene =
			obs_data_get_string(array_obj, "screenRegionScene");
		const char *transition =
			obs_data_get_string(array_obj, "transition");
		int minX = obs_data_get_int(array_obj, "minX");
		int minY = obs_data_get_int(array_obj, "minY");
		int maxX = obs_data_get_int(array_obj, "maxX");
		int maxY = obs_data_get_int(array_obj, "maxY");

		switcher->screenRegionSwitches.emplace_back(
			GetWeakSourceByName(scene),
			GetWeakTransitionByName(transition), minX, minY, maxX,
			maxY);

		obs_data_release(array_obj);
	}
	obs_data_array_release(screenRegionArray);
}

void SceneSwitcher::setupRegionTab()
{
	for (auto &s : switcher->screenRegionSwitches) {
		QListWidgetItem *item;
		item = new QListWidgetItem(ui->screenRegionSwitches);
		ui->screenRegionSwitches->addItem(item);
		ScreenRegionWidget *sw = new ScreenRegionWidget(&s);
		item->setSizeHint(sw->minimumSizeHint());
		ui->screenRegionSwitches->setItemWidget(item, sw);
	}

	if (switcher->screenRegionSwitches.size() == 0)
		addPulse = PulseWidget(ui->screenRegionAdd, QColor(Qt::green));

	// screen region cursor position
	QTimer *screenRegionTimer = new QTimer(this);
	connect(screenRegionTimer, SIGNAL(timeout()), this,
		SLOT(updateScreenRegionCursorPos()));
	screenRegionTimer->start(1000);
}

ScreenRegionWidget::ScreenRegionWidget(ScreenRegionSwitch *s)
	: SwitchWidget(s, false)
{
	minX = new QSpinBox();
	minY = new QSpinBox();
	maxX = new QSpinBox();
	maxY = new QSpinBox();

	cursorLabel = new QLabel("If cursor is in");
	xLabel = new QLabel("x");
	switchLabel = new QLabel("switch to");
	usingLabel = new QLabel("using");

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

	QWidget::connect(minX, SIGNAL(valueChanged(int)), this,
			 SLOT(MinXChanged(int)));
	QWidget::connect(minY, SIGNAL(valueChanged(int)), this,
			 SLOT(MinYChanged(int)));
	QWidget::connect(maxX, SIGNAL(valueChanged(int)), this,
			 SLOT(MaxXChanged(int)));
	QWidget::connect(maxY, SIGNAL(valueChanged(int)), this,
			 SLOT(MaxYChanged(int)));

	if (s) {
		minX->setValue(s->minX);
		minY->setValue(s->minY);
		maxX->setValue(s->maxX);
		maxY->setValue(s->maxY);
	}

	setStyleSheet("* { background-color: transparent; }");

	QHBoxLayout *mainLayout = new QHBoxLayout;

	mainLayout->addWidget(cursorLabel);
	mainLayout->addWidget(minX);
	mainLayout->addWidget(minY);
	mainLayout->addWidget(xLabel);
	mainLayout->addWidget(maxX);
	mainLayout->addWidget(maxY);
	mainLayout->addWidget(switchLabel);
	mainLayout->addWidget(scenes);
	mainLayout->addWidget(usingLabel);
	mainLayout->addWidget(transitions);
	mainLayout->addStretch();

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

void ScreenRegionWidget::MinXChanged(int pos)
{
	if (loading || !switchData)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->minX = pos;

	drawFrame();
}

void ScreenRegionWidget::MinYChanged(int pos)
{
	if (loading || !switchData)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->minY = pos;

	drawFrame();
}

void ScreenRegionWidget::MaxXChanged(int pos)
{
	if (loading || !switchData)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->maxX = pos;

	drawFrame();
}

void ScreenRegionWidget::MaxYChanged(int pos)
{
	if (loading || !switchData)
		return;
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

	if (switchData)
		helperFrame.setGeometry(switchData->minX, switchData->minY,
					switchData->maxX - switchData->minX,
					switchData->maxY - switchData->minY);
}
