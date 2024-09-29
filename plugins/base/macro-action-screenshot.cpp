#include "macro-action-screenshot.hpp"
#include "layout-helpers.hpp"
#include "selection-helpers.hpp"

#include <obs-frontend-api.h>
#include <QBuffer>
#include <QByteArray>

namespace advss {

const std::string MacroActionScreenshot::id = "screenshot";

bool MacroActionScreenshot::_registered = MacroActionFactory::Register(
	MacroActionScreenshot::id,
	{MacroActionScreenshot::Create, MacroActionScreenshotEdit::Create,
	 "AdvSceneSwitcher.action.screenshot"});

void MacroActionScreenshot::FrontendScreenshot(OBSWeakSource &source) const
{
#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(26, 0, 0)
	if (source) {
		auto s = OBSGetStrongRef(source);
		obs_frontend_take_source_screenshot(s);
	} else {
		obs_frontend_take_screenshot();
	}
#endif
}

void MacroActionScreenshot::CustomScreenshot(OBSWeakSource &source) const
{
	if (!source && _targetType == TargetType::SCENE) {
		return;
	}
	auto s = OBSGetStrongRef(source);
	Screenshot screenshot(s, QRect(), true, 3000, true, _path);
}

void MacroActionScreenshot::VariableScreenshot(OBSWeakSource &source) const
{
	if (!source && _targetType == TargetType::SCENE) {
		return;
	}

	auto variable = _variable.lock();
	if (!variable) {
		return;
	}

	auto s = OBSGetStrongRef(source);
	Screenshot screenshot(s, QRect(), true, 3000);

	QByteArray byteArray;
	QBuffer buffer(&byteArray);
	buffer.open(QIODevice::WriteOnly);
	if (!screenshot.GetImage().save(&buffer, "PNG")) {
		blog(LOG_WARNING, "Failed to save screenshot to variable!");
	}

	variable->SetValue(byteArray.toBase64().toStdString());
}

bool MacroActionScreenshot::PerformAction()
{
	OBSWeakSource source = nullptr;
	switch (_targetType) {
	case MacroActionScreenshot::TargetType::SOURCE:
		source = _source.GetSource();
		break;
	case MacroActionScreenshot::TargetType::SCENE:
		source = _scene.GetScene(false);
		break;
	case MacroActionScreenshot::TargetType::MAIN_OUTPUT:
		break;
	}

	switch (_saveType) {
	case MacroActionScreenshot::SaveType::OBS_DEFAULT_PATH:
		FrontendScreenshot(source);
		break;
	case MacroActionScreenshot::SaveType::CUSTOM_PATH:
		CustomScreenshot(source);
		break;
	case MacroActionScreenshot::SaveType::VARIABLE:
		VariableScreenshot(source);
		break;
	default:
		break;
	}

	return true;
}

void MacroActionScreenshot::LogAction() const
{
	switch (_targetType) {
	case MacroActionScreenshot::TargetType::SOURCE:
		ablog(LOG_INFO, "trigger screenshot of \"%s\"",
		      _source.ToString(true).c_str());
		break;
	case MacroActionScreenshot::TargetType::SCENE:
		ablog(LOG_INFO, "trigger screenshot of \"%s\"",
		      _scene.ToString(true).c_str());
		break;
	case MacroActionScreenshot::TargetType::MAIN_OUTPUT:
		ablog(LOG_INFO, "trigger screenshot of main output");
		break;
	}
}

bool MacroActionScreenshot::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	_scene.Save(obj);
	_source.Save(obj);
	obs_data_set_int(obj, "saveType", static_cast<int>(_saveType));
	obs_data_set_int(obj, "targetType", static_cast<int>(_targetType));
	_path.Save(obj, "savePath");
	obs_data_set_string(obj, "variable",
			    GetWeakVariableName(_variable).c_str());
	obs_data_set_int(obj, "version", 1);
	return true;
}

bool MacroActionScreenshot::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_scene.Load(obj);
	_source.Load(obj);
	_saveType = static_cast<SaveType>(obs_data_get_int(obj, "saveType"));
	_targetType =
		static_cast<TargetType>(obs_data_get_int(obj, "targetType"));
	_path.Load(obj, "savePath");
	_variable =
		GetWeakVariableByName(obs_data_get_string(obj, "variableName"));

	// TODO: Remove fallback for older versions
	if (!obs_data_has_user_value(obj, "version")) {
		if (!_source.GetSource() && !_scene.GetScene(false)) {
			_targetType = TargetType::MAIN_OUTPUT;
		}
	}
	return true;
}

std::string MacroActionScreenshot::GetShortDesc() const
{
	if (_targetType == TargetType::SCENE) {
		return _scene.ToString();
	} else {
		return _source.ToString();
	}
	return "";
}

std::shared_ptr<MacroAction> MacroActionScreenshot::Create(Macro *m)
{
	return std::make_shared<MacroActionScreenshot>(m);
}

