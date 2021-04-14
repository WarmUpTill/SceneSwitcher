#include <QFileDialog>
#include <QTextStream>

#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

constexpr auto max_extend_text_size = 150;

bool SceneSequenceSwitch::pause = false;
static QMetaObject::Connection addPulse;

void AdvSceneSwitcher::on_sceneSequenceAdd_clicked()
{
	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->sceneSequenceSwitches.emplace_back();

	listAddClicked(ui->sceneSequenceSwitches,
		       new SequenceWidget(
			       this, &switcher->sceneSequenceSwitches.back()),
		       ui->sceneSequenceAdd, &addPulse);

	ui->sequenceHelp->setVisible(false);
}

void AdvSceneSwitcher::on_sceneSequenceRemove_clicked()
{
	QListWidgetItem *item = ui->sceneSequenceSwitches->currentItem();
	if (!item) {
		return;
	}

	{
		std::lock_guard<std::mutex> lock(switcher->m);
		int idx = ui->sceneSequenceSwitches->currentRow();
		auto &switches = switcher->sceneSequenceSwitches;
		switches.erase(switches.begin() + idx);
	}

	delete item;
}

void AdvSceneSwitcher::on_sceneSequenceUp_clicked()
{
	int index = ui->sceneSequenceSwitches->currentRow();
	if (!listMoveUp(ui->sceneSequenceSwitches)) {
		return;
	}

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

	if (!listMoveDown(ui->sceneSequenceSwitches)) {
		return;
	}

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
	if (directory.isEmpty()) {
		return;
	}
	QFile file(directory);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		return;
	}

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
	if (directory.isEmpty()) {
		return;
	}

	QFile file(directory);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		return;
	}

	obs_data_t *obj = obs_data_create_from_json_file(
		file.fileName().toUtf8().constData());

	if (!obj) {
		DisplayMessage(obs_module_text(
			"AdvSceneSwitcher.sceneSequenceTab.loadFail"));
		return;
	}

	switcher->loadSceneSequenceSwitches(obj);

	obs_data_release(obj);

	DisplayMessage(obs_module_text(
		"AdvSceneSwitcher.sceneSequenceTab.loadSuccess"));
	close();
}

void AdvSceneSwitcher::OpenSequenceExtendEdit(SequenceWidget *sw)
{
	QDialog edit;
	SequenceWidget editWidget(this, sw->getSwitchData(), false, true,
				  false);
	QHBoxLayout layout;
	layout.setSizeConstraint(QLayout::SetFixedSize);
	layout.addWidget(&editWidget);
	edit.setLayout(&layout);
	edit.setWindowTitle(obs_module_text(
		"AdvSceneSwitcher.sceneSequenceTab.extendEdit"));
	edit.exec();

	sw->UpdateWidgetStatus(true);
}

void AdvSceneSwitcher::on_sequenceEdit_clicked()
{
	int index = ui->sceneSequenceSwitches->currentRow();
	if (index == -1) {
		return;
	}

	SequenceWidget *currentWidget =
		(SequenceWidget *)ui->sceneSequenceSwitches->itemWidget(
			ui->sceneSequenceSwitches->item(index));

	OpenSequenceExtendEdit(currentWidget);
}

void AdvSceneSwitcher::on_sceneSequenceSwitches_itemDoubleClicked(
	QListWidgetItem *item)
{
	SequenceWidget *currentWidget =
		(SequenceWidget *)ui->sceneSequenceSwitches->itemWidget(item);
	OpenSequenceExtendEdit(currentWidget);
}

