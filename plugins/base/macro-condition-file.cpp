#include "macro-condition-file.hpp"
#include "curl-helper.hpp"
#include "layout-helpers.hpp"
#include "plugin-state-helpers.hpp"
#include "utility.hpp"

#include <QFileDialog>
#include <QTextStream>
#include <regex>

namespace advss {

const std::string MacroConditionFile::id = "file";

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
	CurlHelper::SetOpt(CURLOPT_URL, url.c_str());
	CurlHelper::SetOpt(CURLOPT_WRITEFUNCTION, WriteCallback);
	CurlHelper::SetOpt(CURLOPT_WRITEDATA, &readBuffer);
	// Set timeout to at least one second
	int timeout = GetIntervalValue() / 1000;
	if (timeout == 0) {
		timeout = 1;
	}
	CurlHelper::SetOpt(CURLOPT_TIMEOUT, 1);
	CurlHelper::Perform();
	return readBuffer;
}

void MacroConditionFile::SetCondition(Condition condition)
{
	_condition = condition;
	SetupTempVars();
}

bool MacroConditionFile::MatchFileContent(QString &filedata)
{
	if (_onlyMatchIfChanged) {
		size_t newHash = strHash(filedata.toUtf8().constData());
		if (newHash == _lastHash) {
			return false;
		}
		_lastHash = newHash;
	}

	if (_regex.Enabled()) {
		return _regex.Matches(filedata, QString::fromStdString(_text));
	}

	QString text = QString::fromStdString(_text);
	return CompareIgnoringLineEnding(text, filedata);
}

bool MacroConditionFile::CheckRemoteFileContent()
{
	std::string path = _file;
	std::string data = getRemoteData(path);
	SetVariableValue(data);
	SetTempVarValue("content", data);
	QString qdata = QString::fromStdString(data);
	return MatchFileContent(qdata);
}

bool MacroConditionFile::CheckLocalFileContent()
{
	QFile file(QString::fromStdString(_file));
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
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
	SetVariableValue(filedata.toStdString());
	SetTempVarValue("content", filedata.toStdString());
	bool match = MatchFileContent(filedata);

	file.close();
	return match;
}

bool MacroConditionFile::CheckChangeContent()
{
	QString filedata;
	switch (_fileType) {
	case FileType::LOCAL: {
		std::string path = _file;
		QFile file(QString::fromStdString(path));
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
			return false;
		}
		filedata = QTextStream(&file).readAll();
		file.close();
	} break;
	case FileType::REMOTE: {
		std::string path = _file;
		std::string data = getRemoteData(path);
		QString filedata = QString::fromStdString(data);
	} break;
	default:
		break;
	}

	SetTempVarValue("content", filedata.toStdString());
	size_t newHash = strHash(filedata.toUtf8().constData());
	const bool contentChanged = newHash != _lastHash;
	_lastHash = newHash;
	return contentChanged;
}

bool MacroConditionFile::CheckChangeDate()
{
	if (_fileType == FileType::REMOTE) {
		return false;
	}

	QFile file(QString::fromStdString(_file));
	QDateTime newLastMod = QFileInfo(file).lastModified();
	SetVariableValue(newLastMod.toString().toStdString());
	const bool dateChanged = _lastMod != newLastMod;
	_lastMod = newLastMod;
	SetTempVarValue("date", newLastMod.toString(Qt::ISODate).toStdString());
	return dateChanged;
}

void MacroConditionFile::SetupTempVars()
{
	MacroCondition::SetupTempVars();
	if (_condition == Condition::DATE_CHANGE) {
		AddTempvar(
			"date",
			obs_module_text("AdvSceneSwitcher.tempVar.file.date"));
	} else {
		AddTempvar("content",
			   obs_module_text(
				   "AdvSceneSwitcher.tempVar.file.content"));
	}
}

