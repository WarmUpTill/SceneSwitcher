#include "macro-condition-window.hpp"
#include "layout-helpers.hpp"
#include "plugin-state-helpers.hpp"
#include "platform-funcs.hpp"

#include <regex>

namespace advss {

const std::string MacroConditionWindow::id = "window";

bool MacroConditionWindow::_registered = MacroConditionFactory::Register(
	MacroConditionWindow::id,
	{MacroConditionWindow::Create, MacroConditionWindowEdit::Create,
	 "AdvSceneSwitcher.condition.window"});

void MacroConditionWindow::SetCheckText(bool value)
{
#ifdef _WIN32
	_checkText = value;
#else
	(void)value;
	_checkText = false;
#endif
	SetupTempVars();
}

bool MacroConditionWindow::GetCheckText()
{
	return _checkText;
}

bool MacroConditionWindow::WindowMatchesRequirements(
	const WindowInfo &info) const
{
	if (_focus && !info.focused) {
		return false;
	}
	if (_fullscreen && !info.fullscreen) {
		return false;
	}
	if (_maximized && !info.maximized) {
		return false;
	}
	if (_checkText) {
		if (!info.text.has_value()) {
			return false;
		}
		if (_textRegex.Enabled()) {
			if (!_textRegex.Matches(*info.text, _text)) {
				return false;
			}
		} else {
			if (*info.text != std::string(_text)) {
				return false;
			}
		}
	}
	return true;
}

bool MacroConditionWindow::FindMatch(const std::vector<WindowInfo> &windows)
{
	// When regex is enabled the title check is always active (the backend
	// uses ".*" when the user disables the title check), so we can use a
	// single predicate for both modes.
	for (const auto &info : windows) {
		const bool titleOK =
			_windowRegex.Enabled()
				? _windowRegex.Matches(info.title, _window)
				: (!_checkTitle ||
				   info.title == std::string(_window));
		if (!titleOK || !WindowMatchesRequirements(info)) {
			continue;
		}
		SetVariableValueBasedOnMatch(&info);
		return true;
	}
	SetVariableValueBasedOnMatch(nullptr);
	return false;
}

void MacroConditionWindow::SetVariableValueBasedOnMatch(const WindowInfo *info)
{
	const std::string title = info ? info->title : "";
	SetTempVarValue("window", title);
	SetTempVarValue("windowX", info ? std::to_string(info->x) : "");
	SetTempVarValue("windowY", info ? std::to_string(info->y) : "");
	SetTempVarValue("windowWidth", info ? std::to_string(info->width) : "");
	SetTempVarValue("windowHeight",
			info ? std::to_string(info->height) : "");
#ifdef _WIN32
	SetTempVarValue("windowClass", info ? info->windowClass : "");
	if (_checkText) {
		SetTempVarValue("windowText",
				(info && info->text) ? *info->text : "");
	}
#endif
	if (!IsReferencedInVars()) {
		return;
	}
	if (_checkText) {
		SetVariableValue((info && info->text) ? *info->text : "");
	} else {
		SetVariableValue(ForegroundWindowTitle());
	}
}

static bool foregroundWindowChanged()
{
	return ForegroundWindowTitle() != PreviousForegroundWindowTitle();
}

bool MacroConditionWindow::CheckCondition()
{
	WindowQueryOptions options;
	options.focus = _focus;
	options.fullscreen = _fullscreen;
	options.maximized = _maximized;
	options.geometry = true;
#ifdef _WIN32
	options.windowClass = true;
	options.text = _checkText;
#endif

	const auto windows = GetWindows(options);
	bool match = FindMatch(windows);
	match = match && (!_windowFocusChanged || foregroundWindowChanged());
	return match;
}

bool MacroConditionWindow::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	obs_data_set_bool(obj, "checkTitle", _checkTitle);
	_window.Save(obj, "window");
	_windowRegex.Save(obj, "windowRegexConfig");
	obs_data_set_bool(obj, "fullscreen", _fullscreen);
	obs_data_set_bool(obj, "maximized", _maximized);
	obs_data_set_bool(obj, "focus", _focus);
	obs_data_set_bool(obj, "windowFocusChanged", _windowFocusChanged);
	obs_data_set_bool(obj, "checkWindowText", _checkText);
	_text.Save(obj, "text");
	_textRegex.Save(obj, "textRegexConfig");
	obs_data_set_int(obj, "version", 1);
	return true;
}

