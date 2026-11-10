#include "macro-action-variable-input.hpp"
#include "help-icon.hpp"
#include "layout-helpers.hpp"
#include "macro.hpp"
#include "non-modal-dialog.hpp"

#include <thread>

namespace advss {

using Action = MacroActionVariable::Action;

static bool registered = RegisterVariableActionGroup(
	{50,
	 {
		 {Action::USER_INPUT,
		  "AdvSceneSwitcher.action.variable.type.askForValue"},
		 {Action::ENV_VARIABLE,
		  "AdvSceneSwitcher.action.variable.type.environmentVariable",
		  "AdvSceneSwitcher.action.variable.layout.environmentVariable",
		  true},
	 },
	 []() { return std::make_unique<VariableActionInput>(); },
	 VariableActionInputEdit::Create});

struct AskForInputParams {
	AskForInputParams(const QString &prompt_, const QString &placeholder_,
			  ValueInputType type_, const QStringList &options_)
		: prompt(prompt_),
		  placeholder(placeholder_),
		  type(type_),
		  options(options_){};
	QString prompt;
	QString placeholder;
	ValueInputType type;
	QStringList options;
	std::optional<std::string> result;
	std::atomic_bool resultReady = {false};
};

static bool isChoiceInputType(ValueInputType type)
{
	return type == ValueInputType::YesNo ||
	       type == ValueInputType::OkCancel;
}

static std::string getChoiceValue(ValueInputType type, bool accepted)
{
	if (type == ValueInputType::YesNo) {
		return accepted ? "yes" : "no";
	}
	return accepted ? "ok" : "cancel";
}

static void askForInput(void *param)
{
	auto parameters =
		*static_cast<std::shared_ptr<AskForInputParams> *>(param);
	if (isChoiceInputType(parameters->type)) {
		auto dialog = new NonModalMessageDialog(
			parameters->prompt, parameters->type, "", {});
		const bool accepted = dialog->ShowMessage() == QMessageBox::Yes;
		parameters->result = getChoiceValue(parameters->type, accepted);
		parameters->resultReady = true;
		return;
	}

	NonModalMessageDialog *dialog = nullptr;
	if (parameters->type == ValueInputType::String) {
		dialog = new NonModalMessageDialog(
			parameters->prompt, NonModalMessageDialog::Type::INPUT,
			false);
		dialog->SetInput(parameters->placeholder);
	} else {
		dialog = new NonModalMessageDialog(
			parameters->prompt, parameters->type,
			parameters->placeholder.toStdString(),
			parameters->options);
	}
	parameters->result = dialog->GetInput();
	parameters->resultReady = true;
}

std::unique_ptr<VariableActionGroup> VariableActionInput::Copy() const
{
	return std::make_unique<VariableActionInput>(*this);
}

void VariableActionInput::Save(obs_data_t *obj,
			       const MacroActionVariable &) const
{
	obs_data_set_bool(obj, "useCustomPrompt", _useCustomPrompt);
	_inputPrompt.Save(obj, "inputPrompt");
	obs_data_set_bool(obj, "useInputPlaceholder", _useInputPlaceholder);
	_inputPlaceholder.Save(obj, "inputPlaceholder");
	obs_data_set_int(obj, "inputType", static_cast<int>(_inputType));
	_inputOptions.Save(obj, "inputOptions", "value");
	_envVariableName.Save(obj, "environmentVariableName");
}

void VariableActionInput::Load(obs_data_t *obj, MacroActionVariable &)
{
	_useCustomPrompt = obs_data_get_bool(obj, "useCustomPrompt");
	_inputPrompt.Load(obj, "inputPrompt");
	_useInputPlaceholder = obs_data_get_bool(obj, "useInputPlaceholder");
	_inputPlaceholder.Load(obj, "inputPlaceholder");
	obs_data_set_default_int(obj, "inputType",
				 static_cast<int>(ValueInputType::String));
	_inputType =
		static_cast<ValueInputType>(obs_data_get_int(obj, "inputType"));
	_inputOptions.Load(obj, "inputOptions", "value");
	_envVariableName.Load(obj, "environmentVariableName");
}

void VariableActionInput::ResolveVariables()
{
	_inputPrompt.ResolveVariables();
	_inputPlaceholder.ResolveVariables();
	_inputOptions.ResolveVariables();
	_envVariableName.ResolveVariables();
}

bool VariableActionInput::AskForUserInput(MacroActionVariable &action,
					  Variable &var)
{
	QStringList options;
	for (const auto &option : _inputOptions) {
		options << QString::fromStdString(option);
	}
	const bool useCustomPrompt = _useCustomPrompt ||
				     isChoiceInputType(_inputType);
	auto params = std::make_shared<AskForInputParams>(
		useCustomPrompt
			? QString::fromStdString(_inputPrompt)
			: QString(obs_module_text(
					  "AdvSceneSwitcher.action.variable.askForValuePromptDefault"))
				  .arg(QString::fromStdString(var.Name())),
		useCustomPrompt && _useInputPlaceholder
			? QString::fromStdString(_inputPlaceholder)
			: "",
		_inputType, options);
	obs_queue_task(OBS_TASK_UI, askForInput, &params, false);
	while (!params->resultReady) {
		if (action.GetMacro()->GetStop()) {
			return false;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	if (!params->result.has_value()) {
		return true;
	}
	var.SetValue(*params->result);
	return true;
}

bool VariableActionInput::Perform(MacroActionVariable &action,
				  MacroActionVariable::Action type,
				  Variable &var)
{
	switch (type) {
	case Action::USER_INPUT:
		return AskForUserInput(action, var);
	case Action::ENV_VARIABLE: {
		auto value = std::getenv(_envVariableName.c_str());
		if (value) {
			var.SetValue(value);
		}
		return true;
	}
	default:
		return true;
	}
}

VariableActionInputEdit::VariableActionInputEdit(
	QWidget *parent, std::shared_ptr<MacroActionVariable> entryData)
	: VariableActionGroupEdit(parent, entryData),
	  _data(entryData ? entryData->GetGroup<VariableActionInput>()
			  : nullptr),
	  _promptLayout(new QHBoxLayout()),
	  _useCustomPrompt(new QCheckBox(this)),
	  _inputPrompt(new VariableLineEdit(this)),
	  _choicePromptLayout(new QHBoxLayout()),
	  _choicePrompt(new VariableLineEdit(this)),
	  _placeholderLayout(new QHBoxLayout()),
	  _useInputPlaceholder(new QCheckBox(this)),
	  _inputPlaceholder(new VariableLineEdit(this)),
	  _inputTypeLayout(new QHBoxLayout()),
	  _inputType(new QComboBox(this)),
	  _inputTypeHelp(new HelpIcon("", this)),
	  _inputOptions(new StringListEdit(this)),
	  _envVariable(new VariableLineEdit(this))
{
	_inputPrompt->setSizePolicy(QSizePolicy::MinimumExpanding,
				    QSizePolicy::Preferred);
	_choicePrompt->setSizePolicy(QSizePolicy::MinimumExpanding,
				     QSizePolicy::Preferred);
	_inputOptions->SetMaxStringSize(99999999);
	PopulateValueInputTypeSelection(_inputType);
	_inputType->addItem(
		obs_module_text("AdvSceneSwitcher.valueInputType.yesNo"),
		static_cast<int>(ValueInputType::YesNo));
	_inputType->addItem(
		obs_module_text("AdvSceneSwitcher.valueInputType.okCancel"),
		static_cast<int>(ValueInputType::OkCancel));

	QWidget::connect(_useCustomPrompt, SIGNAL(stateChanged(int)), this,
			 SLOT(UseCustomPromptChanged(int)));
	QWidget::connect(_inputPrompt, SIGNAL(editingFinished()), this,
			 SLOT(InputPromptChanged()));
	QWidget::connect(_choicePrompt, SIGNAL(editingFinished()), this,
			 SLOT(ChoicePromptChanged()));
	QWidget::connect(_useInputPlaceholder, SIGNAL(stateChanged(int)), this,
			 SLOT(UseInputPlaceholderChanged(int)));
	QWidget::connect(_inputPlaceholder, SIGNAL(editingFinished()), this,
			 SLOT(InputPlaceholderChanged()));
	QWidget::connect(_inputType, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(InputTypeChanged(int)));
	QWidget::connect(_inputOptions,
			 SIGNAL(StringListChanged(const StringList &)), this,
			 SLOT(InputOptionsChanged(const StringList &)));
	QWidget::connect(_envVariable, SIGNAL(editingFinished()), this,
			 SLOT(EnvVariableChanged()));

	const std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{useCustomPrompt}}", _useCustomPrompt},
		{"{{inputPrompt}}", _inputPrompt},
		{"{{choicePrompt}}", _choicePrompt},
		{"{{useInputPlaceholder}}", _useInputPlaceholder},
		{"{{inputPlaceholder}}", _inputPlaceholder},
		{"{{inputType}}", _inputType},
		{"{{inputTypeHelp}}", _inputTypeHelp},
	};
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.action.variable.layout.userInput.type"),
		_inputTypeLayout, widgetPlaceholders);
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.action.variable.layout.userInput.customPrompt"),
		_promptLayout, widgetPlaceholders);
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.action.variable.layout.userInput.choicePrompt"),
		_choicePromptLayout, widgetPlaceholders, false);
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.action.variable.layout.userInput.placeholder"),
		_placeholderLayout, widgetPlaceholders);

	auto layout = new QVBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addLayout(_inputTypeLayout);
	layout->addWidget(_inputOptions);
	layout->addLayout(_promptLayout);
	layout->addLayout(_choicePromptLayout);
	layout->addLayout(_placeholderLayout);
	setLayout(layout);
}

