#include "macro-action-screenshot.hpp"
#include "advanced-scene-switcher.hpp"
#include "utility.hpp"

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
	new (&_screenshot) ScreenshotHelper(s, false, 0, true, _path);
	obs_source_release(s);
}

bool MacroActionScreenshot::PerformAction()
{
	OBSWeakSource source = nullptr;
	switch (_targetType) {
	case MacroActionScreenshot::TargetType::SOURCE:
		source = _source;
		break;
	case MacroActionScreenshot::TargetType::SCENE:
		source = _scene.GetScene(false);
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

void MacroActionScreenshot::LogAction()
{
	vblog(LOG_INFO, "trigger screenshot for \"%s\"",
	      _targetType == TargetType::SOURCE
		      ? GetWeakSourceName(_source).c_str()
		      : _scene.ToString().c_str());
}

bool MacroActionScreenshot::Save(obs_data_t *obj)
{
	MacroAction::Save(obj);
	_scene.Save(obj);
	obs_data_set_string(obj, "source", GetWeakSourceName(_source).c_str());
	obs_data_set_int(obj, "saveType", static_cast<int>(_saveType));
	obs_data_set_int(obj, "targetType", static_cast<int>(_targetType));
	obs_data_set_string(obj, "savePath", _path.c_str());
	return true;
}

bool MacroActionScreenshot::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_scene.Load(obj);
	const char *sourceName = obs_data_get_string(obj, "source");
	_source = GetWeakSourceByName(sourceName);
	_saveType = static_cast<SaveType>(obs_data_get_int(obj, "saveType"));
	_targetType =
		static_cast<TargetType>(obs_data_get_int(obj, "targetType"));
	_path = obs_data_get_string(obj, "savePath");
	return true;
}

std::string MacroActionScreenshot::GetShortDesc()
{
	if (_targetType == TargetType::SCENE) {
		return _scene.ToString();
	} else {
		return GetWeakSourceName(_source);
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
}

MacroActionScreenshotEdit::MacroActionScreenshotEdit(
	QWidget *parent, std::shared_ptr<MacroActionScreenshot> entryData)
	: QWidget(parent),
	  _scenes(new SceneSelectionWidget(this, true, false, true, true,
					   true)),
	  _sources(new QComboBox()),
	  _saveType(new QComboBox()),
	  _targetType(new QComboBox()),
	  _savePath(new FileSelection(FileSelection::Type::WRITE, this))
{
	setToolTip(obs_module_text(
		"AdvSceneSwitcher.action.screenshot.blackscreenNote"));
	populateVideoSelection(_sources, true);
	populateSaveTypeSelection(_saveType);
	populateTargetTypeSelection(_targetType);

	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 this, SLOT(SceneChanged(const SceneSelection &)));
	QWidget::connect(_sources, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(SourceChanged(const QString &)));
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
	placeWidgets(
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

	if (_entryData->_source) {
		_sources->setCurrentText(
			GetWeakSourceName(_entryData->_source).c_str());
	} else {
		_sources->setCurrentIndex(1);
	}
	_scenes->SetScene(_entryData->_scene);
	_saveType->setCurrentIndex(static_cast<int>(_entryData->_saveType));
	_targetType->setCurrentIndex(static_cast<int>(_entryData->_targetType));
	_savePath->SetPath(QString::fromStdString(_entryData->_path));
	SetWidgetVisibility();
}

void MacroActionScreenshotEdit::SceneChanged(const SceneSelection &s)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
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

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_path = text.toUtf8().constData();
}

void MacroActionScreenshotEdit::SourceChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_source = GetWeakSourceByQString(text);
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