bool MacroConditionWindow::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	if (!obs_data_has_user_value(obj, "version")) {
		_checkTitle = true;
		_windowRegex.CreateBackwardsCompatibleRegex(true, true);
	} else {
		_checkTitle = obs_data_get_bool(obj, "checkTitle");
		_windowRegex.Load(obj, "windowRegexConfig");
	}
	_window.Load(obj, "window");
	_fullscreen = obs_data_get_bool(obj, "fullscreen");
	_maximized = obs_data_get_bool(obj, "maximized");
	_focus = obs_data_get_bool(obj, "focus");
	_windowFocusChanged = obs_data_get_bool(obj, "windowFocusChanged");
#ifdef _WIN32
	_checkText = obs_data_get_bool(obj, "checkWindowText");
#else
	_checkText = false;
#endif
	_text.Load(obj, "text");
	_textRegex.Load(obj, "textRegexConfig");
	return true;
}

std::string MacroConditionWindow::GetShortDesc() const
{
	return _window;
}

void MacroConditionWindow::SetupTempVars()
{
	MacroCondition::SetupTempVars();
	AddTempvar(
		"window",
		obs_module_text("AdvSceneSwitcher.tempVar.window.window"),
		obs_module_text(
			"AdvSceneSwitcher.tempVar.window.window.description"));
	AddTempvar(
		"windowX",
		obs_module_text("AdvSceneSwitcher.tempVar.window.windowX"),
		obs_module_text(
			"AdvSceneSwitcher.tempVar.window.windowX.description"));
	AddTempvar(
		"windowY",
		obs_module_text("AdvSceneSwitcher.tempVar.window.windowY"),
		obs_module_text(
			"AdvSceneSwitcher.tempVar.window.windowY.description"));
	AddTempvar(
		"windowWidth",
		obs_module_text("AdvSceneSwitcher.tempVar.window.windowWidth"),
		obs_module_text(
			"AdvSceneSwitcher.tempVar.window.windowWidth.description"));
	AddTempvar(
		"windowHeight",
		obs_module_text("AdvSceneSwitcher.tempVar.window.windowHeight"),
		obs_module_text(
			"AdvSceneSwitcher.tempVar.window.windowHeight.description"));
#ifdef _WIN32
	AddTempvar(
		"windowClass",
		obs_module_text("AdvSceneSwitcher.tempVar.window.windowClass"),
		obs_module_text(
			"AdvSceneSwitcher.tempVar.window.windowClass.description"));

	if (!_checkText) {
		return;
	}

	AddTempvar(
		"windowText",
		obs_module_text("AdvSceneSwitcher.tempVar.window.windowText"),
		obs_module_text(
			"AdvSceneSwitcher.tempVar.window.windowText.description"));
#endif
}

