#pragma once
#include "macro-action-edit.hpp"
#include "variable-spinbox.hpp"
#include "variable-line-edit.hpp"

#include <QComboBox>
#include <QHBoxLayout>
#include <chrono>

namespace advss {

class MacroActionStream : public MacroAction {
public:
	MacroActionStream(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m);
	std::shared_ptr<MacroAction> Copy() const;
	void ResolveVariablesToFixedValues();

	enum class Action {
		STOP,
		START,
		KEYFRAME_INTERVAL,
		SERVER,
		STREAM_KEY,
		USERNAME,
		PASSWORD,
	};
	Action _action = Action::STOP;
	IntVariable _keyFrameInterval = 0;
	StringVariable _stringValue;

private:
	void SetKeyFrameInterval() const;
	void SetStreamSettingsValue(const char *name, const std::string &value,
				    bool enableAuth = false) const;
	bool CooldownDurationReached() const;
	static std::chrono::high_resolution_clock::time_point s_lastAttempt;

	static bool _registered;
	static const std::string id;
};

class MacroActionStreamEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionStreamEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionStream> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionStreamEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionStream>(action));
	}

private slots:
	void ActionChanged(int value);
	void KeyFrameIntervalChanged(const NumberVariable<int> &);
	void StringValueChanged();
	void ShowPassword();
	void HidePassword();

protected:
	QComboBox *_actions;
	VariableSpinBox *_keyFrameInterval;
	VariableLineEdit *_stringValue;
	QPushButton *_showPassword;
	std::shared_ptr<MacroActionStream> _entryData;

private:
	void SetWidgetVisiblity();

	QHBoxLayout *_layout;
	bool _loading = true;
};

} // namespace advss
