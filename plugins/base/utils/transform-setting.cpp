#include "transform-setting.hpp"
#include "obs-module-helper.hpp"
#include "math-helpers.hpp"
#include "scene-item-transform-helpers.hpp"
#include "utility.hpp"

#include <nlohmann/json.hpp>
#include <QLayout>

Q_DECLARE_METATYPE(advss::TransformSetting);

namespace advss {

TransformSetting::TransformSetting(const std::string &id,
				   const std::string &nestedId)
	: _id(id),
	  _nestedId(nestedId)
{
}

bool TransformSetting::Save(obs_data_t *obj) const
{
	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_string(data, "id", _id.c_str());
	obs_data_set_string(data, "nestedId", _nestedId.c_str());
	obs_data_set_obj(obj, "transformSetting", data);
	return true;
}

bool TransformSetting::Load(obs_data_t *obj)
{
	OBSDataAutoRelease data = obs_data_get_obj(obj, "transformSetting");
	_id = obs_data_get_string(data, "id");
	_nestedId = obs_data_get_string(data, "nestedId");
	return true;
}

bool TransformSetting::operator==(const TransformSetting &other) const
{
	return _id == other._id && _nestedId == other._nestedId;
}

static void
getSoruceTransformSettingsHelper(std::vector<TransformSetting> &settings,
				 const std::string &jsonString,
				 const std::string &nestedId = "")
{
	try {
		auto transform = nlohmann::json::parse(jsonString);
		for (const auto &[key, value] : transform.items()) {
			if (value.is_object() && nestedId.empty()) {
				getSoruceTransformSettingsHelper(
					settings, value.dump(), key);
			} else {
				settings.emplace_back(key, nestedId);
			}
		}
	} catch (const nlohmann::json::exception &) {
		return;
	}
}

std::vector<TransformSetting> GetSoruceTransformSettings(obs_scene_item *source)
{
	std::vector<TransformSetting> settings;
	auto jsonString = GetSceneItemTransform(source);
	getSoruceTransformSettingsHelper(settings, jsonString);
	return settings;
}

std::optional<std::string>
GetTransformSettingValue(obs_scene_item *source,
			 const TransformSetting &setting)
{
	auto jsonString = GetSceneItemTransform(source);
	if (setting.GetNestedID().empty()) {
		return GetJsonField(jsonString, setting.GetID());
	}
	auto nestedJsonString = GetJsonField(jsonString, setting.GetNestedID());
	if (nestedJsonString.has_value()) {
		return GetJsonField(*nestedJsonString, setting.GetID());
	}
	return {};
}

template<class T>
static void logConversionError(T exception, const char *func,
			       const char *target, const char *value)
{
	if (std::is_same<T, std::invalid_argument>::value) {
		blog(LOG_WARNING, "%s invalid %s value (%s)", func, target,
		     value);
	} else {
		blog(LOG_WARNING, "%s value out of range for %s (%s)", func,
		     target, value);
	}
}

static void handleCrop(struct obs_sceneitem_crop &crop, const std::string &id,
		       const std::string &value)
{
	int val = 0;
	try {
		val = std::stoi(value);
	} catch (const std::invalid_argument &e) {
		logConversionError(e, __func__, id.c_str(), value.c_str());
		return;
	} catch (const std::out_of_range &e) {
		logConversionError(e, __func__, id.c_str(), value.c_str());
		return;
	}

	if (id == "left") {
		crop.left = val;
	} else if (id == "top") {
		crop.top = val;
	} else if (id == "right") {
		crop.right = val;
	} else if (id == "bottom") {
		crop.bottom = val;
	}
}

static void handlePosOrScaleOrBounds(struct obs_transform_info &info,
				     const TransformSetting &setting,
				     const std::string &value)
{
	float val = 0;
	try {
		val = std::stof(value);
	} catch (const std::invalid_argument &e) {
		logConversionError(
			e, __func__,
			(setting.GetID() + "-" + setting.GetNestedID()).c_str(),
			value.c_str());
		return;
	} catch (const std::out_of_range &e) {
		logConversionError(
			e, __func__,
			(setting.GetID() + "-" + setting.GetNestedID()).c_str(),
			value.c_str());
		return;
	}

	struct vec2 *target = nullptr;
	if (setting.GetNestedID() == "scale") {
		target = &info.scale;
	} else if (setting.GetNestedID() == "bounds") {
		target = &info.bounds;
	}

	if (setting.GetID() == "x") {
		target->x = val;
	} else if (setting.GetID() == "y") {
		target->y = val;
	}
}

static void handleRot(struct obs_transform_info &info, const std::string &value)
{
	float val = 0;
	try {
		val = std::stof(value);
	} catch (const std::invalid_argument &e) {
		logConversionError(e, __func__, "rot", value.c_str());
		return;
	} catch (const std::out_of_range &e) {
		logConversionError(e, __func__, "rot", value.c_str());
		return;
	}

	info.rot = val;
}

void SetTransformSetting(obs_scene_item *source,
			 const TransformSetting &setting,
			 const std::string &value)
{
	auto id = setting.GetID();

	struct obs_transform_info info = {};
	struct obs_sceneitem_crop crop = {};

	obs_sceneitem_get_info(source, &info);
	obs_sceneitem_get_crop(source, &crop);

	if (id == "alignment") {
		try {
			auto val = std::stoul(value);
			info.alignment = val;
		} catch (const std::invalid_argument &e) {
			logConversionError(e, __func__, id.c_str(),
					   value.c_str());
		} catch (const std::out_of_range &e) {
			logConversionError(e, __func__, id.c_str(),
					   value.c_str());
		}
	} else if (id == "left" || id == "top" || id == "right" ||
		   id == "bottom") {
		handleCrop(crop, id, value);
	} else if (id == "bounds_alignment") {
		uint32_t val = 0;
		try {
			val = std::stoul(value);
			info.bounds_alignment = val;
		} catch (const std::invalid_argument &e) {
			logConversionError(e, __func__, id.c_str(),
					   value.c_str());
		} catch (const std::out_of_range &e) {
			logConversionError(e, __func__, id.c_str(),
					   value.c_str());
		}
	} else if (id == "bounds_type") {
		try {
			obs_bounds_type val =
				static_cast<obs_bounds_type>(std::stoul(value));
			info.bounds_type = val;
		} catch (const std::invalid_argument &e) {
			logConversionError(e, __func__, id.c_str(),
					   value.c_str());
		} catch (const std::out_of_range &e) {
			logConversionError(e, __func__, id.c_str(),
					   value.c_str());
		}
	} else if (id == "x" || id == "y") {
		handlePosOrScaleOrBounds(info, setting, value);
	} else if (id == "width" || id == "height") {
		float val = 0;
		try {
			val = std::stof(value);
		} catch (const std::invalid_argument &e) {
			logConversionError(
				e, __func__,
				(setting.GetID() + "-" + setting.GetNestedID())
					.c_str(),
				value.c_str());
			return;
		} catch (const std::out_of_range &e) {
			logConversionError(
				e, __func__,
				(setting.GetID() + "-" + setting.GetNestedID())
					.c_str(),
				value.c_str());
			return;
		}
		auto source2 = obs_sceneitem_get_source(source);
		if (setting.GetID() == "width") {
			val = val / obs_source_get_width(source2);
			handlePosOrScaleOrBounds(info,
						 TransformSetting("x", "scale"),
						 std::to_string(val));
		} else if (setting.GetID() == "height") {
			val = val / obs_source_get_height(source2);
			handlePosOrScaleOrBounds(info,
						 TransformSetting("y", "scale"),
						 std::to_string(val));
		}
	} else if (id == "rot") {
		handleRot(info, value);
	}

	obs_sceneitem_defer_update_begin(source);
	obs_sceneitem_set_info(source, &info);
	obs_sceneitem_set_crop(source, &crop);
	obs_sceneitem_defer_update_end(source);
	obs_sceneitem_force_update_transform(source);
}

TransformSettingSelection::TransformSettingSelection(QWidget *parent)
	: QWidget(parent),
	  _settings(new FilterComboBox(
		  this, obs_module_text("AdvSceneSwitcher.setting.select")))
{
	_settings->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	_settings->setMaximumWidth(350);

	QWidget::connect(_settings, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(SelectionIdxChanged(int)));

	auto layout = new QHBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(_settings);
	setLayout(layout);
}

void TransformSettingSelection::SetSource(obs_scene_item *source)
{
	_settings->clear();
	Populate(source);
}

void TransformSettingSelection::SetSetting(const TransformSetting &setting)
{
	QVariant variant;
	variant.setValue(setting);
	_settings->setCurrentIndex(_settings->findData(variant));
}

void TransformSettingSelection::SelectionIdxChanged(int idx)
{
	if (idx == -1) {
		return;
	}
	auto setting = _settings->itemData(idx).value<TransformSetting>();
	emit SelectionChanged(setting);
}

static QString mapIdToLocale(const TransformSetting &setting)
{
	static const std::unordered_map<std::string_view, std::string_view>
		idMap = {
			{"alignment",
			 "AdvSceneSwitcher.setting.transform.alignment"},
			{"bottom",
			 "AdvSceneSwitcher.setting.transform.cropBottom"},
			{"left", "AdvSceneSwitcher.setting.transform.cropLeft"},
			{"right",
			 "AdvSceneSwitcher.setting.transform.cropRight"},
			{"top", "AdvSceneSwitcher.setting.transform.cropTop"},
			{"bounds_alignment",
			 "AdvSceneSwitcher.setting.transform.boundingBoxAlign"},
			{"bounds_type",
			 "AdvSceneSwitcher.setting.transform.boundingBoxType"},
			{"rot", "AdvSceneSwitcher.setting.transform.rotation"},
			{"height", "AdvSceneSwitcher.setting.transform.height"},
			{"width", "AdvSceneSwitcher.setting.transform.width"},
			{"x", "x"},
			{"y", "y"},
		};

	static const std::unordered_map<std::string_view, std::string_view>
		nestedIdMap = {
			{"bounds",
			 "AdvSceneSwitcher.setting.transform.boundingBoxSize"},
			{"pos", "AdvSceneSwitcher.setting.transform.position"},
			{"scale", "AdvSceneSwitcher.setting.transform.scale"},
			{"size", "AdvSceneSwitcher.setting.transform.size"},
		};

	auto idNameIt = idMap.find(setting.GetID());
	QString name = idNameIt == idMap.end()
			       ? QString::fromStdString(setting.GetID())
			       : obs_module_text(idNameIt->second.data());

	if (!setting.GetNestedID().empty()) {
		auto idNameIt = nestedIdMap.find(setting.GetNestedID());
		const auto nestedName =
			idNameIt == nestedIdMap.end()
				? QString::fromStdString(setting.GetID())
				: obs_module_text(idNameIt->second.data());
		name = "[" + nestedName + "] " + name;
	}
	return name;
}

void TransformSettingSelection::Populate(obs_scene_item *source)
{
	auto settings = GetSoruceTransformSettings(source);
	for (const auto &setting : settings) {
		QVariant variant;
		variant.setValue(setting);
		auto name = mapIdToLocale(setting);
		_settings->addItem(name, variant);
	}
	_settings->setCurrentIndex(-1);
	TransformSetting emptySetting;
	emit SelectionChanged(emptySetting);

	adjustSize();
	updateGeometry();
}

} // namespace advss
