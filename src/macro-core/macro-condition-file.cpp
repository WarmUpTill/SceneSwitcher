#include "macro-condition-file.hpp"
#include "utility.hpp"
#include "switcher-data.hpp"
#include "curl-helper.hpp"

#include <QTextStream>
#include <QFileDialog>
#include <array>
#include <tuple>

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

static bool stringsMatch(const QString &text, const std::string &pattern,
			 const RegexConfig &regex)
{
	auto qpattern = QString::fromStdString(pattern);
	if (regex.Enabled()) {
		return regex.Matches(text, qpattern);
	}
	auto textCopy = text;
	return CompareIgnoringLineEnding(qpattern, textCopy);
}

static std::optional<QString> getFileContent(MacroConditionFile::FileType type,
					     const std::string &filePath)
{
	QString filedata;
	switch (type) {
	case MacroConditionFile::FileType::LOCAL: {
		std::string path = filePath;
		QFile file(QString::fromStdString(path));
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
			return {};
		}
		filedata = QTextStream(&file).readAll();
		file.close();
	} break;
	case MacroConditionFile::FileType::REMOTE: {
		std::string path = filePath;
		std::string data = getRemoteData(path);
		filedata = QString::fromStdString(data);
	} break;
	default:
		break;
	}
	return filedata;
}

bool MacroConditionFile::CheckFileContentMatch()
{
	auto fileData = getFileContent(_fileType, _file);
	if (!fileData) {
		return false;
	}
	const bool match = stringsMatch(*fileData, _text, _regex);
	SetVariableValue(fileData->toStdString());
	return match;
}

bool MacroConditionFile::CheckChangeContent()
{
	auto filedata = getFileContent(_fileType, _file);
	if (!filedata) {
		return false;
	}
	size_t newHash = strHash(filedata->toUtf8().constData());
	const bool contentChanged = newHash != _lastHash;
	_lastHash = newHash;
	SetVariableValue(contentChanged ? "true" : "false");
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
	SetVariableValue(dateChanged ? "true" : "false");
	return dateChanged;
}

static QStringList splitLines(const QString &string)
{
	static auto regex = QRegularExpression("\n|\r\n|\r");
	return string.split(regex);
}

bool MacroConditionFile::CheckChangedMatch()
{
	static bool setupDone = false;
	if (!setupDone) {
		auto content = getFileContent(_fileType, _file);
		if (!content) {
			return false;
		}
		_previousContent = splitLines(*content);
		setupDone = true;
		return false;
	}

	auto currentFileContent = getFileContent(_fileType, _file);
	if (!currentFileContent) {
		return false;
	}

	auto currentContent = splitLines(*currentFileContent);
	auto newContent = currentContent;

	QStringList addedLines;
	QStringList removedLines;

	for (const auto &line : newContent) {
		if (auto it = std::find(_previousContent.begin(),
					_previousContent.end(),
					line) == _previousContent.end()) {
			addedLines.push_back(line);
		}
	}

	for (const auto &line : _previousContent) {
		if (std::find(newContent.begin(), newContent.end(), line) ==
		    newContent.end()) {
			removedLines.push_back(line);
		}
	}

	_previousContent = currentContent;

	if (_changeMatchType == ChangeMatchType::ADDED ||
	    _changeMatchType == ChangeMatchType::ANY) {
		for (const auto &line : addedLines) {
			if (stringsMatch(line, _text, _regex)) {
				return true;
			}
		}
	}

	if (_changeMatchType == ChangeMatchType::REMOVED ||
	    _changeMatchType == ChangeMatchType::ANY) {
		for (const auto &line : removedLines) {
			if (stringsMatch(line, _text, _regex)) {
				return true;
			}
		}
	}

	return false;
}

bool MacroConditionFile::CheckCondition()
{
	switch (_condition) {
	case MacroConditionFile::ConditionType::MATCH:
		return CheckFileContentMatch();
	case MacroConditionFile::ConditionType::CONTENT_CHANGE:
		return CheckChangeContent();
	case MacroConditionFile::ConditionType::DATE_CHANGE:
		return CheckChangeDate();
	case MacroConditionFile::ConditionType::CHANGES_MATCH:
		return CheckChangedMatch();
	default:
		break;
	}

	return false;
}

bool MacroConditionFile::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	_regex.Save(obj);
	_file.Save(obj, "file");
	_text.Save(obj, "text");
	obs_data_set_int(obj, "fileType", static_cast<int>(_fileType));
	obs_data_set_int(obj, "changeMatchType",
			 static_cast<int>(_changeMatchType));
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
	_changeMatchType = static_cast<ChangeMatchType>(
		obs_data_get_int(obj, "changeMatchType"));
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
	static constexpr std::array<
		std::tuple<MacroConditionFile::FileType, std::string_view>, 2>
		fileTypes = {{{MacroConditionFile::FileType::LOCAL,
			       "AdvSceneSwitcher.condition.file.local"},
			      {MacroConditionFile::FileType::REMOTE,
			       "AdvSceneSwitcher.condition.file.remote"}}};

	for (const auto &[type, name] : fileTypes) {
		list->addItem(obs_module_text(name.data()),
			      static_cast<int>(type));
	}
}

