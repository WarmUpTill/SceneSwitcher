#include <QFileDialog>
#include <QTextStream>
#include <QDateTime>
#include <functional>
#include <curl/curl.h>

#include "headers/advanced-scene-switcher.hpp"
#include "headers/curl-helper.hpp"
#include "headers/utility.hpp"

#define LOCAL_FILE_IDX 0
#define REMOTE_FILE_IDX 1

std::hash<std::string> strHash;

void AdvSceneSwitcher::on_browseButton_clicked()
{
	QString path = QFileDialog::getOpenFileName(
		this, tr("Select a file to write to ..."), QDir::currentPath(),
		tr("Text files (*.txt)"));
	if (!path.isEmpty())
		ui->writePathLineEdit->setText(path);
}

void AdvSceneSwitcher::on_readFileCheckBox_stateChanged(int state)
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

void AdvSceneSwitcher::on_readPathLineEdit_textChanged(const QString &text)
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

void AdvSceneSwitcher::on_writePathLineEdit_textChanged(const QString &text)
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

void AdvSceneSwitcher::on_browseButton_2_clicked()
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

static size_t WriteCallback(void *contents, size_t size, size_t nmemb,
			    void *userp)
{
	((std::string *)userp)->append((char *)contents, size * nmemb);
	return size * nmemb;
}

std::string getRemoteData(std::string &url)
{
	std::string readBuffer;

	if (switcher->curl && f_curl_setopt && f_curl_perform) {
		f_curl_setopt(switcher->curl, CURLOPT_URL, url.c_str());
		f_curl_setopt(switcher->curl, CURLOPT_WRITEFUNCTION,
			      WriteCallback);
		f_curl_setopt(switcher->curl, CURLOPT_WRITEDATA, &readBuffer);
		f_curl_perform(switcher->curl);
	}
	return readBuffer;
}

bool compareIgnoringLineEnding(QString &s1, QString &s2)
{
	/*
	Im using QTextStream here so the conversion between different lineendings is done by QT.
	QT itself uses only the linefeed internally so the input by the user is always using that,
	but the files selected by the user might use different line endings.
	If you are reading this and know of a cleaner way to do this, please let me know :)
	*/
	QTextStream s1stream(&s1);
	QTextStream s2stream(&s2);

	while (!s1stream.atEnd() && !s2stream.atEnd()) {
		QString s1s = s1stream.readLine();
		QString s2s = s2stream.readLine();
		if (QString::compare(s1s, s2s, Qt::CaseSensitive) != 0) {
			return false;
		}
	}
	return true;
}

bool matchFileContent(QString &filedata, FileSwitch &s)
{
	if (s.onlyMatchIfChanged) {
		size_t newHash = strHash(filedata.toUtf8().constData());
		if (newHash == s.lastHash)
			return false;
		s.lastHash = newHash;
	}

	if (s.useRegex) {
		return filedata.contains(
			QRegularExpression(QString::fromStdString(s.text)));
	}

	QString text = QString::fromStdString(s.text);
	return compareIgnoringLineEnding(text, filedata);
}

bool checkRemoteFileContent(FileSwitch &s)
{
	std::string data = getRemoteData(s.file);
	QString text = QString::fromStdString(s.text);
	return matchFileContent(text, s);
}

bool checkLocalFileContent(FileSwitch &s)
{
	QString t = QString::fromStdString(s.text);
	QFile file(QString::fromStdString(s.file));
	if (!file.open(QIODevice::ReadOnly))
		return false;

	if (s.useTime) {
		QDateTime newLastMod = QFileInfo(file).lastModified();
		if (s.lastMod == newLastMod)
			return false;
		s.lastMod = newLastMod;
	}

	QString filedata = QTextStream(&file).readAll();
	bool match = matchFileContent(filedata, s);

	file.close();
	return match;
}

void SwitcherData::checkFileContent(bool &match, OBSWeakSource &scene,
				    OBSWeakSource &transition)
{
	for (FileSwitch &s : fileSwitches) {
		bool equal = false;
		if (s.remote) {
			equal = checkRemoteFileContent(s);
		} else {
			equal = checkLocalFileContent(s);
		}

		if (equal) {
			scene = s.scene;
			transition = s.transition;
			match = true;

			if (verbose)
				s.logMatch();
			break;
		}
	}
}

