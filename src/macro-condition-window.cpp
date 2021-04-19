#include "headers/macro-condition-edit.hpp"
#include "headers/macro-condition-window.hpp"
#include "headers/utility.hpp"
#include "headers/advanced-scene-switcher.hpp"

const int MacroConditionWindow::id = 1;

bool MacroConditionWindow::_registered = MacroConditionFactory::Register(
	MacroConditionWindow::id,
	{MacroConditionWindow::Create, MacroConditionWindowEdit::Create,
	 "AdvSceneSwitcher.condition.window"});

bool MacroConditionWindow::CheckCondition()
{
	std::string currentWindow;

	GetCurrentWindowTitle(currentWindow);
	return currentWindow == _window;
}

bool MacroConditionWindow::Save(obs_data_t *obj)
{
	return false;
}

bool MacroConditionWindow::Load(obs_data_t *obj)
{
	return false;
}

MacroConditionWindowEdit::MacroConditionWindowEdit(
	std::shared_ptr<MacroConditionWindow> entryData)
{
	_windowSelection = new QComboBox();

	QWidget::connect(_windowSelection,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(WindowChanged(int)));

	AdvSceneSwitcher::populateWindowSelection(_windowSelection);

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{windows}}", _windowSelection},
	};
	placeWidgets(obs_module_text(
			     "AdvSceneSwitcher.macro.condition.window.entry"),
		     mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionWindowEdit::WindowChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_window = text.toStdString();
}

void MacroConditionWindowEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_windowSelection->setCurrentText(_entryData->_window.c_str());
}
