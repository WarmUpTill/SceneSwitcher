#pragma once
#include "macro-action-edit.hpp"
#include "profile-helpers.hpp"

namespace advss {

class MacroActionProfile : public MacroAction {
public:
	MacroActionProfile(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m);
	std::shared_ptr<MacroAction> Copy() const;

	std::string _profile;

private:
	static bool _registered;
	static const std::string id;
};

class MacroActionProfileEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionProfileEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionProfile> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionProfileEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionProfile>(action));
	}

private slots:
	void ProfileChanged(const QString &text);
signals:
	void HeaderInfoChanged(const QString &);

private:
	ProfileSelectionWidget *_profiles;

	std::shared_ptr<MacroActionProfile> _entryData;
	bool _loading = true;
};

} // namespace advss