void AdvSceneSwitcher::on_browseButton_3_clicked()
{
	QString path = QFileDialog::getOpenFileName(
		this, tr("Select a file to read from ..."), QDir::currentPath(),
		tr("Any files (*.*)"));
	if (!path.isEmpty())
		ui->filePathLineEdit->setText(path);
}

void AdvSceneSwitcher::on_fileType_currentIndexChanged(int idx)
{
	if (idx == -1)
		return;
	if (idx == LOCAL_FILE_IDX) {
		ui->browseButton_3->setDisabled(false);
		ui->fileContentTimeCheckBox->setDisabled(false);
		ui->remoteFileWarningLabel->hide();
	}
	if (idx == REMOTE_FILE_IDX) {
		ui->browseButton_3->setDisabled(true);
		ui->fileContentTimeCheckBox->setDisabled(true);
		ui->remoteFileWarningLabel->show();
	}
}

void AdvSceneSwitcher::on_fileAdd_clicked()
{
	QString sceneName = ui->fileScenes->currentText();
	QString transitionName = ui->fileTransitions->currentText();
	QString fileName = ui->filePathLineEdit->text();
	QString text = ui->fileTextEdit->toPlainText();
	bool remote = (ui->fileType->currentIndex() == REMOTE_FILE_IDX);
	bool useRegex = ui->fileContentRegExCheckBox->isChecked();
	bool useTime = ui->fileContentTimeCheckBox->isChecked();
	bool onlyMatchIfChanged = ui->onlyMatchIfChanged->isChecked();

	if (sceneName.isEmpty() || transitionName.isEmpty() ||
	    fileName.isEmpty() || text.isEmpty())
		return;

	OBSWeakSource source = GetWeakSourceByQString(sceneName);
	OBSWeakSource transition = GetWeakTransitionByQString(transitionName);

	QString switchText = MakeFileSwitchName(sceneName, transitionName,
						fileName, text, useRegex,
						useTime, onlyMatchIfChanged);
	QVariant v = QVariant::fromValue(switchText);

	QListWidgetItem *item =
		new QListWidgetItem(switchText, ui->fileScenesList);
	item->setData(Qt::UserRole, v);

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->fileSwitches.emplace_back(source, transition,
					    fileName.toUtf8().constData(),
					    text.toUtf8().constData(), remote,
					    useRegex, useTime,
					    onlyMatchIfChanged);
}

void AdvSceneSwitcher::on_fileRemove_clicked()
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

void AdvSceneSwitcher::on_fileScenesList_currentRowChanged(int idx)
{
	if (loading)
		return;
	if (idx == -1)
		return;

	std::lock_guard<std::mutex> lock(switcher->m);

	if ((int)switcher->fileSwitches.size() <= idx)
		return;
	FileSwitch s = switcher->fileSwitches[idx];

	std::string sceneName = GetWeakSourceName(s.scene);
	std::string transitionName = GetWeakSourceName(s.transition);

	ui->fileScenes->setCurrentText(sceneName.c_str());
	ui->fileTransitions->setCurrentText(transitionName.c_str());
	ui->fileTextEdit->setPlainText(s.text.c_str());
	ui->filePathLineEdit->setText(s.file.c_str());
	if (s.remote)
		ui->fileType->setCurrentIndex(REMOTE_FILE_IDX);
	else
		ui->fileType->setCurrentIndex(LOCAL_FILE_IDX);
	ui->fileContentRegExCheckBox->setChecked(s.useRegex);
	ui->fileContentTimeCheckBox->setChecked(s.useTime);
	ui->onlyMatchIfChanged->setChecked(s.onlyMatchIfChanged);
}

void AdvSceneSwitcher::on_fileUp_clicked()
{
	int index = ui->fileScenesList->currentRow();
	if (index != -1 && index != 0) {
		ui->fileScenesList->insertItem(
			index - 1, ui->fileScenesList->takeItem(index));
		ui->fileScenesList->setCurrentRow(index - 1);

		std::lock_guard<std::mutex> lock(switcher->m);

		iter_swap(switcher->fileSwitches.begin() + index,
			  switcher->fileSwitches.begin() + index - 1);
	}
}