static void populateConditions(QComboBox *list)
{
	static constexpr std::array<
		std::tuple<MacroConditionFile::ConditionType, std::string_view>,
		4>
		conditionTypes = {{
			{MacroConditionFile::ConditionType::MATCH,
			 "AdvSceneSwitcher.condition.file.type.match"},
			{MacroConditionFile::ConditionType::CONTENT_CHANGE,
			 "AdvSceneSwitcher.condition.file.type.contentChange"},
			{MacroConditionFile::ConditionType::DATE_CHANGE,
			 "AdvSceneSwitcher.condition.file.type.dateChange"},
			{MacroConditionFile::ConditionType::CHANGES_MATCH,
			 "AdvSceneSwitcher.condition.file.type.changesMatch"},
		}};

	for (const auto &[type, name] : conditionTypes) {
		list->addItem(obs_module_text(name.data()),
			      static_cast<int>(type));
	}
}

static void populateChangeMatchTypes(QComboBox *list)
{
	static constexpr std::array<
		std::tuple<MacroConditionFile::ChangeMatchType, std::string_view>,
		3>
		matchTypes = {{
			{MacroConditionFile::ChangeMatchType::ANY,
			 "AdvSceneSwitcher.condition.file.changeMatchType.any"},
			{MacroConditionFile::ChangeMatchType::ADDED,
			 "AdvSceneSwitcher.condition.file.changeMatchType.added"},
			{MacroConditionFile::ChangeMatchType::REMOVED,
			 "AdvSceneSwitcher.condition.file.changeMatchType.removed"},
		}};

	for (const auto &[type, name] : matchTypes) {
		list->addItem(obs_module_text(name.data()),
			      static_cast<int>(type));
	}
}

MacroConditionFileEdit::MacroConditionFileEdit(
	QWidget *parent, std::shared_ptr<MacroConditionFile> entryData)
	: QWidget(parent),
	  _fileTypes(new QComboBox()),
	  _changeMatchTypes(new QComboBox()),
	  _conditions(new QComboBox()),
	  _filePath(new FileSelection()),
	  _matchText(new VariableTextEdit(this)),
	  _regex(new RegexConfigWidget(parent))
{
	populateFileTypes(_fileTypes);
	populateConditions(_conditions);
	populateChangeMatchTypes(_changeMatchTypes);

	QWidget::connect(_fileTypes, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(FileTypeChanged(int)));
	QWidget::connect(_changeMatchTypes, SIGNAL(currentIndexChanged(int)),
			 this, SLOT(ChangeMatchTypeChanged(int)));
	QWidget::connect(_conditions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(_filePath, SIGNAL(PathChanged(const QString &)), this,
			 SLOT(PathChanged(const QString &)));
	QWidget::connect(_matchText, SIGNAL(textChanged()), this,
			 SLOT(MatchTextChanged()));
	QWidget::connect(_regex, SIGNAL(RegexConfigChanged(RegexConfig)), this,
			 SLOT(RegexChanged(RegexConfig)));

	const std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{fileType}}", _fileTypes},
		{"{{changeMatchType}}", _changeMatchTypes},
		{"{{conditions}}", _conditions},
		{"{{filePath}}", _filePath},
		{"{{matchText}}", _matchText},
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

	_fileTypes->setCurrentIndex(
		_fileTypes->findData(static_cast<int>(_entryData->_fileType)));
	_conditions->setCurrentIndex(_conditions->findData(
		static_cast<int>(_entryData->_condition)));
	_changeMatchTypes->setCurrentIndex(_changeMatchTypes->findData(
		static_cast<int>(_entryData->_changeMatchType)));
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

	auto type = static_cast<MacroConditionFile::FileType>(
		_fileTypes->itemData(index).toInt());
	_filePath->Button()->setEnabled(type ==
					MacroConditionFile::FileType::LOCAL);
	auto lock = LockContext();
	_entryData->_fileType = type;
}

void MacroConditionFileEdit::ChangeMatchTypeChanged(int index)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_changeMatchType =
		static_cast<MacroConditionFile::ChangeMatchType>(
			_changeMatchTypes->itemData(index).toInt());
	SetWidgetVisibility();
}

void MacroConditionFileEdit::ConditionChanged(int index)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_condition = static_cast<MacroConditionFile::ConditionType>(
		_conditions->itemData(index).toInt());
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

	_changeMatchTypes->setVisible(
		_entryData->_condition ==
		MacroConditionFile::ConditionType::CHANGES_MATCH);
	_matchText->setVisible(
		_entryData->_condition ==
			MacroConditionFile::ConditionType::MATCH ||
		_entryData->_condition ==
			MacroConditionFile::ConditionType::CHANGES_MATCH);
	_regex->setVisible(
		_entryData->_condition ==
			MacroConditionFile::ConditionType::MATCH ||
		_entryData->_condition ==
			MacroConditionFile::ConditionType::CHANGES_MATCH);
	adjustSize();
	updateGeometry();
}

} // namespace advss
