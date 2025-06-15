#include "macro-condition-folder.hpp"
#include "help-icon.hpp"
#include "macro-helpers.hpp"
#include "layout-helpers.hpp"

#include <QDir>
#include <QFileInfo>

namespace advss {

const std::string MacroConditionFolder::id = "folder";

bool MacroConditionFolder::_registered = MacroConditionFactory::Register(
	MacroConditionFolder::id,
	{MacroConditionFolder::Create, MacroConditionFolderEdit::Create,
	 "AdvSceneSwitcher.condition.folder"});

static const std::map<MacroConditionFolder::Condition, std::string> conditions =
	{
		{MacroConditionFolder::Condition::ANY,
		 "AdvSceneSwitcher.condition.folder.condition.any"},
		{MacroConditionFolder::Condition::FILE_ADD,
		 "AdvSceneSwitcher.condition.folder.condition.fileAdd"},
		{MacroConditionFolder::Condition::FILE_CHANGE,
		 "AdvSceneSwitcher.condition.folder.condition.fileChange"},
		{MacroConditionFolder::Condition::FILE_REMOVE,
		 "AdvSceneSwitcher.condition.folder.condition.fileRemove"},
		{MacroConditionFolder::Condition::FOLDER_ADD,
		 "AdvSceneSwitcher.condition.folder.condition.folderAdd"},
		{MacroConditionFolder::Condition::FOLDER_REMOVE,
		 "AdvSceneSwitcher.condition.folder.condition.folderRemove"},
};

MacroConditionFolder::MacroConditionFolder(Macro *m) : MacroCondition(m, true)
{
}

bool MacroConditionFolder::CheckCondition()
{
	std::lock_guard<std::mutex> lock(_mutex);
	bool ret = _matched;
	if (_lastWatchedValue != _folder.UnresolvedValue()) {
		SetupWatcher();
	}

	SetTempVarValues();

	_newFiles.clear();
	_changedFiles.clear();
	_removedFiles.clear();
	_newDirs.clear();
	_removedDirs.clear();
	_matched = false;

	return ret;
}

bool MacroConditionFolder::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	_folder.Save(obj, "file");
	obs_data_set_bool(obj, "enableFilter", _enableFilter);
	_regex.Save(obj);
	_filter.Save(obj, "filter");
	obs_data_set_int(obj, "condition", static_cast<int>(_condition));
	return true;
}

bool MacroConditionFolder::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_folder.Load(obj, "file");
	_enableFilter = obs_data_get_bool(obj, "enableFilter");
	_regex.Load(obj);
	_regex.SetEnabled(true); // Already controlled via _enableFilter
	_filter.Load(obj, "filter");
	_condition = static_cast<Condition>(obs_data_get_int(obj, "condition"));
	SetupWatcher();
	return true;
}

std::string MacroConditionFolder::GetShortDesc() const
{
	return _folder.UnresolvedValue();
}

void MacroConditionFolder::SetFolder(const std::string &folder)
{
	_folder = folder;
	SetupWatcher();
}

static QSet<QString> getFilesInDir(const QString &path)
{
	QSet<QString> result;
	for (const auto &file : QDir(path).entryList(QDir::Files)) {
		result << file;
	}
	return result;
}

static QSet<QString> getDirsInDir(const QString &path)
{
	QSet<QString> result;
	for (const auto &dir : QDir(path).entryList(QDir::AllDirs)) {
		result << dir;
	}
	return result;
}

static void reduceSetToPatternMatch(QSet<QString> &set,
				    const RegexConfig &regex,
				    const std::string &pattern)
{
	QSet<QString> copy = set;
	for (const auto &value : copy) {
		if (!regex.Matches(value.toStdString(), pattern)) {
			set.remove(value);
		}
	}
}

