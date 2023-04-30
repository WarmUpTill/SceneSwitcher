#pragma once
#include "macro-condition-edit.hpp"
#include "variable-spinbox.hpp"

#include <QWidget>
#include <QComboBox>

namespace advss {

class MacroConditionStream : public MacroCondition {
public:
	MacroConditionStream(Macro *m) : MacroCondition(m) {}
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionStream>(m);
	}

	enum class Condition {
		STOP,
		START,
		STARTING,
		STOPPING,
		KEYFRAME_INTERVAL,
	};
	Condition _condition = Condition::STOP;
	NumberVariable<int> _keyFrameInterval = 0;

private:
	int GetKeyFrameInterval();

	std::chrono::high_resolution_clock::time_point _lastStreamStartingTime{};
	std::chrono::high_resolution_clock::time_point _lastStreamStoppingTime{};

	static bool _registered;
	static const std::string id;
};

class MacroConditionStreamEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionStreamEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionStream> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionStreamEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionStream>(cond));
	}

private slots:
	void StateChanged(int value);
	void KeyFrameIntervalChanged(const NumberVariable<int> &);

protected:
	QComboBox *_streamState;
	VariableSpinBox *_keyFrameInterval;
	std::shared_ptr<MacroConditionStream> _entryData;

private:
	void SetWidgetVisiblity();

	bool _loading = true;
};

} // namespace advss
