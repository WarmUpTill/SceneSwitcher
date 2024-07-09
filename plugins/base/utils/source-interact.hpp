#pragma once
#include "variable-number.hpp"
#include "variable-spinbox.hpp"

#include <chrono>
#include <memory>
#include <obs.hpp>
#include <QWidget>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QLayout>

namespace advss {

struct Position {
	void Save(obs_data_t *obj, const char *name = "position") const;
	void Load(obs_data_t *obj, const char *name = "position");
	IntVariable x = 0;
	IntVariable y = 0;
};

class Modifier {
public:
	void Save(obs_data_t *obj, const char *name = "modifier") const;
	void Load(obs_data_t *obj, const char *name = "modifier");

	void EnableShift(bool value) { _shift = value; }
	void EnableCtrl(bool value) { _ctrl = value; }
	void EnableAlt(bool value) { _alt = value; }
	void EnableMeta(bool value) { _meta = value; }
	uint32_t GetModifiers() const;

private:
	bool _shift = false;
	bool _ctrl = false;
	bool _alt = false;
	bool _meta = false;

	friend class ModifierSelection;
};

class SourceInteractionInstance;

class SourceInteraction {
public:
	SourceInteraction();
	SourceInteraction(const SourceInteraction &);
	void Save(obs_data_t *obj, const char *name = "interaction") const;
	void Load(obs_data_t *obj, const char *name = "interaction");

	enum class Type {
		CLICK,
		MOVE,
		WHEEL,
		KEY,
	};
	void SetType(Type);
	void SetSettings(SourceInteractionInstance *);
	void SendToSource(obs_source_t *source) const;

private:
	void UpdateType();

	Type _type = Type::CLICK;
	std::unique_ptr<SourceInteractionInstance> _interaction;
	friend class SourceInteractionWidget;
};

class SourceInteractionInstance {
public:
	virtual void Save(obs_data_t *obj, const char *name) const = 0;
	virtual void Load(obs_data_t *obj, const char *name) = 0;

	void SetModifier(Modifier modifier) { _modifier = modifier; }
	virtual void SendToSource(obs_source_t *source) const = 0;

	virtual ~SourceInteractionInstance(){};

protected:
	Modifier _modifier;
};

/* Interaction types below */

class MouseClick : public SourceInteractionInstance {
public:
	void Save(obs_data_t *obj,
		  const char *name = "mouseClick") const override;
	void Load(obs_data_t *obj, const char *name = "mouseClick") override;

	enum class Button {
		LEFT,
		MIDDLE,
		RIGHT,
	};

	void SetButton(Button button) { _button = button; }
	void SetDirection(bool up) { _up = up; }
	void SetCount(const IntVariable &count) { _count = count; }
	void SetPosition(const Position &pos) { _pos = pos; }

	void SendToSource(obs_source_t *source) const override;

private:
	Button _button = Button::LEFT;
	Position _pos;
	bool _up = false;
	IntVariable _count = 1;

	friend class MouseClickSettings;
};

class MouseMove : public SourceInteractionInstance {
public:
	void Save(obs_data_t *obj,
		  const char *name = "mouseMove") const override;
	void Load(obs_data_t *obj, const char *name = "mouseMove") override;

	void SetLeave(bool leave) { _leaveSource = leave; }
	void SetPosition(const Position &pos) { _pos = pos; }

	void SendToSource(obs_source_t *source) const override;

private:
	Position _pos;
	bool _leaveSource = false;

	friend class MouseMoveSettings;
};

class MouseWheel : public SourceInteractionInstance {
public:
	void Save(obs_data_t *obj,
		  const char *name = "mouseWheel") const override;
	void Load(obs_data_t *obj, const char *name = "mouseWheel") override;

	void SetPosition(const Position &pos) { _pos = pos; }
	void SetAmount(const Position &amount) { _amount = amount; }

	void SendToSource(obs_source_t *source) const override;

private:
	Position _pos;
	Position _amount;

	friend class MouseWheelSettings;
};

class KeyPress : public SourceInteractionInstance {
public:
	void Save(obs_data_t *obj,
		  const char *name = "keyPress") const override;
	void Load(obs_data_t *obj, const char *name = "keyPress") override;

	void SetDirection(bool up) { _up = up; }
	void SetKey(char value) { _text = value; }

	void SendToSource(obs_source_t *source) const override;

private:
	char _text;
	bool _up = false;

