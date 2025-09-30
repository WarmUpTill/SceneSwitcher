#include "macro-condition-virtual-cam.hpp"
#include "layout-helpers.hpp"

#include <obs.hpp>
#include <obs-frontend-api.h>

namespace advss {

const std::string MacroConditionVCam::id = "virtual_cam";

bool MacroConditionVCam::_registered = MacroConditionFactory::Register(
	MacroConditionVCam::id,
	{MacroConditionVCam::Create, MacroConditionVCamEdit::Create,
	 "AdvSceneSwitcher.condition.virtualCamera"});

bool MacroConditionVCam::CheckCondition()
{
	bool match = false;
#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(27, 0, 0)
	switch (_state) {
	case Condition::STOP:
		match = !obs_frontend_virtualcam_active();
		break;
	case Condition::START:
		match = obs_frontend_virtualcam_active();
		break;
	default:
		break;
	}
#endif
	return match;
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
	_state = static_cast<Condition>(obs_data_get_int(obj, "state"));
	return true;
}

static inline void populateStateSelection(QComboBox *list)
{
	static const std::map<MacroConditionVCam::Condition, std::string>
		conditions = {
			{MacroConditionVCam::Condition::STOP,
			 "AdvSceneSwitcher.condition.virtualCamera.state.stop"},
			{MacroConditionVCam::Condition::START,
			 "AdvSceneSwitcher.condition.virtualCamera.state.start"},
		};

	for (const auto &[_, name] : conditions) {
		list->addItem(obs_module_text(name.c_str()));
	}
}

MacroConditionVCamEdit::MacroConditionVCamEdit(
	QWidget *parent, std::shared_ptr<MacroConditionVCam> entryData)
	: QWidget(parent),
	  _states(new QComboBox(this))
{
	populateStateSelection(_states);
	QWidget::connect(_states, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(StateChanged(int)));

	auto layout = new QHBoxLayout;
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.virtualCamera.entry"),
		     layout, {{"{{states}}", _states}});
	setLayout(layout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionVCamEdit::StateChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_state = static_cast<MacroConditionVCam::Condition>(value);
}

void MacroConditionVCamEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_states->setCurrentIndex(static_cast<int>(_entryData->_state));
}

} // namespace advss
