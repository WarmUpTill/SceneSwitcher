#include "macro-action-window.hpp"
#include "layout-helpers.hpp"
#include "platform-funcs.hpp"
#include "selection-helpers.hpp"
#include "ui-helpers.hpp"

namespace advss {

const std::string MacroActionWindow::id = "window";

#ifdef _WIN32
bool MacroActionWindow::_registered = MacroActionFactory::Register(
	MacroActionWindow::id,
	{MacroActionWindow::Create, MacroActionWindowEdit::Create,
	 "AdvSceneSwitcher.action.window"});
#else
bool MacroActionWindow::_registered = false;
#endif // _WIN32

const static std::map<MacroActionWindow::Action, std::string> actionTypes = {
	{MacroActionWindow::Action::SET_FOCUS_WINDOW,
	 "AdvSceneSwitcher.action.window.type.setFocusWindow"},
	{MacroActionWindow::Action::MAXIMIZE_WINDOW,
	 "AdvSceneSwitcher.action.window.type.maximizeWindow"},
	{MacroActionWindow::Action::MINIMIZE_WINDOW,
	 "AdvSceneSwitcher.action.window.type.minimizeWindow"},
	{MacroActionWindow::Action::CLOSE_WINDOW,
	 "AdvSceneSwitcher.action.window.type.closeWindow"},
};

#ifdef _WIN32
void SetFocusWindow(const std::string &);
void MaximizeWindow(const std::string &);
void MinimizeWindow(const std::string &);
void CloseWindow(const std::string &);
#else
void SetFocusWindow(const std::string &) {}
void MaximizeWindow(const std::string &) {}
void MinimizeWindow(const std::string &) {}
void CloseWindow(const std::string &) {}
#endif // _WIN32

std::optional<std::string> MacroActionWindow::GetMatchingWindow() const
{
	std::vector<std::string> windowList;
	GetWindowList(windowList);

	if (!_regex.Enabled()) {
		if (std::find(windowList.begin(), windowList.end(),
			      std::string(_window)) == windowList.end()) {
			return {};
		}
		return _window;
	}

	for (const auto &window : windowList) {
		if (_regex.Matches(window, _window)) {
			return window;
		}
	}

	return {};
}

bool MacroActionWindow::PerformAction()
{
	auto window = GetMatchingWindow();
	if (!window) {
		return true;
	}

	switch (_action) {
	case Action::SET_FOCUS_WINDOW:
		SetFocusWindow(*window);
		break;
	case Action::MAXIMIZE_WINDOW:
		MaximizeWindow(*window);
		break;
	case Action::MINIMIZE_WINDOW:;
		MinimizeWindow(*window);
		break;
	case Action::CLOSE_WINDOW:
		CloseWindow(*window);
		break;
	default:
		break;
	}

	return true;
}

void MacroActionWindow::LogAction() const
{
	auto window = GetMatchingWindow();
	switch (_action) {
	case Action::SET_FOCUS_WINDOW:
		blog(LOG_INFO, "focus window \"%s\"",
		     window ? window->c_str() : "nothing matched");
		break;
	case Action::MAXIMIZE_WINDOW:
		blog(LOG_INFO, "maximize window \"%s\"",
		     window ? window->c_str() : "nothing matched");
		break;
	case Action::MINIMIZE_WINDOW:
		blog(LOG_INFO, "minimize window \"%s\"",
		     window ? window->c_str() : "nothing matched");
		break;
	case Action::CLOSE_WINDOW:
		blog(LOG_INFO, "close window \"%s\"",
		     window ? window->c_str() : "nothing matched");
		break;
	default:
		break;
	}
}

bool MacroActionWindow::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	_window.Save(obj, "window");
	_regex.Save(obj, "regex");
	return true;
}

bool MacroActionWindow::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_action = static_cast<Action>(obs_data_get_int(obj, "action"));
	_window.Load(obj, "window");
	_regex.Load(obj, "regex");
	return true;
}

std::string MacroActionWindow::GetShortDesc() const
{
	return _window;
}

std::shared_ptr<MacroAction> MacroActionWindow::Create(Macro *m)
{
	return std::make_shared<MacroActionWindow>(m);
}

std::shared_ptr<MacroAction> MacroActionWindow::Copy() const
{
	return std::make_shared<MacroActionWindow>(*this);
}

void MacroActionWindow::ResolveVariablesToFixedValues()
{
	_window.ResolveVariables();
}

static inline void populateActionSelection(QComboBox *list)
{
	for (const auto &[_, name] : actionTypes) {
		list->addItem(obs_module_text(name.c_str()));
	}
}

MacroActionWindowEdit::MacroActionWindowEdit(
	QWidget *parent, std::shared_ptr<MacroActionWindow> entryData)
	: QWidget(parent),
	  _actions(new QComboBox()),
	  _windows(new QComboBox()),
	  _regex(new RegexConfigWidget(this)),
	  _infoLayout(new QHBoxLayout())
{
	populateActionSelection(_actions);

	_windows->setEditable(true);
	_windows->setMaxVisibleItems(20);
	PopulateWindowSelection(_windows);

	auto focusLimitation = new QLabel(obs_module_text(
		"AdvSceneSwitcher.action.window.type.setFocusWindow.limitation"));
	_infoLayout->addWidget(focusLimitation);

	const QString path = GetThemeTypeName() == "Light"
				     ? ":/res/images/help.svg"
				     : ":/res/images/help_light.svg";
	const QIcon icon(path);
	const QPixmap pixmap = icon.pixmap(QSize(16, 16));
	auto focusLimitationDetails = new QLabel();
	focusLimitationDetails->setPixmap(pixmap);
	focusLimitationDetails->setToolTip(obs_module_text(
		"AdvSceneSwitcher.action.window.type.setFocusWindow.limitation.details"));
	_infoLayout->addWidget(focusLimitationDetails);
	_infoLayout->addStretch();

	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_windows, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(WindowChanged(const QString &)));
	QWidget::connect(_regex,
			 SIGNAL(RegexConfigChanged(const RegexConfig &)), this,
			 SLOT(RegexChanged(const RegexConfig &)));

	auto entryLayout = new QHBoxLayout;
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.action.window.entry"),
		     entryLayout,
		     {{"{{actions}}", _actions},
		      {"{{windows}}", _windows},
		      {"{{regex}}", _regex}});
	auto layout = new QVBoxLayout();
	layout->addLayout(entryLayout);
	layout->addLayout(_infoLayout);
	setLayout(layout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionWindowEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}
	_actions->setCurrentIndex(static_cast<int>(_entryData->_action));
	_windows->setCurrentText(QString::fromStdString(_entryData->_window));
	_regex->SetRegexConfig(_entryData->_regex);
	SetWidgetVisibility();
}

void MacroActionWindowEdit::WindowChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_window = text.toStdString();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionWindowEdit::RegexChanged(const RegexConfig &regex)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_regex = regex;
	adjustSize();
	updateGeometry();
}

void MacroActionWindowEdit::SetWidgetVisibility()
{
	SetLayoutVisible(_infoLayout,
			 _entryData->_action ==
				 MacroActionWindow::Action::SET_FOCUS_WINDOW);
	adjustSize();
	updateGeometry();
}

void MacroActionWindowEdit::ActionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_action = static_cast<MacroActionWindow::Action>(value);
	SetWidgetVisibility();
}

} // namespace advss
