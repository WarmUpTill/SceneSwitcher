#include "source-interact.hpp"
#include "obs-module-helper.hpp"
#include "layout-helpers.hpp"

#include <QLayout>
#include <QLabel>

namespace advss {

void Position::Save(obs_data_t *obj, const char *name) const
{
	OBSDataAutoRelease data = obs_data_create();
	x.Save(data, "x");
	y.Save(data, "y");
	obs_data_set_obj(obj, name, data);
}

void Position::Load(obs_data_t *obj, const char *name)
{
	OBSDataAutoRelease data = obs_data_get_obj(obj, name);
	x.Load(data, "x");
	y.Load(data, "y");
}

void Modifier::Save(obs_data_t *obj, const char *name) const
{
	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_bool(data, "shift", _shift);
	obs_data_set_bool(data, "ctrl", _ctrl);
	obs_data_set_bool(data, "alt", _alt);
	obs_data_set_bool(data, "meta", _meta);
	obs_data_set_obj(obj, name, data);
}

void Modifier::Load(obs_data_t *obj, const char *name)
{
	OBSDataAutoRelease data = obs_data_get_obj(obj, name);
	_shift = obs_data_get_bool(data, "shift");
	_ctrl = obs_data_get_bool(data, "ctrl");
	_alt = obs_data_get_bool(data, "alt");
	_meta = obs_data_get_bool(data, "meta");
}

uint32_t Modifier::GetModifiers() const
{
	uint32_t modifiers = INTERACT_NONE;
	if (_shift) {
		modifiers |= INTERACT_SHIFT_KEY;
	}
	if (_ctrl) {
		modifiers |= INTERACT_CONTROL_KEY;
	}
	if (_alt) {
		modifiers |= INTERACT_ALT_KEY;
	}
	if (_alt) {
		modifiers |= INTERACT_COMMAND_KEY;
	}
	return modifiers;
}

SourceInteraction::SourceInteraction()
{
	_interaction = std::make_unique<MouseClick>();
}

SourceInteraction::SourceInteraction(const SourceInteraction &other)
{
	OBSDataAutoRelease data = obs_data_create();
	other.Save(data);
	Load(data);
}

void SourceInteraction::Save(obs_data_t *obj, const char *name) const
{
	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_int(data, "type", static_cast<int>(_type));
	_interaction->Save(data, "settings");
	obs_data_set_obj(obj, name, data);
}

void SourceInteraction::Load(obs_data_t *obj, const char *name)
{
	OBSDataAutoRelease data = obs_data_get_obj(obj, name);
	_type = static_cast<Type>(obs_data_get_int(data, "type"));
	UpdateType();
	_interaction->Load(data, "settings");
}

void SourceInteraction::SetSettings(SourceInteractionInstance *value)
{
	_interaction.reset(value);
}

void SourceInteraction::SetType(Type t)
{
	_type = t;
	UpdateType();
}

void SourceInteraction::SendToSource(obs_source_t *source) const
{
	_interaction->SendToSource(source);
}

void SourceInteraction::UpdateType()
{
	switch (_type) {
	case SourceInteraction::Type::CLICK:
		_interaction.reset(new MouseClick);
		break;
	case SourceInteraction::Type::MOVE:
		_interaction.reset(new MouseMove);
		break;
	case SourceInteraction::Type::WHEEL:
		_interaction.reset(new MouseWheel);
		break;
	case SourceInteraction::Type::KEY:
		_interaction.reset(new KeyPress);
		break;
	}
}

void MouseClick::Save(obs_data_t *obj, const char *name) const
{
	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_int(data, "button", static_cast<int>(_button));
	obs_data_set_bool(data, "up", _up);
	_pos.Save(data);
	_count.Save(data, "count");
	_modifier.Save(data);
	obs_data_set_obj(obj, name, data);
}

void MouseClick::Load(obs_data_t *obj, const char *name)
{
	OBSDataAutoRelease data = obs_data_get_obj(obj, name);
	_button = static_cast<Button>(obs_data_get_int(data, "button"));
	_up = obs_data_get_bool(data, "up");
	_pos.Load(data);
	_count.Load(data, "count");
	_modifier.Load(data);
}

void MouseClick::SendToSource(obs_source_t *source) const
{
	obs_mouse_event ev = {_modifier.GetModifiers(), _pos.x, _pos.y};
	obs_source_send_mouse_click(source, &ev, static_cast<int32_t>(_button),
				    _up, _count);
}

void MouseMove::Save(obs_data_t *obj, const char *name) const
{
	OBSDataAutoRelease data = obs_data_create();
	_pos.Save(data);
	_modifier.Save(data);
	obs_data_set_obj(obj, name, data);
}

void MouseMove::Load(obs_data_t *obj, const char *name)
{
	OBSDataAutoRelease data = obs_data_get_obj(obj, name);
	_pos.Load(data);
	_modifier.Load(data);
}

void MouseMove::SendToSource(obs_source_t *source) const
{
	obs_mouse_event ev = {_modifier.GetModifiers(), _pos.x, _pos.y};
	obs_source_send_mouse_move(source, &ev, _leaveSource);
}

void MouseWheel::Save(obs_data_t *obj, const char *name) const
{
	OBSDataAutoRelease data = obs_data_create();
	_pos.Save(data);
	_amount.Save(data, "amount");
	_pos.Save(data);
	_modifier.Save(data);
	obs_data_set_obj(obj, name, data);
}

void MouseWheel::Load(obs_data_t *obj, const char *name)
{
	OBSDataAutoRelease data = obs_data_get_obj(obj, name);
	_pos.Load(data);
	_amount.Load(data, "amount");
	_modifier.Load(data);
}

void MouseWheel::SendToSource(obs_source_t *source) const
{
	obs_mouse_event ev = {_modifier.GetModifiers(), _pos.x, _pos.y};
	obs_source_send_mouse_wheel(source, &ev, _amount.x, _amount.y);
}

void KeyPress::Save(obs_data_t *obj, const char *name) const
{
	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_bool(data, "up", _up);
	_modifier.Save(data);
	obs_data_set_obj(obj, name, data);
}

void KeyPress::Load(obs_data_t *obj, const char *name)
{
	OBSDataAutoRelease data = obs_data_get_obj(obj, name);
	_up = obs_data_get_bool(data, "up");
	_modifier.Load(data);
}

void KeyPress::SendToSource(obs_source_t *source) const
{
	char text = _text;
	obs_key_event ev = {_modifier.GetModifiers(), &text, 0, 0, 0};
	obs_source_send_key_click(source, &ev, _up);
}

PositionSelection::PositionSelection(int min, int max, QWidget *parent)
	: QWidget(parent),
	  _x(new VariableSpinBox),
	  _y(new VariableSpinBox)
{
	_x->setMinimum(min);
	_y->setMinimum(min);
	_x->setMaximum(max);
	_y->setMaximum(max);

	connect(_x, SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(XChanged(const NumberVariable<int> &)));
	connect(_y, SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(YChanged(const NumberVariable<int> &)));

	auto layout = new QHBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(_x);
	layout->setStretch(0, 3);
	auto label = new QLabel("x");
	label->setAlignment(Qt::AlignCenter);
	layout->addWidget(label);
	layout->addWidget(_y);
	layout->setStretch(2, 3);
	setLayout(layout);
}

void PositionSelection::SetPosition(const Position &pos)
{
	_x->SetValue(pos.x);
	_y->SetValue(pos.y);
}

Position PositionSelection::GetPosition() const
{
	return Position{_x->Value(), _y->Value()};
}

void PositionSelection::XChanged(const NumberVariable<int> &value)
{
	emit PositionChanged(Position{value, _y->Value()});
}

void PositionSelection::YChanged(const NumberVariable<int> &value)
{
	emit PositionChanged(Position{_x->Value(), value});
}

ModifierSelection::ModifierSelection(QWidget *parent)
	: QWidget(parent),
	  _shift(new QCheckBox(
		  obs_module_text("AdvSceneSwitcher.modifierKey.shift"))),
	  _ctrl(new QCheckBox(
		  obs_module_text("AdvSceneSwitcher.modifierKey.ctrl"))),
	  _alt(new QCheckBox(
		  obs_module_text("AdvSceneSwitcher.modifierKey.alt"))),
	  _meta(new QCheckBox(
		  obs_module_text("AdvSceneSwitcher.modifierKey.meta")))
{
	connect(_shift, SIGNAL(stateChanged(int)), this,
		SLOT(ShiftChanged(int)));
	connect(_ctrl, SIGNAL(stateChanged(int)), this, SLOT(CtrlChanged(int)));
	connect(_alt, SIGNAL(stateChanged(int)), this, SLOT(AltChanged(int)));
	connect(_meta, SIGNAL(stateChanged(int)), this, SLOT(MetaChanged(int)));

	auto layout = new QHBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(_shift);
	layout->addWidget(_ctrl);
	layout->addWidget(_alt);
	layout->addWidget(_meta);
	setLayout(layout);
}

void ModifierSelection::SetModifier(const Modifier &modifier)
{
	_shift->setChecked(modifier._shift);
	_ctrl->setChecked(modifier._ctrl);
	_alt->setChecked(modifier._alt);
	_meta->setChecked(modifier._meta);
}

Modifier ModifierSelection::GetCurrentModifier() const
{
	Modifier modifier;
	modifier.EnableShift(_shift->isChecked());
	modifier.EnableCtrl(_ctrl->isChecked());
	modifier.EnableAlt(_alt->isChecked());
	modifier.EnableMeta(_meta->isChecked());
	return modifier;
}

void ModifierSelection::ShiftChanged(int value)
{
	auto modifier = GetCurrentModifier();
	modifier.EnableShift(value);
	emit ModifierChanged(modifier);
}

void ModifierSelection::CtrlChanged(int value)
{
	auto modifier = GetCurrentModifier();
	modifier.EnableCtrl(value);
	emit ModifierChanged(modifier);
}

void ModifierSelection::AltChanged(int value)
{
	auto modifier = GetCurrentModifier();
	modifier.EnableAlt(value);
	emit ModifierChanged(modifier);
}

void ModifierSelection::MetaChanged(int value)
{
	auto modifier = GetCurrentModifier();
	modifier.EnableMeta(value);
	emit ModifierChanged(modifier);
}

static inline void populateInteractionTypeSelection(QComboBox *list)
{
	list->addItem(
		obs_module_text(
			"AdvSceneSwitcher.action.source.type.interact.mouseClick"),
		(int)SourceInteraction::Type::CLICK);
	list->addItem(
		obs_module_text(
			"AdvSceneSwitcher.action.source.type.interact.mouseMove"),
		(int)SourceInteraction::Type::MOVE);
	list->addItem(
		obs_module_text(
			"AdvSceneSwitcher.action.source.type.interact.mouseWheel"),
		(int)SourceInteraction::Type::WHEEL);
	list->addItem(
		obs_module_text(
			"AdvSceneSwitcher.action.source.type.interact.keyPress"),
		(int)SourceInteraction::Type::KEY);
}

SourceInteractionWidget::SourceInteractionWidget(QWidget *parent)
	: QWidget(parent),
	  _type(new QComboBox),
	  _settingsLayout(new QHBoxLayout)
{
	populateInteractionTypeSelection(_type);

	connect(_type, SIGNAL(currentIndexChanged(int)), this,
		SLOT(TypeChange(int)));
	auto typeLayout = new QHBoxLayout();
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.action.source.type.interact.selectType"),
		typeLayout, {{"{{type}}", _type}});

