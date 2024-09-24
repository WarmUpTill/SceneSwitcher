#include "macro-action-scene-transform.hpp"
#include "json-helpers.hpp"
#include "scene-item-transform-helpers.hpp"
#include "layout-helpers.hpp"

#include <graphics/matrix4.h>

namespace advss {

const std::string MacroActionSceneTransform::id = "scene_transform";

bool MacroActionSceneTransform::_registered = MacroActionFactory::Register(
	MacroActionSceneTransform::id,
	{MacroActionSceneTransform::Create,
	 MacroActionSceneTransformEdit::Create,
	 "AdvSceneSwitcher.action.sceneTransform"});

const static std::map<MacroActionSceneTransform::Action, std::string> actions = {
	{MacroActionSceneTransform::Action::MANUAL_TRANSFORM,
	 "AdvSceneSwitcher.action.sceneTransform.type.manual"},
	{MacroActionSceneTransform::Action::SINGLE_TRANSFORM_SETTING,
	 "AdvSceneSwitcher.action.sceneTransform.type.setSingleSetting"},
	{MacroActionSceneTransform::Action::RESET,
	 "AdvSceneSwitcher.action.sceneTransform.type.reset"},
	{MacroActionSceneTransform::Action::ROTATE,
	 "AdvSceneSwitcher.action.sceneTransform.type.rotate"},
	{MacroActionSceneTransform::Action::FLIP_HORIZONTAL,
	 "AdvSceneSwitcher.action.sceneTransform.type.flipHorizontal"},
	{MacroActionSceneTransform::Action::FLIP_VERTICAL,
	 "AdvSceneSwitcher.action.sceneTransform.type.flipVertical"},
	{MacroActionSceneTransform::Action::FIT_TO_SCREEN,
	 "AdvSceneSwitcher.action.sceneTransform.type.fitToScreen"},
	{MacroActionSceneTransform::Action::STRETCH_TO_SCREEN,
	 "AdvSceneSwitcher.action.sceneTransform.type.stretchToScreen"},
	{MacroActionSceneTransform::Action::CENTER_TO_SCREEN,
	 "AdvSceneSwitcher.action.sceneTransform.type.centerToScreen"},
	{MacroActionSceneTransform::Action::CENTER_VERTICALLY,
	 "AdvSceneSwitcher.action.sceneTransform.type.centerVertically"},
	{MacroActionSceneTransform::Action::CENTER_HORIZONTALLY,
	 "AdvSceneSwitcher.action.sceneTransform.type.centerHorizontally"},
};

/* --------------- Helpers based on OBS window-basic-main.cpp --------------- */

static void getItemBox(obs_sceneitem_t *item, vec3 &tl, vec3 &br)
{
	matrix4 boxTransform;
	obs_sceneitem_get_box_transform(item, &boxTransform);

	vec3_set(&tl, M_INFINITE, M_INFINITE, 0.0f);
	vec3_set(&br, -M_INFINITE, -M_INFINITE, 0.0f);

	auto GetMinPos = [&](float x, float y) {
		vec3 pos;
		vec3_set(&pos, x, y, 0.0f);
		vec3_transform(&pos, &pos, &boxTransform);
		vec3_min(&tl, &tl, &pos);
		vec3_max(&br, &br, &pos);
	};

	GetMinPos(0.0f, 0.0f);
	GetMinPos(1.0f, 0.0f);
	GetMinPos(0.0f, 1.0f);
	GetMinPos(1.0f, 1.0f);
}

static vec3 getItemTL(obs_sceneitem_t *item)
{
	vec3 tl, br;
	getItemBox(item, tl, br);
	return tl;
}

static void setItemTL(obs_sceneitem_t *item, const vec3 &tl)
{
	vec3 newTL;
	vec2 pos;

	obs_sceneitem_get_pos(item, &pos);
	newTL = getItemTL(item);
	pos.x += tl.x - newTL.x;
	pos.y += tl.y - newTL.y;
	obs_sceneitem_set_pos(item, &pos);
}

static void reset(obs_sceneitem_t *item)
{
	obs_transform_info info;
	vec2_set(&info.pos, 0.0f, 0.0f);
	vec2_set(&info.scale, 1.0f, 1.0f);
	info.rot = 0.0f;
	info.alignment = OBS_ALIGN_TOP | OBS_ALIGN_LEFT;
	info.bounds_type = OBS_BOUNDS_NONE;
	info.bounds_alignment = OBS_ALIGN_CENTER;
	vec2_set(&info.bounds, 0.0f, 0.0f);
#if (LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(30, 1, 0))
	obs_sceneitem_set_info2(item, &info);
#else
	obs_sceneitem_set_info(item, &info);
#endif

	obs_sceneitem_crop crop = {};
	obs_sceneitem_set_crop(item, &crop);
}

static void rotate(obs_sceneitem_t *item, float rot)
{
	vec3 tl = getItemTL(item);
	rot += obs_sceneitem_get_rot(item);
	if (rot >= 360.0f) {
		rot -= 360.0f;
	} else if (rot <= -360.0f) {
		rot += 360.0f;
	}
	obs_sceneitem_set_rot(item, rot);
	obs_sceneitem_force_update_transform(item);
	setItemTL(item, tl);
};

static void flip(obs_sceneitem_t *item, const vec2 &mul)
{
	vec3 tl = getItemTL(item);
	vec2 scale;
	obs_sceneitem_get_scale(item, &scale);
	vec2_mul(&scale, &scale, &mul);
	obs_sceneitem_set_scale(item, &scale);
	obs_sceneitem_force_update_transform(item);
	setItemTL(item, tl);
}

static void centerAlign(obs_sceneitem_t *item, obs_bounds_type boundsType)
{
	obs_video_info ovi;
	obs_get_video_info(&ovi);

	obs_transform_info itemInfo;
	vec2_set(&itemInfo.pos, 0.0f, 0.0f);
	vec2_set(&itemInfo.scale, 1.0f, 1.0f);
	itemInfo.alignment = OBS_ALIGN_LEFT | OBS_ALIGN_TOP;
	itemInfo.rot = 0.0f;

	vec2_set(&itemInfo.bounds, float(ovi.base_width),
		 float(ovi.base_height));
	itemInfo.bounds_type = boundsType;
	itemInfo.bounds_alignment = OBS_ALIGN_CENTER;

#if (LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(30, 1, 0))
	obs_sceneitem_set_info2(item, &itemInfo);
#else
	obs_sceneitem_set_info(item, &itemInfo);
#endif
}

enum class CenterType {
	SCREEN,
	VERTICAL,
	HORIZONTAL,
};

static void center(obs_sceneitem_t *item, const CenterType &centerType)
{
	// Get center x, y coordinates of items
	vec3 tl, br;
	getItemBox(item, tl, br);

	float left = tl.x;
	float top = tl.y;
	float right = br.x;
	float bottom = br.y;

	vec3 center;
	center.x = (right + left) / 2.0f;
	center.y = (top + bottom) / 2.0f;
	center.z = 0.0f;

	// Get coordinates of screen center
	obs_video_info ovi;
	obs_get_video_info(&ovi);

	vec3 screenCenter;
	vec3_set(&screenCenter, float(ovi.base_width), float(ovi.base_height),
		 0.0f);

	vec3_mulf(&screenCenter, &screenCenter, 0.5f);

	// Calculate difference between screen center and item center
	vec3 offset;
	vec3_sub(&offset, &screenCenter, &center);

	// Shift item by offset
	vec3_add(&tl, &tl, &offset);

	vec3 itemTL = getItemTL(item);

	if (centerType == CenterType::VERTICAL) {
		tl.x = itemTL.x;
	} else if (centerType == CenterType::HORIZONTAL) {
		tl.y = itemTL.y;
	}

	setItemTL(item, tl);
}

/* ------------------------------------------------------------------------- */

void MacroActionSceneTransform::Transform(obs_scene_item *item)
{
	if (!item) {
		return;
	}

	switch (_action) {
	case Action::RESET:
		obs_sceneitem_defer_update_begin(item);
		reset(item);
		obs_sceneitem_defer_update_end(item);

		break;
	case Action::ROTATE:
		rotate(item, _rotation);
		break;
	case Action::FLIP_HORIZONTAL: {
		vec2 scale;
		vec2_set(&scale, -1.0f, 1.0f);
		flip(item, scale);
		break;
	}
	case Action::FLIP_VERTICAL: {
		vec2 scale;
		vec2_set(&scale, 1.0f, -1.0f);
		flip(item, scale);
		break;
	}
	case Action::FIT_TO_SCREEN:
		centerAlign(item, OBS_BOUNDS_SCALE_INNER);
		break;
	case Action::STRETCH_TO_SCREEN:
		centerAlign(item, OBS_BOUNDS_STRETCH);
		break;
	case Action::CENTER_TO_SCREEN:
		center(item, CenterType::SCREEN);
		break;
	case Action::CENTER_VERTICALLY:
		center(item, CenterType::VERTICAL);
		break;
	case Action::CENTER_HORIZONTALLY:
		center(item, CenterType::HORIZONTAL);
		break;
	case Action::MANUAL_TRANSFORM:
		ApplySettings(_transformString); // Resolves variables
		obs_sceneitem_defer_update_begin(item);
#if (LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(30, 1, 0))
		obs_sceneitem_set_info2(item, &_info);
#else
		obs_sceneitem_set_info(item, &_info);
#endif
		obs_sceneitem_set_crop(item, &_crop);
		obs_sceneitem_defer_update_end(item);
		break;
	case Action::SINGLE_TRANSFORM_SETTING:
		SetTransformSetting(item, _settingSelection,
				    _singleSettingsValue);
	}
}

bool MacroActionSceneTransform::PerformAction()
{
	auto items = _source.GetSceneItems(_scene);
	for (const auto &item : items) {
		Transform(item);
	}
	return true;
}

void MacroActionSceneTransform::LogAction() const
{
	ablog(LOG_INFO,
	      "performed transform action %d for source \"%s\" on scene \"%s\"",
	      static_cast<int>(_action), _source.ToString(true).c_str(),
	      _scene.ToString(true).c_str());
}

bool MacroActionSceneTransform::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	_scene.Save(obj);
	_source.Save(obj);
	_rotation.Save(obj, "rotation");
	_transformString.Save(obj, "settings");
	_singleSettingsValue.Save(obj, "singleSettingsValue");
	_settingSelection.Save(obj);
	return true;
}

