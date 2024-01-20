#include "source-setting.hpp"
#include "obs-module-helper.hpp"
#include "math-helpers.hpp"
#include "ui-helpers.hpp"
#include "utility.hpp"

#include <nlohmann/json.hpp>
#include <QLayout>

Q_DECLARE_METATYPE(advss::SourceSetting);

namespace advss {

SourceSetting::SourceSetting(const std::string &id,
			     const std::string &description,
			     const std::string &longDescription)
	: _id(id), _description(description), _longDescription(longDescription)
{
}

bool SourceSetting::Save(obs_data_t *obj) const
{
	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_string(data, "id", _id.c_str());
	obs_data_set_string(data, "description", _description.c_str());
	obs_data_set_obj(obj, "sourceSetting", data);
	return true;
}

bool SourceSetting::Load(obs_data_t *obj)
{
	OBSDataAutoRelease data = obs_data_get_obj(obj, "sourceSetting");
	_id = obs_data_get_string(data, "id");
	_description = obs_data_get_string(data, "description");
	return true;
}

bool SourceSetting::operator==(const SourceSetting &other) const
{
	return _id == other._id;
}

std::vector<SourceSetting> GetSoruceSettings(obs_source_t *source)
{
	auto properties = obs_source_properties(source);
	if (!properties) {
		return {};
	}
	std::vector<SourceSetting> settings;
	auto it = obs_properties_first(properties);
	do {
		if (!it) {
			continue;
		}
		auto name = obs_property_name(it);
		if (!name) {
			continue;
		}
		auto description = obs_property_description(it);
		if (!description) {
			continue;
		}
		auto longDescription = obs_property_long_description(it);
		SourceSetting setting(name, description,
				      longDescription ? longDescription : "");
		settings.emplace_back(setting);
	} while (obs_property_next(&it));
	obs_properties_destroy(properties);
	return settings;
}

std::string GetSourceSettingValue(const OBSWeakSource &ws,
				  const SourceSetting &setting)
{
	OBSSourceAutoRelease source = obs_weak_source_get_source(ws);
	OBSDataAutoRelease data = obs_source_get_settings(source);
	if (!data) {
		return "";
	}
	OBSDataAutoRelease dataWithDefaults = obs_data_get_defaults(data);
	obs_data_apply(dataWithDefaults, data);
	auto json = obs_data_get_json(dataWithDefaults);
	if (!json) {
		return "";
	}
	auto value = GetJsonField(json, setting.GetID());
	return value.value_or("");
}

void SetSourceSetting(obs_source_t *source, const SourceSetting &setting,
		      const std::string &value)
{
	auto id = setting.GetID();
	OBSDataAutoRelease data = obs_source_get_settings(source);
	auto item = obs_data_item_byname(data, id.c_str());
	auto type = obs_data_item_gettype(item);
	switch (type) {
	case OBS_DATA_NULL:
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
			obs_data_set_int(data, id.c_str(), *numValue);
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

SourceSettingSelection::SourceSettingSelection(QWidget *parent)
	: QWidget(parent),
	  _settings(new FilterComboBox(
		  this, obs_module_text("AdvSceneSwitcher.selectSetting"))),
	  _tooltip(new QLabel())
{
	QString path = GetThemeTypeName() == "Light"
			       ? ":/res/images/help.svg"
			       : ":/res/images/help_light.svg";
	QIcon icon(path);
	QPixmap pixmap = icon.pixmap(QSize(16, 16));
	_tooltip->setPixmap(pixmap);
	_tooltip->hide();

	QWidget::connect(_settings, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(SelectionIdxChanged(int)));

	auto layout = new QHBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(_settings);
	layout->addWidget(_tooltip);
	setLayout(layout);
}

void SourceSettingSelection::SetSource(const OBSWeakSource &source)
{
	_settings->clear();
	Populate(source);
}

void SourceSettingSelection::SetSetting(const SourceSetting &setting)
{
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

void SourceSettingSelection::Populate(const OBSWeakSource &source)
{
	OBSSourceAutoRelease s = obs_weak_source_get_source(source);
	auto settings = GetSoruceSettings(s);
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
