#include "headers/macro-condition-edit.hpp"
#include "headers/macro-condition-file.hpp"
#include "headers/utility.hpp"
#include "headers/advanced-scene-switcher.hpp"
#include "headers/curl-helper.hpp"

#include <QTextStream>
#include <QFileDialog>

const int MacroConditionFile::id = 4;

bool MacroConditionFile::_registered = MacroConditionFactory::Register(
	MacroConditionFile::id,
	{MacroConditionFile::Create, MacroConditionFileEdit::Create,
	 "AdvSceneSwitcher.condition.file"});

static std::hash<std::string> strHash;

static size_t WriteCallback(void *contents, size_t size, size_t nmemb,
			    void *userp)
{
	((std::string *)userp)->append((char *)contents, size * nmemb);
	return size * nmemb;
}

static std::string getRemoteData(std::string &url)
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

bool MacroConditionFile::matchFileContent(QString &filedata)
{
	if (_onlyMatchIfChanged) {
		size_t newHash = strHash(filedata.toUtf8().constData());
		if (newHash == _lastHash) {
			return false;
		}
		_lastHash = newHash;
	}

	if (_useRegex) {
		QRegExp rx(QString::fromStdString(_text));
		return rx.exactMatch(filedata);
	}

	QString text = QString::fromStdString(_text);
	return compareIgnoringLineEnding(text, filedata);
}

bool MacroConditionFile::checkRemoteFileContent()
{
	std::string data = getRemoteData(_file);
	QString qdata = QString::fromStdString(data);
	return matchFileContent(qdata);
}

bool MacroConditionFile::checkLocalFileContent()
{
	QString t = QString::fromStdString(_text);
	QFile file(QString::fromStdString(_file));
	if (_file.empty() || !file.open(QIODevice::ReadOnly)) {
		return false;
	}

	if (_useTime) {
		QDateTime newLastMod = QFileInfo(file).lastModified();
		if (_lastMod == newLastMod) {
			return false;
		}
		_lastMod = newLastMod;
	}

	QString filedata = QTextStream(&file).readAll();
	bool match = matchFileContent(filedata);

	file.close();
	return match;
}

bool MacroConditionFile::CheckCondition()
{
	if (_fileType == FileType::REMOTE) {
		return checkRemoteFileContent();
	} else {
		return checkLocalFileContent();
	}
}

bool MacroConditionFile::Save(obs_data_t *obj)
{
	MacroCondition::Save(obj);
	obs_data_set_string(obj, "file", _file.c_str());
	obs_data_set_string(obj, "text", _text.c_str());
	obs_data_set_int(obj, "fileType", static_cast<int>(_fileType));
	obs_data_set_bool(obj, "useRegex", _useRegex);
	obs_data_set_bool(obj, "useTime", _useTime);
	obs_data_set_bool(obj, "onlyMatchIfChanged", _onlyMatchIfChanged);
	return true;
}

bool MacroConditionFile::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_file = obs_data_get_string(obj, "file");
	_text = obs_data_get_string(obj, "text");
	_fileType = static_cast<FileType>(obs_data_get_int(obj, "fileType"));
	_useRegex = obs_data_get_bool(obj, "useRegex");
	_useTime = obs_data_get_bool(obj, "useTime");
	_onlyMatchIfChanged = obs_data_get_bool(obj, "onlyMatchIfChanged");
	return true;
}