bool MacroActionSceneTransform::Load(obs_data_t *obj)
{
	// Convert old data format
	// TODO: Remove in future version
	if (obs_data_has_user_value(obj, "source")) {
		auto sourceName = obs_data_get_string(obj, "source");
		obs_data_set_string(obj, "sceneItem", sourceName);
	}

	MacroAction::Load(obj);
	if (!obs_data_has_user_value(obj, "action")) {
		_action =
			Action::MANUAL_TRANSFORM; // TODO: Remove fallback in future
	} else {
		_action = static_cast<Action>(obs_data_get_int(obj, "action"));
	}
	_scene.Load(obj);
	_source.Load(obj);
	_rotation.Load(obj, "rotation");
	_transformString.Load(obj, "settings");
	_singleSettingsValue.Load(obj, "singleSettingsValue");
	_settingSelection.Load(obj);

	// Convert old data format
	// TODO: Remove in future version
	if (!obs_data_has_user_value(obj, "settings")) {
		LoadTransformState(obj, _info, _crop);
		_transformString = ConvertSettings();
	}
	return true;
}

std::string MacroActionSceneTransform::GetShortDesc() const
{
	if (_source.ToString().empty()) {
		return "";
	}
	return _scene.ToString() + " - " + _source.ToString();
}

std::shared_ptr<MacroAction> MacroActionSceneTransform::Create(Macro *m)
{
	return std::make_shared<MacroActionSceneTransform>(m);
}

