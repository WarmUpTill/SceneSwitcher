#include "usb-helpers.hpp"

#include <utility.hpp>
#include <obs-module-helper.hpp>
#include <switcher-data.hpp>

namespace advss {

QStringList GetUSBDevices()
{
	libusb_init(NULL);

	// Get the list of connected USB devices
	libusb_device **devices;
	ssize_t count = libusb_get_device_list(NULL, &devices);
	if (count < 0) {
		libusb_exit(NULL);
		return {};
	}

	// Iterate through the list of devices and print their information

	QStringList result;

	for (int i = 0; i < count; i++) {
		libusb_device *device = devices[i];
		struct libusb_device_descriptor descriptor;

		int ret = libusb_get_device_descriptor(device, &descriptor);
		if (ret < 0) {
			vblog(LOG_INFO, "Error getting device descriptor");
			continue;
		}

		libusb_device_handle *handle;
		ret = libusb_open(device, &handle);
		if (ret < 0) {
			vblog(LOG_INFO, "Error opening device");
			continue;
		}

		char vendor_name[256];
		char product_name[256];
		char serial_number[256];
		// Get vendor name, product name, and serial number if available
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

		QString deviceStr = "Device %1:\n"
				    "  Vendor ID: %2\n"
				    "  Product ID: %3\n"
				    "  Bus Number: %4\n"
				    "  Device Address: %5\n"
				    "  Vendor Name: %6\n"
				    "  Product Name: %7\n"
				    "  Serial Number: %8\n";

		deviceStr = deviceStr.arg(
			QString::number(i + 1),
			QString::number(descriptor.idVendor),
			QString::number(descriptor.idProduct),
			QString::number(libusb_get_bus_number(device)),
			QString::number(libusb_get_device_address(device)),
			vendor_name, product_name, serial_number);
		result << deviceStr;
	}

	libusb_free_device_list(devices, 1);

	libusb_exit(NULL);
	return result;
}

} // namespace advss
