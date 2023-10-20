#include "macro-action-widget.hpp"

#include <log-helper.hpp>
#include <utility.hpp>

namespace advss {

const std::string MacroActionWidget::id = "widget";

bool MacroActionWidget::_registered = MacroActionFactory::Register(
	MacroActionWidget::id,
	{MacroActionWidget::Create, MacroActionWidgetEdit::Create,
	 "AdvSceneSwitcher.action.widget"});

const static std::map<MacroActionWidget::Action, std::string> actionTypes = {
	{MacroActionWidget::Action::MESSAGE_DIALOG,
	 "AdvSceneSwitcher.action.widget.type.messageDialog"},
	{MacroActionWidget::Action::ERROR_DIALOG,
	 "AdvSceneSwitcher.action.widget.type.errorDialog"},
};

bool MacroActionWidget::PerformAction()
{
	switch (_action) {
	case MacroActionWidget::Action::MESSAGE_DIALOG:
		DisplayMessageDialog();
		break;
	case MacroActionWidget::Action::ERROR_DIALOG:
		DisplayErrorDialog();
		break;
	default:
		break;
	}

	return true;
}

void MacroActionWidget::DisplayMessageDialog() const
{
	auto message = std::string(_message);

	if (message.empty()) {
		blog(LOG_WARNING, "No message for popup provided.");
	}

	DisplayMessage(message.c_str());
}

void MacroActionWidget::DisplayErrorDialog() const
{
	auto errorMessage = std::string(_errorMessage);

	if (errorMessage.empty()) {
		blog(LOG_WARNING, "No error message for popup provided.");
	}

	DisplayErrorMessage(errorMessage.c_str());
}

bool MacroActionWidget::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	_message.Save(obj, "message");
	_errorMessage.Save(obj, "errorMessage");

	return true;
}

bool MacroActionWidget::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_action = static_cast<Action>(obs_data_get_int(obj, "action"));
	_message.Load(obj, "message");
	_errorMessage.Load(obj, "errorMessage");

	return true;
}

static inline void populateActionSelection(QComboBox *list)
{
	for (const auto &[_, name] : actionTypes) {
		list->addItem(obs_module_text(name.c_str()));
	}
}

MacroActionWidgetEdit::MacroActionWidgetEdit(
	QWidget *parent, std::shared_ptr<MacroActionWidget> entryData)
	: QWidget(parent),
	  _actions(new QComboBox()),
	  _message(new VariableTextEdit(this)),
	  _errorMessage(new VariableTextEdit(this))
{
	populateActionSelection(_actions);

	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_message, SIGNAL(textChanged()), this,
			 SLOT(MessageChanged()));
	QWidget::connect(_errorMessage, SIGNAL(textChanged()), this,
			 SLOT(ErrorMessageChanged()));

	auto mainLayout = new QVBoxLayout();
	mainLayout->addWidget(_actions);
	mainLayout->addWidget(_message);
	mainLayout->addWidget(_errorMessage);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionWidgetEdit::ActionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_action = static_cast<MacroActionWidget::Action>(value);

	SetupWidgetVisibility();
}

void MacroActionWidgetEdit::MessageChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_message = _message->toPlainText().toStdString();

	adjustSize();
	updateGeometry();
}

void MacroActionWidgetEdit::ErrorMessageChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_errorMessage = _errorMessage->toPlainText().toStdString();

	adjustSize();
	updateGeometry();
}

void MacroActionWidgetEdit::SetupWidgetVisibility()
{
	_message->setVisible(_entryData->_action ==
			     MacroActionWidget::Action::MESSAGE_DIALOG);
	_errorMessage->setVisible(_entryData->_action ==
				  MacroActionWidget::Action::ERROR_DIALOG);

	adjustSize();
	updateGeometry();
}

void MacroActionWidgetEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_actions->setCurrentIndex(static_cast<int>(_entryData->_action));
	_message->setPlainText(_entryData->_message);
	_errorMessage->setPlainText(_entryData->_errorMessage);

	SetupWidgetVisibility();
}

} // namespace advss
