#include "macro-condition-cursor.hpp"
#include "cursor-helpers.hpp"
#include "layout-helpers.hpp"
#include "utility.hpp"

#include <QGuiApplication>
#include <QScreen>

namespace advss {

const std::string MacroConditionCursor::id = "cursor";

bool MacroConditionCursor::_registered = MacroConditionFactory::Register(
	MacroConditionCursor::id,
	{MacroConditionCursor::Create, MacroConditionCursorEdit::Create,
	 "AdvSceneSwitcher.condition.cursor"});

const static std::map<MacroConditionCursor::Condition, std::string>
	cursorConditionTypes = {
		{MacroConditionCursor::Condition::REGION,
		 "AdvSceneSwitcher.condition.cursor.type.region"},
		{MacroConditionCursor::Condition::MOVING,
		 "AdvSceneSwitcher.condition.cursor.type.moving"},
#ifdef _WIN32 // Not implemented for MacOS and Linux
		{MacroConditionCursor::Condition::CLICK,
		 "AdvSceneSwitcher.condition.cursor.type.click"},
#endif
};

const static std::map<MacroConditionCursor::Button, std::string> buttonTypes = {
	{MacroConditionCursor::Button::LEFT,
	 "AdvSceneSwitcher.condition.cursor.button.left"},
	{MacroConditionCursor::Button::MIDDLE,
	 "AdvSceneSwitcher.condition.cursor.button.middle"},
	{MacroConditionCursor::Button::RIGHT,
	 "AdvSceneSwitcher.condition.cursor.button.right"},
};

bool MacroConditionCursor::CheckClick()
{
	switch (_button) {
	case MacroConditionCursor::Button::LEFT:
		return _lastCheckTime < GetLastMouseLeftClickTime();
	case MacroConditionCursor::Button::MIDDLE:
		return _lastCheckTime < GetLastMouseMiddleClickTime();
	case MacroConditionCursor::Button::RIGHT:
		return _lastCheckTime < GetLastMouseRightClickTime();
	}
	return false;
}

bool MacroConditionCursor::CheckCondition()
{
	bool ret = false;
	const auto &[x, y] = GetCursorPos();
	SetTempVarValue("x", std::to_string(x));
	SetTempVarValue("y", std::to_string(y));

	switch (_condition) {
	case Condition::REGION:
		ret = x >= _minX && y >= _minY && x <= _maxX && y <= _maxY;
		SetVariableValue(std::to_string(x) + " " + std::to_string(y));
		break;
	case Condition::MOVING:
		ret = x != _lastX || y != _lastY;
		break;
	case Condition::CLICK:
		ret = CheckClick();
		break;
	}

	_lastCheckTime = std::chrono::high_resolution_clock::now();
	_lastX = x;
	_lastY = y;

	if (GetVariableValue().empty()) {
		SetVariableValue(ret ? "true" : "false");
	}
	return ret;
}

bool MacroConditionCursor::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	obs_data_set_int(obj, "condition", static_cast<int>(_condition));
	obs_data_set_int(obj, "button", static_cast<int>(_button));
	_minX.Save(obj, "minX");
	_minY.Save(obj, "minY");
	_maxX.Save(obj, "maxX");
	_maxY.Save(obj, "maxY");
	obs_data_set_int(obj, "version", 1);
	return true;
}

bool MacroConditionCursor::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_condition = static_cast<Condition>(obs_data_get_int(obj, "condition"));
	_button = static_cast<Button>(obs_data_get_int(obj, "button"));
	if (!obs_data_has_user_value(obj, "version")) {
		_minX = obs_data_get_int(obj, "minX");
		_minY = obs_data_get_int(obj, "minY");
		_maxX = obs_data_get_int(obj, "maxX");
		_maxY = obs_data_get_int(obj, "maxY");
	} else {
		_minX.Load(obj, "minX");
		_minY.Load(obj, "minY");
		_maxX.Load(obj, "maxX");
		_maxY.Load(obj, "maxY");
	}
	return true;
}

