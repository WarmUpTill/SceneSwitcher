#include "macro-action-recording.hpp"
#include "layout-helpers.hpp"

#include <obs-frontend-api.h>
#include <util/config-file.h>

namespace advss {

const std::string MacroActionRecord::id = "recording";

bool MacroActionRecord::_registered = MacroActionFactory::Register(
	MacroActionRecord::id,
	{MacroActionRecord::Create, MacroActionRecordEdit::Create,
	 "AdvSceneSwitcher.action.recording"});

const static std::map<MacroActionRecord::Action, std::string> actionTypes = {
	{MacroActionRecord::Action::STOP,
	 "AdvSceneSwitcher.action.recording.type.stop"},
	{MacroActionRecord::Action::START,
	 "AdvSceneSwitcher.action.recording.type.start"},
	{MacroActionRecord::Action::PAUSE,
	 "AdvSceneSwitcher.action.recording.type.pause"},
	{MacroActionRecord::Action::UNPAUSE,
	 "AdvSceneSwitcher.action.recording.type.unpause"},
#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(28, 0, 0)
	{MacroActionRecord::Action::SPLIT,
	 "AdvSceneSwitcher.action.recording.type.split"},
#endif
	{MacroActionRecord::Action::FOLDER,
	 "AdvSceneSwitcher.action.recording.type.changeOutputFolder"},
	{MacroActionRecord::Action::FILE_FORMAT,
	 "AdvSceneSwitcher.action.recording.type.changeOutputFileFormat"},
#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(30, 2, 0)
	{MacroActionRecord::Action::ADD_CHAPTER,
	 "AdvSceneSwitcher.action.recording.type.addChapter"},
#endif
};

bool MacroActionRecord::PerformAction()
{
	switch (_action) {
	case Action::STOP:
		if (obs_frontend_recording_active()) {
			obs_frontend_recording_stop();
		}
		break;
	case Action::START:
		if (!obs_frontend_recording_active()) {
			obs_frontend_recording_start();
		}
		break;
	case Action::PAUSE:
		if (obs_frontend_recording_active() &&
		    !obs_frontend_recording_paused()) {
			obs_frontend_recording_pause(true);
		}
		break;
	case Action::UNPAUSE:
		if (obs_frontend_recording_active() &&
		    obs_frontend_recording_paused()) {
			obs_frontend_recording_pause(false);
		}
		break;
#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(28, 0, 0)
	case Action::SPLIT:
		obs_frontend_recording_split_file();
		break;
#endif
	case Action::FOLDER: {
		std::string folder = _folder; // Trigger variable resolve

		auto conf = obs_frontend_get_profile_config();
		config_set_string(conf, "SimpleOutput", "FilePath",
				  folder.c_str());
		config_set_string(conf, "AdvOut", "FFFilePath", folder.c_str());
		config_set_string(conf, "AdvOut", "RecFilePath",
				  folder.c_str());
		if (config_save(conf) != CONFIG_SUCCESS) {
			blog(LOG_WARNING,
			     "failed to set recoding output folder");
		}
		break;
	}
	case Action::FILE_FORMAT: {
		std::string format = _fileFormat; // Trigger variable resolve

		auto conf = obs_frontend_get_profile_config();
		config_set_string(conf, "Output", "FilenameFormatting",
				  format.c_str());
		if (config_save(conf) != CONFIG_SUCCESS) {
			blog(LOG_WARNING,
			     "failed to set recoding file format string");
		}
		break;
	}
#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(30, 2, 0)
	case Action::ADD_CHAPTER:
		if (!obs_frontend_recording_add_chapter(_chapterName.c_str())) {
			blog(LOG_WARNING, "failed to add recoding chapter!");
		}
		break;
#endif
	default:
		break;
	}
	return true;
}

void MacroActionRecord::LogAction() const
{
	auto it = actionTypes.find(_action);
	if (it != actionTypes.end()) {
		ablog(LOG_INFO, "performed action \"%s\"", it->second.c_str());
	} else {
		blog(LOG_WARNING, "ignored unknown recording action %d",
		     static_cast<int>(_action));
	}
}

bool MacroActionRecord::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	_folder.Save(obj, "folder");
	_fileFormat.Save(obj, "format");
	_chapterName.Save(obj, "chapterName");
	return true;
}

