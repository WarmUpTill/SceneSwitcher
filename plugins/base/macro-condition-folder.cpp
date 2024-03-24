#include "macro-condition-folder.hpp"
#include "macro-helpers.hpp"
#include "layout-helpers.hpp"

#include <QFileInfo>

// TODO:
// * Set up temp vars
// * Finish locale
// * Finish condition logic to support file / folder change / remove

namespace advss {

const std::string MacroConditionFolder::id = "folder";

bool MacroConditionFolder::_registered = MacroConditionFactory::Register(
	MacroConditionFolder::id,
	{MacroConditionFolder::Create, MacroConditionFolderEdit::Create,
	 "AdvSceneSwitcher.condition.folder"});

static const std::unordered_map<MacroConditionFolder::Condition, std::string>
	conditions = {
		{MacroConditionFolder::Condition::ANY,
		 "AdvSceneSwitcher.condition.folder.condition.any"},
		{MacroConditionFolder::Condition::FILE_CHANGE,
		 "AdvSceneSwitcher.condition.folder.condition.fileChange"},
		{MacroConditionFolder::Condition::FILE_REMOVE,
		 "AdvSceneSwitcher.condition.folder.condition.fileRemove"},
		{MacroConditionFolder::Condition::FOLDER_CHANGE,
		 "AdvSceneSwitcher.condition.folder.condition.folderChange"},
		{MacroConditionFolder::Condition::FOLDER_REMOVE,
		 "AdvSceneSwitcher.condition.folder.condition.folderRemove"},
};

MacroConditionFolder::MacroConditionFolder(Macro *m) : MacroCondition(m, true)
{
}

bool MacroConditionFolder::CheckCondition()
{
	bool ret = _matched;

	if (_lastWatchedValue != _folder.UnresolvedValue()) {
		SetupWatcher();
	}

	_newOrChangedPaths.clear();
	_removedPaths.clear();
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
	_filter.Save(obj, "filter");
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

void MacroConditionFolder::FileChanged(const QString &path)
{
	if (MacroIsPaused(GetMacro())) {
		return;
	}

	if (_condition != Condition::ANY &&
	    _condition != Condition::FILE_CHANGE &&
	    _condition != Condition::FILE_REMOVE) {
		return;
	}

	if (_enableFilter && !_regex.Matches(path.toStdString(), _filter)) {
		return;
	}

	QFileInfo fileInfo(path);
	if (fileInfo.exists()) {
		_newOrChangedPaths << path;
	} else {
		_removedPaths << path;
	}

	_matched = true;
}

void MacroConditionFolder::DirectoryChanged(const QString &path)
{
	if (MacroIsPaused(GetMacro())) {
		return;
	}

	if (_condition != Condition::ANY &&
	    _condition != Condition::FOLDER_CHANGE &&
	    _condition != Condition::FOLDER_REMOVE) {
		return;
	}

	if (_enableFilter && !_regex.Matches(path.toStdString(), _filter)) {
		return;
	}

	QFileInfo fileInfo(path);
	if (fileInfo.exists()) {
		_newOrChangedPaths << path;
	} else {
		_removedPaths << path;
	}

	_matched = true;
}

void MacroConditionFolder::SetupWatcher()
{
	_watcher = std::make_unique<QFileSystemWatcher>();
	_watcher->addPath(QString::fromStdString(_folder));
	_lastWatchedValue = _folder.UnresolvedValue();
	connect(_watcher.get(), SIGNAL(directoryChanged(const QString &)), this,
		SLOT(DirectoryChanged(const QString &)));
	connect(_watcher.get(), SIGNAL(fileChanged(const QString &)), this,
		SLOT(FileChanged(const QString &)));
}

static void populateConditions(QComboBox *list)
{
	for (const auto &[_, name] : conditions) {
		list->addItem(obs_module_text(name.c_str()));
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

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{conditions}}", _conditions},
		{"{{folder}}", _folder},
		{"{{regex}}", _regex},
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

	_conditions->setCurrentIndex(static_cast<int>(_entryData->_condition));
	_folder->SetPath(_entryData->GetFolder());
	_enableFilter->setChecked(_entryData->_enableFilter);
	_regex->SetRegexConfig(_entryData->_regex);
	_filter->setText(_entryData->_filter);

	SetWidgetVisibility();
}

void MacroConditionFolderEdit::ConditionChanged(int index)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_condition =
		static_cast<MacroConditionFolder::Condition>(index);
}

void MacroConditionFolderEdit::PathChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->SetFolder(text.toStdString());
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionFolderEdit::RegexChanged(const RegexConfig &regex)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_regex = regex;
}

void MacroConditionFolderEdit::FilterChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_filter = _filter->text().toStdString();
}

void MacroConditionFolderEdit::EnableFilterChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
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
