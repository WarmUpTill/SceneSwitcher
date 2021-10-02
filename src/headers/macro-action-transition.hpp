#pragma once
#include "macro-action-edit.hpp"
#include "duration-control.hpp"
#include "transition-selection.hpp"

#include <QCheckBox>
#include <QHBoxLayout>

class MacroActionTransition : public MacroAction {
public:
	bool PerformAction();
	void LogAction();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetShortDesc();
	std::string GetId() { return id; };
	static std::shared_ptr<MacroAction> Create()
	{
		return std::make_shared<MacroActionTransition>();
	}

	bool _setDuration = true;
	bool _setType = true;
	TransitionSelection _transition;
	Duration _duration;

private:
	static bool _registered;
	static const std::string id;
};

class MacroActionTransitionEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionTransitionEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionTransition> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionTransitionEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionTransition>(
				action));
	}

private slots:
	void SetTypeChanged(int state);
	void SetDurationChanged(int state);
	void TransitionChanged(const TransitionSelection &);
	void DurationChanged(double seconds);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	QCheckBox *_setType;
	QCheckBox *_setDuration;
	TransitionSelectionWidget *_transitions;
	DurationSelection *_duration;
	QHBoxLayout *_typeLayout;
	QHBoxLayout *_durationLayout;
	std::shared_ptr<MacroActionTransition> _entryData;

private:
	bool _loading = true;
};
