#include <QFileDialog>
#include <QTextStream>
#include <QMessageBox>

#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

bool SceneSequenceSwitch::pause = false;
static QMetaObject::Connection addPulse;

void AdvSceneSwitcher::on_sceneSequenceAdd_clicked()
{
	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->sceneSequenceSwitches.emplace_back();

	listAddClicked(
		ui->sceneSequenceSwitches,
		new SequenceWidget(&switcher->sceneSequenceSwitches.back()),
		ui->sceneSequenceAdd, &addPulse);
}

void AdvSceneSwitcher::on_sceneSequenceRemove_clicked()
{
	QListWidgetItem *item = ui->sceneSequenceSwitches->currentItem();
	if (!item)
		return;

	{
		// might be in waiting state of sequence
		// causing invalid access after wakeup
		// thus we need to stop the main thread before delete
		bool wasRunning = !switcher->stop;
		switcher->Stop();

		std::lock_guard<std::mutex> lock(switcher->m);
		int idx = ui->sceneSequenceSwitches->currentRow();
		auto &switches = switcher->sceneSequenceSwitches;
		switches.erase(switches.begin() + idx);

		if (wasRunning)
			switcher->Start();
	}

	delete item;
}

void AdvSceneSwitcher::on_sceneSequenceUp_clicked()
{
	int index = ui->sceneSequenceSwitches->currentRow();
	if (!listMoveUp(ui->sceneSequenceSwitches))
		return;

	SequenceWidget *s1 =
		(SequenceWidget *)ui->sceneSequenceSwitches->itemWidget(
			ui->sceneSequenceSwitches->item(index));
	SequenceWidget *s2 =
		(SequenceWidget *)ui->sceneSequenceSwitches->itemWidget(
			ui->sceneSequenceSwitches->item(index - 1));
	SequenceWidget::swapSwitchData(s1, s2);

	std::lock_guard<std::mutex> lock(switcher->m);

	std::swap(switcher->sceneSequenceSwitches[index],
		  switcher->sceneSequenceSwitches[index - 1]);
}

void AdvSceneSwitcher::on_sceneSequenceDown_clicked()
{
	int index = ui->sceneSequenceSwitches->currentRow();

	if (!listMoveDown(ui->sceneSequenceSwitches))
		return;

	SequenceWidget *s1 =
		(SequenceWidget *)ui->sceneSequenceSwitches->itemWidget(
			ui->sceneSequenceSwitches->item(index));
	SequenceWidget *s2 =
		(SequenceWidget *)ui->sceneSequenceSwitches->itemWidget(
			ui->sceneSequenceSwitches->item(index + 1));
	SequenceWidget::swapSwitchData(s1, s2);

	std::lock_guard<std::mutex> lock(switcher->m);

	std::swap(switcher->sceneSequenceSwitches[index],
		  switcher->sceneSequenceSwitches[index + 1]);
}

void AdvSceneSwitcher::on_sceneSequenceSave_clicked()
{
	QString directory = QFileDialog::getSaveFileName(
		this,
		tr(obs_module_text(
			"AdvSceneSwitcher.sceneSequenceTab.saveTitle")),
		QDir::currentPath(),
		tr(obs_module_text(
			"AdvSceneSwitcher.sceneSequenceTab.fileType")));
	if (directory.isEmpty())
		return;
	QFile file(directory);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
		return;

	obs_data_t *obj = obs_data_create();
	switcher->saveSceneSequenceSwitches(obj);
	obs_data_save_json(obj, file.fileName().toUtf8().constData());
	obs_data_release(obj);
}

void AdvSceneSwitcher::on_sceneSequenceLoad_clicked()
{
	QString directory = QFileDialog::getOpenFileName(
		this,
		tr(obs_module_text(
			"AdvSceneSwitcher.sceneSequenceTab.loadTitle")),
		QDir::currentPath(),
		tr(obs_module_text(
			"AdvSceneSwitcher.sceneSequenceTab.fileType")));
	if (directory.isEmpty())
		return;

	QFile file(directory);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
		return;

	obs_data_t *obj = obs_data_create_from_json_file(
		file.fileName().toUtf8().constData());

	if (!obj) {
		QMessageBox Msgbox;
		Msgbox.setText(obs_module_text(
			"AdvSceneSwitcher.sceneSequenceTab.loadFail"));
		Msgbox.exec();
		return;
	}

	// might be in waiting state of sequence
	// causing invalid access after wakeup
	// thus we need to stop the main thread before delete
	bool wasRunning = !switcher->stop;
	switcher->Stop();

	switcher->loadSceneSequenceSwitches(obj);

	if (wasRunning)
		switcher->Start();

	obs_data_release(obj);

	QMessageBox Msgbox;
	Msgbox.setText(obs_module_text(
		"AdvSceneSwitcher.sceneSequenceTab.loadSuccess"));
	Msgbox.exec();
	close();
}

