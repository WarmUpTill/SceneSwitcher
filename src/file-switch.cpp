#include <QFileDialog>
#include <QTextStream>
#include <QDateTime>
#include <obs.hpp>
#include "headers/advanced-scene-switcher.hpp"

void SceneSwitcher::on_browseButton_clicked()
{
	QString path = QFileDialog::getOpenFileName(
		this, tr("Select a file to write to ..."), QDir::currentPath(),
		tr("Text files (*.txt)"));
	if (!path.isEmpty())
		ui->writePathLineEdit->setText(path);
}

void SceneSwitcher::on_readFileCheckBox_stateChanged(int state)
{
	if (loading)
		return;

	std::lock_guard<std::mutex> lock(switcher->m);
	if (!state) {
		ui->browseButton_2->setDisabled(true);
		ui->readPathLineEdit->setDisabled(true);
		switcher->fileIO.readEnabled = false;
	} else {
		ui->browseButton_2->setDisabled(false);
		ui->readPathLineEdit->setDisabled(false);
		switcher->fileIO.readEnabled = true;
	}
}

void SceneSwitcher::on_readPathLineEdit_textChanged(const QString &text)
{
	if (loading)
		return;

	std::lock_guard<std::mutex> lock(switcher->m);
	if (text.isEmpty()) {
		switcher->fileIO.readEnabled = false;
		return;
	}
	switcher->fileIO.readEnabled = true;
	switcher->fileIO.readPath = text.toUtf8().constData();
}

void SceneSwitcher::on_writePathLineEdit_textChanged(const QString &text)
{
	if (loading)
		return;

	std::lock_guard<std::mutex> lock(switcher->m);
	if (text.isEmpty()) {
		switcher->fileIO.writeEnabled = false;
		return;
	}
	switcher->fileIO.writeEnabled = true;
	switcher->fileIO.writePath = text.toUtf8().constData();
}

void SceneSwitcher::on_browseButton_2_clicked()
{
	QString path = QFileDialog::getOpenFileName(
		this, tr("Select a file to read from ..."), QDir::currentPath(),
		tr("Any files (*.*)"));
	if (!path.isEmpty())
		ui->readPathLineEdit->setText(path);
}

void SwitcherData::writeSceneInfoToFile()
{
	if (!fileIO.writeEnabled || fileIO.writePath.empty())
		return;

	obs_source_t *currentSource = obs_frontend_get_current_scene();

	QFile file(QString::fromStdString(fileIO.writePath));
	if (file.open(QIODevice::WriteOnly)) {
		const char *msg = obs_source_get_name(currentSource);
		file.write(msg, qstrlen(msg));
		file.close();
	}
	obs_source_release(currentSource);
}

void SwitcherData::checkSwitchInfoFromFile(bool &match, OBSWeakSource &scene,
					   OBSWeakSource &transition)
{
	if (!fileIO.readEnabled || fileIO.readPath.empty())
		return;

	QFile file(QString::fromStdString(fileIO.readPath));
	if (file.open(QIODevice::ReadOnly)) {
		QTextStream in(&file);
		QString sceneStr = in.readLine();
		obs_source_t *sceneRead =
			obs_get_source_by_name(sceneStr.toUtf8().constData());
		if (sceneRead) {
			obs_weak_source_t *sceneReadWs =
				obs_source_get_weak_source(sceneRead);

			match = true;
			scene = sceneReadWs;
			transition = nullptr;

			obs_weak_source_release(sceneReadWs);
			obs_source_release(sceneRead);
		}
		file.close();
	}
}

void SwitcherData::checkFileContent(bool &match, OBSWeakSource &scene,
				    OBSWeakSource &transition)
{
	for (FileSwitch &s : fileSwitches) {
		bool equal = false;
		QString t = QString::fromStdString(s.text);
		QFile file(QString::fromStdString(s.file));
		if (!file.open(QIODevice::ReadOnly))
			continue;

		if (s.useTime) {
			QDateTime newLastMod = QFileInfo(file).lastModified();
			if (s.lastMod == newLastMod)
				continue;
			s.lastMod = newLastMod;
		}

		if (s.useRegex) {
			QTextStream in(&file);
			QRegExp rx(t);
			equal = rx.exactMatch(in.readAll());
		} else {
			/*Im using QTextStream here so the conversion between different lineendings is done by QT.
			 *QT itself uses only the linefeed internally so the input by the user is always using that,
			 *but the files selected by the user might use different line endings.
			 *If you are reading this and know of a cleaner way to do this, please let me know :)
			 */
			QTextStream in(&file);
			QTextStream text(&t);
			while (!in.atEnd() && !text.atEnd()) {
				QString fileLine = in.readLine();
				QString textLine = text.readLine();
				if (QString::compare(fileLine, textLine,
						     Qt::CaseSensitive) != 0) {
					equal = false;
					break;
				} else {
					equal = true;
				}
			}
		}
		file.close();

		if (equal) {
			scene = s.scene;
			transition = s.transition;
			match = true;

			break;
		}
	}
}

void SceneSwitcher::on_browseButton_3_clicked()
{
	QString path = QFileDialog::getOpenFileName(
		this, tr("Select a file to read from ..."), QDir::currentPath(),
		tr("Any files (*.*)"));
	if (!path.isEmpty())
		ui->filePathLineEdit->setText(path);
}

