#pragma once
#include "filter-combo-box.hpp"

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
	SourceSetting(const std::string &id, const std::string &description,
		      const std::string &longDescription = "");
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetID() const { return _id; }
	EXPORT bool operator==(const SourceSetting &other) const;

private:
	std::string _id = "";
	std::string _description = "";
	std::string _longDescription = "";

	friend class SourceSettingSelection;
};

std::vector<SourceSetting> GetSoruceSettings(obs_source_t *source);
std::optional<std::string> GetSourceSettingValue(const OBSWeakSource &source,
						 const SourceSetting &setting);
void SetSourceSetting(obs_source_t *source, const SourceSetting &setting,
		      const std::string &value);

class SourceSettingSelection : public QWidget {
	Q_OBJECT

public:
	SourceSettingSelection(QWidget *parent = nullptr);
	void SetSource(const OBSWeakSource &);
	void SetSetting(const SourceSetting &);

private slots:
	void SelectionIdxChanged(int);

signals:
	void SelectionChanged(const SourceSetting &);

private:
	void Populate(const OBSWeakSource &);

	FilterComboBox *_settings;
	QLabel *_tooltip;
};

} // namespace advss