bool SwitcherData::checkSceneSequence(OBSWeakSource &scene,
				      OBSWeakSource &transition, int &linger)
{
	if (SceneSequenceSwitch::pause) {
		return false;
	}

	obs_source_t *currentSceneSource = obs_frontend_get_current_scene();
	obs_weak_source_t *currentScene =
		obs_source_get_weak_source(currentSceneSource);
	bool match = false;

	for (SceneSequenceSwitch &s : sceneSequenceSwitches) {
		// Continue the active uninterruptible sequence and skip others
		if (uninterruptibleSceneSequenceActive &&
		    s.activeSequence == nullptr) {
			continue;
		}

		bool matched = s.checkMatch(currentScene, linger);

		if (!match && matched) {
			match = matched;

			if (s.activeSequence) {
				scene = s.activeSequence->getScene();
				transition = s.activeSequence->transition;
			} else {
				scene = s.getScene();
				transition = s.transition;
				if (verbose) {
					s.logMatch();
				}
			}

			s.advanceActiveSequence();
			if (verbose) {
				s.logAdvanceSequence();
			}

			// Ignore other switching methods if sequence is not
			// interruptible and has not reached its end
			if (s.activeSequence) {
				uninterruptibleSceneSequenceActive =
					!s.interruptible;
			}
		}
	}

	if (!match) {
		uninterruptibleSceneSequenceActive = false;
	}

	obs_source_release(currentSceneSource);
	obs_weak_source_release(currentScene);

	return match;
}

