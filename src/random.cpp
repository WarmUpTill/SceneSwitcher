#include "headers/advanced-scene-switcher.hpp"
#include <random>

void SceneSwitcher::on_randomScenesList_currentRowChanged(int idx)
{
	if (loading)
		return;
	if (idx == -1)
		return;

	QListWidgetItem *item = ui->randomScenesList->item(idx);

	QString randomSceneStr = item->data(Qt::UserRole).toString();

	std::lock_guard<std::mutex> lock(switcher->m);
	for (auto &s : switcher->randomSwitches) {
		if (randomSceneStr.compare(s.randomSwitchStr.c_str()) == 0) {
			QString sceneName = GetWeakSourceName(s.scene).c_str();
			QString transitionName =
				GetWeakSourceName(s.transition).c_str();
			ui->randomScenes->setCurrentText(sceneName);
			ui->randomSpinBox->setValue(s.delay);
			ui->randomTransitions->setCurrentText(transitionName);
			break;
		}
	}
}

int SceneSwitcher::randomFindByData(const QString &randomStr)
{
	int count = ui->randomScenesList->count();

	for (int i = 0; i < count; i++) {
		QListWidgetItem *item = ui->randomScenesList->item(i);
		QString str = item->data(Qt::UserRole).toString();

		if (str == randomStr)
			return i;
	}

	return -1;
}

void SceneSwitcher::on_randomAdd_clicked()
{
	QString sceneName = ui->randomScenes->currentText();
	QString transitionName = ui->randomTransitions->currentText();
	double delay = ui->randomSpinBox->value();

	if (sceneName.isEmpty())
		return;

	OBSWeakSource source = GetWeakSourceByQString(sceneName);
	OBSWeakSource transition = GetWeakTransitionByQString(transitionName);

	QString text = MakeRandomSwitchName(sceneName, transitionName, delay);
	QVariant v = QVariant::fromValue(text);

	int idx = randomFindByData(text);

	if (idx == -1) {
		std::lock_guard<std::mutex> lock(switcher->m);
		switcher->randomSwitches.emplace_back(
			source, transition, delay, text.toUtf8().constData());

		QListWidgetItem *item =
			new QListWidgetItem(text, ui->randomScenesList);
		item->setData(Qt::UserRole, v);
	} else {
		QListWidgetItem *item = ui->randomScenesList->item(idx);
		item->setText(text);

		{
			std::lock_guard<std::mutex> lock(switcher->m);
			for (auto &s : switcher->randomSwitches) {
				if (s.scene == source) {
					s.delay = delay;
					s.transition = transition;
					s.randomSwitchStr =
						text.toUtf8().constData();
					;
					break;
				}
			}
		}

		ui->randomScenesList->sortItems();
	}
}

void SceneSwitcher::on_randomRemove_clicked()
{
	QListWidgetItem *item = ui->randomScenesList->currentItem();
	if (!item)
		return;

	std::string text =
		item->data(Qt::UserRole).toString().toUtf8().constData();

	{
		std::lock_guard<std::mutex> lock(switcher->m);
		auto &switches = switcher->randomSwitches;

		for (auto it = switches.begin(); it != switches.end(); ++it) {
			auto &s = *it;

			if (s.randomSwitchStr == text) {
				switches.erase(it);
				break;
			}
		}
	}

	delete item;
}

void SwitcherData::checkRandom(bool &match, OBSWeakSource &scene,
			       OBSWeakSource &transition, int &delay)
{
	if (randomSwitches.size() == 0)
		return;

	std::vector<RandomSwitch> rs(randomSwitches);
	std::random_device rng;
	std::mt19937 urng(rng());
	std::shuffle(rs.begin(), rs.end(), urng);
	for (RandomSwitch &r : rs) {
		if (r.scene == lastRandomScene)
			continue;
		scene = r.scene;
		transition = r.transition;
		delay = (int)r.delay * 1000;
		match = true;
		lastRandomScene = r.scene;

		if (verbose)
			blog(LOG_INFO, "Advanced Scene Switcher random match");

		break;
	}
}

void SwitcherData::saveRandomSwitches(obs_data_t *obj)
{
	obs_data_array_t *randomArray = obs_data_array_create();
	for (RandomSwitch &s : switcher->randomSwitches) {
		obs_data_t *array_obj = obs_data_create();

		obs_source_t *source = obs_weak_source_get_source(s.scene);
		obs_source_t *transition =
			obs_weak_source_get_source(s.transition);

		if (source && transition) {
			const char *sceneName = obs_source_get_name(source);
			const char *transitionName =
				obs_source_get_name(transition);
			obs_data_set_string(array_obj, "scene", sceneName);
			obs_data_set_string(array_obj, "transition",
					    transitionName);
			obs_data_set_double(array_obj, "delay", s.delay);
			obs_data_set_string(array_obj, "str",
					    s.randomSwitchStr.c_str());
			obs_data_array_push_back(randomArray, array_obj);
			obs_source_release(source);
			obs_source_release(transition);
		}

		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "randomSwitches", randomArray);
	obs_data_array_release(randomArray);
}

void SwitcherData::loadRandomSwitches(obs_data_t *obj)
{
	switcher->randomSwitches.clear();

	obs_data_array_t *randomArray =
		obs_data_get_array(obj, "randomSwitches");
	size_t count = obs_data_array_count(randomArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj = obs_data_array_item(randomArray, i);

		const char *scene = obs_data_get_string(array_obj, "scene");
		const char *transition =
			obs_data_get_string(array_obj, "transition");
		double delay = obs_data_get_double(array_obj, "delay");
		const char *str = obs_data_get_string(array_obj, "str");

		switcher->randomSwitches.emplace_back(
			GetWeakSourceByName(scene),
			GetWeakTransitionByName(transition), delay, str);

		obs_data_release(array_obj);
	}
	obs_data_array_release(randomArray);
}