	SetupSettingsWidget(SourceInteraction::Type::CLICK);

	auto layout = new QVBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addLayout(typeLayout);
	layout->addLayout(_settingsLayout);
	setLayout(layout);
}

void SourceInteractionWidget::SetupSettingsWidget(SourceInteraction::Type type)
{
	_type->setCurrentIndex(_type->findData(static_cast<int>(type)));
	ClearLayout(_settingsLayout);
	switch (type) {
	case SourceInteraction::Type::CLICK:
		SetupMouseClick();
		break;
	case SourceInteraction::Type::MOVE:
		SetupMouseMove();
		break;
	case SourceInteraction::Type::WHEEL:
		SetupMouseWheel();
		break;
	case SourceInteraction::Type::KEY:
		SetupKeyPress();
		break;
	}
	_settingsLayout->update();
	adjustSize();
	updateGeometry();
}

void SourceInteractionWidget::SetupMouseClick()
{
	auto widget = new MouseClickSettings();
	connect(widget, SIGNAL(MouseClickSettingsChanged(const MouseClick &)),
		this, SLOT(MouseClickSettingsChanged(const MouseClick &)));
	_settingsLayout->addWidget(widget);
}

void SourceInteractionWidget::SetupMouseMove()
{
	auto widget = new MouseMoveSettings();
	connect(widget, SIGNAL(MouseMoveSettingsChanged(const MouseMove &)),
		this, SLOT(MouseMoveSettingsChanged(const MouseMove &)));
	_settingsLayout->addWidget(widget);
}

