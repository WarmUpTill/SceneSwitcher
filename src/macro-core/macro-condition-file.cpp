#include "macro-condition-file.hpp"
#include "utility.hpp"
#include "switcher-data.hpp"
#include "curl-helper.hpp"

#include <QTextStream>
#include <QFileDialog>
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
	switcher->curl.SetOpt(CURLOPT_URL, url.c_str());
	switcher->curl.SetOpt(CURLOPT_WRITEFUNCTION, WriteCallback);
	switcher->curl.SetOpt(CURLOPT_WRITEDATA, &readBuffer);
	// Set timeout to at least one second
	int timeout = switcher->interval / 1000;
	if (timeout == 0) {
		timeout = 1;
	}
	switcher->curl.SetOpt(CURLOPT_TIMEOUT, 1);
	switcher->curl.Perform();
	return readBuffer;
}

bool MacroConditionFile::MatchFileContent(QString &filedata) const
{
	if (_regex.Enabled()) {
		auto expr = _regex.GetRegularExpression(_text);
		if (!expr.isValid()) {
			return false;
		}
		auto match = expr.match(filedata);
		return match.hasMatch();
	}

	QString text = QString::fromStdString(_text);
	return CompareIgnoringLineEnding(text, filedata);
}

bool MacroConditionFile::CheckRemoteFileContent()
{
	std::string path = _file;
	std::string data = getRemoteData(path);
	SetVariableValue(data);
	QString qdata = QString::fromStdString(data);
	return MatchFileContent(qdata);
}

bool MacroConditionFile::CheckLocalFileContent()
{
	QFile file(QString::fromStdString(_file));
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		return false;
	}

	QString filedata = QTextStream(&file).readAll();
	SetVariableValue(filedata.toStdString());
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
	return dateChanged;
}

bool MacroConditionFile::CheckCondition()
{
	bool ret = false;
	switch (_condition) {
	case MacroConditionFile::ConditionType::MATCH:
		if (_fileType == FileType::REMOTE) {
			ret = CheckRemoteFileContent();
			break;
		}
		ret = CheckLocalFileContent();
		break;
	case MacroConditionFile::ConditionType::CONTENT_CHANGE:
		ret = CheckChangeContent();
		break;
	case MacroConditionFile::ConditionType::DATE_CHANGE:
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
	_condition =
		static_cast<ConditionType>(obs_data_get_int(obj, "condition"));
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
	  _regex(new RegexConfigWidget(parent))
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
	QWidget::connect(_regex, SIGNAL(RegexConfigChanged(RegexConfig)), this,
			 SLOT(RegexChanged(RegexConfig)));

	const std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{fileType}}", _fileTypes}, {"{{conditions}}", _conditions},
		{"{{filePath}}", _filePath},  {"{{matchText}}", _matchText},
		{"{{useRegex}}", _regex},
	};

	auto line1Layout = new QHBoxLayout;
	line1Layout->setContentsMargins(0, 0, 0, 0);
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.condition.file.entry.line1"),
		line1Layout, widgetPlaceholders);
	auto line2Layout = new QHBoxLayout;
	line2Layout->setContentsMargins(0, 0, 0, 0);
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.condition.file.entry.line2"),
		line2Layout, widgetPlaceholders, false);

	auto mainLayout = new QVBoxLayout;
	mainLayout->addLayout(line1Layout);
	mainLayout->addLayout(line2Layout);
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
	_conditions->setCurrentIndex(static_cast<int>(_entryData->_condition));
	_filePath->SetPath(_entryData->_file);
	_matchText->setPlainText(_entryData->_text);
	_regex->SetRegexConfig(_entryData->_regex);

	SetWidgetVisibility();
}

void MacroConditionFileEdit::FileTypeChanged(int index)
{
	if (_loading || !_entryData) {
		return;
	}

	auto type = static_cast<MacroConditionFile::FileType>(index);
	_filePath->Button()->setEnabled(type ==
					MacroConditionFile::FileType::LOCAL);
	auto lock = LockContext();
	_entryData->_fileType = type;
}

void MacroConditionFileEdit::ConditionChanged(int index)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_condition =
		static_cast<MacroConditionFile::ConditionType>(index);
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

void MacroConditionFileEdit::RegexChanged(RegexConfig conf)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_regex = conf;
	adjustSize();
	updateGeometry();
}

void MacroConditionFileEdit::SetWidgetVisibility()
{
	if (!_entryData) {
		return;
	}

	_matchText->setVisible(_entryData->_condition ==
			       MacroConditionFile::ConditionType::MATCH);
	_regex->setVisible(_entryData->_condition ==
			   MacroConditionFile::ConditionType::MATCH);
	adjustSize();
	updateGeometry();
}

} // namespace advss
