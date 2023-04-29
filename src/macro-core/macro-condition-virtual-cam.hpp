#pragma once
#include "macro-condition-edit.hpp"

#include <QWidget>
#include <QComboBox>

namespace advss {

enum class VCamState {
	STOP,
	START,
};

class MacroConditionVCam : public MacroCondition {
public:
	MacroConditionVCam(Macro *m) : MacroCondition(m) {}
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionVCam>(m);
	}

	VCamState _state = VCamState::STOP;

private:
	static bool _registered;
	static const std::string id;
};

class MacroConditionVCamEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionVCamEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionVCam> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionVCamEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionVCam>(cond));
	}

private slots:
	void StateChanged(int value);

protected:
	QComboBox *_states;
	std::shared_ptr<MacroConditionVCam> _entryData;

private:
	bool _loading = true;
};

} // namespace advss