void MacroConditionCursor::SetupTempVars()
{
	AddTempvar("x", obs_module_text("AdvSceneSwitcher.tempVar.cursor.x"));
	AddTempvar("y", obs_module_text("AdvSceneSwitcher.tempVar.cursor.y"));
}

static inline void populateConditionSelection(QComboBox *list)
{
	for (const auto &[condition, name] : cursorConditionTypes) {
		list->addItem(obs_module_text(name.c_str()),
			      static_cast<int>(condition));
	}
}

static inline void populateButtonSelection(QComboBox *list)
{
	for (const auto &[button, name] : buttonTypes) {
		list->addItem(obs_module_text(name.c_str()),
			      static_cast<int>(button));
	}
}

MacroConditionCursorEdit::MacroConditionCursorEdit(
	QWidget *parent, std::shared_ptr<MacroConditionCursor> entryData)
	: QWidget(parent),
	  _minX(new VariableSpinBox()),
	  _minY(new VariableSpinBox()),
	  _maxX(new VariableSpinBox()),
	  _maxY(new VariableSpinBox()),
	  _conditions(new QComboBox()),
	  _buttons(new QComboBox()),
	  _frameToggle(new QPushButton(obs_module_text(
		  "AdvSceneSwitcher.condition.cursor.showFrame"))),
	  _xPos(new QLabel("-")),
	  _yPos(new QLabel("-")),
	  _curentPosLayout(new QHBoxLayout())
{
	_frame.setAttribute(Qt::WA_TransparentForMouseEvents);

	populateConditionSelection(_conditions);
	populateButtonSelection(_buttons);

	_minX->setPrefix("Min X: ");
	_minY->setPrefix("Min Y: ");
	_maxX->setPrefix("Max X: ");
	_maxY->setPrefix("Max Y: ");

	_minX->setMinimum(-1000000);
	_minY->setMinimum(-1000000);
	_maxX->setMinimum(-1000000);
	_maxY->setMinimum(-1000000);

	_minX->setMaximum(1000000);
	_minY->setMaximum(1000000);
	_maxX->setMaximum(1000000);
	_maxY->setMaximum(1000000);

	QWidget::connect(_conditions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(_buttons, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ButtonChanged(int)));
	QWidget::connect(
		_minX,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(MinXChanged(const NumberVariable<int> &)));
	QWidget::connect(
		_minY,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(MinYChanged(const NumberVariable<int> &)));
	QWidget::connect(
		_maxX,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(MaxXChanged(const NumberVariable<int> &)));
	QWidget::connect(
		_maxY,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(MaxYChanged(const NumberVariable<int> &)));
	QWidget::connect(_frameToggle, SIGNAL(clicked()), this,
			 SLOT(ToggleFrame()));

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{conditions}}", _conditions},
		{"{{buttons}}", _buttons},
		{"{{minX}}", _minX},
		{"{{minY}}", _minY},
		{"{{maxX}}", _maxX},
		{"{{maxY}}", _maxY},
		{"{{xPos}}", _xPos},
		{"{{yPos}}", _yPos},
		{"{{toggleFrameButton}}", _frameToggle},
	};
	QHBoxLayout *line1 = new QHBoxLayout;
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.cursor.entry.line1"),
		     line1, widgetPlaceholders);
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.cursor.entry.line2"),
		     _curentPosLayout, widgetPlaceholders);
	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(line1);
	mainLayout->addLayout(_curentPosLayout);
	setLayout(mainLayout);

	connect(&_timer, &QTimer::timeout, this,
		&MacroConditionCursorEdit::UpdateCursorPos);
	_timer.start(1000);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionCursorEdit::ConditionChanged(int index)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_condition = static_cast<MacroConditionCursor::Condition>(
		_conditions->itemData(index).toInt());
	SetWidgetVisibility();
}

void MacroConditionCursorEdit::ButtonChanged(int index)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_button = static_cast<MacroConditionCursor::Button>(
		_buttons->itemData(index).toInt());
}

void MacroConditionCursorEdit::MinXChanged(const NumberVariable<int> &pos)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_minX = pos;
	SetupFrame();
}