void SourceInteractionWidget::SetupMouseWheel()
{
	auto widget = new MouseWheelSettings();
	connect(widget, SIGNAL(MouseWheelSettingsChanged(const MouseWheel &)),
		this, SLOT(MouseWheelSettingsChanged(const MouseWheel &)));
	_settingsLayout->addWidget(widget);
}

void SourceInteractionWidget::SetupKeyPress()
{
	auto widget = new KeyPressSettings();
	connect(widget, SIGNAL(KeyPressSettingsChanged(const KeyPress &)), this,
		SLOT(KeyPressSettingsChanged(const KeyPress &)));
	_settingsLayout->addWidget(widget);
}

void SourceInteractionWidget::TypeChange(int idx)
{
	auto type = static_cast<SourceInteraction::Type>(
		_type->itemData(idx).toInt());
	SetupSettingsWidget(type);
	emit TypeChanged(type);
}

void SourceInteractionWidget::SetSourceInteractionSelection(
	const SourceInteraction &interaction)
{
	SetupSettingsWidget(interaction._type);

	auto item = _settingsLayout->itemAt(0);
	if (!item) {
		return;
	}
	auto widget = item->widget();
	if (!widget) {
		return;
	}

	switch (interaction._type) {
	case SourceInteraction::Type::CLICK: {
		auto mouseClickWidget =
			dynamic_cast<MouseClickSettings *>(widget);
		if (!mouseClickWidget) {
			return;
		}
		auto mouseClickSettings = dynamic_cast<MouseClick *>(
			interaction._interaction.get());
		if (!mouseClickSettings) {
			return;
		}
		mouseClickWidget->SetMouseClickSettings(*mouseClickSettings);
	}
		return;
	case SourceInteraction::Type::MOVE: {

		auto mouseMoveWidget =
			dynamic_cast<MouseMoveSettings *>(widget);
		if (!mouseMoveWidget) {
			return;
		}
		auto mouseMoveSettings = dynamic_cast<MouseMove *>(
			interaction._interaction.get());
		if (!mouseMoveSettings) {
			return;
		}
		mouseMoveWidget->SetMouseMoveSettings(*mouseMoveSettings);
	}
		return;
	case SourceInteraction::Type::WHEEL: {
		auto mouseWheelWidget =
			dynamic_cast<MouseWheelSettings *>(widget);
		if (!mouseWheelWidget) {
			return;
		}
		auto mouseWheelSettings = dynamic_cast<MouseWheel *>(
			interaction._interaction.get());
		if (!mouseWheelSettings) {
			return;
		}
		mouseWheelWidget->SetMouseWheelSettings(*mouseWheelSettings);
	}
		return;
	case SourceInteraction::Type::KEY: {
		auto KeyPressWidget = dynamic_cast<KeyPressSettings *>(widget);
		if (!KeyPressWidget) {
			return;
		}
		auto KeyPressSettings = dynamic_cast<KeyPress *>(
			interaction._interaction.get());
		if (!KeyPressSettings) {
			return;
		}
		KeyPressWidget->SetKeyPressSettings(*KeyPressSettings);
	}
		return;
	}
	adjustSize();
	updateGeometry();
}