VariableActionGroupEdit *
VariableActionInputEdit::Create(QWidget *parent,
				std::shared_ptr<MacroActionVariable> entryData)
{
	return new VariableActionInputEdit(parent, entryData);
}

std::unordered_map<std::string, QWidget *>
VariableActionInputEdit::GetInlineWidgets()
{
	return {{"{{envVariableName}}", _envVariable}};
}

void VariableActionInputEdit::UpdateEntryData()
{
	if (!_data) {
		return;
	}

	_useCustomPrompt->setChecked(_data->_useCustomPrompt);
	_inputPrompt->setText(_data->_inputPrompt);
	_choicePrompt->setText(_data->_inputPrompt);
	_useInputPlaceholder->setChecked(_data->_useInputPlaceholder);
	_inputPlaceholder->setText(_data->_inputPlaceholder);
	_inputType->setCurrentIndex(
		_inputType->findData(static_cast<int>(_data->_inputType)));
	_inputOptions->SetStringList(_data->_inputOptions);
	_envVariable->setText(_data->_envVariableName);
	_loading = false;
}

void VariableActionInputEdit::SetWidgetVisibility(
	MacroActionVariable::Action action)
{
	if (!_data) {
		return;
	}

	const bool isUserInput = action == Action::USER_INPUT;
	const bool isChoice = isUserInput &&
			      isChoiceInputType(_data->_inputType);
	SetLayoutVisible(_inputTypeLayout, isUserInput);
	_inputTypeHelp->setVisible(isChoice);
	_inputTypeHelp->setToolTip(
		_data->_inputType == ValueInputType::YesNo
			? obs_module_text(
				  "AdvSceneSwitcher.action.variable.userInput.yesNo.help")
			: obs_module_text(
				  "AdvSceneSwitcher.action.variable.userInput.okCancel.help"));
	_inputOptions->setVisible(
		isUserInput && _data->_inputType == ValueInputType::Selection);
	SetLayoutVisible(_promptLayout, isUserInput && !isChoice);
	_inputPrompt->setVisible(isUserInput && !isChoice &&
				 _data->_useCustomPrompt);
	SetLayoutVisible(_choicePromptLayout, isChoice);
	if (_data->_useCustomPrompt) {
		RemoveStretchIfPresent(_promptLayout);
	} else {
		AddStretchIfNecessary(_promptLayout);
	}
	const bool showPlaceholder = isUserInput && !isChoice &&
				     _data->_useCustomPrompt;
	SetLayoutVisible(_placeholderLayout, showPlaceholder);
	_useInputPlaceholder->setVisible(showPlaceholder);
	_inputPlaceholder->setVisible(showPlaceholder &&
				      _data->_useInputPlaceholder);
	if (_data->_useInputPlaceholder) {
		RemoveStretchIfPresent(_placeholderLayout);
	} else {
		AddStretchIfNecessary(_placeholderLayout);
	}
	setVisible(isUserInput);
	UpdateSize();
}

