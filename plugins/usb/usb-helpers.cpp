#include "usb-helpers.hpp"
#include "log-helper.hpp"
#include "plugin-state-helpers.hpp"

#include <libusb.h>

namespace advss {

static bool setup();
static bool setupDone = setup();

static bool setup()
{
	AddPluginInitStep([]() { libusb_init(NULL); });
	AddPluginCleanupStep([]() { libusb_exit(NULL); });
	return true;
}

std::vector<USBDeviceInfo> GetUSBDevices()
{
	libusb_device **devices;
	ssize_t count = libusb_get_device_list(NULL, &devices);
	if (count < 0) {
		libusb_exit(NULL);
		return {};
	}

	std::vector<USBDeviceInfo> result;

	for (int i = 0; i < count; i++) {
		libusb_device *device = devices[i];
		struct libusb_device_descriptor descriptor;

		int ret = libusb_get_device_descriptor(device, &descriptor);
		if (ret < 0) {
			vblog(LOG_INFO,
			      "[usb] Error getting device descriptor");
			continue;
		}

		libusb_device_handle *handle;
		ret = libusb_open(device, &handle);
		if (ret < 0) {
			vblog(LOG_INFO, "[usb] Error opening device");
			continue;
		}

		char vendor_name[256];
		char product_name[256];
		char serial_number[256];
		libusb_get_string_descriptor_ascii(handle,
						   descriptor.iManufacturer,
						   (unsigned char *)vendor_name,
						   sizeof(vendor_name));
		libusb_get_string_descriptor_ascii(
			handle, descriptor.iProduct,
			(unsigned char *)product_name, sizeof(product_name));
		libusb_get_string_descriptor_ascii(
			handle, descriptor.iSerialNumber,
			(unsigned char *)serial_number, sizeof(serial_number));

		libusb_close(handle);

		const USBDeviceInfo deviceInfo = {
			std::to_string(descriptor.idVendor),
			std::to_string(descriptor.idProduct),
			std::to_string(libusb_get_bus_number(device)),
			std::to_string(libusb_get_device_address(device)),
			vendor_name,
			product_name,
			serial_number};

		result.emplace_back(deviceInfo);
	}

	libusb_free_device_list(devices, 1);

	return result;
}

QStringList GetUSBDevicesStringList()
{
	QStringList result;
	const auto devices = GetUSBDevices();
	for (const auto &device : devices) {
		result << device.ToQString();
	}
	return result;
}

std::string USBDeviceInfo::ToString() const
{
	return "Vendor ID: " + vendorID + "\nProduct ID: " + productID +
	       "\nBus Number:" + busNumber +
	       "\nDevice Address:" + deviceAddress +
	       "\nVendor Name:" + vendorName + "\nProduct Name:" + productName +
	       "\nSerial Number:" + serialNumber;
}

QString USBDeviceInfo::ToQString() const
{
	return QString::fromStdString(ToString());
}

} // namespace advss