void SwitcherData::saveSceneSequenceSwitches(obs_data_t *obj)
{
	obs_data_array_t *sceneSequenceArray = obs_data_array_create();
	for (SceneSequenceSwitch &s : switcher->sceneSequenceSwitches) {
		obs_data_t *array_obj = obs_data_create();

		s.save(array_obj);
		obs_data_array_push_back(sceneSequenceArray, array_obj);

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

		switcher->sceneSequenceSwitches.emplace_back();
		sceneSequenceSwitches.back().load(array_obj);

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
		SequenceWidget *sw = new SequenceWidget(this, &s);
		item->setSizeHint(sw->minimumSizeHint());
		ui->sceneSequenceSwitches->setItemWidget(item, sw);
	}

	if (switcher->sceneSequenceSwitches.size() == 0) {
		addPulse = PulseWidget(ui->sceneSequenceAdd, QColor(Qt::green));
		ui->sequenceHelp->setVisible(true);
	} else {
		ui->sequenceHelp->setVisible(false);
	}
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

void SceneSequenceSwitch::save(obs_data_t *obj, bool saveExt)
{
	SceneSwitcherEntry::save(obj);

	obs_data_set_int(obj, "startTargetType",
			 static_cast<int>(startTargetType));
	obs_data_set_string(obj, "startScene",
			    GetWeakSourceName(startScene).c_str());
	obs_data_set_double(obj, "delay", delay);
	obs_data_set_int(obj, "delayMultiplier", delayMultiplier);
	obs_data_set_bool(obj, "interruptible", interruptible);

	if (saveExt) {
		auto cur = extendedSequence.get();

		obs_data_array_t *extendScenes = obs_data_array_create();
		while (cur) {
			obs_data_t *array_obj = obs_data_create();
			cur->save(array_obj, false);
			obs_data_array_push_back(extendScenes, array_obj);
			obs_data_release(array_obj);
			cur = cur->extendedSequence.get();
		}
		obs_data_set_array(obj, "extendScenes", extendScenes);
		obs_data_array_release(extendScenes);
	}
}

void SceneSequenceSwitch::load(obs_data_t *obj, bool saveExt)
{
	SceneSwitcherEntry::load(obj);
	startTargetType = static_cast<SwitchTargetType>(
		obs_data_get_int(obj, "startTargetType"));
	const char *scene = obs_data_get_string(obj, "startScene");
	startScene = GetWeakSourceByName(scene);

	delay = obs_data_get_double(obj, "delay");

	delayMultiplier = obs_data_get_int(obj, "delayMultiplier");
	if (delayMultiplier == 0 ||
	    (delayMultiplier != 1 && delayMultiplier % 60 != 0))
		delayMultiplier = 1;

	interruptible = obs_data_get_bool(obj, "interruptible");

	if (saveExt) {
		auto cur = this;

		obs_data_array_t *extendScenes =
			obs_data_get_array(obj, "extendScenes");
		size_t count = obs_data_array_count(extendScenes);
		for (size_t i = 0; i < count; i++) {
			obs_data_t *array_obj =
				obs_data_array_item(extendScenes, i);

			cur->extendedSequence =
				std::make_unique<SceneSequenceSwitch>();

			cur->extendedSequence->load(array_obj, false);

			cur = cur->extendedSequence.get();
			obs_data_release(array_obj);
		}
		obs_data_array_release(extendScenes);
	}
}

bool SceneSequenceSwitch::reduce()
{
	// Reset activeSequence just in case it was one of the deleted entries
	activeSequence = nullptr;

	if (!extendedSequence) {
		return true;
	}
	if (extendedSequence->reduce()) {
		extendedSequence.reset(nullptr);
	}
	return false;
}

SceneSequenceSwitch *SceneSequenceSwitch::extend()
{
	if (extendedSequence) {
		return extendedSequence->extend();
	}
	extendedSequence = std::make_unique<SceneSequenceSwitch>();
	extendedSequence->startScene = scene;
	return extendedSequence.get();
}

bool SceneSequenceSwitch::checkMatch(OBSWeakSource currentScene, int &linger,
				     SceneSequenceSwitch *root)
{
	if (!initialized()) {
		if (root) {
			root->activeSequence = nullptr;
		}
		return false;
	}

	bool match = false;

	if (activeSequence) {
		return activeSequence->checkMatch(currentScene, linger, this);
	}

	if (startScene == currentScene) {
		if (interruptible) {
			match = checkDurationMatchInterruptible();
		} else {
			match = true;
			prepareUninterruptibleMatch(currentScene, linger);
		}
	} else {
		matchCount = 0;

		if (root) {
			root->activeSequence = nullptr;
			logSequenceCanceled();
		}
	}

	return match;
}

bool SceneSequenceSwitch::checkDurationMatchInterruptible()
{
	bool durationReached = matchCount * (switcher->interval / 1000.0) >=
			       delay;
	matchCount++;
	if (durationReached) {
		matchCount = 0;
		return true;
	}
	return false;
}

void SceneSequenceSwitch::prepareUninterruptibleMatch(
	OBSWeakSource currentScene, int &linger)
{
	int dur = delay * 1000;
	if (dur > 0) {
		switcher->waitScene = obs_weak_source_get_source(currentScene);
		obs_source_release(switcher->waitScene);
		linger = dur;
	}
}

void SceneSequenceSwitch::advanceActiveSequence()
{
	// Set start Scene
	OBSWeakSource currentSceneGroupScene = nullptr;
	if (targetType == SwitchTargetType::SceneGroup && group) {
		currentSceneGroupScene = group->getCurrentScene();
	}

	if (activeSequence) {
		activeSequence = activeSequence->extendedSequence.get();
	} else {
		activeSequence = extendedSequence.get();
	}

	if (activeSequence) {
		if (activeSequence->startTargetType ==
		    SwitchTargetType::SceneGroup) {
			activeSequence->startScene = currentSceneGroupScene;
		}
		if (activeSequence->targetType == SwitchTargetType::Scene &&
		    !activeSequence->scene) {
			blog(LOG_WARNING,
			     "cannot advance sequence - null scene set");
			activeSequence = nullptr;
			return;
		}
		if (activeSequence->targetType ==
			    SwitchTargetType::SceneGroup &&
		    activeSequence->group &&
		    activeSequence->group->scenes.empty()) {
			blog(LOG_WARNING,
			     "cannot advance sequence - no scenes specified in '%s'",
			     activeSequence->group->name.c_str());
			activeSequence = nullptr;
			return;
		}

		// Reinit old matchCount value in case it was previously set
		activeSequence->matchCount = 0;
	}
}

void SceneSequenceSwitch::logAdvanceSequence()
{
	if (activeSequence) {
		std::string targetName =
			GetWeakSourceName(activeSequence->scene);

		if (activeSequence->targetType ==
			    SwitchTargetType::SceneGroup &&
		    activeSequence->group) {
			targetName = activeSequence->group->name;
		}

		blog(LOG_INFO, "continuing sequence with '%s' -> '%s'",
		     GetWeakSourceName(activeSequence->startScene).c_str(),
		     targetName.c_str());
	}
}

void SceneSequenceSwitch::logSequenceCanceled()
{
	blog(LOG_INFO, "unexpected scene change - cancel sequence");
}

QString delayMultiplierToString(int delayMultiplier)
{
	switch (delayMultiplier) {
	case 1:
		return obs_module_text("AdvSceneSwitcher.unit.secends");
	case 60:
		return obs_module_text("AdvSceneSwitcher.unit.minutes");
	case 60 * 60:
		return obs_module_text("AdvSceneSwitcher.unit.hours");
	}
	return "???";
}

QString makeExtendText(SceneSequenceSwitch *s, int curLen = 0)
{
	if (!s) {
		return "";
	}

	QString ext = "";

	ext = QString::number(s->delay / s->delayMultiplier) + " ";
	ext += delayMultiplierToString(s->delayMultiplier);

	QString sceneName = GetWeakSourceName(s->scene).c_str();
	if (s->targetType == SwitchTargetType::SceneGroup && s->group) {
		sceneName = QString::fromStdString(s->group->name);
	}
	if (sceneName.isEmpty()) {
		sceneName = obs_module_text("AdvSceneSwitcher.selectScene");
	}
	ext += " -> [" + sceneName + "]";

	if (ext.length() + curLen > max_extend_text_size) {
		return "...";
	}

	if (s->extendedSequence.get()) {
		return ext +=
		       "    |    " + makeExtendText(s->extendedSequence.get(),
						    curLen + ext.length());
	} else {
		return ext;
	}
}

void populateDelayUnits(QComboBox *list)
{
	list->addItem(obs_module_text("AdvSceneSwitcher.unit.secends"));
	list->addItem(obs_module_text("AdvSceneSwitcher.unit.minutes"));
	list->addItem(obs_module_text("AdvSceneSwitcher.unit.hours"));
}

SequenceWidget::SequenceWidget(QWidget *parent, SceneSequenceSwitch *s,
			       bool extendSequence, bool editExtendMode,
			       bool showExtendText)
	: SwitchWidget(parent, s, !extendSequence, true)
{
	this->setParent(parent);

	delay = new QDoubleSpinBox();
	delayUnits = new QComboBox();
	startScenes = new QComboBox();
	interruptible = new QCheckBox(obs_module_text(
		"AdvSceneSwitcher.sceneSequenceTab.interruptible"));
	extendText = new QLabel();
	extend = new QPushButton();
	reduce = new QPushButton();

	extend->setProperty("themeID",
			    QVariant(QStringLiteral("addIconSmall")));
	reduce->setProperty("themeID",
			    QVariant(QStringLiteral("removeIconSmall")));

	extend->setMaximumSize(22, 22);
	reduce->setMaximumSize(22, 22);

	// We need to extend the generic SwitchWidget::SceneChanged()
	// with our own SequenceWidget::SceneChanged()
	// so the old singal / slot needs to be disconnected
	scenes->disconnect();
	QWidget::connect(scenes, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(SceneChanged(const QString &)));

	QWidget::connect(delay, SIGNAL(valueChanged(double)), this,
			 SLOT(DelayChanged(double)));
	QWidget::connect(delayUnits, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(DelayUnitsChanged(int)));
	QWidget::connect(startScenes,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(StartSceneChanged(const QString &)));
	QWidget::connect(interruptible, SIGNAL(stateChanged(int)), this,
			 SLOT(InterruptibleChanged(int)));

	QWidget::connect(extend, SIGNAL(clicked()), this,
			 SLOT(ExtendClicked()));
	QWidget::connect(reduce, SIGNAL(clicked()), this,
			 SLOT(ReduceClicked()));

	delay->setMaximum(99999.000000);
	AdvSceneSwitcher::populateSceneSelection(startScenes, false);
	populateDelayUnits(delayUnits);
	interruptible->setToolTip(obs_module_text(
		"AdvSceneSwitcher.sceneSequenceTab.interruptibleHint"));

	// The extended sequence widgets never exist on their own and are always
	// place inside a non-extend sequence widget
	if (extendSequence) {
		QHBoxLayout *mainLayout = new QHBoxLayout;
		std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
			{"{{scenes}}", scenes},
			{"{{delay}}", delay},
			{"{{delayUnits}}", delayUnits},
			{"{{transitions}}", transitions}};
		placeWidgets(
			obs_module_text(
				"AdvSceneSwitcher.sceneSequenceTab.extendEntry"),
			mainLayout, widgetPlaceholders);
		setLayout(mainLayout);
	} else {
		QHBoxLayout *startSequence = new QHBoxLayout;
		std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
			{"{{startScenes}}", startScenes},
			{"{{scenes}}", scenes},
			{"{{delay}}", delay},
			{"{{delayUnits}}", delayUnits},
			{"{{transitions}}", transitions},
			{"{{interruptible}}", interruptible}};
		placeWidgets(obs_module_text(
				     "AdvSceneSwitcher.sceneSequenceTab.entry"),
			     startSequence, widgetPlaceholders);

		// The exetend widgets placed here
		extendSequenceLayout = new QVBoxLayout;
		if (s) {
			if (!editExtendMode) {
				extendText->setText(makeExtendText(
					s->extendedSequence.get()));
			} else {
				auto cur = s->extendedSequence.get();
				while (cur != nullptr) {
					extendSequenceLayout->addWidget(
						new SequenceWidget(parent, cur,
								   true, true));
					cur = cur->extendedSequence.get();
				}
			}
		}

		QHBoxLayout *extendSequenceControlsLayout = new QHBoxLayout;
		if (editExtendMode) {
			extendSequenceControlsLayout->addWidget(extend);
			extendSequenceControlsLayout->addWidget(reduce);
		}
		extendSequenceControlsLayout->addStretch();

		QVBoxLayout *mainLayout = new QVBoxLayout;
		mainLayout->addLayout(startSequence);
		mainLayout->addLayout(extendSequenceLayout);
		mainLayout->addWidget(extendText);
		mainLayout->addLayout(extendSequenceControlsLayout);
		setLayout(mainLayout);
	}

	switchData = s;
	UpdateWidgetStatus(showExtendText);

	loading = false;
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
	if (switchData) {
		delay->setValue(switchData->delay /
				switchData->delayMultiplier);
	}
}