MacroConditionFileEdit::MacroConditionFileEdit(
	QWidget *parent, std::shared_ptr<MacroConditionFile> entryData)
	: QWidget(parent)
{
	_fileType = new QComboBox();
	_filePath = new QLineEdit();
	_browseButton =
		new QPushButton(obs_module_text("AdvSceneSwitcher.browse"));
	_browseButton->setStyleSheet("border:1px solid gray;");
	_matchText = new QPlainTextEdit();
	_useRegex = new QCheckBox(
		obs_module_text("AdvSceneSwitcher.fileTab.useRegExp"));
	_checkModificationDate = new QCheckBox(obs_module_text(
		"AdvSceneSwitcher.fileTab.checkfileContentTime"));
	_checkFileContent = new QCheckBox(
		obs_module_text("AdvSceneSwitcher.fileTab.checkfileContent"));

	QWidget::connect(_fileType, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(FileTypeChanged(int)));
	QWidget::connect(_filePath, SIGNAL(editingFinished()), this,
			 SLOT(FilePathChanged()));
	QWidget::connect(_browseButton, SIGNAL(clicked()), this,
			 SLOT(BrowseButtonClicked()));
	QWidget::connect(_matchText, SIGNAL(textChanged()), this,
			 SLOT(MatchTextChanged()));
	QWidget::connect(_useRegex, SIGNAL(stateChanged(int)), this,
			 SLOT(UseRegexChanged(int)));
	QWidget::connect(_checkModificationDate, SIGNAL(stateChanged(int)),
			 this, SLOT(CheckModificationDateChanged(int)));
	QWidget::connect(_checkFileContent, SIGNAL(stateChanged(int)), this,
			 SLOT(OnlyMatchIfChangedChanged(int)));

	_fileType->addItem(obs_module_text("AdvSceneSwitcher.fileTab.local"));
	_fileType->addItem(obs_module_text("AdvSceneSwitcher.fileTab.remote"));

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{fileType}}", _fileType},
		{"{{filePath}}", _filePath},
		{"{{browseButton}}", _browseButton},
		{"{{matchText}}", _matchText},
		{"{{useRegex}}", _useRegex},
		{"{{checkModificationDate}}", _checkModificationDate},
		{"{{checkFileContent}}", _checkFileContent},
	};

	QVBoxLayout *mainLayout = new QVBoxLayout;
	QHBoxLayout *line1Layout = new QHBoxLayout;
	QHBoxLayout *line2Layout = new QHBoxLayout;
	QHBoxLayout *line3Layout = new QHBoxLayout;
	placeWidgets(
		obs_module_text(
			"AdvSceneSwitcher.macro.condition.file.entry.line1"),
		line1Layout, widgetPlaceholders);
	placeWidgets(
		obs_module_text(
			"AdvSceneSwitcher.macro.condition.file.entry.line2"),
		line2Layout, widgetPlaceholders, false);
	placeWidgets(
		obs_module_text(
			"AdvSceneSwitcher.macro.condition.file.entry.line3"),
		line3Layout, widgetPlaceholders);
	mainLayout->addLayout(line1Layout);
	mainLayout->addLayout(line2Layout);
	mainLayout->addLayout(line3Layout);

	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionFileEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_fileType->setCurrentIndex(static_cast<int>(_entryData->_fileType));

	_filePath->setText(QString::fromStdString(_entryData->_file));
	_matchText->setPlainText(QString::fromStdString(_entryData->_text));
	_useRegex->setChecked(_entryData->_useRegex);
	_checkModificationDate->setChecked(_entryData->_useTime);
	_checkFileContent->setChecked(_entryData->_onlyMatchIfChanged);
}

void MacroConditionFileEdit::FileTypeChanged(int index)
{
	if (_loading || !_entryData) {
		return;
	}

	FileType type = static_cast<FileType>(index);

	if (type == FileType::LOCAL) {
		_browseButton->setDisabled(false);
		_checkModificationDate->setDisabled(false);
	} else {
		_browseButton->setDisabled(true);
		_checkModificationDate->setDisabled(true);
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_fileType = type;
}

void MacroConditionFileEdit::FilePathChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_file = _filePath->text().toUtf8().constData();
}

void MacroConditionFileEdit::BrowseButtonClicked()
{
	if (_loading || !_entryData) {
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

	_filePath->setText(path);
	FilePathChanged();
}

void MacroConditionFileEdit::MatchTextChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_text = _matchText->toPlainText().toUtf8().constData();
}

void MacroConditionFileEdit::UseRegexChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_useRegex = state;
}

void MacroConditionFileEdit::CheckModificationDateChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_useTime = state;
}

void MacroConditionFileEdit::OnlyMatchIfChangedChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_onlyMatchIfChanged = state;
}
