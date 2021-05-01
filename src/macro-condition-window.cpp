#include "headers/macro-condition-edit.hpp"
#include "headers/macro-condition-window.hpp"
#include "headers/utility.hpp"
#include "headers/advanced-scene-switcher.hpp"

#include <regex>

const int MacroConditionWindow::id = 1;

bool MacroConditionWindow::_registered = MacroConditionFactory::Register(
	MacroConditionWindow::id,
	{MacroConditionWindow::Create, MacroConditionWindowEdit::Create,
	 "AdvSceneSwitcher.condition.window"});

bool MacroConditionWindow::CheckWindowTitleSwitchDirect(
	std::string &currentWindowTitle)
{
	bool focus = (!_focus || _window == currentWindowTitle);
	bool fullscreen = (!_fullscreen || isFullscreen(_window));
	bool max = (!_maximized || isMaximized(_window));

	return focus && fullscreen && max;
}

bool MacroConditionWindow::CheckWindowTitleSwitchRegex(
	std::string &currentWindowTitle, std::vector<std::string> &windowList)
{
	bool match = false;
	for (auto &window : windowList) {
		try {
			std::regex expr(_window);
			if (!std::regex_match(window, expr)) {
				continue;
			}
		} catch (const std::regex_error &) {
		}

		bool focus = (!_focus || window == currentWindowTitle);
		bool fullscreen = (!_fullscreen || isFullscreen(window));
		bool max = (!_maximized || isMaximized(window));

		if (focus && fullscreen && max) {
			match = true;
			break;
		}
	}
	return match;
}

bool MacroConditionWindow::CheckCondition()
{
	std::string currentWindowTitle;
	GetCurrentWindowTitle(currentWindowTitle);

	std::vector<std::string> windowList;
	GetWindowList(windowList);

	bool match = false;

	if (std::find(windowList.begin(), windowList.end(), _window) !=
	    windowList.end()) {
		match = CheckWindowTitleSwitchDirect(currentWindowTitle);
	} else {
		match = CheckWindowTitleSwitchRegex(currentWindowTitle,
						    windowList);
	}

	return match;
}

bool MacroConditionWindow::Save(obs_data_t *obj)
{
	MacroCondition::Save(obj);
	obs_data_set_string(obj, "window", _window.c_str());
	obs_data_set_bool(obj, "fullscreen", _fullscreen);
	obs_data_set_bool(obj, "maximized", _maximized);
	obs_data_set_bool(obj, "focus", _focus);
	return true;
}

bool MacroConditionWindow::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_window = obs_data_get_string(obj, "window");
	_fullscreen = obs_data_get_bool(obj, "fullscreen");
#if __APPLE__
	// TODO: Implement maximized check on MacOS
	_maximized = false;
#else
	_maximized = obs_data_get_bool(obj, "maximized");
#endif
	_focus = obs_data_get_bool(obj, "focus");
	return true;
}

MacroConditionWindowEdit::MacroConditionWindowEdit(
	QWidget *parent, std::shared_ptr<MacroConditionWindow> entryData)
	: QWidget(parent)
{
	_windowSelection = new QComboBox();
	_windowSelection->setEditable(true);
	_windowSelection->setMaxVisibleItems(20);

	_fullscreen = new QCheckBox();
	_maximized = new QCheckBox();
	_focused = new QCheckBox();

	QWidget::connect(_windowSelection,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(WindowChanged(const QString &)));
	QWidget::connect(_fullscreen, SIGNAL(stateChanged(int)), this,
			 SLOT(FullscreenChanged(int)));
	QWidget::connect(_maximized, SIGNAL(stateChanged(int)), this,
			 SLOT(MaximizedChanged(int)));
	QWidget::connect(_focused, SIGNAL(stateChanged(int)), this,
			 SLOT(FocusChanged(int)));

	AdvSceneSwitcher::populateWindowSelection(_windowSelection);

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{windows}}", _windowSelection},
		{"{{fullscreen}}", _fullscreen},
		{"{{maximized}}", _maximized},
		{"{{focused}}", _focused},
	};

	QVBoxLayout *mainLayout = new QVBoxLayout;
	QHBoxLayout *line1Layout = new QHBoxLayout;
	QHBoxLayout *line2Layout = new QHBoxLayout;
	placeWidgets(
		obs_module_text(
			"AdvSceneSwitcher.macro.condition.window.entry.line1"),
		line1Layout, widgetPlaceholders);
	placeWidgets(
		obs_module_text(
			"AdvSceneSwitcher.macro.condition.window.entry.line2"),
		line2Layout, widgetPlaceholders);
	mainLayout->addLayout(line1Layout);
	mainLayout->addLayout(line2Layout);
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

void MacroConditionWindowEdit::FullscreenChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_fullscreen = state;
}

void MacroConditionWindowEdit::MaximizedChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_maximized = state;
}

void MacroConditionWindowEdit::FocusChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_focus = state;
}

void MacroConditionWindowEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_windowSelection->setCurrentText(_entryData->_window.c_str());
	_fullscreen->setChecked(_entryData->_fullscreen);
	_maximized->setChecked(_entryData->_maximized);
	_focused->setChecked(_entryData->_focus);
}