void MacroConditionCursorEdit::MinYChanged(const NumberVariable<int> &pos)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_minY = pos;
	SetupFrame();
}

void MacroConditionCursorEdit::MaxXChanged(const NumberVariable<int> &pos)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_maxX = pos;
	SetupFrame();
}

void MacroConditionCursorEdit::MaxYChanged(const NumberVariable<int> &pos)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_maxY = pos;
	SetupFrame();
}

void MacroConditionCursorEdit::UpdateCursorPos()
{
	std::pair<int, int> position = GetCursorPos();
	_xPos->setText(QString::number(position.first));
	_yPos->setText(QString::number(position.second));
}

void MacroConditionCursorEdit::ToggleFrame()
{
	SetupFrame();
	if (_frame.isVisible()) {
		_frameToggle->setText(obs_module_text(
			"AdvSceneSwitcher.condition.cursor.showFrame"));
		_frame.hide();
	} else {
		_frameToggle->setText(obs_module_text(
			"AdvSceneSwitcher.condition.cursor.hideFrame"));
		_frame.show();
	}
}

void MacroConditionCursorEdit::SetWidgetVisibility()
{
	const bool isRegionCondition = _entryData->_condition ==
				       MacroConditionCursor::Condition::REGION;
	_minX->setVisible(isRegionCondition);
	_minY->setVisible(isRegionCondition);
	_maxX->setVisible(isRegionCondition);
	_maxY->setVisible(isRegionCondition);
	_frameToggle->setVisible(isRegionCondition);
	SetLayoutVisible(_curentPosLayout, isRegionCondition);
	if (_frame.isVisible()) {
		ToggleFrame();
	}

	_buttons->setVisible(_entryData->_condition ==
			     MacroConditionCursor::Condition::CLICK);

	adjustSize();
}

static QRect getScreenUnionRect()
{
	QPoint screenTopLeft(0, 0);
	QPoint screenBottomRight(0, 0);
	auto screens = QGuiApplication::screens();
	for (const auto &screen : screens) {
		const auto geo = screen->geometry();
		if (geo.topLeft().x() < screenTopLeft.x()) {
			screenTopLeft.setX(geo.topLeft().x());
		}
		if (geo.topLeft().y() < screenTopLeft.y()) {
			screenTopLeft.setY(geo.topLeft().y());
		}
		if (geo.bottomRight().x() > screenBottomRight.x()) {
			screenBottomRight.setX(geo.bottomRight().x());
		}
		if (geo.bottomRight().y() > screenBottomRight.y()) {
			screenBottomRight.setY(geo.bottomRight().y());
		}
	}
	return QRect(screenTopLeft, screenBottomRight);
}

void MacroConditionCursorEdit::SetupFrame()
{
	_frame.setFrameStyle(QFrame::Box | QFrame::Plain);
	_frame.setWindowFlags(Qt::FramelessWindowHint | Qt::Tool |
			      Qt::WindowTransparentForInput |
			      Qt::WindowDoesNotAcceptFocus |
			      Qt::WindowStaysOnTopHint);
	_frame.setAttribute(Qt::WA_TranslucentBackground, true);

	if (!_entryData) {
		return;
	}

	QRect rect(_entryData->_minX, _entryData->_minY,
		   _entryData->_maxX - _entryData->_minX,
		   _entryData->_maxY - _entryData->_minY);

	if (rect.size().height() <= 0 || rect.size().width() <= 0) {
		_frame.setGeometry(0, 0, 1, 1);
		return;
	}

	_frame.setGeometry(getScreenUnionRect().intersected(rect));
}

void MacroConditionCursorEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_conditions->setCurrentIndex(static_cast<int>(_entryData->_condition));
	_buttons->setCurrentIndex(static_cast<int>(_entryData->_button));
	_minX->SetValue(_entryData->_minX);
	_minY->SetValue(_entryData->_minY);
	_maxX->SetValue(_entryData->_maxX);
	_maxY->SetValue(_entryData->_maxY);
	SetWidgetVisibility();
}

} // namespace advss
