#include <QWidget>
#include <QHBoxLayout>

#include "headers/advanced-scene-switcher.hpp"
#include "headers/volume-control.hpp"

void SceneSwitcher::on_audioSwitches_currentRowChanged(int idx)
{
	if (loading)
		return;
	if (idx == -1)
		return;

	QListWidgetItem *item = ui->audioSwitches->item(idx);

	QString audioScenestr = item->data(Qt::UserRole).toString();

	std::lock_guard<std::mutex> lock(switcher->m);
	for (auto &s : switcher->audioSwitches) {
		if (audioScenestr.compare(s.audioSwitchStr.c_str()) == 0) {
			QString sceneName = GetWeakSourceName(s.scene).c_str();
			QString audioSrouceName =
				GetWeakSourceName(s.audioSource).c_str();
			QString transitionName =
				GetWeakSourceName(s.transition).c_str();
			ui->audioScenes->setCurrentText(sceneName);
			ui->audioTransitions->setCurrentText(transitionName);
			ui->audioSources->setCurrentText(audioSrouceName);
			ui->audioVolumeThreshold->setValue(s.volumeThreshold);
			volMeter->GetSlider()->setValue(s.volumeThreshold);
			break;
		}
	}
}

void SceneSwitcher::on_audioSources_currentTextChanged(const QString &text)
{
	SetAudioVolumeMeter(text);
}

void SceneSwitcher::SetAudioVolumeMeter(const QString &name)
{
	obs_source_t *soruce =
		obs_get_source_by_name(name.toUtf8().constData());
	if (!soruce) {
		return;
	}

	if (volMeter) {
		ui->audioControlLayout->removeWidget(volMeter);
		delete volMeter;
	}

	volMeter = new VolControl(soruce);
	ui->audioControlLayout->addWidget(volMeter);
	obs_source_release(soruce);

	QWidget::connect(volMeter->GetSlider(), SIGNAL(valueChanged(int)),
			 ui->audioVolumeThreshold, SLOT(setValue(int)));
	QWidget::connect(ui->audioVolumeThreshold, SIGNAL(valueChanged(int)),
			 volMeter->GetSlider(), SLOT(setValue(int)));
}

int SceneSwitcher::audioFindByData(const QString &source, const int &volume)
{
	QRegExp rx(MakeAudioSwitchName(QStringLiteral(".*"),
				       QStringLiteral(".*"), source, volume));
	int count = ui->audioSwitches->count();

	for (int i = 0; i < count; i++) {
		QListWidgetItem *item = ui->audioSwitches->item(i);
		QString str = item->data(Qt::UserRole).toString();

		if (rx.exactMatch(str))
			return i;
	}

	return -1;
}

void SceneSwitcher::on_audioAdd_clicked()
{
	QString sceneName = ui->audioScenes->currentText();
	QString transitionName = ui->audioTransitions->currentText();
	QString audioSourceName = ui->audioSources->currentText();
	int vol = ui->audioVolumeThreshold->value();

	if (sceneName.isEmpty() || audioSourceName.isEmpty())
		return;

	OBSWeakSource source = GetWeakSourceByQString(sceneName);
	OBSWeakSource audioSource = GetWeakSourceByQString(audioSourceName);
	OBSWeakSource transition = GetWeakTransitionByQString(transitionName);

	QString text = MakeAudioSwitchName(sceneName, transitionName,
					   audioSourceName, vol);
	QVariant v = QVariant::fromValue(text);

	int idx = audioFindByData(audioSourceName, vol);

	if (idx == -1) {
		std::lock_guard<std::mutex> lock(switcher->m);
		switcher->audioSwitches.emplace_back(
			source, transition, audioSource, vol,
			(sceneName == QString(previous_scene_name)),
			text.toUtf8().constData());
		QListWidgetItem *item =
			new QListWidgetItem(text, ui->audioSwitches);
		item->setData(Qt::UserRole, v);
	} else {
		QListWidgetItem *item = ui->audioSwitches->item(idx);
		item->setText(text);
		item->setData(Qt::UserRole, v);

		{
			std::lock_guard<std::mutex> lock(switcher->m);
			for (auto &s : switcher->audioSwitches) {
				if (s.audioSource == audioSource &&
				    s.volumeThreshold == vol) {
					s.scene = source;
					s.transition = transition;
					s.usePreviousScene =
						(sceneName ==
						 QString(previous_scene_name));
					s.audioSwitchStr =
						text.toUtf8().constData();
					break;
				}
			}
		}
	}
}