void SourceInteractionWidget::MouseClickSettingsChanged(const MouseClick &value)
{
	auto click = new MouseClick();
	*click = value;
	emit SettingsChanged(click);
}

void SourceInteractionWidget::MouseMoveSettingsChanged(const MouseMove &value)
{
	auto move = new MouseMove();
	*move = value;
	emit SettingsChanged(move);
}

void SourceInteractionWidget::MouseWheelSettingsChanged(const MouseWheel &value)
{
	auto wheel = new MouseWheel();
	*wheel = value;
	emit SettingsChanged(wheel);
}

void SourceInteractionWidget::KeyPressSettingsChanged(const KeyPress &value)
{
	auto key = new KeyPress();
	*key = value;
	emit SettingsChanged(key);
}

static inline void populateMouseButtonSelection(QComboBox *list)
{
	list->addItem(obs_module_text("AdvSceneSwitcher.mouse.button.left"));
	list->addItem(obs_module_text("AdvSceneSwitcher.mouse.button.middle"));
	list->addItem(obs_module_text("AdvSceneSwitcher.mouse.button.right"));
}

MouseClickSettings::MouseClickSettings(QWidget *parent)
	: QWidget(parent),
	  _button(new QComboBox),
	  _position(new PositionSelection(0, 999999)),
	  _up(new QCheckBox),
	  _count(new VariableSpinBox),
	  _modifier(new ModifierSelection)
{
	_count->setMinimum(1);
	_count->setMaximum(99);
	populateMouseButtonSelection(_button);

	connect(_button, SIGNAL(currentIndexChanged(int)), this,
		SLOT(ButtonChanged(int)));
	connect(_position, SIGNAL(PositionChanged(const Position &)), this,
		SLOT(PositionChanged(const Position &)));
	connect(_up, SIGNAL(stateChanged(int)), this, SLOT(UpChanged(int)));
	connect(_count,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(CountChanged(const NumberVariable<int> &)));
	connect(_modifier, SIGNAL(ModifierChanged(const Modifier &)), this,
		SLOT(ModifierChanged(const Modifier &)));

	auto layout = new QGridLayout();
	layout->setColumnStretch(1, 3);
	layout->setContentsMargins(0, 0, 0, 0);
	int row = 0;
	layout->addWidget(
		new QLabel(obs_module_text("AdvSceneSwitcher.mouse.button")),
		row, 0);
	layout->addWidget(_button, row, 1);
	++row;
	layout->addWidget(
		new QLabel(obs_module_text("AdvSceneSwitcher.mouse.position")),
		row, 0);
	layout->addWidget(_position, row, 1);
	++row;
	layout->addWidget(
		new QLabel(obs_module_text("AdvSceneSwitcher.mouse.up")), row,
		0);
	layout->addWidget(_up, row, 1);
	++row;
	layout->addWidget(
		new QLabel(obs_module_text("AdvSceneSwitcher.mouse.count")),
		row, 0);
	layout->addWidget(_count, row, 1);
	++row;
	layout->addWidget(
		new QLabel(obs_module_text("AdvSceneSwitcher.modifierKey")),
		row, 0);
	layout->addWidget(_modifier, row, 1);
	++row;
	setLayout(layout);
}

