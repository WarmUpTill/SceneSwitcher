#include "macro-condition-virtual-cam.hpp"
#include "layout-helpers.hpp"

#include <obs-frontend-api.h>

namespace advss {

const std::string MacroConditionVCam::id = "virtual_cam";

bool MacroConditionVCam::_registered = MacroConditionFactory::Register(
	MacroConditionVCam::id,
	{MacroConditionVCam::Create, MacroConditionVCamEdit::Create,
	 "AdvSceneSwitcher.condition.virtualCamera"});

const static std::map<VCamState, std::string> VCamStates = {
	{VCamState::STOP,
	 "AdvSceneSwitcher.condition.virtualCamera.state.stop"},
	{VCamState::START,
	 "AdvSceneSwitcher.condition.virtualCamera.state.start"},
};

bool MacroConditionVCam::CheckCondition()
{
	bool stateMatch = false;
#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(27, 0, 0)
	switch (_state) {
	case VCamState::STOP:
		stateMatch = !obs_frontend_virtualcam_active();
		break;
	case VCamState::START:
		stateMatch = obs_frontend_virtualcam_active();
		break;
	default:
		break;
	}
#endif
	return stateMatch;
}

bool MacroConditionVCam::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	obs_data_set_int(obj, "state", static_cast<int>(_state));
	return true;
}

bool MacroConditionVCam::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_state = static_cast<VCamState>(obs_data_get_int(obj, "state"));
	return true;
}

static inline void populateStateSelection(QComboBox *list)
{
	for (auto entry : VCamStates) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroConditionVCamEdit::MacroConditionVCamEdit(
	QWidget *parent, std::shared_ptr<MacroConditionVCam> entryData)
	: QWidget(parent)
{
	_states = new QComboBox();

	QWidget::connect(_states, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(StateChanged(int)));

	populateStateSelection(_states);

	QHBoxLayout *mainLayout = new QHBoxLayout;

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{states}}", _states},
	};

	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.virtualCamera.entry"),
		     mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionVCamEdit::StateChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_state = static_cast<VCamState>(value);
}

void MacroConditionVCamEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_states->setCurrentIndex(static_cast<int>(_entryData->_state));
}

} // namespace advss