std::shared_ptr<MacroAction> MacroActionScreenshot::Copy() const
{
	auto result = Create(GetMacro());
	OBSDataAutoRelease data = obs_data_create();
	Save(data);
	result->Load(data);
	result->PostLoad();
	return result;
}

void MacroActionScreenshot::ResolveVariablesToFixedValues()
{
	_scene.ResolveVariables();
	_source.ResolveVariables();
	_path.ResolveVariables();
}

static void populateSaveTypeSelection(QComboBox *list)
{
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.action.screenshot.save.default"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.action.screenshot.save.custom"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.action.screenshot.save.variable"));
}

static void populateTargetTypeSelection(QComboBox *list)
{
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.action.screenshot.type.source"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.action.screenshot.type.scene"));
	list->addItem(obs_module_text("AdvSceneSwitcher.OBSVideoOutput"));
}

MacroActionScreenshotEdit::MacroActionScreenshotEdit(
	QWidget *parent, std::shared_ptr<MacroActionScreenshot> entryData)
	: QWidget(parent),
	  _scenes(new SceneSelectionWidget(this, true, false, true, true,
					   true)),
	  _sources(new SourceSelectionWidget(this, QStringList(), true)),
	  _saveType(new QComboBox()),
	  _targetType(new QComboBox()),
	  _savePath(new FileSelection(FileSelection::Type::WRITE, this)),
	  _variables(new VariableSelection(this))
{
	setToolTip(obs_module_text(
		"AdvSceneSwitcher.action.screenshot.blackscreenNote"));
	auto sources = GetVideoSourceNames();
	sources.sort();
	_sources->SetSourceNameList(sources);

	populateSaveTypeSelection(_saveType);
	populateTargetTypeSelection(_targetType);

	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 this, SLOT(SceneChanged(const SceneSelection &)));
	QWidget::connect(_sources,
			 SIGNAL(SourceChanged(const SourceSelection &)), this,
			 SLOT(SourceChanged(const SourceSelection &)));
	QWidget::connect(_saveType, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(SaveTypeChanged(int)));
	QWidget::connect(_targetType, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(TargetTypeChanged(int)));
	QWidget::connect(_savePath, SIGNAL(PathChanged(const QString &)), this,
			 SLOT(PathChanged(const QString &)));
	QWidget::connect(_variables, SIGNAL(SelectionChanged(const QString &)),
			 this, SLOT(VariableChanged(const QString &)));

	auto layout = new QHBoxLayout;
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.action.screenshot.entry"),
		layout,
		{{"{{sources}}", _sources},
		 {"{{scenes}}", _scenes},
		 {"{{saveType}}", _saveType},
		 {"{{targetType}}", _targetType},
		 {"{{variables}}", _variables}});

	auto mainLayout = new QVBoxLayout;
	mainLayout->addLayout(layout);
	mainLayout->addWidget(_savePath);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionScreenshotEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_sources->SetSource(_entryData->_source);
	_scenes->SetScene(_entryData->_scene);
	_saveType->setCurrentIndex(static_cast<int>(_entryData->_saveType));
	_targetType->setCurrentIndex(static_cast<int>(_entryData->_targetType));
	_savePath->SetPath(_entryData->_path);
	_variables->SetVariable(_entryData->_variable);
	SetWidgetVisibility();
}

void MacroActionScreenshotEdit::SceneChanged(const SceneSelection &s)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_scene = s;
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionScreenshotEdit::SaveTypeChanged(int index)
{
	if (_loading || !_entryData) {
	}

	_entryData->_saveType =
		static_cast<MacroActionScreenshot::SaveType>(index);
	SetWidgetVisibility();
}

void MacroActionScreenshotEdit::TargetTypeChanged(int index)
{
	if (_loading || !_entryData) {
	}

	_entryData->_targetType =
		static_cast<MacroActionScreenshot::TargetType>(index);
	SetWidgetVisibility();
}

void MacroActionScreenshotEdit::PathChanged(const QString &text)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_path = text.toStdString();
}

void MacroActionScreenshotEdit::SourceChanged(const SourceSelection &source)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_source = source;
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionScreenshotEdit::VariableChanged(const QString &text)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_variable = GetWeakVariableByQString(text);
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionScreenshotEdit::SetWidgetVisibility()
{
	if (!_entryData) {
		return;
	}
	_savePath->setVisible(_entryData->_saveType ==
			      MacroActionScreenshot::SaveType::CUSTOM_PATH);
	_sources->setVisible(_entryData->_targetType ==
			     MacroActionScreenshot::TargetType::SOURCE);
	_scenes->setVisible(_entryData->_targetType ==
			    MacroActionScreenshot::TargetType::SCENE);
	_variables->setVisible(_entryData->_saveType ==
			       MacroActionScreenshot::SaveType::VARIABLE);

	adjustSize();
	updateGeometry();
}

} // namespace advss