void SwitcherData::checkSceneSequence(bool &match, OBSWeakSource &scene,
				      OBSWeakSource &transition,
				      std::unique_lock<std::mutex> &lock)
{
	if (SceneSequenceSwitch::pause)
		return;

	obs_source_t *currentSource = obs_frontend_get_current_scene();
	obs_weak_source_t *ws = obs_source_get_weak_source(currentSource);

	for (SceneSequenceSwitch &s : sceneSequenceSwitches) {
		if (!s.initialized())
			continue;

		if (s.startScene == ws) {
			int dur = s.delay * 1000 - interval;
			if (dur > 0) {
				waitScene = currentSource;

				if (verbose)
					s.logSleep(dur);

				cv.wait_for(lock,
					    std::chrono::milliseconds(dur));
				waitScene = nullptr;
			}
			obs_source_t *currentSource2 =
				obs_frontend_get_current_scene();

			// only switch if user hasn't changed scene manually
			if (currentSource == currentSource2) {
				match = true;
				scene = (s.usePreviousScene) ? previousScene
							     : s.scene;
				transition = s.transition;
				if (verbose)
					s.logMatch();
			} else if (verbose) {
				blog(LOG_INFO, "sequence canceled");
			}

			obs_source_release(currentSource2);
			break;
		}
	}
	obs_source_release(currentSource);
	obs_weak_source_release(ws);
}

void SwitcherData::saveSceneSequenceSwitches(obs_data_t *obj)
{
	obs_data_array_t *sceneSequenceArray = obs_data_array_create();
	for (SceneSequenceSwitch &s : switcher->sceneSequenceSwitches) {
		obs_data_t *array_obj = obs_data_create();

		obs_source_t *source1 =
			obs_weak_source_get_source(s.startScene);
		obs_source_t *source2 = obs_weak_source_get_source(s.scene);
		obs_source_t *transition =
			obs_weak_source_get_source(s.transition);
		if (source1 && (s.usePreviousScene || source2) && transition) {
			const char *sceneName1 = obs_source_get_name(source1);
			const char *sceneName2 = obs_source_get_name(source2);
			const char *transitionName =
				obs_source_get_name(transition);
			obs_data_set_string(array_obj, "sceneRoundTripScene1",
					    sceneName1);
			obs_data_set_string(array_obj, "sceneRoundTripScene2",
					    s.usePreviousScene
						    ? previous_scene_name
						    : sceneName2);
			obs_data_set_string(array_obj, "transition",
					    transitionName);
			obs_data_set_double(array_obj, "delay", s.delay);
			obs_data_set_int(array_obj, "delayMultiplier",
					 s.delayMultiplier);
			obs_data_array_push_back(sceneSequenceArray, array_obj);
		}

		obs_source_release(source1);
		obs_source_release(source2);
		obs_source_release(transition);
		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "sceneRoundTrip", sceneSequenceArray);
	obs_data_array_release(sceneSequenceArray);
}

void SwitcherData::loadSceneSequenceSwitches(obs_data_t *obj)
{
	switcher->sceneSequenceSwitches.clear();

	obs_data_array_t *sceneSequenceArray =
		obs_data_get_array(obj, "sceneRoundTrip");
	size_t count = obs_data_array_count(sceneSequenceArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj =
			obs_data_array_item(sceneSequenceArray, i);

		const char *scene1 =
			obs_data_get_string(array_obj, "sceneRoundTripScene1");
		const char *scene2 =
			obs_data_get_string(array_obj, "sceneRoundTripScene2");
		const char *transition =
			obs_data_get_string(array_obj, "transition");
		double delay = obs_data_get_double(array_obj, "delay");
		int delayMultiplier =
			obs_data_get_int(array_obj, "delayMultiplier");
		if (delayMultiplier == 0 ||
		    (delayMultiplier != 1 && delayMultiplier % 60 != 0))
			delayMultiplier = 1;

		switcher->sceneSequenceSwitches.emplace_back(
			GetWeakSourceByName(scene1),
			GetWeakSourceByName(scene2),
			GetWeakTransitionByName(transition), delay,
			delayMultiplier,
			(strcmp(scene2, previous_scene_name) == 0));

		obs_data_release(array_obj);
	}
	obs_data_array_release(sceneSequenceArray);
}

