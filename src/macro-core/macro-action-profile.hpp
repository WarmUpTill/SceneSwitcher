#pragma once
#include "macro-action-edit.hpp"

#include <QComboBox>

class MacroActionProfile : public MacroAction {
public:
	MacroActionProfile(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m)
	{
		return std::make_shared<MacroActionProfile>(m);
	}

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

protected:
	QComboBox *_profiles;
	std::shared_ptr<MacroActionProfile> _entryData;

private:
	bool _loading = true;
};
