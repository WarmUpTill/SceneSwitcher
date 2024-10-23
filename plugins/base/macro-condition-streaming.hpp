#pragma once
#include "macro-condition-edit.hpp"
#include "variable-spinbox.hpp"
#include "regex-config.hpp"
#include "variable-string.hpp"
#include "variable-line-edit.hpp"

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
		STREAM_KEY,
		SERVICE,
	};
	Condition _condition = Condition::STOP;
	NumberVariable<int> _keyFrameInterval = 0;
	StringVariable _streamKey;
	StringVariable _serviceName = "";
	RegexConfig _regex;

private:
	void SetupTempVars();

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
	void ConditionChanged(int value);
	void KeyFrameIntervalChanged(const NumberVariable<int> &);
	void StreamKeyChanged();
	void ServiceNameChanged();
	void RegexChanged(const RegexConfig &);

private:
	void SetWidgetVisibility();

	QComboBox *_conditions;
	VariableSpinBox *_keyFrameInterval;
	VariableLineEdit *_streamKey;
	VariableLineEdit *_serviceName;
	QLabel *_currentService;
	RegexConfigWidget *_regex;

	std::shared_ptr<MacroConditionStream> _entryData;
	bool _loading = true;
};

} // namespace advss
