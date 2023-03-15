#pragma once
#include "macro-action-edit.hpp"
#include "hotkey.hpp"
#include "duration-control.hpp"

#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QHBoxLayout>

class MacroActionHotkey : public MacroAction {
public:
	MacroActionHotkey(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m)
	{
		return std::make_shared<MacroActionHotkey>(m);
	}

	OBSWeakSource _HotkeySource;
	HotkeyType _key = HotkeyType::Key_NoKey;
	bool _leftShift = false;
	bool _rightShift = false;
	bool _leftCtrl = false;
	bool _rightCtrl = false;
	bool _leftAlt = false;
	bool _rightAlt = false;
	bool _leftMeta = false;
	bool _rightMeta = false;
	Duration _duration = 0.3;
#ifdef __APPLE__
	bool _onlySendToObs = true;
#else
	bool _onlySendToObs = false;
#endif

private:
	static bool _registered;
	static const std::string id;
};

class MacroActionHotkeyEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionHotkeyEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionHotkey> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionHotkeyEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionHotkey>(action));
	}

private slots:
	void KeyChanged(int key);
	void LShiftChanged(int state);
	void RShiftChanged(int state);
	void LCtrlChanged(int state);
	void RCtrlChanged(int state);
	void LAltChanged(int state);
	void RAltChanged(int state);
	void LMetaChanged(int state);
	void RMetaChanged(int state);
	void DurationChanged(const Duration &);
	void OnlySendToOBSChanged(int state);

protected:
	QComboBox *_keys;
	QCheckBox *_leftShift;
	QCheckBox *_rightShift;
	QCheckBox *_leftCtrl;
	QCheckBox *_rightCtrl;
	QCheckBox *_leftAlt;
	QCheckBox *_rightAlt;
	QCheckBox *_leftMeta;
	QCheckBox *_rightMeta;
	DurationSelection *_duration;
	QCheckBox *_onlySendToOBS;
	QLabel *_noKeyPressSimulationWarning;

	std::shared_ptr<MacroActionHotkey> _entryData;

private:
	void SetWarningVisibility();
	QHBoxLayout *_mainLayout;
	bool _loading = true;
};
