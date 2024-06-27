#pragma once
#include "filter-combo-box.hpp"

#include <obs.hpp>
#include <optional>
#include <string>
#include <vector>
#include <QWidget>
#include <QLabel>

namespace advss {

class TransformSetting {
public:
	TransformSetting() = default;
	TransformSetting(const std::string &id, const std::string &nestedId);
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetID() const { return _id; }
	std::string GetNestedID() const { return _nestedId; }
	EXPORT bool operator==(const TransformSetting &other) const;

private:
	std::string _id = "";
	std::string _nestedId = "";
	friend class TransformSettingSelection;
};

std::vector<TransformSetting>
GetSoruceTransformSettings(obs_scene_item *source);
std::optional<std::string>
GetTransformSettingValue(obs_scene_item *source,
			 const TransformSetting &setting);
void SetTransformSetting(obs_scene_item *source,
			 const TransformSetting &setting,
			 const std::string &value);

class TransformSettingSelection : public QWidget {
	Q_OBJECT

public:
	TransformSettingSelection(QWidget *parent = nullptr);
	void SetSource(obs_scene_item *);
	void SetSetting(const TransformSetting &);

private slots:
	void SelectionIdxChanged(int);

signals:
	void SelectionChanged(const TransformSetting &);

private:
	void Populate(obs_scene_item *);

	FilterComboBox *_settings;
};

} // namespace advss