void SequenceWidget::DelayChanged(double delay)
{
	if (loading || !switchData) {
		return;
	}

	switchData->delay = delay * switchData->delayMultiplier;
}

void SequenceWidget::DelayUnitsChanged(int idx)
{
	if (loading || !switchData) {
		return;
	}

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

void SequenceWidget::setExtendedSequenceStartScene()
{
	switchData->extendedSequence->startScene = switchData->scene;
	switchData->extendedSequence->startTargetType = SwitchTargetType::Scene;

	if (switchData->targetType == SwitchTargetType::SceneGroup) {
		switchData->extendedSequence->startScene = nullptr;
		switchData->extendedSequence->startTargetType =
			SwitchTargetType::SceneGroup;
	}
}

void SequenceWidget::SceneChanged(const QString &text)
{
	if (loading || !switchData) {
		return;
	}

	SwitchWidget::SceneChanged(text);
	std::lock_guard<std::mutex> lock(switcher->m);

	if (switchData->extendedSequence) {
		setExtendedSequenceStartScene();
	}
}

void SequenceWidget::StartSceneChanged(const QString &text)
{
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->startScene = GetWeakSourceByQString(text);
}

void SequenceWidget::InterruptibleChanged(int state)
{
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->interruptible = state;

	auto cur = switchData->extendedSequence.get();
	while (cur != nullptr) {
		cur->interruptible = state;
		cur = cur->extendedSequence.get();
	}
}

void SequenceWidget::UpdateWidgetStatus(bool showExtendText)
{
	if (showExtendText) {
		extendText->setText(
			makeExtendText(switchData->extendedSequence.get()));
	}

	switch (switchData->delayMultiplier) {
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
	UpdateDelay();

	startScenes->setCurrentText(
		GetWeakSourceName(switchData->startScene).c_str());
	interruptible->setChecked(switchData->interruptible);

	SwitchWidget::showSwitchData();
}

void SequenceWidget::ExtendClicked()
{
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	auto es = switchData->extend();

	SequenceWidget *ew = new SequenceWidget(this->parentWidget(), es, true);
	extendSequenceLayout->addWidget(ew);
}

void SequenceWidget::ReduceClicked()
{
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->reduce();

	int count = extendSequenceLayout->count();
	auto item = extendSequenceLayout->takeAt(count - 1);
	if (item) {
		item->widget()->setVisible(false);
		delete item;
	}
}
