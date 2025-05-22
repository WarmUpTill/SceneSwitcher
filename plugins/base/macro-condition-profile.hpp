#pragma once
#include "macro-condition-edit.hpp"
#include "profile-helpers.hpp"

namespace advss {

class MacroConditionProfile : public MacroCondition {
public:
	MacroConditionProfile(Macro *m) : MacroCondition(m) {}
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionProfile>(m);
	}

	std::string _profile;

private:
	static bool _registered;
	static const std::string id;
};

class MacroConditionProfileEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionProfileEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionProfile> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionProfileEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionProfile>(cond));
	}

private slots:
	void ProfileChanged(const QString &text);
signals:
	void HeaderInfoChanged(const QString &);

private:
	ProfileSelectionWidget *_profiles;

	std::shared_ptr<MacroConditionProfile> _entryData;
	bool _loading = true;
};

} // namespace advss
