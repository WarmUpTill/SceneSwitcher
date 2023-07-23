#pragma once
#include <QString>
#include <QStringList>
#include <string>
#include <vector>

namespace advss {

struct USBDeviceInfo {
	std::string vendorID;
	std::string productID;
	std::string busNumber;
	std::string deviceAddress;
	std::string vendorName;
	std::string productName;
	std::string serialNumber;

	std::string ToString() const;
	QString ToQString() const;

	bool operator==(const USBDeviceInfo &other);
};

std::vector<USBDeviceInfo> GetUSBDevices();
QStringList GetUSBDevicesStringList();

} // namespace advss