void MouseClickSettings::SetMouseClickSettings(const MouseClick &click)
{
	_button->setCurrentIndex(static_cast<int>(click._button));
	_position->SetPosition(click._pos);
	_up->setChecked(click._up);
	_count->SetValue(click._count);
	_modifier->SetModifier(click._modifier);
}

void MouseClickSettings::ButtonChanged(int button)
{
	auto click = GetCurrentMouseClick();
	click.SetButton(static_cast<MouseClick::Button>(button));
	emit MouseClickSettingsChanged(click);
}

void MouseClickSettings::PositionChanged(const Position &pos)
{
	auto click = GetCurrentMouseClick();
	click.SetPosition(pos);
	emit MouseClickSettingsChanged(click);
}

void MouseClickSettings::UpChanged(int value)
{
	auto click = GetCurrentMouseClick();
	click.SetDirection(value);
	emit MouseClickSettingsChanged(click);
}

void MouseClickSettings::CountChanged(const IntVariable &value)
{
	auto click = GetCurrentMouseClick();
	click.SetCount(value);
	emit MouseClickSettingsChanged(click);
}

void MouseClickSettings::ModifierChanged(const Modifier &modifier)
{
	auto click = GetCurrentMouseClick();
	click.SetModifier(modifier);
	emit MouseClickSettingsChanged(click);
}