void SceneSwitcher::on_audioRemove_clicked()
{
	QListWidgetItem *item = ui->audioSwitches->currentItem();
	if (!item)
		return;

	std::string text =
		item->data(Qt::UserRole).toString().toUtf8().constData();

	{
		std::lock_guard<std::mutex> lock(switcher->m);
		auto &switches = switcher->audioSwitches;

		for (auto it = switches.begin(); it != switches.end(); ++it) {
			auto &s = *it;

			if (s.audioSwitchStr == text) {
				switches.erase(it);
				break;
			}
		}
	}

	delete item;
}

void SceneSwitcher::on_audioUp_clicked()
{
	int index = ui->audioSwitches->currentRow();
	if (index != -1 && index != 0) {
		ui->audioSwitches->insertItem(
			index - 1, ui->audioSwitches->takeItem(index));
		ui->audioSwitches->setCurrentRow(index - 1);

		std::lock_guard<std::mutex> lock(switcher->m);

		iter_swap(switcher->audioSwitches.begin() + index,
			  switcher->audioSwitches.begin() + index - 1);
	}
}

void SceneSwitcher::on_audioDown_clicked()
{
	int index = ui->audioSwitches->currentRow();
	if (index != -1 && index != ui->audioSwitches->count() - 1) {
		ui->audioSwitches->insertItem(
			index + 1, ui->audioSwitches->takeItem(index));
		ui->audioSwitches->setCurrentRow(index + 1);

		std::lock_guard<std::mutex> lock(switcher->m);

		iter_swap(switcher->audioSwitches.begin() + index,
			  switcher->audioSwitches.begin() + index + 1);
	}
}

void SwitcherData::checkAudioSwitch(bool &match, OBSWeakSource &scene,
				    OBSWeakSource &transition)
{
	if (audioSwitches.size() == 0)
		return;

	for (AudioSwitch &s : audioSwitches) {
		obs_source_t *as = obs_weak_source_get_source(s.audioSource);
		bool audioActive = obs_source_active(as);
		obs_source_release(as);

		// peak will have a value from -60 db to 0 db
		bool volumeThresholdreached = (s.peak + 60) * 1.7 >
					      s.volumeThreshold;

		if (volumeThresholdreached && audioActive) {
			scene = (s.usePreviousScene) ? previousScene : s.scene;
			transition = s.transition;
			match = true;

			if (verbose)
				s.logMatch();
			break;
		}
	}
}

void SwitcherData::saveAudioSwitches(obs_data_t *obj)
{
	obs_data_array_t *audioArray = obs_data_array_create();
	for (AudioSwitch &s : switcher->audioSwitches) {
		obs_data_t *array_obj = obs_data_create();

		obs_source_t *sceneSource = obs_weak_source_get_source(s.scene);
		obs_source_t *transition =
			obs_weak_source_get_source(s.transition);
		obs_source_t *audioSrouce =
			obs_weak_source_get_source(s.audioSource);
		if ((s.usePreviousScene || sceneSource) && transition) {
			const char *sceneName =
				obs_source_get_name(sceneSource);
			const char *transitionName =
				obs_source_get_name(transition);
			const char *audioSourceName =
				obs_source_get_name(audioSrouce);
			obs_data_set_string(array_obj, "scene",
					    s.usePreviousScene
						    ? previous_scene_name
						    : sceneName);
			obs_data_set_string(array_obj, "transition",
					    transitionName);
			obs_data_set_string(array_obj, "audioSource",
					    audioSourceName);
			obs_data_set_int(array_obj, "volume",
					 s.volumeThreshold);
			obs_data_array_push_back(audioArray, array_obj);
		}
		obs_source_release(sceneSource);
		obs_source_release(transition);
		obs_source_release(audioSrouce);

		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "audioSwitches", audioArray);
	obs_data_array_release(audioArray);
}

void SwitcherData::loadAudioSwitches(obs_data_t *obj)
{
	switcher->audioSwitches.clear();

	obs_data_array_t *audioArray = obs_data_get_array(obj, "audioSwitches");
	size_t count = obs_data_array_count(audioArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj = obs_data_array_item(audioArray, i);

		const char *scene = obs_data_get_string(array_obj, "scene");
		const char *transition =
			obs_data_get_string(array_obj, "transition");
		const char *audioSource =
			obs_data_get_string(array_obj, "audioSource");
		int vol = obs_data_get_int(array_obj, "volume");

		std::string audioSwitchStr =
			MakeAudioSwitchName(scene, transition, audioSource, vol)
				.toUtf8()
				.constData();

		switcher->audioSwitches.emplace_back(
			GetWeakSourceByName(scene),
			GetWeakTransitionByName(transition),
			GetWeakSourceByName(audioSource), vol,
			(strcmp(scene, previous_scene_name) == 0),
			audioSwitchStr);

		obs_data_release(array_obj);
	}
	obs_data_array_release(audioArray);
}

