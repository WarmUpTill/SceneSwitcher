#include "macro-action-log.hpp"
#include "layout-helpers.hpp"

namespace advss {

const std::string MacroActionLog::id = "log";

bool MacroActionLog::_registered = MacroActionFactory::Register(
	MacroActionLog::id, {MacroActionLog::Create, MacroActionLogEdit::Create,
			     "AdvSceneSwitcher.action.log"});

bool MacroActionLog::PerformAction()
{
	blog(LOG_INFO, "%s", std::string(_logMessage).c_str());
	return true;
}

bool MacroActionLog::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	_logMessage.Save(obj, "logMessage");
	return true;
}

bool MacroActionLog::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_logMessage.Load(obj, "logMessage");
	return true;
}

std::shared_ptr<MacroAction> MacroActionLog::Create(Macro *m)
{
	return std::make_shared<MacroActionLog>(m);
}

std::shared_ptr<MacroAction> MacroActionLog::Copy() const
{
	return std::make_shared<MacroActionLog>(*this);
}

void MacroActionLog::ResolveVariablesToFixedValues()
{
	_logMessage.ResolveVariables();
}

MacroActionLogEdit::MacroActionLogEdit(
	QWidget *parent, std::shared_ptr<MacroActionLog> entryData)
	: QWidget(parent),
	  _logMessage(new VariableTextEdit(this, 5, 1, 1))
{
	QWidget::connect(_logMessage, SIGNAL(textChanged()), this,
			 SLOT(LogMessageChanged()));

	auto layout = new QHBoxLayout();
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.action.log.entry"),
		     layout, {{"{{logMessage}}", _logMessage}}, false);
	setLayout(layout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionLogEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_logMessage->setPlainText(_entryData->_logMessage);
	adjustSize();
	updateGeometry();
}

void MacroActionLogEdit::LogMessageChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_logMessage = _logMessage->toPlainText().toStdString();

	adjustSize();
	updateGeometry();
}

} // namespace advss