void MacroConditionFolder::DirectoryChanged(const QString &path)
{
	std::lock_guard<std::mutex> lock(_mutex);
	if (MacroIsPaused(GetMacro())) {
		return;
	}

	auto currentFiles = getFilesInDir(path);
	auto currentDirs = getDirsInDir(path);

	if (currentFiles.count() > _currentFiles.count()) {
		_newFiles += currentFiles - _currentFiles;
	} else {
		_removedFiles += _currentFiles - currentFiles;
	}

	if (currentDirs.count() > _currentDirs.count()) {
		_newDirs += currentDirs - _currentDirs;
	} else {
		_removedDirs += _currentDirs - currentDirs;
	}

	if (_enableFilter) {
		reduceSetToPatternMatch(_newFiles, _regex, _filter);
		reduceSetToPatternMatch(_removedFiles, _regex, _filter);
		reduceSetToPatternMatch(_newDirs, _regex, _filter);
		reduceSetToPatternMatch(_removedDirs, _regex, _filter);
	}

	switch (_condition) {
	case Condition::ANY:
		_matched = _matched || _newFiles.count() > 0 ||
			   _removedFiles.count() > 0 || _newDirs.count() > 0 ||
			   _removedDirs.count() > 0;
		break;
	case Condition::FILE_ADD:
		_matched = _newFiles.count() > 0;
		break;
	case Condition::FILE_REMOVE:
		_matched = _removedFiles.count() > 0;
		break;
	case Condition::FOLDER_ADD:
		_matched = _newDirs.count() > 0;
		break;
	case Condition::FOLDER_REMOVE:
		_matched = _removedDirs.count() > 0;
		break;
	default:
		break;
	}

	for (const auto &newFile : _newFiles) {
		_watcher->addPath(path + "/" + newFile);
	}

	_currentFiles = currentFiles;
	_currentDirs = currentDirs;
}

void MacroConditionFolder::FileChanged(const QString &path)
{
	std::lock_guard<std::mutex> lock(_mutex);
	QFileInfo fileInfo(path);
	if (!fileInfo.exists()) {
		return;
	}

	const auto fileName = fileInfo.fileName();
	if (_enableFilter && !_regex.Matches(fileName.toStdString(), _filter)) {
		return;
	}

	_changedFiles << fileName;
	if (_condition == Condition::FILE_CHANGE ||
	    _condition == Condition::ANY) {
		_matched = true;
	}
}

static QStringList getFileWatcherList(const QSet<QString> &files,
				      const QString &dirName)
{
	QStringList list;
	for (const auto &value : files) {
		list << (dirName + "/" + value);
	}
	return list;
}

void MacroConditionFolder::SetupWatcher()
{
	_watcher = std::make_unique<QFileSystemWatcher>();
	const auto path = QString::fromStdString(_folder);
	_currentFiles = getFilesInDir(path);
	_currentDirs = getDirsInDir(path);
	_lastWatchedValue = _folder.UnresolvedValue();
	connect(_watcher.get(), SIGNAL(directoryChanged(const QString &)), this,
		SLOT(DirectoryChanged(const QString &)));
	connect(_watcher.get(), SIGNAL(fileChanged(const QString &)), this,
		SLOT(FileChanged(const QString &)));
	_watcher.get()->addPaths(getFileWatcherList(_currentFiles, path));
	_watcher->addPath(path);
}

void MacroConditionFolder::SetTempVarValues()
{
	auto setVarHelper = [this](const QSet<QString> &set,
				   const std::string &id) {
		std::string result;
		for (const auto &value : set) {
			result += value.toStdString() + "\n";
		}
		if (result.size() > 0) {
			result.pop_back();
		}
		SetTempVarValue(id, result);
	};

	setVarHelper(_newFiles, "newFiles");
	setVarHelper(_changedFiles, "changedFiles");
	setVarHelper(_removedFiles, "removedFiles");
	setVarHelper(_newDirs, "newDirs");
	setVarHelper(_removedDirs, "removedDirs");
}

