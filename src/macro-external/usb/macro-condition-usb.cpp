#include "macro-condition-usb.hpp"
#include "utility.hpp"

namespace advss {

const std::string MacroConditionUSB::id = "usb";

bool MacroConditionUSB::_registered = MacroConditionFactory::Register(
	MacroConditionUSB::id,
	{MacroConditionUSB::Create, MacroConditionUSBEdit::Create,
	 "AdvSceneSwitcher.condition.usb"});

bool MacroConditionUSB::CheckCondition()
{
	return false;
}

bool MacroConditionUSB::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	return true;
}

bool MacroConditionUSB::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	return true;
}

std::string MacroConditionUSB::GetShortDesc() const
{
	return "";
}

MacroConditionUSBEdit::MacroConditionUSBEdit(
	QWidget *parent, std::shared_ptr<MacroConditionUSB> entryData)
	: QWidget(parent),
	  _listen(new QPushButton(
		  obs_module_text("AdvSceneSwitcher.midi.startListen"))),
	  _test(new QLabel())
{
	QWidget::connect(_listen, SIGNAL(clicked()), this,
			 SLOT(ToggleListen()));

	auto layout = new QHBoxLayout;
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.condition.midi.entry.listen"),
		layout, {{"{{listenButton}}", _listen}});
	layout->addWidget(_test);
	setLayout(layout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

MacroConditionUSBEdit::~MacroConditionUSBEdit()
{
	// libusb deinit?
}

void MacroConditionUSBEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	adjustSize();
	updateGeometry();
}

void MacroConditionUSBEdit::ToggleListen()
{
	if (!_entryData) {
		return;
	}

	auto usbDevs = GetUSBDevices();
	QString test;
	for (const auto &dev : usbDevs) {
		test += dev;
	}
	_test->setText(test);
}

} // namespace advss