void SceneSwitcher::setupAudioTab()
{
	populateSceneSelection(ui->audioScenes, true);
	populateTransitionSelection(ui->audioTransitions);
	populateAudioSelection(ui->audioSources);

	for (auto &s : switcher->audioSwitches) {
		std::string sceneName = (s.usePreviousScene)
						? previous_scene_name
						: GetWeakSourceName(s.scene);
		std::string transitionName = GetWeakSourceName(s.transition);
		std::string audioSourceName = GetWeakSourceName(s.audioSource);
		QString listText = MakeAudioSwitchName(sceneName.c_str(),
						       transitionName.c_str(),
						       audioSourceName.c_str(),
						       s.volumeThreshold);

		//QListWidgetItem *item =
		//	new QListWidgetItem(listText, ui->audioSwitches);
		//item->setData(Qt::UserRole, listText);

		QListWidgetItem *item;
		item = new QListWidgetItem(ui->audioSwitches);
		ui->audioSwitches->addItem(item);
		AudioSwitchWidget *sw = new AudioSwitchWidget(
			nullptr, sceneName.c_str(), transitionName.c_str(),
			audioSourceName.c_str(), s.volumeThreshold);
		item->setSizeHint(sw->minimumSizeHint());
		ui->audioSwitches->setItemWidget(item, sw);
	}
}

static inline QString MakeAudioSwitchName(const QString &scene,
					  const QString &transition,
					  const QString &audioSrouce,
					  const int &volume)
{
	QString switchName = QStringLiteral("When volume of ") + audioSrouce +
			     QStringLiteral(" is above ") +
			     QString::number(volume) +
			     QStringLiteral("% switch to ") + scene +
			     QStringLiteral(" using ") + transition;
	return switchName;
}

AudioSwitchWidget::AudioSwitchWidget(QWidget *parent, const QString &audioScene,
				     const QString &audioTransition,
				     const QString &audioSource,
				     const int &threshold)
	: QWidget(parent)
{
	audioScenes = new QComboBox();
	audioTransitions = new QComboBox();
	audioSources = new QComboBox();
	audioVolumeThreshold = new QSpinBox();

	obs_source_t *soruce =
		obs_get_source_by_name(audioSource.toUtf8().constData());
	volMeter = new VolControl(soruce);
	obs_source_release(soruce);

	whenLabel = new QLabel("When the volume of");
	aboveLabel = new QLabel("is above");
	switchLabel = new QLabel("switch to");
	usingLabel = new QLabel("using");

	audioVolumeThreshold->setSuffix("%");
	audioVolumeThreshold->setMaximum(100);
	audioVolumeThreshold->setMinimum(0);

	QWidget::connect(volMeter->GetSlider(), SIGNAL(valueChanged(int)),
			 audioVolumeThreshold, SLOT(setValue(int)));
	QWidget::connect(audioVolumeThreshold, SIGNAL(valueChanged(int)),
			 volMeter->GetSlider(), SLOT(setValue(int)));

	SceneSwitcher::populateSceneSelection(audioScenes, true);
	SceneSwitcher::populateTransitionSelection(audioTransitions);
	SceneSwitcher::populateAudioSelection(audioSources);

	audioScenes->setCurrentText(audioScene);
	audioTransitions->setCurrentText(audioTransition);
	audioSources->setCurrentText(audioSource);
	audioVolumeThreshold->setValue(threshold);

	setStyleSheet("* { background-color: transparent; }");

	QHBoxLayout *switchLayout = new QHBoxLayout;

	switchLayout->addWidget(whenLabel);
	switchLayout->addWidget(audioSources);
	switchLayout->addWidget(aboveLabel);
	switchLayout->addWidget(audioVolumeThreshold);
	switchLayout->addWidget(switchLabel);
	switchLayout->addWidget(audioScenes);
	switchLayout->addWidget(usingLabel);
	switchLayout->addWidget(audioTransitions);
	switchLayout->addStretch();

	QVBoxLayout *mainLayout = new QVBoxLayout;

	mainLayout->addLayout(switchLayout);
	mainLayout->addWidget(volMeter);

	setLayout(mainLayout);
}