std::shared_ptr<MacroAction> MacroActionSceneTransform::Copy() const
{
	return std::make_shared<MacroActionSceneTransform>(*this);
}

void MacroActionSceneTransform::ResolveVariablesToFixedValues()
{
	_scene.ResolveVariables();
	_source.ResolveVariables();
	_transformString.ResolveVariables();
	_rotation.ResolveVariables();
	_singleSettingsValue.ResolveVariables();
}

std::string MacroActionSceneTransform::ConvertSettings()
{
	auto data = obs_data_create();
	SaveTransformState(data, _info, _crop);
	std::string json = obs_data_get_json(data);
	obs_data_release(data);
	return json;
}

void MacroActionSceneTransform::ApplySettings(const std::string &settings)
{
	auto data = obs_data_create_from_json(settings.c_str());
	if (!data) {
		return;
	}
	LoadTransformState(data, _info, _crop);
	auto items = _source.GetSceneItems(_scene);
	if (items.empty()) {
		return;
	}
	if (obs_data_has_user_value(data, "size")) {
		auto obj = obs_data_get_obj(data, "size");
		auto source = obs_sceneitem_get_source(items[0]);
		if (double h = obs_data_get_double(obj, "height")) {
			_info.scale.y =
				h / double(obs_source_get_height(source));
		}
		if (double w = obs_data_get_double(obj, "width")) {
			_info.scale.x =
				w / double(obs_source_get_width(source));
		}
		obs_data_release(obj);
	}
	obs_data_release(data);
}

