#ifdef VCAM_SUPPORTED

#pragma once
#include "macro.hpp"
#include <QWidget>
#include <QComboBox>

enum class VCamState {
	STOP,
	START,
};

class MacroConditionVCam : public MacroCondition {
public:
	bool CheckCondition();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetId() { return id; };
	static std::shared_ptr<MacroCondition> Create()
	{
		return std::make_shared<MacroConditionVCam>();
	}

	VCamState _state;

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

#endif
