#include "macro-action-projector.hpp"
#include "layout-helpers.hpp"
#include "selection-helpers.hpp"
#include "source-helpers.hpp"

#include <obs-frontend-api.h>
#include <QApplication>
#include <QWindow>

namespace advss {

const std::string MacroActionProjector::id = "projector";

bool MacroActionProjector::_registered = MacroActionFactory::Register(
	MacroActionProjector::id,
	{MacroActionProjector::Create, MacroActionProjectorEdit::Create,
	 "AdvSceneSwitcher.action.projector"});

const static std::map<MacroActionProjector::Type, std::string> selectionTypes = {
	{MacroActionProjector::Type::SOURCE,
	 "AdvSceneSwitcher.action.projector.type.source"},
	{MacroActionProjector::Type::SCENE,
	 "AdvSceneSwitcher.action.projector.type.scene"},
	{MacroActionProjector::Type::PREVIEW,
	 "AdvSceneSwitcher.action.projector.type.preview"},
	{MacroActionProjector::Type::PROGRAM,
	 "AdvSceneSwitcher.action.projector.type.program"},
	{MacroActionProjector::Type::MULTIVIEW,
	 "AdvSceneSwitcher.action.projector.type.multiview"},
};

static void closeOBSProjectorWindows(const std::string expectedWindowTitle,
				     const RegexConfig &regex)
{
	for (QWindow *widget : QApplication::allWindows()) {
		if (!widget->property("isOBSProjectorWindow").toBool()) {
			continue;
		}

		auto const windowTitle = widget->title().toStdString();
		if (!regex.Enabled() && expectedWindowTitle != windowTitle) {
			continue;
		}

		if (!regex.Matches(windowTitle, expectedWindowTitle)) {
			continue;
		}

		widget->close();
	}
}

bool MacroActionProjector::PerformAction()
{
	if (_action == Action::CLOSE) {
		closeOBSProjectorWindows(_projectorWindowName, _regex);
		return true;
	}

	std::string name = "";
	const char *type = "";

	switch (_type) {
	case MacroActionProjector::Type::SOURCE:
		type = "Source";
		name = GetWeakSourceName(_source.GetSource());
		if (name.empty()) {
			return true;
		}
		break;
	case MacroActionProjector::Type::SCENE:
		type = "Scene";
		name = GetWeakSourceName(_scene.GetScene());
		if (name.empty()) {
			return true;
		}
		break;
	case MacroActionProjector::Type::PREVIEW:
		type = "Preview";
		break;
	case MacroActionProjector::Type::PROGRAM:
		type = "StudioProgram";
		break;
	case MacroActionProjector::Type::MULTIVIEW:
		type = "Multiview";
		break;
	}

	if (_fullscreen && _monitor == -1) {
		blog(LOG_INFO, "refusing to open fullscreen projector "
			       "with invalid display selection");
		return true;
	}
	if (_fullscreen && MonitorSetupChanged()) {
		blog(LOG_INFO, "refusing to open fullscreen projector - "
			       "monitor setup seems to have changed!");
		return true;
	}

	obs_frontend_open_projector(type, _fullscreen ? _monitor : -1, "",
				    name.c_str());

	return true;
}

void MacroActionProjector::LogAction() const
{
	if (_action == Action::CLOSE) {
		ablog(LOG_INFO, "closing projector window \"%s\"",
		      _projectorWindowName.c_str());
		return;
	}

	auto it = selectionTypes.find(_type);
	if (it != selectionTypes.end()) {
		ablog(LOG_INFO,
		      "open projector \"%s\" with"
		      "source \"%s\","
		      "scene \"%s\","
		      "monitor %d",
		      it->second.c_str(), _source.ToString(true).c_str(),
		      _scene.ToString(true).c_str(), _monitor);
	} else {
		blog(LOG_WARNING, "ignored unknown projector action %d",
		     static_cast<int>(_type));
	}
}

bool MacroActionProjector::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	obs_data_set_int(obj, "type", static_cast<int>(_type));
	obs_data_set_int(obj, "monitor", _monitor);
	obs_data_set_string(obj, "monitorName", _monitorName.c_str());
	obs_data_set_bool(obj, "fullscreen", _fullscreen);
	_scene.Save(obj);
	_source.Save(obj);
	_projectorWindowName.Save(obj, "projectorWindowName");
	_regex.Save(obj);
	return true;
}