MacroConditionWindowEdit::MacroConditionWindowEdit(
	QWidget *parent, std::shared_ptr<MacroConditionWindow> entryData)
	: QWidget(parent),
	  _windowSelection(new WindowSelectionWidget(this)),
	  _windowRegex(new RegexConfigWidget(this)),
	  _checkTitle(new QCheckBox()),
	  _fullscreen(new QCheckBox()),
	  _maximized(new QCheckBox()),
	  _focused(new QCheckBox()),
	  _windowFocusChanged(new QCheckBox()),
	  _checkText(new QCheckBox()),
	  _text(new VariableTextEdit(this)),
	  _textRegex(new RegexConfigWidget(this)),
	  _focusWindow(new QLabel()),
	  _currentFocusLayout(new QHBoxLayout())
{
	_checkText->setToolTip(obs_module_text(
		"AdvSceneSwitcher.condition.window.entry.text.note"));
	_text->setToolTip(obs_module_text(
		"AdvSceneSwitcher.condition.window.entry.text.note"));

	_windowSelection->setToolTip(
		obs_module_text("AdvSceneSwitcher.tooltip.availableVariables"));

	QWidget::connect(_windowSelection,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(WindowChanged(const QString &)));
	QWidget::connect(_windowRegex,
			 SIGNAL(RegexConfigChanged(const RegexConfig &)), this,
			 SLOT(WindowRegexChanged(const RegexConfig &)));
	QWidget::connect(_checkTitle, SIGNAL(stateChanged(int)), this,
			 SLOT(CheckTitleChanged(int)));
	QWidget::connect(_fullscreen, SIGNAL(stateChanged(int)), this,
			 SLOT(FullscreenChanged(int)));
	QWidget::connect(_maximized, SIGNAL(stateChanged(int)), this,
			 SLOT(MaximizedChanged(int)));
	QWidget::connect(_focused, SIGNAL(stateChanged(int)), this,
			 SLOT(FocusedChanged(int)));
	QWidget::connect(_windowFocusChanged, SIGNAL(stateChanged(int)), this,
			 SLOT(WindowFocusChanged(int)));
	QWidget::connect(_checkText, SIGNAL(stateChanged(int)), this,
			 SLOT(CheckTextChanged(int)));
	QWidget::connect(_text, SIGNAL(textChanged()), this,
			 SLOT(WindowTextChanged()));
	QWidget::connect(_textRegex,
			 SIGNAL(RegexConfigChanged(const RegexConfig &)), this,
			 SLOT(TextRegexChanged(const RegexConfig &)));
	QWidget::connect(&_timer, SIGNAL(timeout()), this,
			 SLOT(UpdateFocusWindow()));

	const std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{windows}}", _windowSelection},
		{"{{windowRegex}}", _windowRegex},
		{"{{checkTitle}}", _checkTitle},
		{"{{fullscreen}}", _fullscreen},
		{"{{maximized}}", _maximized},
		{"{{focused}}", _focused},
		{"{{windowFocusChanged}}", _windowFocusChanged},
		{"{{focusWindow}}", _focusWindow},
		{"{{checkText}}", _checkText},
		{"{{windowText}}", _text},
		{"{{textRegex}}", _textRegex},
	};

	auto titleLayout = new QHBoxLayout;
	titleLayout->setContentsMargins(0, 0, 0, 0);
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.window.entry.window"),
		     titleLayout, widgetPlaceholders);
	auto fullscreenLayout = new QHBoxLayout;
	fullscreenLayout->setContentsMargins(0, 0, 0, 0);
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.window.entry.fullscreen"),
		fullscreenLayout, widgetPlaceholders);
	auto maximizedLayout = new QHBoxLayout;
	maximizedLayout->setContentsMargins(0, 0, 0, 0);
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.window.entry.maximized"),
		maximizedLayout, widgetPlaceholders);
	auto focusedLayout = new QHBoxLayout;
	focusedLayout->setContentsMargins(0, 0, 0, 0);
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.window.entry.focused"),
		     focusedLayout, widgetPlaceholders);
	auto focusChangedLayout = new QHBoxLayout;
	focusChangedLayout->setContentsMargins(0, 0, 0, 0);
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.window.entry.focusedChange"),
		focusChangedLayout, widgetPlaceholders);
	auto textLayout = new QHBoxLayout;
	textLayout->setContentsMargins(0, 0, 0, 0);
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.condition.window.entry.text"),
		textLayout, widgetPlaceholders);
	_text->setSizePolicy(QSizePolicy::MinimumExpanding,
			     QSizePolicy::Preferred);
	textLayout->setStretchFactor(_text, 10);
	_currentFocusLayout->setContentsMargins(0, 0, 0, 0);
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.window.entry.currentFocus"),
		_currentFocusLayout, widgetPlaceholders);

	auto mainLayout = new QVBoxLayout;
	mainLayout->addLayout(titleLayout);
	mainLayout->addLayout(fullscreenLayout);
	mainLayout->addLayout(maximizedLayout);
	mainLayout->addLayout(focusedLayout);
	mainLayout->addLayout(focusChangedLayout);
	mainLayout->addLayout(textLayout);
