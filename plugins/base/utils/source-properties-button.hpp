#pragma once
#include "filter-combo-box.hpp"

#include <obs.hpp>
#include <qmetatype.h>
#include <string>
#include <vector>

namespace advss {

class SourceSelection;
class FilterSelection;

struct SourceSettingButton {
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string ToString() const;

	std::string id = "";
	std::string description = "";
};

std::vector<SourceSettingButton> GetSourceButtons(OBSWeakSource source);
void PressSourceButton(const SourceSettingButton &button, obs_source_t *source);

class SourceSettingsButtonSelection : public FilterComboBox {
	Q_OBJECT

public:
	SourceSettingsButtonSelection(QWidget *parent = nullptr);
	void SetSource(const OBSWeakSource &source,
		       bool restorePreviousSelection = true);
	void SetSelection(const OBSWeakSource &source,
			  const SourceSettingButton &button);

signals:
	void SelectionChanged(const SourceSettingButton &);

private:
	void PopulateSelection(const OBSWeakSource &source);
};

} // namespace advss

Q_DECLARE_METATYPE(advss::SourceSettingButton);