bool MacroActionRecord::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_action = static_cast<Action>(obs_data_get_int(obj, "action"));
	_folder.Load(obj, "folder");
	_fileFormat.Load(obj, "format");
	_chapterName.Load(obj, "chapterName");
	return true;
}

std::shared_ptr<MacroAction> MacroActionRecord::Create(Macro *m)
{
	return std::make_shared<MacroActionRecord>(m);
}

std::shared_ptr<MacroAction> MacroActionRecord::Copy() const
{
	return std::make_shared<MacroActionRecord>(*this);
}

void MacroActionRecord::ResolveVariablesToFixedValues()
{
	_folder.ResolveVariables();
	_fileFormat.ResolveVariables();
	_chapterName.ResolveVariables();
}

static inline void populateActionSelection(QComboBox *list)
{
	for (const auto &[_, name] : actionTypes) {
		list->addItem(obs_module_text(name.c_str()));
	}
}

MacroActionRecordEdit::MacroActionRecordEdit(
	QWidget *parent, std::shared_ptr<MacroActionRecord> entryData)
	: QWidget(parent),
	  _actions(new QComboBox()),
	  _pauseHint(new QLabel(obs_module_text(
		  "AdvSceneSwitcher.action.recording.pause.hint"))),
	  _splitHint(new QLabel(obs_module_text(
		  "AdvSceneSwitcher.action.recording.split.hint"))),
	  _recordFolder(new FileSelection(FileSelection::Type::FOLDER, this)),
	  _recordFileFormat(new VariableLineEdit(this)),
	  _chapterName(new VariableLineEdit(this))
{
	populateActionSelection(_actions);

	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_recordFolder, SIGNAL(PathChanged(const QString &)),
			 this, SLOT(FolderChanged(const QString &)));
	QWidget::connect(_recordFileFormat, SIGNAL(editingFinished()), this,
			 SLOT(FormatStringChanged()));
	QWidget::connect(_chapterName, SIGNAL(editingFinished()), this,
			 SLOT(ChapterNameChanged()));

	auto mainLayout = new QHBoxLayout;
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.action.recording.entry"),
		     mainLayout,
		     {{"{{actions}}", _actions},
		      {"{{pauseHint}}", _pauseHint},
		      {"{{splitHint}}", _splitHint},
		      {"{{recordFolder}}", _recordFolder},
		      {"{{recordFileFormat}}", _recordFileFormat},
		      {"{{chapterName}}", _chapterName}});
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionRecordEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}
	_actions->setCurrentIndex(static_cast<int>(_entryData->_action));
	_recordFolder->SetPath(_entryData->_folder);
	_recordFileFormat->setText(_entryData->_fileFormat);
	_chapterName->setText(_entryData->_chapterName);
	SetWidgetVisibility();
}

static bool isPauseAction(MacroActionRecord::Action a)
{
	return a == MacroActionRecord::Action::PAUSE ||
	       a == MacroActionRecord::Action::UNPAUSE;
}

void MacroActionRecordEdit::FolderChanged(const QString &folder)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_folder = folder.toStdString();
}

void MacroActionRecordEdit::FormatStringChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_fileFormat = _recordFileFormat->text().toStdString();
}

void MacroActionRecordEdit::ChapterNameChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_chapterName = _chapterName->text().toStdString();
}

void MacroActionRecordEdit::SetWidgetVisibility()
{
	_pauseHint->setVisible(isPauseAction(_entryData->_action));
	_splitHint->setVisible(_entryData->_action ==
			       MacroActionRecord::Action::SPLIT);
	_recordFolder->setVisible(_entryData->_action ==
				  MacroActionRecord::Action::FOLDER);
	_recordFileFormat->setVisible(_entryData->_action ==
				      MacroActionRecord::Action::FILE_FORMAT);
	_chapterName->setVisible(_entryData->_action ==
				 MacroActionRecord::Action::ADD_CHAPTER);
}

void MacroActionRecordEdit::ActionChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_action = static_cast<MacroActionRecord::Action>(value);
	SetWidgetVisibility();
}

} // namespace advss
