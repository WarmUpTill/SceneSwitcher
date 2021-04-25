#include "headers/macro-condition-edit.hpp"
#include "headers/macro-condition-region.hpp"
#include "headers/utility.hpp"
#include "headers/advanced-scene-switcher.hpp"

const int MacroConditionRegion::id = 2;

bool MacroConditionRegion::_registered = MacroConditionFactory::Register(
	MacroConditionRegion::id,
	{MacroConditionRegion::Create, MacroConditionRegionEdit::Create,
	 "AdvSceneSwitcher.condition.region"});

bool MacroConditionRegion::CheckCondition()
{
	std::pair<int, int> cursorPos = getCursorPos();
	return cursorPos.first >= _minX && cursorPos.second >= _minY &&
	       cursorPos.first <= _maxX && cursorPos.second <= _maxY;
}

bool MacroConditionRegion::Save(obs_data_t *obj)
{
	MacroCondition::Save(obj);
	obs_data_set_int(obj, "minX", _minX);
	obs_data_set_int(obj, "minY", _minY);
	obs_data_set_int(obj, "maxX", _maxX);
	obs_data_set_int(obj, "maxY", _maxY);
	return true;
}

bool MacroConditionRegion::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_minX = obs_data_get_int(obj, "minX");
	_minY = obs_data_get_int(obj, "minY");
	_maxX = obs_data_get_int(obj, "maxX");
	_maxY = obs_data_get_int(obj, "maxY");
	return true;
}

MacroConditionRegionEdit::MacroConditionRegionEdit(
	std::shared_ptr<MacroConditionRegion> entryData)
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

	placeWidgets(obs_module_text(
			     "AdvSceneSwitcher.macro.condition.region.entry"),
		     mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionRegionEdit::MinXChanged(int pos)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_minX = pos;
}

void MacroConditionRegionEdit::MinYChanged(int pos)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_minY = pos;
}

void MacroConditionRegionEdit::MaxXChanged(int pos)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_maxX = pos;
}

void MacroConditionRegionEdit::MaxYChanged(int pos)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_maxY = pos;
}

void MacroConditionRegionEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_minX->setValue(_entryData->_minX);
	_minY->setValue(_entryData->_minY);
	_maxX->setValue(_entryData->_maxX);
	_maxY->setValue(_entryData->_maxY);
}