void AdvSceneSwitcher::on_fileDown_clicked()
{
	int index = ui->fileScenesList->currentRow();
	if (index != -1 && index != ui->fileScenesList->count() - 1) {
		ui->fileScenesList->insertItem(
			index + 1, ui->fileScenesList->takeItem(index));
		ui->fileScenesList->setCurrentRow(index + 1);

		std::lock_guard<std::mutex> lock(switcher->m);

		iter_swap(switcher->fileSwitches.begin() + index,
			  switcher->fileSwitches.begin() + index + 1);
	}
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
			obs_data_set_bool(array_obj, "remote", s.remote);
			obs_data_set_bool(array_obj, "useRegex", s.useRegex);
			obs_data_set_bool(array_obj, "useTime", s.useTime);
			obs_data_set_bool(array_obj, "onlyMatchIfChanged",
					  s.onlyMatchIfChanged);
			obs_data_array_push_back(fileArray, array_obj);
		}
		obs_source_release(source);
		obs_source_release(transition);
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
		bool remote = obs_data_get_bool(array_obj, "remote");
		bool useRegex = obs_data_get_bool(array_obj, "useRegex");
		bool useTime = obs_data_get_bool(array_obj, "useTime");
		bool onlyMatchIfChanged =
			obs_data_get_bool(array_obj, "onlyMatchIfChanged");

		switcher->fileSwitches.emplace_back(
			GetWeakSourceByName(scene),
			GetWeakTransitionByName(transition), file, text, remote,
			useRegex, useTime, onlyMatchIfChanged);

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

void AdvSceneSwitcher::setupFileTab()
{
	populateSceneSelection(ui->fileScenes, false);
	populateTransitionSelection(ui->fileTransitions);

	ui->fileType->addItem("local");
	ui->fileType->addItem("remote");
	ui->remoteFileWarningLabel->setText(
		"Note that the scene switcher will try to access the remote location every " +
		QString::number(switcher->interval) + "ms");

	if (switcher->curl)
		ui->libcurlWarning->setVisible(false);

	for (auto &s : switcher->fileSwitches) {
		std::string sceneName = GetWeakSourceName(s.scene);
		std::string transitionName = GetWeakSourceName(s.transition);
		QString listText = MakeFileSwitchName(
			sceneName.c_str(), transitionName.c_str(),
			s.file.c_str(), s.text.c_str(), s.useRegex, s.useTime,
			s.onlyMatchIfChanged);

		QListWidgetItem *item =
			new QListWidgetItem(listText, ui->fileScenesList);
		item->setData(Qt::UserRole, listText);
	}

	ui->readPathLineEdit->setText(
		QString::fromStdString(switcher->fileIO.readPath.c_str()));
	ui->readFileCheckBox->setChecked(switcher->fileIO.readEnabled);
	ui->writePathLineEdit->setText(
		QString::fromStdString(switcher->fileIO.writePath.c_str()));

	if (ui->readFileCheckBox->checkState()) {
		ui->browseButton_2->setDisabled(false);
		ui->readPathLineEdit->setDisabled(false);
	} else {
		ui->browseButton_2->setDisabled(true);
		ui->readPathLineEdit->setDisabled(true);
	}
}

static inline QString MakeFileSwitchName(const QString &scene,
					 const QString &transition,
					 const QString &fileName,
					 const QString &text, bool useRegex,
					 bool useTime, bool onlyMatchIfChanged)
{
	QString switchName = QStringLiteral("Switch to ") + scene +
			     QStringLiteral(" using ") + transition +
			     QStringLiteral(" if ") + fileName;
	if (useTime)
		switchName += QStringLiteral(" was modified and");
	if (onlyMatchIfChanged)
		switchName += QStringLiteral(" if the contents changed and");
	switchName += QStringLiteral(" contains");
	if (useRegex)
		switchName += QStringLiteral(" (RegEx): \n\"");
	else
		switchName += QStringLiteral(": \n\"");
	if (text.length() > 30)
		switchName += text.left(27) + QStringLiteral("...\"");
	else
		switchName += text + QStringLiteral("\"");

	return switchName;
}
