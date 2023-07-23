#include "macro-condition-usb.hpp"
#include "layout-helpers.hpp"

#include "obs.hpp"

namespace advss {

const std::string MacroConditionUSB::id = "usb";

bool MacroConditionUSB::_registered = MacroConditionFactory::Register(
	MacroConditionUSB::id,
	{MacroConditionUSB::Create, MacroConditionUSBEdit::Create,
	 "AdvSceneSwitcher.condition.usb"});

void MacroConditionUSB::DeviceMatchOption::Save(obs_data_t *obj,
						const char *name) const
{
	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_string(data, "pattern", pattern.c_str());
	regex.Save(data);
	obs_data_set_obj(obj, name, data);
}

void MacroConditionUSB::DeviceMatchOption::Load(obs_data_t *obj,
						const char *name)
{
	OBSDataAutoRelease data = obs_data_get_obj(obj, name);
	pattern = obs_data_get_string(data, "pattern");
	regex.Load(data);
}

bool MacroConditionUSB::CheckCondition()
{
	return false;
}

bool MacroConditionUSB::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	_vendorID.Save(obj, "vendorID");
	_productID.Save(obj, "productID");
	_busNumber.Save(obj, "busNumber");
	_deviceAddress.Save(obj, "deviceAddress");
	_vendorName.Save(obj, "vendorName");
	_productName.Save(obj, "productName");
	_serialNumber.Save(obj, "serialNumber");
	return true;
}

bool MacroConditionUSB::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_vendorID.Load(obj, "vendorID");
	_productID.Load(obj, "productID");
	_busNumber.Load(obj, "busNumber");
	_deviceAddress.Load(obj, "deviceAddress");
	_vendorName.Load(obj, "vendorName");
	_productName.Load(obj, "productName");
	_serialNumber.Load(obj, "serialNumber");
	return true;
}

std::string MacroConditionUSB::GetShortDesc() const
{
	return "";
}

void setupDevicePropertySelection(QComboBox *list, const QStringList &items)
{
	list->addItems(items);
	list->setEditable(true);
	list->setMaxVisibleItems(20);
}

MacroConditionUSBEdit::MacroConditionUSBEdit(
	QWidget *parent, std::shared_ptr<MacroConditionUSB> entryData)
	: QWidget(parent),
	  _vendorID(new QComboBox()),
	  _productID(new QComboBox()),
	  _busNumber(new QComboBox()),
	  _deviceAddress(new QComboBox()),
	  _vendorName(new QComboBox()),
	  _productName(new QComboBox()),
	  _serialNumber(new QComboBox())
{
	const auto usbDevs = GetUSBDevicesStringList();

	setupDevicePropertySelection(_vendorID, usbDevs);
	setupDevicePropertySelection(_productID, usbDevs);
	setupDevicePropertySelection(_busNumber, usbDevs);
	setupDevicePropertySelection(_deviceAddress, usbDevs);
	setupDevicePropertySelection(_vendorName, usbDevs);
	setupDevicePropertySelection(_productName, usbDevs);
	setupDevicePropertySelection(_serialNumber, usbDevs);

	auto layout = new QHBoxLayout;
	//PlaceWidgets(
	//	obs_module_text("AdvSceneSwitcher.condition.midi.entry.listen"),
	//	layout, {{"{{listenButton}}", _listen}});
	setLayout(layout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionUSBEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	adjustSize();
	updateGeometry();
}

void MacroConditionUSBEdit::VendorIDChanged(const QString &text)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_vendorID.pattern = text.toStdString();
}

void MacroConditionUSBEdit::ProductIDChanged(const QString &text)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_productID.pattern = text.toStdString();
}

void MacroConditionUSBEdit::BusNumberChanged(const QString &text)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_busNumber.pattern = text.toStdString();
}

void MacroConditionUSBEdit::DeviceAddressChanged(const QString &text)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_deviceAddress.pattern = text.toStdString();
}

void MacroConditionUSBEdit::VendorNameChanged(const QString &text)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_vendorName.pattern = text.toStdString();
}

void MacroConditionUSBEdit::ProductNameChanged(const QString &text)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_productName.pattern = text.toStdString();
}

void MacroConditionUSBEdit::SerialNumberChanged(const QString &text)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_serialNumber.pattern = text.toStdString();
}

} // namespace advss
