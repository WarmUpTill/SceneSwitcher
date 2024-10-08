#pragma once
#include "macro-action-edit.hpp"
#include "duration-control.hpp"

#include <QHBoxLayout>

namespace advss {

class MacroActionWait : public MacroAction {
public:
	MacroActionWait(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m);
	std::shared_ptr<MacroAction> Copy() const;
	void ResolveVariablesToFixedValues();

	Duration _duration;
	Duration _duration2;

	enum class Type {
		FIXED,
		RANDOM,
	};
	Type _waitType = Type::FIXED;

private:
	static bool _registered;
	static const std::string id;
};

class MacroActionWaitEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionWaitEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionWait> entryData = nullptr);
	void UpdateEntryData();
	void SetupFixedDurationEdit();
	void SetupRandomDurationEdit();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionWaitEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionWait>(action));
	}

private slots:
	void DurationChanged(const Duration &value);
	void Duration2Changed(const Duration &value);
	void TypeChanged(int value);

signals:
	void HeaderInfoChanged(const QString &);

private:
	DurationSelection *_duration;
	DurationSelection *_duration2;
	QComboBox *_waitType;
	QHBoxLayout *_mainLayout;

	std::shared_ptr<MacroActionWait> _entryData;
	bool _loading = true;
};

} // namespace advss