bool MacroConditionFile::CheckCondition()
{
	bool ret = false;
	switch (_condition) {
	case Condition::MATCH:
		if (_fileType == FileType::REMOTE) {
			ret = CheckRemoteFileContent();
			break;
		}
		ret = CheckLocalFileContent();
		break;
	case Condition::CONTENT_CHANGE:
		ret = CheckChangeContent();
		break;
	case Condition::DATE_CHANGE:
		ret = CheckChangeDate();
		break;
	default:
		break;
	}

	if (GetVariableValue().empty()) {
		SetVariableValue(ret ? "true" : "false");
	}
	return ret;
}

bool MacroConditionFile::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	_regex.Save(obj);
	_file.Save(obj, "file");
	_text.Save(obj, "text");
	obs_data_set_int(obj, "fileType", static_cast<int>(_fileType));
	obs_data_set_int(obj, "condition", static_cast<int>(_condition));
	obs_data_set_bool(obj, "useTime", _useTime);
	obs_data_set_bool(obj, "onlyMatchIfChanged", _onlyMatchIfChanged);
	return true;
}

bool MacroConditionFile::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_regex.Load(obj);
	// TODO: remove in future version
	if (obs_data_has_user_value(obj, "useRegex")) {
		_regex.CreateBackwardsCompatibleRegex(
			obs_data_get_bool(obj, "useRegex"));
	}
	_file.Load(obj, "file");
	_text.Load(obj, "text");
	_fileType = static_cast<FileType>(obs_data_get_int(obj, "fileType"));
	SetCondition(
		static_cast<Condition>(obs_data_get_int(obj, "condition")));
	_useTime = obs_data_get_bool(obj, "useTime");
	_onlyMatchIfChanged = obs_data_get_bool(obj, "onlyMatchIfChanged");
	return true;
}

std::string MacroConditionFile::GetShortDesc() const
{
	return _file.UnresolvedValue();
}

static void populateFileTypes(QComboBox *list)
{
	list->addItem(obs_module_text("AdvSceneSwitcher.condition.file.local"));
	list->addItem(
		obs_module_text("AdvSceneSwitcher.condition.file.remote"));
}

static void populateConditions(QComboBox *list)
{
	list->addItem(
		obs_module_text("AdvSceneSwitcher.condition.file.type.match"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.condition.file.type.contentChange"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.condition.file.type.dateChange"));
}

MacroConditionFileEdit::MacroConditionFileEdit(
	QWidget *parent, std::shared_ptr<MacroConditionFile> entryData)
	: QWidget(parent),
	  _fileTypes(new QComboBox()),
	  _conditions(new QComboBox()),
	  _filePath(new FileSelection()),
	  _matchText(new VariableTextEdit(this)),
	  _regex(new RegexConfigWidget(parent)),
	  _checkModificationDate(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.fileTab.checkfileContentTime"))),
	  _checkFileContent(new QCheckBox(
		  obs_module_text("AdvSceneSwitcher.fileTab.checkfileContent")))
{
	populateFileTypes(_fileTypes);
	populateConditions(_conditions);

	QWidget::connect(_fileTypes, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(FileTypeChanged(int)));
	QWidget::connect(_conditions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(_filePath, SIGNAL(PathChanged(const QString &)), this,
			 SLOT(PathChanged(const QString &)));
	QWidget::connect(_matchText, SIGNAL(textChanged()), this,
			 SLOT(MatchTextChanged()));
	QWidget::connect(_regex,
			 SIGNAL(RegexConfigChanged(const RegexConfig &)), this,
			 SLOT(RegexChanged(const RegexConfig &)));
	QWidget::connect(_checkModificationDate, SIGNAL(stateChanged(int)),
			 this, SLOT(CheckModificationDateChanged(int)));
	QWidget::connect(_checkFileContent, SIGNAL(stateChanged(int)), this,
			 SLOT(OnlyMatchIfChangedChanged(int)));

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{fileType}}", _fileTypes},
		{"{{conditions}}", _conditions},
		{"{{filePath}}", _filePath},
		{"{{matchText}}", _matchText},
		{"{{useRegex}}", _regex},
		{"{{checkModificationDate}}", _checkModificationDate},
		{"{{checkFileContent}}", _checkFileContent},
	};

	QVBoxLayout *mainLayout = new QVBoxLayout;
	QHBoxLayout *line1Layout = new QHBoxLayout;
	QHBoxLayout *line2Layout = new QHBoxLayout;
	QHBoxLayout *line3Layout = new QHBoxLayout;
	line1Layout->setContentsMargins(0, 0, 0, 0);
	line2Layout->setContentsMargins(0, 0, 0, 0);
	line3Layout->setContentsMargins(0, 0, 0, 0);
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.condition.file.entry.line1"),
		line1Layout, widgetPlaceholders);
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.condition.file.entry.line2"),
		line2Layout, widgetPlaceholders, false);
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.condition.file.entry.line3"),
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

	_fileTypes->setCurrentIndex(static_cast<int>(_entryData->_fileType));
	_conditions->setCurrentIndex(
		static_cast<int>(_entryData->GetCondition()));
	_filePath->SetPath(_entryData->_file);
	_matchText->setPlainText(_entryData->_text);
	_regex->SetRegexConfig(_entryData->_regex);
	_checkModificationDate->setChecked(_entryData->_useTime);
	_checkFileContent->setChecked(_entryData->_onlyMatchIfChanged);

	// TODO: Remove in future version
	if (!_entryData->_useTime) {
		_checkModificationDate->hide();
	}
	if (!_entryData->_onlyMatchIfChanged) {
		_checkFileContent->hide();
	}

	SetWidgetVisibility();
}

