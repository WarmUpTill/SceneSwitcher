#include "source-setting.hpp"
#include "json-helpers.hpp"
#include "log-helper.hpp"
#include "obs-module-helper.hpp"
#include "math-helpers.hpp"

#include <nlohmann/json.hpp>
#include <QLayout>

Q_DECLARE_METATYPE(advss::SourceSetting);

using properties_delete_t = decltype(&obs_properties_destroy);
using properties_t = std::unique_ptr<obs_properties_t, properties_delete_t>;

namespace advss {

SourceSetting::SourceSetting(const std::string &id, obs_property_type type,
			     const std::string &description,
			     const std::string &longDescription)
	: _id(id),
	  _type(type),
	  _description(description),
	  _longDescription(longDescription)
{
}

bool SourceSetting::Save(obs_data_t *obj) const
{
	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_string(data, "id", _id.c_str());
	obs_data_set_int(data, "type", _type);
	obs_data_set_string(data, "description", _description.c_str());
	obs_data_set_obj(obj, "sourceSetting", data);
	return true;
}

bool SourceSetting::Load(obs_data_t *obj)
{
	OBSDataAutoRelease data = obs_data_get_obj(obj, "sourceSetting");
	_id = obs_data_get_string(data, "id");
	_type = static_cast<obs_property_type>(obs_data_get_int(data, "type"));
	_description = obs_data_get_string(data, "description");
	return true;
}

static bool isListType(obs_property_type type)
{
	return type == OBS_PROPERTY_LIST || type == OBS_PROPERTY_EDITABLE_LIST;
}

bool SourceSetting::IsList() const
{
	return isListType(_type);
}

bool SourceSetting::operator==(const SourceSetting &other) const
{
	return _id == other._id;
}

static const char *getPropertySuffix(obs_property_type type)
{
	switch (type) {
	case OBS_PROPERTY_INVALID:
		return "AdvSceneSwitcher.settings.suffix.type.invalid";
	case OBS_PROPERTY_BOOL:
		return "AdvSceneSwitcher.settings.suffix.type.bool";
	case OBS_PROPERTY_INT:
		return "AdvSceneSwitcher.settings.suffix.type.int";
	case OBS_PROPERTY_FLOAT:
		return "AdvSceneSwitcher.settings.suffix.type.float";
	case OBS_PROPERTY_TEXT:
		return "AdvSceneSwitcher.settings.suffix.type.text";
	case OBS_PROPERTY_PATH:
		return "AdvSceneSwitcher.settings.suffix.type.path";
	case OBS_PROPERTY_LIST:
		return "AdvSceneSwitcher.settings.suffix.type.list";
	case OBS_PROPERTY_COLOR:
		return "AdvSceneSwitcher.settings.suffix.type.color";
	case OBS_PROPERTY_BUTTON:
		return "AdvSceneSwitcher.settings.suffix.type.button";
	case OBS_PROPERTY_FONT:
		return "AdvSceneSwitcher.settings.suffix.type.font";
	case OBS_PROPERTY_EDITABLE_LIST:
		return "AdvSceneSwitcher.settings.suffix.type.editableList";
	case OBS_PROPERTY_FRAME_RATE:
		return "AdvSceneSwitcher.settings.suffix.type.frameRate";
	case OBS_PROPERTY_GROUP:
		return "AdvSceneSwitcher.settings.suffix.type.group";
	case OBS_PROPERTY_COLOR_ALPHA:
		return "AdvSceneSwitcher.settings.suffix.type.colorAlpha";
	}
	return "";
}

static void addSettingsHelper(obs_property_t *property,
			      std::vector<SourceSetting> &settings,
			      const std::string &prefix = "")
{
	do {
		if (!property) {
			continue;
		}
		const auto type = obs_property_get_type(property);
		if (type == OBS_PROPERTY_GROUP) {
			auto group = obs_property_group_content(property);
			auto description = obs_property_description(property);
			std::string prefix =
				description
					? std::string("[") + description + "] "
					: "";
			addSettingsHelper(obs_properties_first(group), settings,
					  prefix);
			continue;
		}

		auto name = obs_property_name(property);
		if (!name) {
			continue;
		}
		auto description = obs_property_description(property);
		if (!description) {
			continue;
		}
		std::string descriptionWithPrefixAndSuffix =
			prefix + description +
			obs_module_text(getPropertySuffix(type));
		auto longDescription = obs_property_long_description(property);
		SourceSetting setting(name, type,
				      descriptionWithPrefixAndSuffix,
				      longDescription ? longDescription : "");
		settings.emplace_back(setting);
	} while (obs_property_next(&property));
}

static std::optional<std::string>
getDataJsonWithDefaults(const OBSWeakSource &ws)
{
	OBSSourceAutoRelease source = obs_weak_source_get_source(ws);
	OBSDataAutoRelease data = obs_source_get_settings(source);
	if (!data) {
		return {};
	}
	OBSDataAutoRelease dataWithDefaults = obs_data_get_defaults(data);
	obs_data_apply(dataWithDefaults, data);
	auto json = obs_data_get_json(dataWithDefaults);
	if (!json) {
		return {};
	}
	return json;
}

std::optional<std::string> GetSourceSettingValue(const OBSWeakSource &source,
						 const SourceSetting &setting)
{
	const auto json = getDataJsonWithDefaults(source);
	if (!json) {
		return {};
	}
	auto value = GetJsonField(*json, setting.GetID());
	return value;
}

static obs_property_t *findListPropertyByIdHelper(obs_property_t *property,
						  const std::string &id)
{
	do {
		if (!property) {
			continue;
		}

		const auto type = obs_property_get_type(property);
		if (type == OBS_PROPERTY_GROUP) {
			auto group = obs_property_group_content(property);
			findListPropertyByIdHelper(obs_properties_first(group),
						   id);
			continue;
		}

		if (!isListType(type)) {
			continue;
		}

		auto name = obs_property_name(property);
		if (name != id) {
			continue;
		}

		return property;

	} while (obs_property_next(&property));

	return nullptr;
}

static std::pair<obs_property_t *, properties_t>
findListPropertyById(const obs_source_t *source, const std::string &id)
{
	// Note:
	// Using obs_source_properties() here might cause flickering for some
	// sources, as this will call obs_properties_apply_settings()
	// implicitly.
	// So, we use obs_get_source_properties() instead.
	//
	// 20250617: This might cause issues with the move plugin as it doesn't
	// handle obs_get_source_properties() properly in the move filter
	auto properties = obs_get_source_properties(obs_source_get_id(source));
	if (!properties) {
		return {nullptr, properties_t(nullptr, obs_properties_destroy)};
	}
	auto property = obs_properties_first(properties);
	return {findListPropertyByIdHelper(property, id),
		properties_t(properties, obs_properties_destroy)};
}

static std::pair<obs_property_t *, properties_t>
findListPropertyById(const OBSWeakSource &ws, const std::string &id)
{
	OBSSourceAutoRelease source = obs_weak_source_get_source(ws);
	return findListPropertyById(source.Get(), id);
}

std::optional<std::string>
GetSourceSettingListEntryName(const OBSWeakSource &source,
			      const SourceSetting &setting)
{
	if (!setting.IsList()) {
		return {};
	}

	const auto json = getDataJsonWithDefaults(source);
	if (!json) {
		return {};
	}

	const auto &[property, properties] =
		findListPropertyById(source, setting.GetID());
	if (!property) {
		return {};
	}

	const auto type = obs_property_list_format(property);
	if (type == OBS_COMBO_FORMAT_INVALID) {
		return {};
	}

	auto currentValue = GetJsonField(*json, setting.GetID());
	if (!currentValue) {
		return {};
	}

	const auto count = obs_property_list_item_count(property);
	for (size_t i = 0; i < count; i++) {
		std::string value;
		switch (type) {
		case OBS_COMBO_FORMAT_INT:
			value = std::to_string(
				obs_property_list_item_int(property, i));
			break;
		case OBS_COMBO_FORMAT_FLOAT:
			value = std::to_string(
				obs_property_list_item_float(property, i));
			break;
		case OBS_COMBO_FORMAT_STRING:
			value = obs_property_list_item_string(property, i);
			break;
		case OBS_COMBO_FORMAT_BOOL:
			value = std::to_string(
				obs_property_list_item_bool(property, i));
			break;
		default:
			break;
		}

		if (value == currentValue) {
			return obs_property_list_item_name(property, i);
		}
	}

	return {};
}

void SetSourceSetting(obs_source_t *source, const SourceSetting &setting,
		      const std::string &value)
{
	auto id = setting.GetID();
	OBSDataAutoRelease data = obs_source_get_settings(source);
	OBSDataAutoRelease dataWithDefaults = obs_data_get_defaults(data);
	obs_data_apply(dataWithDefaults, data);
	auto item = obs_data_item_byname(dataWithDefaults, id.c_str());
	auto type = obs_data_item_gettype(item);
	switch (type) {
	case OBS_DATA_NULL:
		vblog(LOG_WARNING,
		      "could not set settings value, as the value does not (yet) exist!");
		break;
	case OBS_DATA_STRING:
		obs_data_set_string(data, id.c_str(), value.c_str());
		break;
	case OBS_DATA_NUMBER: {
		auto type = obs_data_item_numtype(item);
		switch (type) {
		case OBS_DATA_NUM_INVALID:
			break;
		case OBS_DATA_NUM_INT: {
			auto intValue = GetInt(value);
			if (intValue.has_value()) {
				obs_data_set_int(data, id.c_str(), *intValue);
				break;
			}

			auto doubleValue = GetDouble(value);
			if (doubleValue.has_value()) {
				obs_data_set_int(data, id.c_str(),
						 *doubleValue);
			}

			break;
		}
		case OBS_DATA_NUM_DOUBLE: {
			auto numValue = GetDouble(value);
			if (!numValue.has_value()) {
				break;
			}

			obs_data_set_double(data, id.c_str(), *numValue);
			break;
		}
		}
		break;
	}
	case OBS_DATA_BOOLEAN:
		obs_data_set_bool(data, id.c_str(), value == "true");
		break;
	case OBS_DATA_OBJECT: {
		OBSDataAutoRelease json =
			obs_data_create_from_json(value.c_str());
		obs_data_set_obj(data, id.c_str(), json);
		break;
	}
	case OBS_DATA_ARRAY: {
		auto jsonStr = obs_data_get_json(data);
		if (!jsonStr) {
			break;
		}
		std::string resultJsonStr;
		try {
			auto json = nlohmann::json::parse(jsonStr);
			auto jsonArray = nlohmann::json::parse(value);
			json[id] = jsonArray;
			resultJsonStr = json.dump();

		} catch (const nlohmann::json::exception &) {
			break;
		}
		OBSDataAutoRelease newData =
			obs_data_create_from_json(resultJsonStr.c_str());
		if (!newData) {
			break;
		}
		obs_data_clear(data);
		obs_data_apply(data, newData);
	}
	}
	obs_data_item_release(&item);
	obs_source_update(source, data);
}

void SetSourceSettingListEntryValueByName(obs_source_t *source,
					  const SourceSetting &setting,
					  const std::string &name)
{
	if (!setting.IsList()) {
		return;
	}

	const auto &[property, properties] =
		findListPropertyById(source, setting.GetID());
	if (!property) {
		return;
	}

	const auto type = obs_property_list_format(property);
	if (type == OBS_COMBO_FORMAT_INVALID) {
		return;
	}

	const auto count = obs_property_list_item_count(property);
	bool found = false;
	size_t idx = 0;
	for (; idx < count; idx++) {
		if (obs_property_list_item_name(property, idx) == name) {
			found = true;
			break;
		}
	}

	if (!found) {
		return;
	}

	std::string value;
	switch (type) {
	case OBS_COMBO_FORMAT_INT:
		value = std::to_string(
			obs_property_list_item_int(property, idx));
		break;
	case OBS_COMBO_FORMAT_FLOAT:
		value = std::to_string(
			obs_property_list_item_float(property, idx));
		break;
	case OBS_COMBO_FORMAT_STRING:
		value = obs_property_list_item_string(property, idx);
		break;
	case OBS_COMBO_FORMAT_BOOL:
		value = std::to_string(
			obs_property_list_item_bool(property, idx));
		break;
	default:
		break;
	}

	SetSourceSetting(source, setting, value);
}

SourceSettingSelection::SourceSettingSelection(QWidget *parent)
	: QWidget(parent),
	  _settings(new FilterComboBox(
		  this, obs_module_text("AdvSceneSwitcher.setting.select"))),
	  _tooltip(new HelpIcon())
{
	_tooltip->hide();

	_settings->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	_settings->setMaximumWidth(350);

	QWidget::connect(_settings, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(SelectionIdxChanged(int)));

	auto layout = new QHBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(_settings);
	layout->addWidget(_tooltip);
	setLayout(layout);
}

void SourceSettingSelection::SetSource(const OBSWeakSource &source,
				       bool restorePreviousSelection)
{
	const auto previousSelection = _settings->currentData();
	Populate(source);
	if (restorePreviousSelection) {
		_settings->setCurrentIndex(
			_settings->findData(previousSelection));
	}
}

void SourceSettingSelection::SetSelection(const OBSWeakSource &source,
					  const SourceSetting &setting)
{
	SetSource(source, false);
	QVariant variant;
	variant.setValue(setting);
	_settings->setCurrentIndex(_settings->findData(variant));
}

void SourceSettingSelection::SelectionIdxChanged(int idx)
{
	if (idx == -1) {
		return;
	}
	auto setting = _settings->itemData(idx).value<SourceSetting>();
	if (setting._longDescription.empty()) {
		_tooltip->setToolTip("");
		_tooltip->hide();
	} else {
		_tooltip->setToolTip(
			QString::fromStdString(setting._longDescription));
		_tooltip->show();
	}
	emit SelectionChanged(setting);
}

static std::vector<SourceSetting> getSourceSettings(obs_source_t *source)
{
	properties_t properties(obs_source_properties(source),
				obs_properties_destroy);
	if (!properties) {
		return {};
	}
	std::vector<SourceSetting> settings;

	auto it = obs_properties_first(properties.get());
	addSettingsHelper(it, settings);
	return settings;
}

void SourceSettingSelection::Populate(const OBSWeakSource &source)
{
	_settings->clear();
	OBSSourceAutoRelease s = obs_weak_source_get_source(source);
	const auto settings = getSourceSettings(s);
	for (const auto &setting : settings) {
		QVariant variant;
		variant.setValue(setting);
		_settings->addItem(QString::fromStdString(setting._description),
				   variant);
	}
	adjustSize();
	updateGeometry();
}

} // namespace advss