void MacroConditionFolder::SetupTempVars()
{
	MacroCondition::SetupTempVars();
	AddTempvar("newFiles",
		   obs_module_text("AdvSceneSwitcher.tempVar.folder.newFiles"));
	AddTempvar("changedFiles",
		   obs_module_text(
			   "AdvSceneSwitcher.tempVar.folder.changedFiles"));
	AddTempvar("removedFiles",
		   obs_module_text(
			   "AdvSceneSwitcher.tempVar.folder.removedFiles"));
	AddTempvar("newDirs",
		   obs_module_text("AdvSceneSwitcher.tempVar.folder.newDirs"));
	AddTempvar(
		"removedDirs",
		obs_module_text("AdvSceneSwitcher.tempVar.folder.removedDirs"));
}

static void populateConditions(QComboBox *list)
{
	for (const auto &[value, name] : conditions) {
		list->addItem(obs_module_text(name.c_str()),
			      static_cast<int>(value));
	}
}

MacroConditionFolderEdit::MacroConditionFolderEdit(
	QWidget *parent, std::shared_ptr<MacroConditionFolder> entryData)
	: QWidget(parent),
	  _conditions(new QComboBox()),
	  _folder(new FileSelection(FileSelection::Type::FOLDER)),
	  _enableFilter(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.condition.folder.enableFilter"))),
	  _filterLayout(new QHBoxLayout()),
	  _regex(new RegexConfigWidget(this, false)),
	  _filter(new VariableLineEdit(this))
{
	populateConditions(_conditions);

	QWidget::connect(_conditions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(_folder, SIGNAL(PathChanged(const QString &)), this,
			 SLOT(PathChanged(const QString &)));
	QWidget::connect(_enableFilter, SIGNAL(stateChanged(int)), this,
			 SLOT(EnableFilterChanged(int)));
	QWidget::connect(_regex,
			 SIGNAL(RegexConfigChanged(const RegexConfig &)), this,
			 SLOT(RegexChanged(const RegexConfig &)));
	QWidget::connect(_filter, SIGNAL(editingFinished()), this,
			 SLOT(FilterChanged()));

	auto tooltipLabel = new HelpIcon(
		obs_module_text("AdvSceneSwitcher.condition.folder.tooltip"));

	const std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{conditions}}", _conditions}, {"{{folder}}", _folder},
		{"{{tooltip}}", tooltipLabel},   {"{{regex}}", _regex},
		{"{{filter}}", _filter},
	};

	auto entryLayout = new QHBoxLayout();
	entryLayout->setContentsMargins(0, 0, 0, 0);
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.condition.folder.entry"),
		     entryLayout, widgetPlaceholders, false);
	_filterLayout->setContentsMargins(0, 0, 0, 0);
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.folder.entry.filter"),
		     _filterLayout, widgetPlaceholders, false);

	auto layout = new QVBoxLayout();
	layout->addLayout(entryLayout);
	layout->addWidget(_enableFilter);
	layout->addLayout(_filterLayout);
	setLayout(layout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionFolderEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_conditions->setCurrentIndex(_conditions->findData(
		static_cast<int>(_entryData->_condition)));
	_folder->SetPath(_entryData->GetFolder());
	_enableFilter->setChecked(_entryData->_enableFilter);
	_regex->SetRegexConfig(_entryData->_regex);
	_filter->setText(_entryData->_filter);

	SetWidgetVisibility();
}

void MacroConditionFolderEdit::ConditionChanged(int index)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_condition = static_cast<MacroConditionFolder::Condition>(
		_conditions->itemData(index).toInt());
}

void MacroConditionFolderEdit::PathChanged(const QString &text)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->SetFolder(text.toStdString());
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionFolderEdit::RegexChanged(const RegexConfig &regex)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_regex = regex;
	_entryData->_regex.SetEnabled(true);
}

void MacroConditionFolderEdit::FilterChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_filter = _filter->text().toStdString();
}

void MacroConditionFolderEdit::EnableFilterChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_enableFilter = value;
	SetWidgetVisibility();
}

void MacroConditionFolderEdit::SetWidgetVisibility()
{
	SetLayoutVisible(_filterLayout, _entryData->_enableFilter);

	adjustSize();
	updateGeometry();
}

} // namespace advss
