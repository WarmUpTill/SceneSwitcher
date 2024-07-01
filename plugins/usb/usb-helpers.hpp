#pragma once
#include <QString>
#include <QStringList>
#include <string>
#include <vector>

namespace advss {

struct USBDeviceInfo {
	const std::string vendorID;
	const std::string productID;
	const std::string busNumber;
	const std::string deviceAddress;
	const std::string vendorName;
	const std::string productName;
	const std::string serialNumber;

	std::string ToString() const;
	QString ToQString() const;
};

std::vector<USBDeviceInfo> GetUSBDevices();
QStringList GetUSBDevicesStringList();

} // namespace advss
