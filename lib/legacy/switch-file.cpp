#include "advanced-scene-switcher.hpp"
#include "curl-helper.hpp"
#include "layout-helpers.hpp"
#include "source-helpers.hpp"
#include "switcher-data.hpp"
#include "ui-helpers.hpp"
#include "utility.hpp"

#include <obs-frontend-api.h>
#include <QtGlobal>
#include <QFileDialog>
#include <QTextStream>
#include <QDateTime>
#include <functional>
#include <regex>
#include <curl/curl.h>

namespace advss {

bool FileSwitch::pause = false;
static QObject *addPulse = nullptr;
static std::hash<std::string> strHash;

static void writeToStatusFile(const QString &msg)
{
	if (!GetSwitcher() || !GetSwitcher()->fileIO.writeEnabled ||
	    GetSwitcher()->fileIO.writePath.empty()) {
		return;
	}

	QFile file(QString::fromStdString(GetSwitcher()->fileIO.writePath));
	if (file.open(QIODevice::ReadWrite)) {
		QTextStream stream(&file);
		stream << msg << Qt::endl;
	}
	file.close();
}

static bool _ = []() {
	AddStartStep(
		[]() { writeToStatusFile("Advanced Scene Switcher running"); });
	AddStopStep(
		[]() { writeToStatusFile("Advanced Scene Switcher stopped"); });
	return true;
}();

void AdvSceneSwitcher::on_browseButton_clicked()
{
	QString path = QFileDialog::getOpenFileName(
		this,
		tr(obs_module_text("AdvSceneSwitcher.fileTab.selectWrite")),
		QDir::currentPath(),
		tr(obs_module_text("AdvSceneSwitcher.fileTab.textFileType")));
	if (!path.isEmpty()) {
		ui->writePathLineEdit->setText(path);
	}
}

void AdvSceneSwitcher::on_readFileCheckBox_stateChanged(int state)
{
	if (loading) {
		return;
	}

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
	if (loading) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	if (text.isEmpty()) {
		switcher->fileIO.readEnabled = false;
		switcher->fileIO.readPath = "";
		return;
	}
	switcher->fileIO.readEnabled = true;
	switcher->fileIO.readPath = text.toUtf8().constData();
}

void AdvSceneSwitcher::on_writePathLineEdit_textChanged(const QString &text)
{
	if (loading) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	if (text.isEmpty()) {
		switcher->fileIO.writeEnabled = false;
		switcher->fileIO.writePath = "";
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
	if (!path.isEmpty()) {
		ui->readPathLineEdit->setText(path);
	}
}

void SwitcherData::writeSceneInfoToFile()
{
	if (!fileIO.writeEnabled || fileIO.writePath.empty()) {
		return;
	}

	QFile file(QString::fromStdString(fileIO.writePath));
	if (file.open(QIODevice::WriteOnly)) {
		// switcher->currentScene cannot be used here as scene might
		// have changed already
		obs_source_t *source = obs_frontend_get_current_scene();
		auto msg = obs_source_get_name(source);
		obs_source_release(source);
		file.write(msg, qstrlen(msg));
		file.close();
	}
}

bool SwitcherData::checkSwitchInfoFromFile(OBSWeakSource &scene,
					   OBSWeakSource &transition)
{
	if (!fileIO.readEnabled || fileIO.readPath.empty() ||
	    FileSwitch::pause) {
		return false;
	}

	QFile file(QString::fromStdString(fileIO.readPath));
	if (!file.open(QIODevice::ReadOnly)) {
		return false;
	}

	bool match = false;
	QTextStream in(&file);

	QString sceneStr = in.readLine();
	OBSWeakSource sceneRead = GetWeakSourceByQString(sceneStr);

	QString transitionStr = in.readLine();
	OBSWeakSource transitionRead =
		GetWeakTransitionByQString(transitionStr);

	if (sceneRead) {
		match = true;
		scene = sceneRead;
		transition = transitionRead;

		vblog(LOG_INFO,
		      "match for 'file' - read scene '%s' and transition '%s' from file '%s'",
		      sceneStr.toUtf8().constData(),
		      transitionStr.toUtf8().constData(),
		      fileIO.readPath.c_str());
	}
	file.close();

	return match;
}

static size_t WriteCallback(void *contents, size_t size, size_t nmemb,
			    void *userp)
{
	((std::string *)userp)->append((char *)contents, size * nmemb);
	return size * nmemb;
}

static std::string getRemoteData(std::string &url)
{
	std::string readBuffer;

	CurlHelper::SetOpt(CURLOPT_URL, url.c_str());
	CurlHelper::SetOpt(CURLOPT_WRITEFUNCTION, WriteCallback);
	CurlHelper::SetOpt(CURLOPT_WRITEDATA, &readBuffer);

	// Set timeout to at least one second
	int timeout = switcher->interval / 1000;
	if (timeout == 0) {
		timeout = 1;
	}
	CurlHelper::SetOpt(CURLOPT_TIMEOUT, 1);
	CurlHelper::Perform();

	return readBuffer;
}

bool matchFileContent(QString &filedata, FileSwitch &s)
{
	if (s.onlyMatchIfChanged) {
		size_t newHash = strHash(filedata.toUtf8().constData());
		if (newHash == s.lastHash) {
			return false;
		}
		s.lastHash = newHash;
	}

	if (s.useRegex) {
		try {
			std::regex expr(s.text);
			return std::regex_match(filedata.toStdString(), expr);
		} catch (const std::regex_error &) {
			return false;
		}
	}

	QString text = QString::fromStdString(s.text);
	return CompareIgnoringLineEnding(text, filedata);
}

bool checkRemoteFileContent(FileSwitch &s)
{
	std::string data = getRemoteData(s.file);
	QString qdata = QString::fromStdString(data);
	return matchFileContent(qdata, s);
}

bool checkLocalFileContent(FileSwitch &s)
{
	QString t = QString::fromStdString(s.text);
	QFile file(QString::fromStdString(s.file));
	if (!file.open(QIODevice::ReadOnly)) {
		return false;
	}

	if (s.useTime) {
		QDateTime newLastMod = QFileInfo(file).lastModified();
		if (s.lastMod == newLastMod) {
			return false;
		}
		s.lastMod = newLastMod;
	}

	QString filedata = QTextStream(&file).readAll();
	bool match = matchFileContent(filedata, s);

	file.close();
	return match;
}

bool SwitcherData::checkFileContent(OBSWeakSource &scene,
				    OBSWeakSource &transition)
{
	if (FileSwitch::pause) {
		return false;
	}

	bool match = false;
	for (FileSwitch &s : fileSwitches) {
		if (!s.initialized()) {
			continue;
		}

		bool equal = false;
		if (s.remote) {
			equal = checkRemoteFileContent(s);
		} else {
			equal = checkLocalFileContent(s);
		}

		if (equal) {
			scene = s.getScene();
			transition = s.transition;
			match = true;

			if (VerboseLoggingEnabled()) {
				s.logMatch();
			}
			break;
		}
	}
	return match;
}

void AdvSceneSwitcher::on_fileAdd_clicked()
{
	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->fileSwitches.emplace_back();

	listAddClicked(ui->fileSwitches,
		       new FileSwitchWidget(this,
					    &switcher->fileSwitches.back()),
		       &addPulse);

	ui->fileHelp->setVisible(false);
}

void AdvSceneSwitcher::on_fileRemove_clicked()
{
	QListWidgetItem *item = ui->fileSwitches->currentItem();
	if (!item) {
		return;
	}

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
	if (loading) {
		return;
	}
	if (idx == -1) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);

	if ((int)switcher->fileSwitches.size() <= idx) {
		return;
	}
	FileSwitch s = switcher->fileSwitches[idx];
	if (s.remote) {
		ui->remoteFileWarningLabel->show();
	} else {
		ui->remoteFileWarningLabel->hide();
	}
}

void AdvSceneSwitcher::on_fileUp_clicked()
{
	int index = ui->fileSwitches->currentRow();
	if (!listMoveUp(ui->fileSwitches)) {
		return;
	}

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

	if (!listMoveDown(ui->fileSwitches)) {
		return;
	}

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
	for (FileSwitch &s : fileSwitches) {
		obs_data_t *array_obj = obs_data_create();

		s.save(array_obj);
		obs_data_array_push_back(fileArray, array_obj);

		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "fileSwitches", fileArray);
	obs_data_array_release(fileArray);

	obs_data_set_bool(obj, "readEnabled", fileIO.readEnabled);
	obs_data_set_string(obj, "readPath", fileIO.readPath.c_str());
	obs_data_set_bool(obj, "writeEnabled", fileIO.writeEnabled);
	obs_data_set_string(obj, "writePath", fileIO.writePath.c_str());
}

void SwitcherData::loadFileSwitches(obs_data_t *obj)
{
	fileSwitches.clear();
	obs_data_array_t *fileArray = obs_data_get_array(obj, "fileSwitches");
	size_t count = obs_data_array_count(fileArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj = obs_data_array_item(fileArray, i);

		fileSwitches.emplace_back();
		fileSwitches.back().load(array_obj);

		obs_data_release(array_obj);
	}
	obs_data_array_release(fileArray);

	obs_data_set_default_bool(obj, "readEnabled", false);
	fileIO.readEnabled = obs_data_get_bool(obj, "readEnabled");
	fileIO.readPath = obs_data_get_string(obj, "readPath");
	obs_data_set_default_bool(obj, "writeEnabled", false);
	fileIO.writeEnabled = obs_data_get_bool(obj, "writeEnabled");
	fileIO.writePath = obs_data_get_string(obj, "writePath");
}

void AdvSceneSwitcher::SetupFileTab()
{
	ui->remoteFileWarningLabel->setText(
		obs_module_text("AdvSceneSwitcher.fileTab.remoteFileWarning1") +
		QString::number(switcher->interval) +
		obs_module_text("AdvSceneSwitcher.fileTab.remoteFileWarning2"));
	ui->remoteFileWarningLabel->hide();

	if (CurlHelper::Initialized()) {
		ui->libcurlWarning->setVisible(false);
	}

	for (auto &s : switcher->fileSwitches) {
		QListWidgetItem *item;
		item = new QListWidgetItem(ui->fileSwitches);
		ui->fileSwitches->addItem(item);
		FileSwitchWidget *sw = new FileSwitchWidget(this, &s);
		item->setSizeHint(sw->minimumSizeHint());
		ui->fileSwitches->setItemWidget(item, sw);
	}

	if (switcher->fileSwitches.size() == 0) {
		if (!switcher->disableHints) {
			addPulse =
				HighlightWidget(ui->fileAdd, QColor(Qt::green));
		}
		ui->fileHelp->setVisible(true);
	} else {
		ui->fileHelp->setVisible(false);
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

void FileSwitch::save(obs_data_t *obj)
{
	SceneSwitcherEntry::save(obj);

	obs_data_set_string(obj, "file", file.c_str());
	obs_data_set_string(obj, "text", text.c_str());
	obs_data_set_bool(obj, "remote", remote);
	obs_data_set_bool(obj, "useRegex", useRegex);
	obs_data_set_bool(obj, "useTime", useTime);
	obs_data_set_bool(obj, "onlyMatchIfChanged", onlyMatchIfChanged);
}

void FileSwitch::load(obs_data_t *obj)
{
	SceneSwitcherEntry::load(obj);

	file = obs_data_get_string(obj, "file");
	text = obs_data_get_string(obj, "text");
	remote = obs_data_get_bool(obj, "remote");
	useRegex = obs_data_get_bool(obj, "useRegex");
	useTime = obs_data_get_bool(obj, "useTime");
	onlyMatchIfChanged = obs_data_get_bool(obj, "onlyMatchIfChanged");
}

FileSwitchWidget::FileSwitchWidget(QWidget *parent, FileSwitch *s)
	: SwitchWidget(parent, s, true, true)
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
		if (s->remote) {
			fileType->setCurrentIndex(1);
		} else {
			fileType->setCurrentIndex(0);
		}
		filePath->setText(QString::fromStdString(s->file));
		matchText->setPlainText(QString::fromStdString(s->text));
		useRegex->setChecked(s->useRegex);
		checkModificationDate->setChecked(s->useTime);
		checkFileContent->setChecked(s->onlyMatchIfChanged);
	}

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
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.fileTab.entry"),
		     line1Layout, widgetPlaceholders);
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.fileTab.entry2"),
		     line2Layout, widgetPlaceholders, false);
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.fileTab.entry3"),
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
	if (loading || !switchData) {
		return;
	}

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
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->file = filePath->text().toUtf8().constData();
}

void FileSwitchWidget::BrowseButtonClicked()
{
	if (loading || !switchData) {
		return;
	}

	QString path = QFileDialog::getOpenFileName(
		this,
		tr(obs_module_text("AdvSceneSwitcher.fileTab.selectRead")),
		QDir::currentPath(),
		tr(obs_module_text("AdvSceneSwitcher.fileTab.anyFileType")));
	if (path.isEmpty()) {
		return;
	}

	filePath->setText(path);
	FilePathChanged();
}

void FileSwitchWidget::MatchTextChanged()
{
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->text = matchText->toPlainText().toUtf8().constData();
}

void FileSwitchWidget::UseRegexChanged(int state)
{
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->useRegex = state;
}

void FileSwitchWidget::CheckModificationDateChanged(int state)
{
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->useTime = state;
}

void FileSwitchWidget::CheckFileContentChanged(int state)
{
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->onlyMatchIfChanged = state;
}

} // namespace advss
