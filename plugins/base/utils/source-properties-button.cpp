#include "source-properties-button.hpp"
#include "log-helper.hpp"
#include "obs-module-helper.hpp"

namespace advss {

bool SourceSettingButton::Save(obs_data_t *obj) const
{
	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_string(data, "id", id.c_str());
	obs_data_set_string(data, "description", description.c_str());
	obs_data_set_obj(obj, "sourceSettingButton", data);
	return true;
}

bool SourceSettingButton::Load(obs_data_t *obj)
{
	OBSDataAutoRelease data = obs_data_get_obj(obj, "sourceSettingButton");
	id = obs_data_get_string(data, "id");
	description = obs_data_get_string(data, "description");
	return true;
}

std::string SourceSettingButton::ToString() const
{
	if (id.empty()) {
		return "";
	}
	return "[" + id + "] " + description;
}

std::vector<SourceSettingButton> GetSourceButtons(OBSWeakSource source)
{
	OBSSourceAutoRelease s = obs_weak_source_get_source(source);
	std::vector<SourceSettingButton> buttons;
	obs_properties_t *sourceProperties = obs_source_properties(s);
	auto it = obs_properties_first(sourceProperties);
	do {
		if (!it || obs_property_get_type(it) != OBS_PROPERTY_BUTTON) {
			continue;
		}
		SourceSettingButton button = {obs_property_name(it),
					      obs_property_description(it)};
		buttons.emplace_back(button);
	} while (obs_property_next(&it));
	obs_properties_destroy(sourceProperties);
	return buttons;
}

void PressSourceButton(const SourceSettingButton &button, obs_source_t *source)
{
	obs_properties_t *sourceProperties = obs_source_properties(source);
	obs_property_t *property =
		obs_properties_get(sourceProperties, button.id.c_str());
	if (!obs_property_button_clicked(property, source)) {
		blog(LOG_WARNING, "Failed to press settings button '%s' for %s",
		     button.id.c_str(), obs_source_get_name(source));
	}
	obs_properties_destroy(sourceProperties);
}

void PopulateSourceButtonSelection(QComboBox *list, OBSWeakSource source)
{
	list->clear();
	auto buttons = GetSourceButtons(source);
	if (buttons.empty()) {
		list->addItem(
			obs_module_text("AdvSceneSwitcher.noSettingsButtons"));
	}

	for (const auto &button : buttons) {
		QVariant value;
		value.setValue(button);
		list->addItem(QString::fromStdString(button.ToString()), value);
	}
}

} // namespace advss
