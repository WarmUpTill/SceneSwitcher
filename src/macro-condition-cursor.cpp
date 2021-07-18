#include "headers/macro-condition-edit.hpp"
#include "headers/macro-condition-cursor.hpp"
#include "headers/utility.hpp"
#include "headers/advanced-scene-switcher.hpp"

const std::string MacroConditionCursor::id = "cursor";

bool MacroConditionCursor::_registered = MacroConditionFactory::Register(
	MacroConditionCursor::id,
	{MacroConditionCursor::Create, MacroConditionCursorEdit::Create,
	 "AdvSceneSwitcher.condition.cursor"});

// TODO: Remove in future version - just added for backwards compatibility
static const std::string idOld = "region";
static bool oldRegisterd = MacroConditionFactory::Register(
	idOld, {MacroConditionCursor::Create, MacroConditionCursorEdit::Create,
		"AdvSceneSwitcher.condition.region"});

bool MacroConditionCursor::CheckCondition()
{
	std::pair<int, int> cursorPos = getCursorPos();
	return cursorPos.first >= _minX && cursorPos.second >= _minY &&
	       cursorPos.first <= _maxX && cursorPos.second <= _maxY;
}

bool MacroConditionCursor::Save(obs_data_t *obj)
{
	MacroCondition::Save(obj);
	obs_data_set_int(obj, "minX", _minX);
	obs_data_set_int(obj, "minY", _minY);
	obs_data_set_int(obj, "maxX", _maxX);
	obs_data_set_int(obj, "maxY", _maxY);
	return true;
}

bool MacroConditionCursor::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_minX = obs_data_get_int(obj, "minX");
	_minY = obs_data_get_int(obj, "minY");
	_maxX = obs_data_get_int(obj, "maxX");
	_maxY = obs_data_get_int(obj, "maxY");
	return true;
}

MacroConditionCursorEdit::MacroConditionCursorEdit(
	QWidget *parent, std::shared_ptr<MacroConditionCursor> entryData)
	: QWidget(parent)
{
	_minX = new QSpinBox();
	_minY = new QSpinBox();
	_maxX = new QSpinBox();
	_maxY = new QSpinBox();

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

	QWidget::connect(_minX, SIGNAL(valueChanged(int)), this,
			 SLOT(MinXChanged(int)));
	QWidget::connect(_minY, SIGNAL(valueChanged(int)), this,
			 SLOT(MinYChanged(int)));
	QWidget::connect(_maxX, SIGNAL(valueChanged(int)), this,
			 SLOT(MaxXChanged(int)));
	QWidget::connect(_maxY, SIGNAL(valueChanged(int)), this,
			 SLOT(MaxYChanged(int)));

	QHBoxLayout *mainLayout = new QHBoxLayout;

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{minX}}", _minX},
		{"{{minY}}", _minY},
		{"{{maxX}}", _maxX},
		{"{{maxY}}", _maxY}};

	placeWidgets(obs_module_text("AdvSceneSwitcher.condition.cursor.entry"),
		     mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionCursorEdit::MinXChanged(int pos)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_minX = pos;
}

void MacroConditionCursorEdit::MinYChanged(int pos)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_minY = pos;
}

void MacroConditionCursorEdit::MaxXChanged(int pos)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_maxX = pos;
}

void MacroConditionCursorEdit::MaxYChanged(int pos)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_maxY = pos;
}

void MacroConditionCursorEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_minX->setValue(_entryData->_minX);
	_minY->setValue(_entryData->_minY);
	_maxX->setValue(_entryData->_maxX);
	_maxY->setValue(_entryData->_maxY);
}
