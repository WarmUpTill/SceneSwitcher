#include "macro-condition-file.hpp"
#include "layout-helpers.hpp"
#include "plugin-state-helpers.hpp"
#include "utility.hpp"

#include <QTextStream>
#include <QFileInfo>

namespace advss {

const std::string MacroConditionFile::id = "file";

bool MacroConditionFile::_registered = MacroConditionFactory::Register(
	MacroConditionFile::id,
	{MacroConditionFile::Create, MacroConditionFileEdit::Create,
	 "AdvSceneSwitcher.condition.file"});

static std::hash<std::string> strHash;

void MacroConditionFile::SetCondition(Condition condition)
{
	_condition = condition;
	SetupTempVars();
}

bool MacroConditionFile::MatchFileContent(QString &filedata)
{
	if (_regex.Enabled()) {
		return _regex.Matches(filedata, QString::fromStdString(_text));
	}

	QString text = QString::fromStdString(_text);
	return CompareIgnoringLineEnding(text, filedata);
}

bool MacroConditionFile::CheckFileContent()
{
	QFile file(QString::fromStdString(_file));
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		return false;
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

	std::string path = _file;
	QFile file(QString::fromStdString(path));
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		return false;
	}
	filedata = QTextStream(&file).readAll();
	file.close();

	SetTempVarValue("content", filedata.toStdString());
	size_t newHash = strHash(filedata.toUtf8().constData());
	const bool contentChanged = !_firstContentCheck &&
				    (newHash != _lastHash);
	_lastHash = newHash;
	_firstContentCheck = false;
	return contentChanged;
}

bool MacroConditionFile::CheckChangeDate()
{
	QFile file(QString::fromStdString(_file));
	QDateTime newLastMod = QFileInfo(file).lastModified();
	SetVariableValue(newLastMod.toString().toStdString());
	const bool dateChanged = _lastMod.isValid() && (_lastMod != newLastMod);
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

	AddTempvar(
		"basename",
		obs_module_text("AdvSceneSwitcher.tempVar.file.basename"),
		obs_module_text(
			"AdvSceneSwitcher.tempVar.file.basename.description"));
	AddTempvar(
		"basenameComplete",
		obs_module_text(
			"AdvSceneSwitcher.tempVar.file.basenameComplete"),
		obs_module_text(
			"AdvSceneSwitcher.tempVar.file.basenameComplete.description"));
	AddTempvar("suffix",
		   obs_module_text("AdvSceneSwitcher.tempVar.file.suffix"),
		   obs_module_text(
			   "AdvSceneSwitcher.tempVar.file.suffix.description"));
	AddTempvar(
		"suffixComplete",
		obs_module_text("AdvSceneSwitcher.tempVar.file.suffixComplete"),
		obs_module_text(
			"AdvSceneSwitcher.tempVar.file.suffixComplete.description"));
	AddTempvar("filename",
		   obs_module_text("AdvSceneSwitcher.tempVar.file.filename"));
	AddTempvar("absoluteFilePath",
		   obs_module_text(
			   "AdvSceneSwitcher.tempVar.file.absoluteFilePath"));
	AddTempvar(
		"absolutePath",
		obs_module_text("AdvSceneSwitcher.tempVar.file.absolutePath"));
	AddTempvar("isAbsolutePath",
		   obs_module_text(
			   "AdvSceneSwitcher.tempVar.file.isAbsolutePath"));
}

