#include "macro-action-screenshot.hpp"
#include "utility.hpp"

namespace advss {

const uint32_t MacroActionScreenshot::_version = 1;

const std::string MacroActionScreenshot::id = "screenshot";

bool MacroActionScreenshot::_registered = MacroActionFactory::Register(
	MacroActionScreenshot::id,
	{MacroActionScreenshot::Create, MacroActionScreenshotEdit::Create,
	 "AdvSceneSwitcher.action.screenshot"});

void MacroActionScreenshot::FrontendScreenshot(OBSWeakSource &source)
{
#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(26, 0, 0)
	if (source) {
		auto s = obs_weak_source_get_source(source);
		obs_frontend_take_source_screenshot(s);
		obs_source_release(s);
	} else {
		obs_frontend_take_screenshot();
	}
#endif
}

void MacroActionScreenshot::CustomScreenshot(OBSWeakSource &source)
{
	if (!source && _targetType == TargetType::SCENE) {
		return;
	}
	auto s = obs_weak_source_get_source(source);
	_screenshot.~ScreenshotHelper();
	new (&_screenshot) ScreenshotHelper(s, QRect(), false, 0, true, _path);
	obs_source_release(s);
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
	case MacroActionScreenshot::SaveType::OBS_DEFAULT:
		FrontendScreenshot(source);
		break;
	case MacroActionScreenshot::SaveType::CUSTOM:
		CustomScreenshot(source);
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
		vblog(LOG_INFO, "trigger screenshot of \"%s\"",
		      _source.ToString(true).c_str());
		break;
	case MacroActionScreenshot::TargetType::SCENE:
		vblog(LOG_INFO, "trigger screenshot of \"%s\"",
		      _scene.ToString(true).c_str());
		break;
	case MacroActionScreenshot::TargetType::MAIN_OUTPUT:
		vblog(LOG_INFO, "trigger screenshot of main output");
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
	obs_data_set_int(obj, "version", _version);
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

static void populateSaveTypeSelection(QComboBox *list)
{
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.action.screenshot.save.default"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.action.screenshot.save.custom"));
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
	  _savePath(new FileSelection(FileSelection::Type::WRITE, this))
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

	QHBoxLayout *layout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{sources}}", _sources},
		{"{{scenes}}", _scenes},
		{"{{saveType}}", _saveType},
		{"{{targetType}}", _targetType},
	};
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.action.screenshot.entry"),
		layout, widgetPlaceholders);

	QVBoxLayout *mainLayout = new QVBoxLayout;
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
	SetWidgetVisibility();
}

void MacroActionScreenshotEdit::SceneChanged(const SceneSelection &s)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
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
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_path = text.toStdString();
}

void MacroActionScreenshotEdit::SourceChanged(const SourceSelection &source)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_source = source;
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionScreenshotEdit::SetWidgetVisibility()
{
	if (!_entryData) {
		return;
	}
	_savePath->setVisible(_entryData->_saveType ==
			      MacroActionScreenshot::SaveType::CUSTOM);
	_sources->setVisible(_entryData->_targetType ==
			     MacroActionScreenshot::TargetType::SOURCE);
	_scenes->setVisible(_entryData->_targetType ==
			    MacroActionScreenshot::TargetType::SCENE);

	adjustSize();
}

} // namespace advss