	friend class KeyPressSettings;
};

/* Helper widgets below */

class PositionSelection : public QWidget {
	Q_OBJECT

public:
	PositionSelection(int min, int max, QWidget *parent = 0);
	void SetPosition(const Position &);
	Position GetPosition() const;

private slots:
	void XChanged(const NumberVariable<int> &);
	void YChanged(const NumberVariable<int> &);
signals:
	void PositionChanged(const Position &value);

private:
	VariableSpinBox *_x;
	VariableSpinBox *_y;
};

class ModifierSelection : public QWidget {
	Q_OBJECT

public:
	ModifierSelection(QWidget *parent = 0);
	void SetModifier(const Modifier &);
	Modifier GetModifier() const { return GetCurrentModifier(); }

private slots:
	void ShiftChanged(int);
	void CtrlChanged(int);
	void AltChanged(int);
	void MetaChanged(int);
signals:
	void ModifierChanged(const Modifier &value);

private:
	Modifier GetCurrentModifier() const;

	QCheckBox *_shift;
	QCheckBox *_ctrl;
	QCheckBox *_alt;
	QCheckBox *_meta;
};

/* Settings widgets below */

class SourceInteractionWidget : public QWidget {
	Q_OBJECT

public:
	SourceInteractionWidget(QWidget *parent = 0);
	void SetSourceInteractionSelection(const SourceInteraction &);

private slots:
	void TypeChange(int);
	void MouseClickSettingsChanged(const MouseClick &);
	void MouseMoveSettingsChanged(const MouseMove &);
	void MouseWheelSettingsChanged(const MouseWheel &);
	void KeyPressSettingsChanged(const KeyPress &);
signals:
	void TypeChanged(SourceInteraction::Type value);
	void SettingsChanged(SourceInteractionInstance *value);

private:
	void SetupSettingsWidget(SourceInteraction::Type);
	void SetupMouseClick();
	void SetupMouseMove();
	void SetupMouseWheel();
	void SetupKeyPress();

	QComboBox *_type;
	QHBoxLayout *_settingsLayout;
};

class MouseClickSettings : public QWidget {
	Q_OBJECT

public:
	MouseClickSettings(QWidget *parent = 0);
	void SetMouseClickSettings(const MouseClick &);

private slots:
	void ButtonChanged(int);
	void PositionChanged(const Position &);
	void UpChanged(int);
	void CountChanged(const NumberVariable<int> &);
	void ModifierChanged(const Modifier &);
signals:
	void MouseClickSettingsChanged(const MouseClick &value);

private:
	MouseClick GetCurrentMouseClick() const;

	QComboBox *_button;
	PositionSelection *_position;
	QCheckBox *_up;
	VariableSpinBox *_count;
	ModifierSelection *_modifier;
};

class MouseMoveSettings : public QWidget {
	Q_OBJECT

public:
	MouseMoveSettings(QWidget *parent = 0);
	void SetMouseMoveSettings(const MouseMove &);

private slots:
	void PositionChanged(const Position &);
	void LeaveChanged(int);
	void ModifierChanged(const Modifier &);
signals:
	void MouseMoveSettingsChanged(const MouseMove &value);

private:
	MouseMove GetCurrentMouseMove() const;

	PositionSelection *_position;
	QCheckBox *_leaveEvent;
	ModifierSelection *_modifier;
};

class MouseWheelSettings : public QWidget {
	Q_OBJECT

public:
	MouseWheelSettings(QWidget *parent = 0);
	void SetMouseWheelSettings(const MouseWheel &);

private slots:
	void PositionChanged(const Position &);
	void AmountChanged(const Position &);
	void ModifierChanged(const Modifier &);
signals:
	void MouseWheelSettingsChanged(const MouseWheel &value);

private:
	MouseWheel GetCurrentMouseWheel() const;

	PositionSelection *_position;
	PositionSelection *_amount;
	ModifierSelection *_modifier;
};

class KeyPressSettings : public QWidget {
	Q_OBJECT

public:
	KeyPressSettings(QWidget *parent = 0);
	void SetKeyPressSettings(const KeyPress &);

private slots:
	void TextChanged();
	void UpChanged(int);
	void ModifierChanged(const Modifier &);
signals:
	void KeyPressSettingsChanged(const KeyPress &value);

private:
	KeyPress GetCurrentKeyPress() const;

	QLineEdit *_text;
	QCheckBox *_up;
	ModifierSelection *_modifier;
};

} // namespace advss
