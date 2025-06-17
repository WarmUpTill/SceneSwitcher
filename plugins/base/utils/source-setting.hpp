#pragma once
#include "filter-combo-box.hpp"
#include "help-icon.hpp"

#include <obs.hpp>
#include <optional>
#include <string>
#include <vector>
#include <QWidget>
#include <QLabel>

namespace advss {

class SourceSetting {
public:
	SourceSetting() = default;
	SourceSetting(const std::string &id, obs_property_type type,
		      const std::string &description,
		      const std::string &longDescription = "");
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetID() const { return _id; }
	bool IsList() const;
	bool operator==(const SourceSetting &other) const;

private:
	std::string _id = "";
	obs_property_type _type = OBS_PROPERTY_INVALID;
	std::string _description = "";
	std::string _longDescription = "";

	friend class SourceSettingSelection;
};

std::vector<SourceSetting> GetSoruceSettings(obs_source_t *source);
std::optional<std::string> GetSourceSettingValue(const OBSWeakSource &source,
						 const SourceSetting &setting);
std::optional<std::string>
GetSourceSettingListEntryName(const OBSWeakSource &source,
			      const SourceSetting &setting);
void SetSourceSetting(obs_source_t *source, const SourceSetting &setting,
		      const std::string &value);
void SetSourceSettingListEntryValueByName(obs_source_t *source,
					  const SourceSetting &setting,
					  const std::string &name);

class SourceSettingSelection : public QWidget {
	Q_OBJECT

public:
	SourceSettingSelection(QWidget *parent = nullptr);
	void SetSource(const OBSWeakSource &,
		       bool restorePreviousSelection = true);
	void SetSelection(const OBSWeakSource &source, const SourceSetting &);

private slots:
	void SelectionIdxChanged(int);

signals:
	void SelectionChanged(const SourceSetting &);

private:
	void Populate(const OBSWeakSource &);

	FilterComboBox *_settings;
	HelpIcon *_tooltip;
};

} // namespace advss
