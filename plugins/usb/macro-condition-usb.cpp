#include "macro-condition-usb.hpp"
#include "layout-helpers.hpp"

#include "obs.hpp"

namespace advss {

const std::string MacroConditionUSB::id = "usb";

bool MacroConditionUSB::_registered = MacroConditionFactory::Register(
	MacroConditionUSB::id,
	{MacroConditionUSB::Create, MacroConditionUSBEdit::Create,
	 "AdvSceneSwitcher.condition.usb"});

bool MacroConditionUSB::DeviceMatchOption::Matches(
	const std::string &value) const
{
	if (!regex.Enabled()) {
		return pattern == value;
	}
	return regex.Matches(value, pattern);
}

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
	const auto devs = GetUSBDevices();
	if (devs.empty()) {
		return false;
	}

	for (const auto &dev : devs) {
		if (_vendorID.Matches(dev.vendorID) &&
		    _productID.Matches(dev.productID) &&
		    _busNumber.Matches(dev.busNumber) &&
		    _deviceAddress.Matches(dev.deviceAddress) &&
		    _vendorName.Matches(dev.vendorName) &&
		    _productName.Matches(dev.productName) &&
		    _serialNumber.Matches(dev.serialNumber)) {
			SetTempVarValue("vendorID", dev.vendorID);
			SetTempVarValue("productID", dev.productID);
			SetTempVarValue("busNumber", dev.busNumber);
			SetTempVarValue("deviceAddress", dev.deviceAddress);
			SetTempVarValue("vendorName", dev.vendorName);
			SetTempVarValue("productName", dev.productName);
			SetTempVarValue("serialNumber", dev.serialNumber);
			return true;
		}
	}
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

void MacroConditionUSB::SetupTempVars()
{
	AddTempvar("vendorName",
		   obs_module_text("AdvSceneSwitcher.tempVar.usb.vendorName"));
	AddTempvar("productName",
		   obs_module_text("AdvSceneSwitcher.tempVar.usb.productName"));
	AddTempvar("vendorID",
		   obs_module_text("AdvSceneSwitcher.tempVar.usb.vendorID"));
	AddTempvar("productID",
		   obs_module_text("AdvSceneSwitcher.tempVar.usb.productID"));
	AddTempvar("busNumber",
		   obs_module_text("AdvSceneSwitcher.tempVar.usb.busNumber"));
	AddTempvar(
		"deviceAddress",
		obs_module_text("AdvSceneSwitcher.tempVar.usb.deviceAddress"));
	AddTempvar(
		"serialNumber",
		obs_module_text("AdvSceneSwitcher.tempVar.usb.serialNumber"));
}

static void setupDevicePropertySelection(QComboBox *list,
					 const QSet<QString> &items)
{
	list->setEditable(true);
	list->setMaxVisibleItems(20);
	list->setDuplicatesEnabled(false);
	for (const auto &item : items) {
		list->addItem(item);
	}
	list->model()->sort(0);
}

void static populateNewLayoutRow(QGridLayout *layout, int &row,
				 const char *labelText, QComboBox *property,
				 RegexConfigWidget *regex)
{
	layout->addWidget(new QLabel(obs_module_text(labelText)), row, 0);
	auto settingsLayout = new QHBoxLayout();
	property->setSizePolicy(QSizePolicy::MinimumExpanding,
				QSizePolicy::MinimumExpanding);
	settingsLayout->addWidget(property);
	settingsLayout->addWidget(regex);
	settingsLayout->setContentsMargins(0, 0, 0, 0);
	layout->addLayout(settingsLayout, row, 1);
	row++;
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
	  _serialNumber(new QComboBox()),
	  _vendorIDRegex(new RegexConfigWidget()),
	  _productIDRegex(new RegexConfigWidget()),
	  _busNumberRegex(new RegexConfigWidget()),
	  _deviceAddressRegex(new RegexConfigWidget()),
	  _vendorNameRegex(new RegexConfigWidget()),
	  _productNameRegex(new RegexConfigWidget()),
	  _serialNumberRegex(new RegexConfigWidget())
{
	QSet<QString> vendorIDs;
	QSet<QString> productIDs;
	QSet<QString> busNumbers;
	QSet<QString> deviceAddresses;
	QSet<QString> vendorNames;
	QSet<QString> productNames;
	QSet<QString> serialNumbers;

	const auto devs = GetUSBDevices();
	for (const auto &dev : devs) {
		vendorIDs.insert(QString::fromStdString(dev.vendorID));
		productIDs.insert(QString::fromStdString(dev.productID));
		busNumbers.insert(QString::fromStdString(dev.busNumber));
		deviceAddresses.insert(
			QString::fromStdString(dev.deviceAddress));
		vendorNames.insert(QString::fromStdString(dev.vendorName));
		productNames.insert(QString::fromStdString(dev.productName));
		serialNumbers.insert(QString::fromStdString(dev.serialNumber));
	}

	setupDevicePropertySelection(_vendorID, vendorIDs);
	setupDevicePropertySelection(_productID, productIDs);
	setupDevicePropertySelection(_busNumber, busNumbers);
	setupDevicePropertySelection(_deviceAddress, deviceAddresses);
	setupDevicePropertySelection(_vendorName, vendorNames);
	setupDevicePropertySelection(_productName, productNames);
	setupDevicePropertySelection(_serialNumber, serialNumbers);

	QWidget::connect(_vendorID, &QComboBox::currentTextChanged, this,
			 &MacroConditionUSBEdit::VendorIDChanged);
	QWidget::connect(_productID, &QComboBox::currentTextChanged, this,
			 &MacroConditionUSBEdit::ProductIDChanged);
	QWidget::connect(_busNumber, &QComboBox::currentTextChanged, this,
			 &MacroConditionUSBEdit::BusNumberChanged);
	QWidget::connect(_deviceAddress, &QComboBox::currentTextChanged, this,
			 &MacroConditionUSBEdit::DeviceAddressChanged);
	QWidget::connect(_vendorName, &QComboBox::currentTextChanged, this,
			 &MacroConditionUSBEdit::VendorNameChanged);
	QWidget::connect(_productName, &QComboBox::currentTextChanged, this,
			 &MacroConditionUSBEdit::ProductNameChanged);
	QWidget::connect(_serialNumber, &QComboBox::currentTextChanged, this,
			 &MacroConditionUSBEdit::SerialNumberChanged);
	QWidget::connect(_vendorIDRegex,
			 SIGNAL(RegexConfigChanged(const RegexConfig &)), this,
			 SLOT(VendorIDRegexChanged(const RegexConfig &)));
	QWidget::connect(_productIDRegex,
			 SIGNAL(RegexConfigChanged(const RegexConfig &)), this,
			 SLOT(ProductIDRegexChanged(const RegexConfig &)));
	QWidget::connect(_busNumberRegex,
			 SIGNAL(RegexConfigChanged(const RegexConfig &)), this,
			 SLOT(BusNumberRegexChanged(const RegexConfig &)));
	QWidget::connect(_deviceAddressRegex,
			 SIGNAL(RegexConfigChanged(const RegexConfig &)), this,
			 SLOT(DeviceAddressRegexChanged(const RegexConfig &)));
	QWidget::connect(_vendorNameRegex,
			 SIGNAL(RegexConfigChanged(const RegexConfig &)), this,
			 SLOT(VendorNameRegexChanged(const RegexConfig &)));
	QWidget::connect(_productNameRegex,
			 SIGNAL(RegexConfigChanged(const RegexConfig &)), this,
			 SLOT(ProductNameRegexChanged(const RegexConfig &)));
	QWidget::connect(_serialNumberRegex,
			 SIGNAL(RegexConfigChanged(const RegexConfig &)), this,
			 SLOT(SerialNumberRegexChanged(const RegexConfig &)));

	int row = 0;
	auto devicePropertyLayout = new QGridLayout;
	populateNewLayoutRow(devicePropertyLayout, row,
			     "AdvSceneSwitcher.condition.usb.vendorName",
			     _vendorName, _vendorNameRegex);
	populateNewLayoutRow(devicePropertyLayout, row,
			     "AdvSceneSwitcher.condition.usb.productName",
			     _productName, _productNameRegex);
	populateNewLayoutRow(devicePropertyLayout, row,
			     "AdvSceneSwitcher.condition.usb.vendorID",
			     _vendorID, _vendorIDRegex);
	populateNewLayoutRow(devicePropertyLayout, row,
			     "AdvSceneSwitcher.condition.usb.productID",
			     _productID, _productIDRegex);
	populateNewLayoutRow(devicePropertyLayout, row,
			     "AdvSceneSwitcher.condition.usb.busNumber",
			     _busNumber, _busNumberRegex);
	populateNewLayoutRow(devicePropertyLayout, row,
			     "AdvSceneSwitcher.condition.usb.deviceAddress",
			     _deviceAddress, _deviceAddressRegex);
	populateNewLayoutRow(devicePropertyLayout, row,
			     "AdvSceneSwitcher.condition.usb.serialNumber",
			     _serialNumber, _serialNumberRegex);
	MinimizeSizeOfColumn(devicePropertyLayout, 0);
	devicePropertyLayout->setContentsMargins(0, 0, 0, 0);

	auto layout = new QVBoxLayout();
	layout->addWidget(new QLabel(
		obs_module_text("AdvSceneSwitcher.condition.usb.description")));
	layout->addLayout(devicePropertyLayout);
	if (devs.empty()) {
		layout->addWidget(new QLabel(obs_module_text(
			"AdvSceneSwitcher.condition.noDevicesFoundWarning")));
	}
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

	_vendorID->setCurrentText(
		QString::fromStdString(_entryData->_vendorID.pattern));
	_productID->setCurrentText(
		QString::fromStdString(_entryData->_productID.pattern));
	_busNumber->setCurrentText(
		QString::fromStdString(_entryData->_busNumber.pattern));
	_deviceAddress->setCurrentText(
		QString::fromStdString(_entryData->_deviceAddress.pattern));
	_vendorName->setCurrentText(
		QString::fromStdString(_entryData->_vendorName.pattern));
	_productName->setCurrentText(
		QString::fromStdString(_entryData->_productName.pattern));
	_serialNumber->setCurrentText(
		QString::fromStdString(_entryData->_serialNumber.pattern));
	_vendorIDRegex->SetRegexConfig(_entryData->_vendorID.regex);
	_productIDRegex->SetRegexConfig(_entryData->_productID.regex);
	_busNumberRegex->SetRegexConfig(_entryData->_busNumber.regex);
	_deviceAddressRegex->SetRegexConfig(_entryData->_deviceAddress.regex);
	_vendorNameRegex->SetRegexConfig(_entryData->_vendorName.regex);
	_productNameRegex->SetRegexConfig(_entryData->_productName.regex);
	_serialNumberRegex->SetRegexConfig(_entryData->_serialNumber.regex);

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

void MacroConditionUSBEdit::VendorIDRegexChanged(const RegexConfig &regex)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_vendorID.regex = regex;
}

void MacroConditionUSBEdit::ProductIDRegexChanged(const RegexConfig &regex)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_productID.regex = regex;
}

void MacroConditionUSBEdit::BusNumberRegexChanged(const RegexConfig &regex)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_busNumber.regex = regex;
}

void MacroConditionUSBEdit::DeviceAddressRegexChanged(const RegexConfig &regex)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_deviceAddress.regex = regex;
}

void MacroConditionUSBEdit::VendorNameRegexChanged(const RegexConfig &regex)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_vendorName.regex = regex;
}

void MacroConditionUSBEdit::ProductNameRegexChanged(const RegexConfig &regex)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_productName.regex = regex;
}

void MacroConditionUSBEdit::SerialNumberRegexChanged(const RegexConfig &regex)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_serialNumber.regex = regex;
}

} // namespace advss