static inline void populateActionSelection(QComboBox *list)
{
	for (const auto &[type, name] : actions) {
		list->addItem(obs_module_text(name.c_str()),
			      static_cast<int>(type));
	}
}

MacroActionSceneTransformEdit::MacroActionSceneTransformEdit(
	QWidget *parent, std::shared_ptr<MacroActionSceneTransform> entryData)
	: QWidget(parent),
	  _scenes(new SceneSelectionWidget(window(), true, false, false, true)),
	  _sources(new SceneItemSelectionWidget(parent)),
	  _action(new QComboBox()),
	  _rotation(new VariableDoubleSpinBox()),
	  _getTransform(new QPushButton(obs_module_text(
		  "AdvSceneSwitcher.action.sceneTransform.getTransform"))),
	  _getCurrentValue(new QPushButton(obs_module_text(
		  "AdvSceneSwitcher.action.sceneTransform.getCurrentValue"))),
	  _transformString(new VariableTextEdit(this)),
	  _settingSelection(new TransformSettingSelection(this)),
	  _singleSettingValue(new VariableLineEdit(this)),
	  _buttonLayout(new QHBoxLayout())
{
	_rotation->setMinimum(-360);
	_rotation->setMaximum(360);
	_rotation->setSuffix("°");

	populateActionSelection(_action);

	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 this, SLOT(SceneChanged(const SceneSelection &)));
	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 _sources, SLOT(SceneChanged(const SceneSelection &)));
	QWidget::connect(_sources,
			 SIGNAL(SceneItemChanged(const SceneItemSelection &)),
			 this, SLOT(SourceChanged(const SceneItemSelection &)));
	QWidget::connect(_action, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(
		_rotation,
		SIGNAL(NumberVariableChanged(const NumberVariable<double> &)),
		this, SLOT(RotationChanged(const NumberVariable<double> &)));
	QWidget::connect(_getTransform, SIGNAL(clicked()), this,
			 SLOT(GetSettingsClicked()));
	QWidget::connect(_getCurrentValue, SIGNAL(clicked()), this,
			 SLOT(GetCurrentValueClicked()));
	QWidget::connect(_transformString, SIGNAL(textChanged()), this,
			 SLOT(TransformStringChanged()));
	QWidget::connect(_singleSettingValue, SIGNAL(editingFinished()), this,
			 SLOT(SettingValueChanged()));
	QWidget::connect(
		_settingSelection,
		SIGNAL(SelectionChanged(const TransformSetting &)), this,
		SLOT(SettingSelectionChanged(const TransformSetting &)));

	QHBoxLayout *entryLayout = new QHBoxLayout;

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{scenes}}", _scenes},
		{"{{rotation}}", _rotation},
		{"{{sources}}", _sources},
		{"{{action}}", _action},
		{"{{settingSelection}}", _settingSelection},
		{"{{singleSettingValue}}", _singleSettingValue},
	};
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.action.sceneTransform.entry"),
		entryLayout, widgetPlaceholders);

	_buttonLayout->addWidget(_getTransform);
	_buttonLayout->addWidget(_getCurrentValue);
	_buttonLayout->addStretch();

	auto mainLayout = new QVBoxLayout;
	mainLayout->addLayout(entryLayout);
	mainLayout->addWidget(_transformString);
	mainLayout->addLayout(_buttonLayout);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionSceneTransformEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_scenes->SetScene(_entryData->_scene);
	_sources->SetSceneItem(_entryData->_source);
	_action->setCurrentIndex(
		_action->findData(static_cast<int>(_entryData->_action)));
	_rotation->SetValue(_entryData->_rotation);
	_transformString->setPlainText(_entryData->_transformString);
	UpdateSettingSelection();
	_settingSelection->SetSetting(_entryData->_settingSelection);
	_singleSettingValue->setText(_entryData->_singleSettingsValue);
	SetWidgetVisibility();
}

