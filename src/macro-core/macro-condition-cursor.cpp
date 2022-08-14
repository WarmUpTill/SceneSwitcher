#include "macro-condition-edit.hpp"
#include "macro-condition-cursor.hpp"
#include "utility.hpp"
#include "advanced-scene-switcher.hpp"

const std::string MacroConditionCursor::id = "cursor";

bool MacroConditionCursor::_registered = MacroConditionFactory::Register(
	MacroConditionCursor::id,
	{MacroConditionCursor::Create, MacroConditionCursorEdit::Create,
	 "AdvSceneSwitcher.condition.cursor"});

static std::map<CursorCondition, std::string> cursorConditionTypes = {
	{CursorCondition::REGION,
	 "AdvSceneSwitcher.condition.cursor.type.region"},
	{CursorCondition::MOVING,
	 "AdvSceneSwitcher.condition.cursor.type.moving"},
};

bool MacroConditionCursor::CheckCondition()
{
	std::pair<int, int> cursorPos = getCursorPos();
	switch (_condition) {
	case CursorCondition::REGION:
		return cursorPos.first >= _minX && cursorPos.second >= _minY &&
		       cursorPos.first <= _maxX && cursorPos.second <= _maxY;
	case CursorCondition::MOVING:
		return switcher->cursorPosChanged;
	default:
		break;
	}
	return false;
}

bool MacroConditionCursor::Save(obs_data_t *obj)
{
	MacroCondition::Save(obj);
	obs_data_set_int(obj, "condition", static_cast<int>(_condition));
	obs_data_set_int(obj, "minX", _minX);
	obs_data_set_int(obj, "minY", _minY);
	obs_data_set_int(obj, "maxX", _maxX);
	obs_data_set_int(obj, "maxY", _maxY);
	return true;
}

bool MacroConditionCursor::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_condition = static_cast<CursorCondition>(
		obs_data_get_int(obj, "condition"));
	_minX = obs_data_get_int(obj, "minX");
	_minY = obs_data_get_int(obj, "minY");
	_maxX = obs_data_get_int(obj, "maxX");
	_maxY = obs_data_get_int(obj, "maxY");
	return true;
}

static inline void populateConditionSelection(QComboBox *list)
{
	for (auto entry : cursorConditionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroConditionCursorEdit::MacroConditionCursorEdit(
	QWidget *parent, std::shared_ptr<MacroConditionCursor> entryData)
	: QWidget(parent),
	  _minX(new QSpinBox()),
	  _minY(new QSpinBox()),
	  _maxX(new QSpinBox()),
	  _maxY(new QSpinBox()),
	  _conditions(new QComboBox()),
	  _frameToggle(new QPushButton(obs_module_text(
		  "AdvSceneSwitcher.condition.cursor.showFrame"))),
	  _xPos(new QLabel("-")),
	  _yPos(new QLabel("-"))
{
	_frame.setAttribute(Qt::WA_TransparentForMouseEvents);

	populateConditionSelection(_conditions);

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
	QWidget::connect(_minX, SIGNAL(valueChanged(int)), this,
			 SLOT(MinXChanged(int)));
	QWidget::connect(_minY, SIGNAL(valueChanged(int)), this,
			 SLOT(MinYChanged(int)));
	QWidget::connect(_maxX, SIGNAL(valueChanged(int)), this,
			 SLOT(MaxXChanged(int)));
	QWidget::connect(_maxY, SIGNAL(valueChanged(int)), this,
			 SLOT(MaxYChanged(int)));
	QWidget::connect(_frameToggle, SIGNAL(clicked()), this,
			 SLOT(ToggleFrame()));

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{conditions}}", _conditions},
		{"{{minX}}", _minX},
		{"{{minY}}", _minY},
		{"{{maxX}}", _maxX},
		{"{{maxY}}", _maxY},
		{"{{xPos}}", _xPos},
		{"{{yPos}}", _yPos},
		{"{{toggleFrameButton}}", _frameToggle},
	};
	QHBoxLayout *line1 = new QHBoxLayout;
	placeWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.cursor.entry.line1"),
		     line1, widgetPlaceholders);
	QHBoxLayout *line2 = new QHBoxLayout;
	placeWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.cursor.entry.line2"),
		     line2, widgetPlaceholders);
	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(line1);
	mainLayout->addLayout(line2);
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

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_condition = static_cast<CursorCondition>(index);
	SetRegionSelectionVisible(_entryData->_condition ==
				  CursorCondition::REGION);
}

void MacroConditionCursorEdit::MinXChanged(int pos)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_minX = pos;
	SetupFrame();
}

void MacroConditionCursorEdit::MinYChanged(int pos)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_minY = pos;
	SetupFrame();
}

void MacroConditionCursorEdit::MaxXChanged(int pos)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_maxX = pos;
	SetupFrame();
}

void MacroConditionCursorEdit::MaxYChanged(int pos)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_maxY = pos;
	SetupFrame();
}

void MacroConditionCursorEdit::UpdateCursorPos()
{
	std::pair<int, int> position = getCursorPos();
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

void MacroConditionCursorEdit::SetRegionSelectionVisible(bool visible)
{
	_minX->setVisible(visible);
	_minY->setVisible(visible);
	_maxX->setVisible(visible);
	_maxY->setVisible(visible);
	_frameToggle->setVisible(visible);
	if (_frame.isVisible()) {
		ToggleFrame();
	}

	adjustSize();
}

void MacroConditionCursorEdit::SetupFrame()
{
	_frame.setFrameStyle(QFrame::Box | QFrame::Plain);
	_frame.setWindowFlags(Qt::FramelessWindowHint | Qt::Tool |
			      Qt::WindowTransparentForInput |
			      Qt::WindowDoesNotAcceptFocus |
			      Qt::WindowStaysOnTopHint);
	_frame.setAttribute(Qt::WA_TranslucentBackground, true);

	if (_entryData) {
		_frame.setGeometry(_entryData->_minX, _entryData->_minY,
				   _entryData->_maxX - _entryData->_minX,
				   _entryData->_maxY - _entryData->_minY);
	}
}

void MacroConditionCursorEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_conditions->setCurrentIndex(static_cast<int>(_entryData->_condition));
	_minX->setValue(_entryData->_minX);
	_minY->setValue(_entryData->_minY);
	_maxX->setValue(_entryData->_maxX);
	_maxY->setValue(_entryData->_maxY);
	SetRegionSelectionVisible(_entryData->_condition ==
				  CursorCondition::REGION);
}