void SceneSwitcher::on_fileAdd_clicked()
{
	QString sceneName = ui->fileScenes->currentText();
	QString transitionName = ui->fileTransitions->currentText();
	QString fileName = ui->filePathLineEdit->text();
	QString text = ui->fileTextEdit->toPlainText();
	bool useRegex = ui->fileContentRegExCheckBox->isChecked();
	bool useTime = ui->fileContentTimeCheckBox->isChecked();

	if (sceneName.isEmpty() || transitionName.isEmpty() ||
	    fileName.isEmpty() || text.isEmpty())
		return;

	OBSWeakSource source = GetWeakSourceByQString(sceneName);
	OBSWeakSource transition = GetWeakTransitionByQString(transitionName);

	QString switchText = MakeFileSwitchName(
		sceneName, transitionName, fileName, text, useRegex, useTime);
	QVariant v = QVariant::fromValue(switchText);

	QListWidgetItem *item =
		new QListWidgetItem(switchText, ui->fileScenesList);
	item->setData(Qt::UserRole, v);

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->fileSwitches.emplace_back(source, transition,
					    fileName.toUtf8().constData(),
					    text.toUtf8().constData(), useRegex,
					    useTime);
}

void SceneSwitcher::on_fileRemove_clicked()
{
	QListWidgetItem *item = ui->fileScenesList->currentItem();
	if (!item)
		return;

	int idx = ui->fileScenesList->currentRow();
	if (idx == -1)
		return;

	{
		std::lock_guard<std::mutex> lock(switcher->m);

		auto &switches = switcher->fileSwitches;
		switches.erase(switches.begin() + idx);
	}
	qDeleteAll(ui->fileScenesList->selectedItems());
}

void SceneSwitcher::on_fileScenesList_currentRowChanged(int idx)
{
	if (loading)
		return;
	if (idx == -1)
		return;

	std::lock_guard<std::mutex> lock(switcher->m);

	if (switcher->fileSwitches.size() <= idx)
		return;
	FileSwitch s = switcher->fileSwitches[idx];

	std::string sceneName = GetWeakSourceName(s.scene);
	std::string transitionName = GetWeakSourceName(s.transition);

	ui->fileScenes->setCurrentText(sceneName.c_str());
	ui->fileTransitions->setCurrentText(transitionName.c_str());
	ui->fileTextEdit->setPlainText(s.text.c_str());
	ui->filePathLineEdit->setText(s.file.c_str());
	ui->fileContentRegExCheckBox->setChecked(s.useRegex);
	ui->fileContentTimeCheckBox->setChecked(s.useTime);
}

void SwitcherData::saveFileSwitches(obs_data_t *obj)
{
	obs_data_array_t *fileArray = obs_data_array_create();
	for (FileSwitch &s : switcher->fileSwitches) {
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
			obs_data_set_string(array_obj, "file", s.file.c_str());
			obs_data_set_string(array_obj, "text", s.text.c_str());
			obs_data_set_bool(array_obj, "useRegex", s.useRegex);
			obs_data_set_bool(array_obj, "useTime", s.useTime);
			obs_data_array_push_back(fileArray, array_obj);
			obs_source_release(source);
			obs_source_release(transition);
		}

		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "fileSwitches", fileArray);
	obs_data_array_release(fileArray);

	obs_data_set_bool(obj, "readEnabled", switcher->fileIO.readEnabled);
	obs_data_set_string(obj, "readPath", switcher->fileIO.readPath.c_str());
	obs_data_set_bool(obj, "writeEnabled", switcher->fileIO.writeEnabled);
	obs_data_set_string(obj, "writePath",
			    switcher->fileIO.writePath.c_str());
}

void SwitcherData::loadFileSwitches(obs_data_t *obj)
{
	switcher->fileSwitches.clear();
	obs_data_array_t *fileArray = obs_data_get_array(obj, "fileSwitches");
	size_t count = obs_data_array_count(fileArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj = obs_data_array_item(fileArray, i);

		const char *scene = obs_data_get_string(array_obj, "scene");
		const char *transition =
			obs_data_get_string(array_obj, "transition");
		const char *file = obs_data_get_string(array_obj, "file");
		const char *text = obs_data_get_string(array_obj, "text");
		bool useRegex = obs_data_get_bool(array_obj, "useRegex");
		bool useTime = obs_data_get_bool(array_obj, "useTime");

		switcher->fileSwitches.emplace_back(
			GetWeakSourceByName(scene),
			GetWeakTransitionByName(transition), file, text,
			useRegex, useTime);

		obs_data_release(array_obj);
	}
	obs_data_array_release(fileArray);

	obs_data_set_default_bool(obj, "readEnabled", false);
	switcher->fileIO.readEnabled = obs_data_get_bool(obj, "readEnabled");
	switcher->fileIO.readPath = obs_data_get_string(obj, "readPath");
	obs_data_set_default_bool(obj, "writeEnabled", false);
	switcher->fileIO.writeEnabled = obs_data_get_bool(obj, "writeEnabled");
	switcher->fileIO.writePath = obs_data_get_string(obj, "writePath");
}
