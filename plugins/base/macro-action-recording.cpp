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
	{MacroActionRecord::Action::SPLIT,
	 "AdvSceneSwitcher.action.recording.type.split"},
	{MacroActionRecord::Action::FOLDER,
	 "AdvSceneSwitcher.action.recording.type.changeOutputFolder"},
	{MacroActionRecord::Action::FILE_FORMAT,
	 "AdvSceneSwitcher.action.recording.type.changeOutputFileFormat"},
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
	default:
		break;
	}
	return true;
}

void MacroActionRecord::LogAction() const
{
	auto it = actionTypes.find(_action);
	if (it != actionTypes.end()) {
		vblog(LOG_INFO, "performed action \"%s\"", it->second.c_str());
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
	return true;
}

bool MacroActionRecord::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_action = static_cast<Action>(obs_data_get_int(obj, "action"));
	_folder.Load(obj, "folder");
	_fileFormat.Load(obj, "format");
	return true;
}

static inline void populateActionSelection(QComboBox *list)
{
	for (auto entry : actionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
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
	  _recordFileFormat(new VariableLineEdit(this))
{
	populateActionSelection(_actions);

	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_recordFolder, SIGNAL(PathChanged(const QString &)),
			 this, SLOT(FolderChanged(const QString &)));
	QWidget::connect(_recordFileFormat, SIGNAL(editingFinished()), this,
			 SLOT(FormatStringChanged()));

	auto mainLayout = new QHBoxLayout;
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.action.recording.entry"),
		     mainLayout,
		     {{"{{actions}}", _actions},
		      {"{{pauseHint}}", _pauseHint},
		      {"{{splitHint}}", _splitHint},
		      {"{{recordFolder}}", _recordFolder},
		      {"{{recordFileFormat}}", _recordFileFormat}});
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
	SetWidgetVisibility();
}

static bool isPauseAction(MacroActionRecord::Action a)
{
	return a == MacroActionRecord::Action::PAUSE ||
	       a == MacroActionRecord::Action::UNPAUSE;
}

void MacroActionRecordEdit::FolderChanged(const QString &folder)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_folder = folder.toStdString();
}

void MacroActionRecordEdit::FormatStringChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_fileFormat = _recordFileFormat->text().toStdString();
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
}

void MacroActionRecordEdit::ActionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_action = static_cast<MacroActionRecord::Action>(value);
	SetWidgetVisibility();
}

} // namespace advss