MouseClick MouseClickSettings::GetCurrentMouseClick() const
{
	MouseClick click;

	click.SetButton(
		static_cast<MouseClick::Button>(_button->currentIndex()));
	click.SetPosition(_position->GetPosition());
	click.SetDirection(_up->isChecked());
	click.SetCount(_count->Value());
	click.SetModifier(_modifier->GetModifier());
	return click;
}

MouseMoveSettings::MouseMoveSettings(QWidget *parent)
	: QWidget(parent),
	  _position(new PositionSelection(0, 999999)),
	  _leaveEvent(new QCheckBox),
	  _modifier(new ModifierSelection)
{
	connect(_position, SIGNAL(PositionChanged(const Position &)), this,
		SLOT(PositionChanged(const Position &)));
	connect(_leaveEvent, SIGNAL(stateChanged(int)), this,
		SLOT(LeaveChanged(int)));
	connect(_modifier, SIGNAL(ModifierChanged(const Modifier &)), this,
		SLOT(ModifierChanged(const Modifier &)));

	auto layout = new QGridLayout();
	layout->setColumnStretch(1, 3);
	layout->setContentsMargins(0, 0, 0, 0);
	int row = 0;
	layout->addWidget(
		new QLabel(obs_module_text("AdvSceneSwitcher.mouse.position")),
		row, 0);
	layout->addWidget(_position, row, 1);
	++row;
	layout->addWidget(
		new QLabel(obs_module_text("AdvSceneSwitcher.mouse.leave")),
		row, 0);
	layout->addWidget(_leaveEvent, row, 1);
	++row;
	layout->addWidget(
		new QLabel(obs_module_text("AdvSceneSwitcher.modifierKey")),
		row, 0);
	layout->addWidget(_modifier, row, 1);
	++row;
	setLayout(layout);
}

void MouseMoveSettings::SetMouseMoveSettings(const MouseMove &move)
{
	_position->SetPosition(move._pos);
	_leaveEvent->setChecked(move._leaveSource);
	_modifier->SetModifier(move._modifier);
}

void MouseMoveSettings::PositionChanged(const Position &pos)
{
	auto move = GetCurrentMouseMove();
	move.SetPosition(pos);
	emit MouseMoveSettingsChanged(move);
}

void MouseMoveSettings::LeaveChanged(int value)
{
	auto move = GetCurrentMouseMove();
	move.SetLeave(value);
	emit MouseMoveSettingsChanged(move);
}

void MouseMoveSettings::ModifierChanged(const Modifier &modifier)
{
	auto move = GetCurrentMouseMove();
	move.SetModifier(modifier);
	emit MouseMoveSettingsChanged(move);
}

MouseMove MouseMoveSettings::GetCurrentMouseMove() const
{
	MouseMove move;
	move.SetPosition(_position->GetPosition());
	move.SetLeave(_leaveEvent->isChecked());
	move.SetModifier(_modifier->GetModifier());
	return move;
}

MouseWheelSettings::MouseWheelSettings(QWidget *parent)
	: QWidget(parent),
	  _position(new PositionSelection(0, 999999)),
	  _amount(new PositionSelection(-999999, 999999)),
	  _modifier(new ModifierSelection)
{
	connect(_position, SIGNAL(PositionChanged(const Position &)), this,
		SLOT(PositionChanged(const Position &)));
	connect(_amount, SIGNAL(PositionChanged(const Position &)), this,
		SLOT(AmountChanged(const Position &)));
	connect(_modifier, SIGNAL(ModifierChanged(const Modifier &)), this,
		SLOT(ModifierChanged(const Modifier &)));

	auto layout = new QGridLayout();
	layout->setColumnStretch(1, 3);
	layout->setContentsMargins(0, 0, 0, 0);
	int row = 0;
	layout->addWidget(
		new QLabel(obs_module_text("AdvSceneSwitcher.mouse.position")),
		row, 0);
	layout->addWidget(_position, row, 1);
	++row;
	layout->addWidget(new QLabel(obs_module_text(
				  "AdvSceneSwitcher.mouse.scrollAmount")),
			  row, 0);
	layout->addWidget(_amount, row, 1);
	++row;
	layout->addWidget(
		new QLabel(obs_module_text("AdvSceneSwitcher.modifierKey")),
		row, 0);
	layout->addWidget(_modifier, row, 1);
	++row;
	setLayout(layout);
}