bool MacroConditionFile::CheckCondition()
{
	bool ret = false;
	switch (_condition) {
	case Condition::MATCH:
		ret = CheckFileContent();
		break;
	case Condition::CONTENT_CHANGE:
		ret = CheckChangeContent();
		break;
	case Condition::DATE_CHANGE:
		ret = CheckChangeDate();
		break;
	case Condition::IS_FILE: {
		QFileInfo info(QString::fromStdString(_file));
		ret = info.isFile();
		break;
	}
	case Condition::IS_FOLDER: {
		QFileInfo info(QString::fromStdString(_file));
		ret = info.isDir();
		break;
	}
	case Condition::EXISTS: {
		QFileInfo info(QString::fromStdString(_file));
		ret = info.exists();
		break;
	}
	default:
		break;
	}

	if (std::string(_file) != _lastFile) {
		QFileInfo info(QString::fromStdString(_file));
		_basename = info.baseName().toStdString();
		_basenameComplete = info.completeBaseName().toStdString();
		_suffix = info.suffix().toStdString();
		_suffixComplete = info.completeSuffix().toStdString();
		_filename = info.fileName().toStdString();
		_absoluteFilePath = info.absoluteFilePath().toStdString();
		_absolutePath = info.absolutePath().toStdString();
		_isAbsolutePath = info.isAbsolute();
		_lastFile = _file;
	}

	SetTempVarValue("basename", _basename);
	SetTempVarValue("basenameComplete", _basenameComplete);
	SetTempVarValue("suffix", _suffix);
	SetTempVarValue("suffixComplete", _suffixComplete);
	SetTempVarValue("filename", _filename);
	SetTempVarValue("absoluteFilePath", _absoluteFilePath);
	SetTempVarValue("absolutePath", _absolutePath);
	SetTempVarValue("isAbsolutePath", _isAbsolutePath);

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
	obs_data_set_int(obj, "condition", static_cast<int>(_condition));
	return true;
}

bool MacroConditionFile::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_regex.Load(obj);
	_file.Load(obj, "file");
	_text.Load(obj, "text");
	SetCondition(
		static_cast<Condition>(obs_data_get_int(obj, "condition")));
	return true;
}

std::string MacroConditionFile::GetShortDesc() const
{
	return _file.UnresolvedValue();
}

static void populateConditions(QComboBox *list)
{
	list->addItem(
		obs_module_text("AdvSceneSwitcher.condition.file.type.match"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.condition.file.type.contentChange"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.condition.file.type.dateChange"));
	list->addItem(
		obs_module_text("AdvSceneSwitcher.condition.file.type.exists"));
	list->addItem(
		obs_module_text("AdvSceneSwitcher.condition.file.type.isFile"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.condition.file.type.isFolder"));
}

MacroConditionFileEdit::MacroConditionFileEdit(
	QWidget *parent, std::shared_ptr<MacroConditionFile> entryData)
	: QWidget(parent),
	  _conditions(new QComboBox()),
	  _filePath(new FileSelection()),
	  _matchText(new VariableTextEdit(this)),
	  _regex(new RegexConfigWidget(parent))
{
	populateConditions(_conditions);

	QWidget::connect(_conditions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(_filePath, SIGNAL(PathChanged(const QString &)), this,
			 SLOT(PathChanged(const QString &)));
	QWidget::connect(_matchText, SIGNAL(textChanged()), this,
			 SLOT(MatchTextChanged()));
	QWidget::connect(_regex,
			 SIGNAL(RegexConfigChanged(const RegexConfig &)), this,
			 SLOT(RegexChanged(const RegexConfig &)));

	auto widgetLayout = new QHBoxLayout;
	widgetLayout->setContentsMargins(0, 0, 0, 0);
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.condition.file.layout"),
		     widgetLayout,
		     {{"{{conditions}}", _conditions},
		      {"{{filePath}}", _filePath},
		      {"{{regex}}", _regex}});

	auto layout = new QVBoxLayout;
	layout->addLayout(widgetLayout);
	layout->addWidget(_matchText);

	setLayout(layout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionFileEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_conditions->setCurrentIndex(
		static_cast<int>(_entryData->GetCondition()));
	_filePath->SetPath(_entryData->_file);
	_matchText->setPlainText(_entryData->_text);
	_regex->SetRegexConfig(_entryData->_regex);

	SetWidgetVisibility();
}

void MacroConditionFileEdit::ConditionChanged(int index)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->SetCondition(
		static_cast<MacroConditionFile::Condition>(index));
	SetWidgetVisibility();
}

void MacroConditionFileEdit::PathChanged(const QString &text)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_file = text.toUtf8().constData();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionFileEdit::MatchTextChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_text = _matchText->toPlainText().toUtf8().constData();

	adjustSize();
	updateGeometry();
}

void MacroConditionFileEdit::RegexChanged(const RegexConfig &conf)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_regex = conf;
	adjustSize();
	updateGeometry();
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

	adjustSize();
	updateGeometry();
}

} // namespace advss