void MacroActionSceneTransformEdit::SceneChanged(const SceneSelection &s)
{
	{
		GUARD_LOADING_AND_LOCK();
		_entryData->_scene = s;
	}

	UpdateSettingSelection();
}

void MacroActionSceneTransformEdit::SourceChanged(const SceneItemSelection &item)
{
	{
		GUARD_LOADING_AND_LOCK();
		_entryData->_source = item;
	}

	UpdateSettingSelection();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
	adjustSize();
	updateGeometry();
}

void MacroActionSceneTransformEdit::ActionChanged(int idx)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_action = static_cast<MacroActionSceneTransform::Action>(
		_action->itemData(idx).toInt());
	SetWidgetVisibility();
}

void MacroActionSceneTransformEdit::RotationChanged(const DoubleVariable &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_rotation = value;
}

void MacroActionSceneTransformEdit::GetSettingsClicked()
{
	if (_loading || !_entryData || !_entryData->_scene.GetScene(false)) {
		return;
	}

	auto items = _entryData->_source.GetSceneItems(_entryData->_scene);
	if (items.empty()) {
		return;
	}

	auto settings = GetSceneItemTransform(items[0]);
	_transformString->setPlainText(FormatJsonString(settings));
}

void MacroActionSceneTransformEdit::GetCurrentValueClicked()
{
	if (_loading || !_entryData || !_entryData->_scene.GetScene(false)) {
		return;
	}

	auto items = _entryData->_source.GetSceneItems(_entryData->_scene);
	if (items.empty()) {
		return;
	}

	auto value = GetTransformSettingValue(items.at(0),
					      _entryData->_settingSelection);
	if (!value) {
		return;
	}

	_singleSettingValue->setText(QString::fromStdString(*value));
	SettingValueChanged();
}

void MacroActionSceneTransformEdit::TransformStringChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_transformString =
		_transformString->toPlainText().toStdString();

	adjustSize();
	updateGeometry();
}

void MacroActionSceneTransformEdit::SettingSelectionChanged(
	const TransformSetting &setting)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_settingSelection = setting;
}

void MacroActionSceneTransformEdit::SettingValueChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_singleSettingsValue =
		_singleSettingValue->text().toStdString();

	adjustSize();
	updateGeometry();
}

void MacroActionSceneTransformEdit::SetWidgetVisibility()
{
	SetLayoutVisible(
		_buttonLayout,
		_entryData->_action ==
			MacroActionSceneTransform::Action::MANUAL_TRANSFORM);
	_transformString->setVisible(
		_entryData->_action ==
		MacroActionSceneTransform::Action::MANUAL_TRANSFORM);
	_rotation->setVisible(_entryData->_action ==
			      MacroActionSceneTransform::Action::ROTATE);
	_getTransform->setVisible(
		_entryData->_action ==
		MacroActionSceneTransform::Action::MANUAL_TRANSFORM);
	_getCurrentValue->setVisible(
		_entryData->_action ==
		MacroActionSceneTransform::Action::SINGLE_TRANSFORM_SETTING);
	_settingSelection->setVisible(
		_entryData->_action ==
		MacroActionSceneTransform::Action::SINGLE_TRANSFORM_SETTING);
	_singleSettingValue->setVisible(
		_entryData->_action ==
		MacroActionSceneTransform::Action::SINGLE_TRANSFORM_SETTING);
	adjustSize();
	updateGeometry();
}

void MacroActionSceneTransformEdit::UpdateSettingSelection() const
{
	if (!_entryData) {
		_settingSelection->SetSource(nullptr);
		return;
	}
	auto items = _entryData->_source.GetSceneItems(_entryData->_scene);
	if (items.empty()) {
		_settingSelection->SetSource(nullptr);
		return;
	}
	_settingSelection->SetSource(items.at(0));
}

} // namespace advss
