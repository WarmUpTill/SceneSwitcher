#include <QFileDialog>
#include <QTextStream>
#include <QDateTime>
#include <functional>
#include <curl/curl.h>

#include "headers/advanced-scene-switcher.hpp"
#include "headers/curl-helper.hpp"
#include "headers/utility.hpp"

static QMetaObject::Connection addPulse;
static std::hash<std::string> strHash;

void AdvSceneSwitcher::on_browseButton_clicked()
{
	QString path = QFileDialog::getOpenFileName(
		this,
		tr(obs_module_text("AdvSceneSwitcher.fileTab.selectWrite")),
		QDir::currentPath(),
		tr(obs_module_text("AdvSceneSwitcher.fileTab.textFileType")));
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
		this,
		tr(obs_module_text("AdvSceneSwitcher.fileTab.selectRead")),
		QDir::currentPath(),
		tr(obs_module_text("AdvSceneSwitcher.fileTab.anyFileType")));
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

void SwitcherData::writeToStatusFile(QString msg)
{
	if (!fileIO.writeEnabled || fileIO.writePath.empty())
		return;

	QFile file(QString::fromStdString(fileIO.writePath));
	if (file.open(QIODevice::ReadWrite)) {
		QTextStream stream(&file);
		stream << msg << endl;
	}
	file.close();
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

	while (!s1stream.atEnd() || !s2stream.atEnd()) {
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
		QRegExp rx(QString::fromStdString(s.text));
		return rx.exactMatch(filedata);
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
	if (s.file.empty() || !file.open(QIODevice::ReadOnly))
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

void AdvSceneSwitcher::on_fileAdd_clicked()
{
	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->fileSwitches.emplace_back();

	QListWidgetItem *item;
	item = new QListWidgetItem(ui->fileSwitches);
	ui->fileSwitches->addItem(item);
	FileSwitchWidget *sw =
		new FileSwitchWidget(&switcher->fileSwitches.back());
	item->setSizeHint(sw->minimumSizeHint());
	ui->fileSwitches->setItemWidget(item, sw);
}

void AdvSceneSwitcher::on_fileRemove_clicked()
{
	QListWidgetItem *item = ui->fileSwitches->currentItem();
	if (!item)
		return;

	{
		std::lock_guard<std::mutex> lock(switcher->m);
		int idx = ui->fileSwitches->currentRow();
		auto &switches = switcher->fileSwitches;
		switches.erase(switches.begin() + idx);
	}

	delete item;
}

void AdvSceneSwitcher::on_fileSwitches_currentRowChanged(int idx)
{
	if (loading)
		return;
	if (idx == -1)
		return;

	std::lock_guard<std::mutex> lock(switcher->m);

	if ((int)switcher->fileSwitches.size() <= idx)
		return;
	FileSwitch s = switcher->fileSwitches[idx];
	if (s.remote)
		ui->remoteFileWarningLabel->show();
	else
		ui->remoteFileWarningLabel->hide();
}

void AdvSceneSwitcher::on_fileUp_clicked()
{
	int index = ui->fileSwitches->currentRow();
	if (!listMoveUp(ui->fileSwitches))
		return;

	FileSwitchWidget *s1 = (FileSwitchWidget *)ui->fileSwitches->itemWidget(
		ui->fileSwitches->item(index));
	FileSwitchWidget *s2 = (FileSwitchWidget *)ui->fileSwitches->itemWidget(
		ui->fileSwitches->item(index - 1));
	FileSwitchWidget::swapSwitchData(s1, s2);

	std::lock_guard<std::mutex> lock(switcher->m);

	std::swap(switcher->fileSwitches[index],
		  switcher->fileSwitches[index - 1]);
}

void AdvSceneSwitcher::on_fileDown_clicked()
{
	int index = ui->fileSwitches->currentRow();

	if (!listMoveDown(ui->fileSwitches))
		return;

	FileSwitchWidget *s1 = (FileSwitchWidget *)ui->fileSwitches->itemWidget(
		ui->fileSwitches->item(index));
	FileSwitchWidget *s2 = (FileSwitchWidget *)ui->fileSwitches->itemWidget(
		ui->fileSwitches->item(index + 1));
	FileSwitchWidget::swapSwitchData(s1, s2);

	std::lock_guard<std::mutex> lock(switcher->m);

	std::swap(switcher->fileSwitches[index],
		  switcher->fileSwitches[index + 1]);
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
	ui->remoteFileWarningLabel->setText(
		obs_module_text("AdvSceneSwitcher.fileTab.remoteFileWarning1") +
		QString::number(switcher->interval) +
		obs_module_text("AdvSceneSwitcher.fileTab.remoteFileWarning2"));
	ui->remoteFileWarningLabel->hide();

	if (switcher->curl)
		ui->libcurlWarning->setVisible(false);

	for (auto &s : switcher->fileSwitches) {
		QListWidgetItem *item;
		item = new QListWidgetItem(ui->fileSwitches);
		ui->fileSwitches->addItem(item);
		FileSwitchWidget *sw = new FileSwitchWidget(&s);
		item->setSizeHint(sw->minimumSizeHint());
		ui->fileSwitches->setItemWidget(item, sw);
	}

	if (switcher->fileSwitches.size() == 0)
		addPulse = PulseWidget(ui->fileAdd, QColor(Qt::green));

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

FileSwitchWidget::FileSwitchWidget(FileSwitch *s) : SwitchWidget(s, false)
{
	fileType = new QComboBox();
	filePath = new QLineEdit();
	browseButton =
		new QPushButton(obs_module_text("AdvSceneSwitcher.browse"));
	browseButton->setStyleSheet("border:1px solid gray;");
	matchText = new QPlainTextEdit();
	useRegex = new QCheckBox(
		obs_module_text("AdvSceneSwitcher.fileTab.useRegExp"));
	checkModificationDate = new QCheckBox(obs_module_text(
		"AdvSceneSwitcher.fileTab.checkfileContentTime"));
	checkFileContent = new QCheckBox(
		obs_module_text("AdvSceneSwitcher.fileTab.checkfileContent"));

	QWidget::connect(fileType, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(FileTypeChanged(int)));
	QWidget::connect(filePath, SIGNAL(editingFinished()), this,
			 SLOT(FilePathChanged()));
	QWidget::connect(browseButton, SIGNAL(clicked()), this,
			 SLOT(BrowseButtonClicked()));
	QWidget::connect(matchText, SIGNAL(textChanged()), this,
			 SLOT(MatchTextChanged()));
	QWidget::connect(useRegex, SIGNAL(stateChanged(int)), this,
			 SLOT(UseRegexChanged(int)));
	QWidget::connect(checkModificationDate, SIGNAL(stateChanged(int)), this,
			 SLOT(CheckModificationDateChanged(int)));
	QWidget::connect(checkFileContent, SIGNAL(stateChanged(int)), this,
			 SLOT(CheckFileContentChanged(int)));

	fileType->addItem(obs_module_text("AdvSceneSwitcher.fileTab.local"));
	fileType->addItem(obs_module_text("AdvSceneSwitcher.fileTab.remote"));

	if (s) {
		if (s->remote)
			fileType->setCurrentIndex(1);
		else
			fileType->setCurrentIndex(0);
		filePath->setText(QString::fromStdString(s->file));
		matchText->setPlainText(QString::fromStdString(s->text));
		useRegex->setChecked(s->useRegex);
		checkModificationDate->setChecked(s->useTime);
		checkFileContent->setChecked(s->onlyMatchIfChanged);
	}

	setStyleSheet("* { background-color: transparent; }");

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{fileType}}", fileType},
		{"{{filePath}}", filePath},
		{"{{browseButton}}", browseButton},
		{"{{matchText}}", matchText},
		{"{{useRegex}}", useRegex},
		{"{{checkModificationDate}}", checkModificationDate},
		{"{{checkFileContent}}", checkFileContent},
		{"{{scenes}}", scenes},
		{"{{transitions}}", transitions}};

	QVBoxLayout *mainLayout = new QVBoxLayout;
	QHBoxLayout *line1Layout = new QHBoxLayout;
	QHBoxLayout *line2Layout = new QHBoxLayout;
	QHBoxLayout *line3Layout = new QHBoxLayout;
	placeWidgets(obs_module_text("AdvSceneSwitcher.fileTab.entry"),
		     line1Layout, widgetPlaceholders);
	placeWidgets(obs_module_text("AdvSceneSwitcher.fileTab.entry2"),
		     line2Layout, widgetPlaceholders, false);
	placeWidgets(obs_module_text("AdvSceneSwitcher.fileTab.entry3"),
		     line3Layout, widgetPlaceholders);
	mainLayout->addLayout(line1Layout);
	mainLayout->addLayout(line2Layout);
	mainLayout->addLayout(line3Layout);

	setLayout(mainLayout);

	switchData = s;

	loading = false;
}

FileSwitch *FileSwitchWidget::getSwitchData()
{
	return switchData;
}

void FileSwitchWidget::setSwitchData(FileSwitch *s)
{
	switchData = s;
}

void FileSwitchWidget::swapSwitchData(FileSwitchWidget *s1,
				      FileSwitchWidget *s2)
{
	SwitchWidget::swapSwitchData(s1, s2);

	FileSwitch *t = s1->getSwitchData();
	s1->setSwitchData(s2->getSwitchData());
	s2->setSwitchData(t);
}

void FileSwitchWidget::FileTypeChanged(int index)
{
	if (loading || !switchData)
		return;

	if ((file_type)index == LOCAL) {
		browseButton->setDisabled(false);
		checkModificationDate->setDisabled(false);
	} else {
		browseButton->setDisabled(true);
		checkModificationDate->setDisabled(true);
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->remote = (file_type)index == REMOTE;
}

void FileSwitchWidget::FilePathChanged()
{
	if (loading || !switchData)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->file = filePath->text().toUtf8().constData();
}

void FileSwitchWidget::BrowseButtonClicked()
{
	if (loading || !switchData)
		return;

	QString path = QFileDialog::getOpenFileName(
		this,
		tr(obs_module_text("AdvSceneSwitcher.fileTab.selectRead")),
		QDir::currentPath(),
		tr(obs_module_text("AdvSceneSwitcher.fileTab.anyFileType")));
	if (path.isEmpty())
		return;

	filePath->setText(path);
	FilePathChanged();
}

void FileSwitchWidget::MatchTextChanged()
{
	if (loading || !switchData)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->text = matchText->toPlainText().toUtf8().constData();
}

void FileSwitchWidget::UseRegexChanged(int state)
{
	if (loading || !switchData)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->useRegex = state;
}

void FileSwitchWidget::CheckModificationDateChanged(int state)
{
	if (loading || !switchData)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->useTime = state;
}

void FileSwitchWidget::CheckFileContentChanged(int state)
{
	if (loading || !switchData)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->onlyMatchIfChanged = state;
}
