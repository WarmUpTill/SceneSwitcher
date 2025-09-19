#pragma once
#include "macro-action-edit.hpp"
#include "variable-spinbox.hpp"
#include "variable-line-edit.hpp"

#include <QComboBox>
#include <QHBoxLayout>
#include <chrono>

namespace advss {

class MacroActionOBSSettings : public MacroAction {
public:
	MacroActionOBSSettings(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m);
	std::shared_ptr<MacroAction> Copy() const;
	void ResolveVariablesToFixedValues();

	enum class Action {
		FPS_TYPE,
		FPS_COMMON_VALUE,
		FPS_INT_VALUE,
		FPS_NUM_VALUE,
		FPS_DEN_VALUE,
		BASE_CANVAS_X_VALUE,
		BASE_CANVAS_Y_VALUE,
		OUTPUT_X_VALUE,
		OUTPUT_Y_VALUE,
		ENABLE_PREVIEW,
		DISABLE_PREVIEW,
		TOGGLE_PREVIEW,
	};
	Action _action = Action::FPS_COMMON_VALUE;
	enum class FPSType {
		COMMON,
		INTEGER,
		FRACTION,
	};
	FPSType _fpsType = FPSType::INTEGER;
	IntVariable _fpsIntValue = 30;
	StringVariable _fpsStringValue = "24 NTSC";
	IntVariable _canvasSizeValue = 1920;

private:
	static bool _registered;
	static const std::string id;
};

class MacroActionOBSSettingsEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionOBSSettingsEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionOBSSettings> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionOBSSettingsEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionOBSSettings>(
				action));
	}

private slots:
	void ActionChanged(int);
	void FPSTypeChanged(int);
	void FPSIntValueChanged(const NumberVariable<int> &);
	void FPSStringValueChanged();
	void CanvasSizeValueChanged(const NumberVariable<int> &);
	void GetCurrentValueClicked();

private:
	void SetWidgetVisibility();

	QComboBox *_actions;
	QComboBox *_fpsType;
	VariableSpinBox *_fpsIntValue;
	VariableLineEdit *_fpsStringValue;
	VariableSpinBox *_canvasSizeValue;
	QPushButton *_getCurrentValue;

	std::shared_ptr<MacroActionOBSSettings> _entryData;
	bool _loading = true;
};

} // namespace advss