void MouseWheelSettings::SetMouseWheelSettings(const MouseWheel &wheel)
{
	_position->SetPosition(wheel._pos);
	_position->SetPosition(wheel._amount);
	_modifier->SetModifier(wheel._modifier);
}

void MouseWheelSettings::PositionChanged(const Position &pos)
{
	auto wheel = GetCurrentMouseWheel();
	wheel.SetPosition(pos);
	emit MouseWheelSettingsChanged(wheel);
}

void MouseWheelSettings::AmountChanged(const Position &amount)
{
	auto wheel = GetCurrentMouseWheel();
	wheel.SetAmount(amount);
	emit MouseWheelSettingsChanged(wheel);
}

void MouseWheelSettings::ModifierChanged(const Modifier &modifier)
{
	auto move = GetCurrentMouseWheel();
	move.SetModifier(modifier);
	emit MouseWheelSettingsChanged(move);
}

MouseWheel MouseWheelSettings::GetCurrentMouseWheel() const
{
	MouseWheel wheel;
	wheel.SetPosition(_position->GetPosition());
	wheel.SetAmount(_amount->GetPosition());
	wheel.SetModifier(_modifier->GetModifier());
	return wheel;
}

KeyPressSettings::KeyPressSettings(QWidget *parent)
	: QWidget(parent),
	  _text(new QLineEdit()),
	  _up(new QCheckBox()),
	  _modifier(new ModifierSelection)
{
	_text->setMaxLength(1);

	connect(_text, SIGNAL(editingFinished()), this, SLOT(TextChanged()));
	connect(_up, SIGNAL(stateChanged(int)), this, SLOT(UpChanged(int)));
	connect(_modifier, SIGNAL(ModifierChanged(const Modifier &)), this,
		SLOT(ModifierChanged(const Modifier &)));

	auto layout = new QGridLayout();
	layout->setColumnStretch(1, 3);
	layout->setContentsMargins(0, 0, 0, 0);
	int row = 0;
	layout->addWidget(new QLabel(obs_module_text("AdvSceneSwitcher.key")),
			  row, 0);
	layout->addWidget(_text, row, 1);
	++row;
	layout->addWidget(
		new QLabel(obs_module_text("AdvSceneSwitcher.key.up")), row, 0);
	layout->addWidget(_up, row, 1);
	++row;
	layout->addWidget(
		new QLabel(obs_module_text("AdvSceneSwitcher.modifierKey")),
		row, 0);
	layout->addWidget(_modifier, row, 1);
	++row;
	setLayout(layout);
}

void KeyPressSettings::SetKeyPressSettings(const KeyPress &key)
{
	_text->setText(QString(key._text));
	_up->setChecked(key._up);
	_modifier->SetModifier(key._modifier);
}

void KeyPressSettings::TextChanged()
{
	auto key = GetCurrentKeyPress();
	key.SetKey(_text->text().at(0).toLatin1());
	emit KeyPressSettingsChanged(key);
}

void KeyPressSettings::UpChanged(int value)
{
	auto key = GetCurrentKeyPress();
	key.SetDirection(value);
	emit KeyPressSettingsChanged(key);
}

void KeyPressSettings::ModifierChanged(const Modifier &modifier)
{
	auto move = GetCurrentKeyPress();
	move.SetModifier(modifier);
	emit KeyPressSettingsChanged(move);
}

KeyPress KeyPressSettings::GetCurrentKeyPress() const
{
	KeyPress key;
	key.SetDirection(_up->isChecked());
	key.SetModifier(_modifier->GetModifier());
	return key;
}

} // namespace advss