void AdvSceneSwitcher::setupSequenceTab()
{
	for (auto &s : switcher->sceneSequenceSwitches) {
		QListWidgetItem *item;
		item = new QListWidgetItem(ui->sceneSequenceSwitches);
		ui->sceneSequenceSwitches->addItem(item);
		SequenceWidget *sw = new SequenceWidget(&s);
		item->setSizeHint(sw->minimumSizeHint());
		ui->sceneSequenceSwitches->setItemWidget(item, sw);
	}

	if (switcher->sceneSequenceSwitches.size() == 0)
		addPulse = PulseWidget(ui->sceneSequenceAdd, QColor(Qt::green));
}

bool SceneSequenceSwitch::initialized()
{
	return SceneSwitcherEntry::initialized() && startScene;
}

bool SceneSequenceSwitch::valid()
{
	return !initialized() ||
	       (SceneSwitcherEntry::valid() && WeakSourceValid(startScene));
}

void SceneSequenceSwitch::logSleep(int dur)
{
	blog(LOG_INFO, "sequence sleep %d", dur);
}

void populateDelayUnits(QComboBox *list)
{
	list->addItem(obs_module_text("AdvSceneSwitcher.unit.secends"));
	list->addItem(obs_module_text("AdvSceneSwitcher.unit.minutes"));
	list->addItem(obs_module_text("AdvSceneSwitcher.unit.hours"));
}

SequenceWidget::SequenceWidget(SceneSequenceSwitch *s) : SwitchWidget(s)
{
	delay = new QDoubleSpinBox();
	delayUnits = new QComboBox();
	startScenes = new QComboBox();

	QWidget::connect(delay, SIGNAL(valueChanged(double)), this,
			 SLOT(DelayChanged(double)));
	QWidget::connect(delayUnits, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(DelayUnitsChanged(int)));
	QWidget::connect(startScenes,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(StartSceneChanged(const QString &)));

	delay->setMaximum(99999.000000);
	AdvSceneSwitcher::populateSceneSelection(startScenes, false);
	populateDelayUnits(delayUnits);

	if (s) {
		switch (s->delayMultiplier) {
		case 1:
			delayUnits->setCurrentIndex(0);
			break;
		case 60:
			delayUnits->setCurrentIndex(1);
			break;
		case 3600:
			delayUnits->setCurrentIndex(2);
			break;
		default:
			delayUnits->setCurrentIndex(0);
		}
		startScenes->setCurrentText(
			GetWeakSourceName(s->startScene).c_str());
	}

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{startScenes}}", startScenes},
		{"{{scenes}}", scenes},
		{"{{delay}}", delay},
		{"{{delayUnits}}", delayUnits},
		{"{{transitions}}", transitions}};
	placeWidgets(obs_module_text("AdvSceneSwitcher.sceneSequenceTab.entry"),
		     mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	switchData = s;

	loading = false;

	UpdateDelay();
}

SceneSequenceSwitch *SequenceWidget::getSwitchData()
{
	return switchData;
}

void SequenceWidget::setSwitchData(SceneSequenceSwitch *s)
{
	switchData = s;
}

void SequenceWidget::swapSwitchData(SequenceWidget *s1, SequenceWidget *s2)
{
	SwitchWidget::swapSwitchData(s1, s2);

	SceneSequenceSwitch *t = s1->getSwitchData();
	s1->setSwitchData(s2->getSwitchData());
	s2->setSwitchData(t);
}

void SequenceWidget::UpdateDelay()
{
	if (switchData)
		delay->setValue(switchData->delay /
				switchData->delayMultiplier);
}

void SequenceWidget::DelayChanged(double delay)
{
	if (loading || !switchData)
		return;
	switchData->delay = delay * switchData->delayMultiplier;
}

void SequenceWidget::DelayUnitsChanged(int idx)
{
	if (loading || !switchData)
		return;

	delay_units unit = (delay_units)idx;

	switch (unit) {
	case SECONDS:
		switchData->delayMultiplier = 1;
		break;
	case MINUTES:
		switchData->delayMultiplier = 60;
		break;
	case HOURS:
		switchData->delayMultiplier = 60 * 60;
		break;
	default:
		break;
	}

	UpdateDelay();
}

void SequenceWidget::StartSceneChanged(const QString &text)
{
	if (loading || !switchData)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->startScene = GetWeakSourceByQString(text);
}