void MacroConditionFileEdit::FileTypeChanged(int index)
{
	if (_loading || !_entryData) {
		return;
	}

	MacroConditionFile::FileType type =
		static_cast<MacroConditionFile::FileType>(index);

	if (type == MacroConditionFile::FileType::LOCAL) {
		_filePath->Button()->setDisabled(false);
		_checkModificationDate->setDisabled(false);
	} else {
		_filePath->Button()->setDisabled(true);
		_checkModificationDate->setDisabled(true);
	}

	auto lock = LockContext();
	_entryData->_fileType = type;
}

void MacroConditionFileEdit::ConditionChanged(int index)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->SetCondition(
		static_cast<MacroConditionFile::Condition>(index));
	SetWidgetVisibility();
}

void MacroConditionFileEdit::PathChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_file = text.toUtf8().constData();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionFileEdit::MatchTextChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_text = _matchText->toPlainText().toUtf8().constData();

	adjustSize();
	updateGeometry();
}

void MacroConditionFileEdit::RegexChanged(const RegexConfig &conf)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_regex = conf;
	adjustSize();
	updateGeometry();
}

void MacroConditionFileEdit::CheckModificationDateChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_useTime = state;
}

void MacroConditionFileEdit::OnlyMatchIfChangedChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_onlyMatchIfChanged = state;
}

void MacroConditionFileEdit::SetWidgetVisibility()
{
	if (!_entryData) {
		return;
	}

	_matchText->setVisible(_entryData->GetCondition() ==
			       MacroConditionFile::Condition::MATCH);
	_regex->setVisible(_entryData->GetCondition() ==
			   MacroConditionFile::Condition::MATCH);
	_checkModificationDate->setVisible(
		_entryData->_useTime &&
		_entryData->GetCondition() ==
			MacroConditionFile::Condition::MATCH);
	_checkFileContent->setVisible(
		_entryData->_onlyMatchIfChanged &&
		_entryData->GetCondition() ==
			MacroConditionFile::Condition::MATCH);

	// TODO: Remove remote file support in future version in favor of HTTP
	// action.
	// Hide the option for now, if it is not used already.
	_fileTypes->setVisible(_entryData->_fileType ==
			       MacroConditionFile::FileType::REMOTE);

	adjustSize();
	updateGeometry();
}

} // namespace advss