void VariableActionInputEdit::UseCustomPromptChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_data->_useCustomPrompt = value;
	SetWidgetVisibility(_entryData->_action);
}

void VariableActionInputEdit::InputPromptChanged()
{
	GUARD_LOADING_AND_LOCK();
	_data->_inputPrompt = _inputPrompt->text().toStdString();
	_choicePrompt->setText(_data->_inputPrompt);
}

void VariableActionInputEdit::ChoicePromptChanged()
{
	GUARD_LOADING_AND_LOCK();
	_data->_inputPrompt = _choicePrompt->text().toStdString();
	_inputPrompt->setText(_data->_inputPrompt);
}

void VariableActionInputEdit::UseInputPlaceholderChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_data->_useInputPlaceholder = value;
	SetWidgetVisibility(_entryData->_action);
}

void VariableActionInputEdit::InputPlaceholderChanged()
{
	GUARD_LOADING_AND_LOCK();
	_data->_inputPlaceholder = _inputPlaceholder->text().toStdString();
}

void VariableActionInputEdit::InputTypeChanged(int)
{
	GUARD_LOADING_AND_LOCK();
	_data->_inputType =
		static_cast<ValueInputType>(_inputType->currentData().toInt());
	SetWidgetVisibility(_entryData->_action);
}

void VariableActionInputEdit::InputOptionsChanged(const StringList &options)
{
	GUARD_LOADING_AND_LOCK();
	_data->_inputOptions = options;
}

void VariableActionInputEdit::EnvVariableChanged()
{
	GUARD_LOADING_AND_LOCK();
	_data->_envVariableName = _envVariable->text().toStdString();
}

} // namespace advss
