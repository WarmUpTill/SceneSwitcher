#include "headers/macro-condition-edit.hpp"
#include "headers/macro-condition-virt-desktop.hpp"
#include "headers/utility.hpp"
#include "headers/advanced-scene-switcher.hpp"

const int MacroConditionVirtDesktop::id = 12;

bool MacroConditionVirtDesktop::_registered = MacroConditionFactory::Register(
	MacroConditionVirtDesktop::id,
	{MacroConditionVirtDesktop::Create,
	 MacroConditionVirtDesktopEdit::Create,
	 "AdvSceneSwitcher.condition.virtDesktop"});

bool MacroConditionVirtDesktop::CheckCondition()
{
	long curDesktop;
	GetCurrentVirtualDesktop(curDesktop);
	return _desktop == curDesktop;
}

bool MacroConditionVirtDesktop::Save(obs_data_t *obj)
{
	MacroCondition::Save(obj);
	obs_data_set_int(obj, "desktop", static_cast<int>(_desktop));
	return true;
}

bool MacroConditionVirtDesktop::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_desktop = obs_data_get_int(obj, "desktop");
	return true;
}

static void populateVirtualDesktopSelection(QComboBox *list)
{
	long count;
	GetVirtualDesktopCount(count);

	for (long i = 0; i < count; i++) {
		list->addItem(std::to_string(i).c_str());
	}
}

MacroConditionVirtDesktopEdit::MacroConditionVirtDesktopEdit(
	QWidget *parent, std::shared_ptr<MacroConditionVirtDesktop> entryData)
	: QWidget(parent)
{
	_virtDesktops = new QComboBox();
	QWidget::connect(_virtDesktops, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(DesktopChanged(int)));
	populateVirtualDesktopSelection(_virtDesktops);

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{virtDesktops}}", _virtDesktops},
	};
	placeWidgets(
		obs_module_text("AdvSceneSwitcher.condition.virtDesktop.entry"),
		mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionVirtDesktopEdit::DesktopChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_desktop = value;
}

void MacroConditionVirtDesktopEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_virtDesktops->setCurrentIndex(_entryData->_desktop);
}