bool MacroActionProjector::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_action = static_cast<Action>(obs_data_get_int(obj, "action"));
	_type = static_cast<Type>(obs_data_get_int(obj, "type"));
	_monitor = obs_data_get_int(obj, "monitor");
	_monitorName = obs_data_get_string(obj, "monitorName");
	_fullscreen = obs_data_get_bool(obj, "fullscreen");
	_scene.Load(obj);
	_source.Load(obj);
	_projectorWindowName.Load(obj, "projectorWindowName");
	_regex.Load(obj);
	return true;
}

std::shared_ptr<MacroAction> MacroActionProjector::Create(Macro *m)
{
	return std::make_shared<MacroActionProjector>(m);
}

std::shared_ptr<MacroAction> MacroActionProjector::Copy() const
{
	return std::make_shared<MacroActionProjector>(*this);
}

void MacroActionProjector::ResolveVariablesToFixedValues()
{
	_source.ResolveVariables();
	_scene.ResolveVariables();
	_projectorWindowName.ResolveVariables();
}

void MacroActionProjector::SetMonitor(int idx)
{
	_monitor = idx;
	auto monitorNames = GetMonitorNames();
	if (_monitor < 0 || _monitor >= monitorNames.size()) {
		// Monitor setup changed while settings were selected?
		_monitorName = "";
		return;
	}
	_monitorName = monitorNames.at(_monitor).toStdString();
}

int MacroActionProjector::GetMonitor() const
{
	if (MonitorSetupChanged()) {
		return -1;
	}
	return _monitor;
}

bool MacroActionProjector::MonitorSetupChanged() const
{
	if (_monitorName.empty()) {
		return false;
	}
	auto monitorNames = GetMonitorNames();
	if (_monitor < 0 || _monitor >= monitorNames.size()) {
		return true;
	}
	return monitorNames.at(_monitor) !=
	       QString::fromStdString(_monitorName);
}

static void populateActionSelection(QComboBox *list)
{
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.action.projector.action.open"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.action.projector.action.close"));
}

static void populateSelectionTypes(QComboBox *list)
{
	for (const auto &[_, name] : selectionTypes) {
		list->addItem(obs_module_text(name.c_str()));
	}
}

static void populateWindowTypes(QComboBox *list)
{
	list->addItem(
		obs_module_text("AdvSceneSwitcher.action.projector.windowed"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.action.projector.fullscreen"));
}

static QStringList getSourcesList()
{
	auto sources = GetSourceNames();
	sources.sort();
	return sources;
}

MacroActionProjectorEdit::MacroActionProjectorEdit(
	QWidget *parent, std::shared_ptr<MacroActionProjector> entryData)
	: QWidget(parent),
	  _actions(new QComboBox()),
	  _types(new QComboBox()),
	  _windowTypes(new QComboBox()),
	  _scenes(new SceneSelectionWidget(this, true, false, true, true,
					   true)),
	  _sources(new SourceSelectionWidget(this, getSourcesList, true)),
	  _monitors(new MonitorSelectionWidget(this)),
	  _projectorWindowName(new VariableLineEdit(this)),
	  _regex(new RegexConfigWidget(this)),
	  _layout(new QHBoxLayout(this))
{
	// The obs_frontend_open_projector() function does not seem to support
	// scenes of secondary canvases
	_scenes->LockToMainCanvas();

	populateActionSelection(_actions);
	populateWindowTypes(_windowTypes);
	populateSelectionTypes(_types);

	_monitors->addItems(GetMonitorNames());
	_monitors->setPlaceholderText(
		obs_module_text("AdvSceneSwitcher.selectDisplay"));
	_monitors->SetAllowUnmatchedSelection(false);

	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_windowTypes, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(WindowTypeChanged(int)));
	QWidget::connect(_types, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(TypeChanged(int)));
	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 this, SLOT(SceneChanged(const SceneSelection &)));
	QWidget::connect(_sources,
			 SIGNAL(SourceChanged(const SourceSelection &)), this,
			 SLOT(SourceChanged(const SourceSelection &)));
	QWidget::connect(_monitors, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(MonitorChanged(int)));
	QWidget::connect(_projectorWindowName, SIGNAL(editingFinished()), this,
			 SLOT(ProjectorWindowNameChanged()));
	QWidget::connect(_regex,
			 SIGNAL(RegexConfigChanged(const RegexConfig &)), this,
			 SLOT(RegexChanged(const RegexConfig &)));

	_entryData = entryData;
	SetWidgetLayout();
	setLayout(_layout);
	UpdateEntryData();
	_loading = false;
}

void MacroActionProjectorEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_actions->setCurrentIndex(static_cast<int>(_entryData->_action));
	_windowTypes->setCurrentIndex(_entryData->_fullscreen ? 1 : 0);
	_types->setCurrentIndex(static_cast<int>(_entryData->_type));
	_scenes->SetScene(_entryData->_scene);
	_sources->SetSource(_entryData->_source);
	_monitors->setCurrentIndex(_entryData->GetMonitor());
	_projectorWindowName->setText(_entryData->_projectorWindowName);
	_regex->SetRegexConfig(_entryData->_regex);
	SetWidgetVisibility();
}

void MacroActionProjectorEdit::SceneChanged(const SceneSelection &s)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_scene = s;
}

void MacroActionProjectorEdit::SourceChanged(const SourceSelection &source)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_source = source;
}

void MacroActionProjectorEdit::MonitorChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->SetMonitor(value);
}

void MacroActionProjectorEdit::ProjectorWindowNameChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_projectorWindowName =
		_projectorWindowName->text().toStdString();
}

void MacroActionProjectorEdit::RegexChanged(const RegexConfig &regex)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_regex = regex;
}

void MacroActionProjectorEdit::WindowTypeChanged(int)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_fullscreen =
		_windowTypes->currentText() ==
		obs_module_text("AdvSceneSwitcher.action.projector.fullscreen");
	SetWidgetLayout();
	SetWidgetVisibility();
}

void MacroActionProjectorEdit::TypeChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_type = static_cast<MacroActionProjector::Type>(value);
	SetWidgetVisibility();
}

void MacroActionProjectorEdit::ActionChanged(int idx)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_action = static_cast<MacroActionProjector::Action>(idx);
	SetWidgetLayout();
	SetWidgetVisibility();
}

void MacroActionProjectorEdit::SetWidgetLayout()
{
	const std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{actions}}", _actions},
		{"{{windowTypes}}", _windowTypes},
		{"{{types}}", _types},
		{"{{scenes}}", _scenes},
		{"{{sources}}", _sources},
		{"{{monitors}}", _monitors},
		{"{{projectorWindowName}}", _projectorWindowName},
		{"{{regex}}", _regex},
	};

	for (const auto &[_, widget] : widgetPlaceholders) {
		_layout->removeWidget(widget);
	}
	ClearLayout(_layout);

	const char *layoutText;
	if (_entryData->_action == MacroActionProjector::Action::CLOSE) {
		layoutText = "AdvSceneSwitcher.action.projector.entry.close";
	} else if (_entryData->_fullscreen) {
		layoutText =
			"AdvSceneSwitcher.action.projector.entry.open.fullscreen";
	} else {
		layoutText =
			"AdvSceneSwitcher.action.projector.entry.open.windowed";
	}

	PlaceWidgets(obs_module_text(layoutText), _layout, widgetPlaceholders);
}

void MacroActionProjectorEdit::SetWidgetVisibility()
{
	if (!_entryData) {
		return;
	}

	const auto &action = _entryData->_action;
	const auto &type = _entryData->_type;

	_projectorWindowName->setVisible(action ==
					 MacroActionProjector::Action::CLOSE);
	_regex->setVisible(action == MacroActionProjector::Action::CLOSE);
	_types->setVisible(action == MacroActionProjector::Action::OPEN);
	_windowTypes->setVisible(action == MacroActionProjector::Action::OPEN);
	_scenes->setVisible(action == MacroActionProjector::Action::OPEN &&
			    type == MacroActionProjector::Type::SCENE);
	_sources->setVisible(action == MacroActionProjector::Action::OPEN &&
			     type == MacroActionProjector::Type::SOURCE);
	_monitors->setVisible(action == MacroActionProjector::Action::OPEN &&
			      _entryData->_fullscreen);

	adjustSize();
	updateGeometry();

	if (action == MacroActionProjector::Action::CLOSE) {
		RemoveStretchIfPresent(_layout);
	} else {
		AddStretchIfNecessary(_layout);
	}
}

} // namespace advss