#ifndef _WIN32
	SetLayoutVisible(textLayout, false);
#endif
	mainLayout->addLayout(_currentFocusLayout);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;

	_timer.start(1000);
}

void MacroConditionWindowEdit::WindowChanged(const QString &text)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_window = text.toStdString();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionWindowEdit::WindowRegexChanged(const RegexConfig &conf)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_windowRegex = conf;
	adjustSize();
	updateGeometry();
}

void MacroConditionWindowEdit::CheckTitleChanged(int state)
{
	GUARD_LOADING_AND_LOCK();
	const QSignalBlocker b1(_windowSelection);
	const QSignalBlocker b2(_windowRegex);
	if (!state) {
		_entryData->_window = ".*";
		_entryData->_windowRegex.SetEnabled(true);
		_windowSelection->setCurrentText(".*");
		_windowRegex->EnableChanged(true);
	}
	_entryData->_checkTitle = state;
	SetWidgetVisibility();
}

void MacroConditionWindowEdit::CheckTextChanged(int state)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->SetCheckText(state);
	SetWidgetVisibility();
}

void MacroConditionWindowEdit::WindowTextChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_text = _text->toPlainText().toStdString();
	adjustSize();
	updateGeometry();
}

void MacroConditionWindowEdit::TextRegexChanged(const RegexConfig &conf)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_textRegex = conf;
	adjustSize();
	updateGeometry();
}

void MacroConditionWindowEdit::FullscreenChanged(int state)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_fullscreen = state;
}

void MacroConditionWindowEdit::MaximizedChanged(int state)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_maximized = state;
}

void MacroConditionWindowEdit::FocusedChanged(int state)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_focus = state;
	SetWidgetVisibility();
}

void MacroConditionWindowEdit::WindowFocusChanged(int state)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_windowFocusChanged = state;
	SetWidgetVisibility();
}

void MacroConditionWindowEdit::UpdateFocusWindow()
{
	_focusWindow->setText(QString::fromStdString(ForegroundWindowTitle()));
}

void MacroConditionWindowEdit::SetWidgetVisibility()
{
	if (!_entryData) {
		return;
	}
	SetLayoutVisible(_currentFocusLayout,
			 _entryData->_focus || _entryData->_windowFocusChanged);
	_windowSelection->setVisible(_entryData->_checkTitle);
	_windowRegex->setVisible(_entryData->_checkTitle);
	_textRegex->setVisible(_entryData->GetCheckText());
	_text->setVisible(_entryData->GetCheckText());
	adjustSize();
	updateGeometry();
}

void MacroConditionWindowEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_windowSelection->setCurrentText(
		_entryData->_window.UnresolvedValue().c_str());
	_windowRegex->SetRegexConfig(_entryData->_windowRegex);
	_checkTitle->setChecked(_entryData->_checkTitle);
	_fullscreen->setChecked(_entryData->_fullscreen);
	_maximized->setChecked(_entryData->_maximized);
	_focused->setChecked(_entryData->_focus);
	_windowFocusChanged->setChecked(_entryData->_windowFocusChanged);
	_checkText->setChecked(_entryData->GetCheckText());
	_text->setPlainText(_entryData->_text);
	_textRegex->SetRegexConfig(_entryData->_textRegex);
	SetWidgetVisibility();
}

} // namespace advss
