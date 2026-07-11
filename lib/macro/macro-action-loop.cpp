#include "macro-action-loop.hpp"

#include "layout-helpers.hpp"
#include "macro-action-factory.hpp"
#include "macro.hpp"

namespace advss {

const std::string MacroActionLoop::id = "loop";

bool MacroActionLoop::_registered = MacroActionFactory::Register(
	MacroActionLoop::id,
	{MacroActionLoop::Create, MacroActionLoopEdit::Create,
	 "AdvSceneSwitcher.action.loop"});

bool MacroActionLoop::PerformAction()
{
	int iterations = 0;
	const int maxIterations = _maxIterations;
	Macro *parentMacro = GetMacro();

	if (maxIterations <= 0) {
		blog(LOG_WARNING,
		     "loop action has invalid max iterations value of %d",
		     maxIterations);
		return true;
	}

	while (_loopMacro->CheckConditions()) {
		if (iterations >= maxIterations) {
			blog(LOG_WARNING,
			     "loop action reached iteration limit of %d",
			     maxIterations);
			break;
		}
		ablog(LOG_INFO, "loop iteration %d", iterations);
		if (!_loopMacro->PerformActions(true, false, true)) {
			break;
		}
		++iterations;
		if (parentMacro && parentMacro->GetStop()) {
			break;
		}
	}
	ablog(LOG_INFO, "loop completed after %d iteration(s)", iterations);
	SetTempVarValue("count", std::to_string(iterations));
	return true;
}

void MacroActionLoop::LogAction() const
{
	ablog(LOG_INFO, "running loop (max %d iterations)",
	      _maxIterations.GetValue());
}

bool MacroActionLoop::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	_maxIterations.Save(obj, "maxIterations");
	OBSDataAutoRelease loopMacroData = obs_data_create();
	_loopMacro->Save(loopMacroData);
	obs_data_set_obj(obj, "loopMacro", loopMacroData);
	obs_data_set_int(obj, "customWidgetHeight", _customWidgetHeight);
	return true;
}

bool MacroActionLoop::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_maxIterations.Load(obj, "maxIterations");
	if (obs_data_has_user_value(obj, "loopMacro")) {
		OBSDataAutoRelease loopMacroData =
			obs_data_get_obj(obj, "loopMacro");
		_loopMacro = std::make_shared<Macro>();
		_loopMacro->Load(loopMacroData);
	}
	_customWidgetHeight = obs_data_get_int(obj, "customWidgetHeight");
	return true;
}

bool MacroActionLoop::PostLoad()
{
	MacroAction::PostLoad();
	_loopMacro->PostLoad();
	_loopMacro->SetActionTriggerMode(Macro::ActionTriggerMode::ALWAYS);
	return true;
}

std::shared_ptr<MacroAction> MacroActionLoop::Create(Macro *m)
{
	return std::make_shared<MacroActionLoop>(m);
}

std::shared_ptr<MacroAction> MacroActionLoop::Copy() const
{
	auto copy = std::make_shared<MacroActionLoop>(*this);

	OBSDataAutoRelease data = obs_data_create();
	_loopMacro->Save(data);

	copy->_loopMacro = std::make_shared<Macro>();
	copy->_loopMacro->Load(data);
	copy->_loopMacro->PostLoad();

	return copy;
}

void MacroActionLoop::ResolveVariablesToFixedValues()
{
	_maxIterations.ResolveVariables();
	for (auto &action : _loopMacro->Actions()) {
		action->ResolveVariablesToFixedValues();
	}
}

void MacroActionLoop::SetupTempVars()
{
	MacroAction::SetupTempVars();
	AddTempvar("count",
		   obs_module_text("AdvSceneSwitcher.tempVar.loop.count"),
		   obs_module_text(
			   "AdvSceneSwitcher.tempVar.loop.count.description"));
}

MacroActionLoopEdit::MacroActionLoopEdit(
	QWidget *parent, std::shared_ptr<MacroActionLoop> entryData)
	: ResizableWidget(parent),
	  _macroEdit(new MacroEdit(
		  this, QStringList()
				<< "AdvSceneSwitcher.action.loop.conditionHelp"
				<< "AdvSceneSwitcher.action.loop.actionHelp"
				<< "")),
	  _maxIterations(new VariableSpinBox())
{
	_maxIterations->setMinimum(1);
	_maxIterations->setMaximum(10000000);

	QWidget::connect(
		_maxIterations,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(MaxIterationsChanged(const NumberVariable<int> &)));

	auto controlsLayout = new QHBoxLayout();
	controlsLayout->addWidget(new QLabel(
		obs_module_text("AdvSceneSwitcher.action.loop.maxIterations")));
	controlsLayout->addWidget(_maxIterations);
	controlsLayout->addStretch();

	auto layout = new QVBoxLayout();
	layout->addLayout(controlsLayout);
	layout->addWidget(_macroEdit);
	setLayout(layout);

	_macroEdit->HideElseSection();

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

MacroActionLoopEdit::~MacroActionLoopEdit()
{
	if (!_entryData) {
		return;
	}
	_entryData->_customWidgetHeight = GetCustomHeight();
	_macroEdit->SetMacro({});
}

void MacroActionLoopEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_maxIterations->SetValue(_entryData->_maxIterations);
	_macroEdit->SetMacro(_entryData->_loopMacro);

	if (_macroEdit->IsEmpty()) {
		_macroEdit->ShowAllMacroSections();
		_entryData->_customWidgetHeight = 600;
	}
	SetResizingEnabled(true);
	SetCustomHeight(_entryData->_customWidgetHeight);

	adjustSize();
	updateGeometry();
}

void MacroActionLoopEdit::MaxIterationsChanged(const NumberVariable<int> &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_maxIterations = value;
}

} // namespace advss
