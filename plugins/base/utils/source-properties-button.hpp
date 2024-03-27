#pragma once
#include <obs.hpp>
#include <QComboBox>
#include <qmetatype.h>
#include <string>
#include <vector>

namespace advss {

struct SourceSettingButton {
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string ToString() const;

	std::string id = "";
	std::string description = "";
};

std::vector<SourceSettingButton> GetSourceButtons(OBSWeakSource source);
void PressSourceButton(const SourceSettingButton &button, obs_source_t *source);
void PopulateSourceButtonSelection(QComboBox *list, OBSWeakSource source);

} // namespace advss

Q_DECLARE_METATYPE(advss::SourceSettingButton);
