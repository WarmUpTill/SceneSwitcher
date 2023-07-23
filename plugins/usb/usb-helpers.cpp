#include "usb-helpers.hpp"
#include "log-helper.hpp"
#include "plugin-state-helpers.hpp"

#include <chrono>
#include <libusb.h>
#include <mutex>

#define LOG_PREFIX "[usb] "

namespace advss {

static bool setup();
static bool setupDone = setup();
static bool hotplugsAreSupported = false;
static std::mutex mutex;

static bool setup()
{
	AddPluginInitStep([]() {
		libusb_init(NULL);
		hotplugsAreSupported =
			libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG);
	});
	AddPluginCleanupStep([]() { libusb_exit(NULL); });
	return true;
}

static void logLibusbError(int value, const char *msg)
{
	if (value >= LIBUSB_SUCCESS) {
		return;
	}
	vblog(LOG_WARNING, LOG_PREFIX "%s: %s", msg, libusb_strerror(value));
}

static USBDeviceInfo
getDeviceInfo(libusb_device *device, libusb_device_handle *handle,
	      const struct libusb_device_descriptor &descriptor)
{
	char vendor_name[256] = {};
	char product_name[256] = {};
	char serial_number[256] = {};
	int ret = libusb_get_string_descriptor_ascii(handle,
						     descriptor.iManufacturer,
						     (uint8_t *)vendor_name,
						     sizeof(vendor_name));
	logLibusbError(ret, "Failed to query vendor name");
	ret = libusb_get_string_descriptor_ascii(handle, descriptor.iProduct,
						 (uint8_t *)product_name,
						 sizeof(product_name));
	logLibusbError(ret, "Failed to query product name");
	ret = libusb_get_string_descriptor_ascii(handle,
						 descriptor.iSerialNumber,
						 (uint8_t *)serial_number,
						 sizeof(serial_number));
	logLibusbError(ret, "Failed to query serial number");

	const USBDeviceInfo deviceInfo = {
		std::to_string(descriptor.idVendor),
		std::to_string(descriptor.idProduct),
		std::to_string(libusb_get_bus_number(device)),
		std::to_string(libusb_get_device_address(device)),
		vendor_name,
		product_name,
		serial_number};
	return deviceInfo;
}

static std::vector<USBDeviceInfo> pollUSBDevices()
{
	libusb_device **devices;
	ssize_t count = libusb_get_device_list(NULL, &devices);
	if (count < 0) {
		logLibusbError(count, "Failed to query device list");
		return {};
	}

	std::vector<USBDeviceInfo> result;

	for (int i = 0; i < count; i++) {
		libusb_device *device = devices[i];
		struct libusb_device_descriptor descriptor;

		int ret = libusb_get_device_descriptor(device, &descriptor);
		if (ret != LIBUSB_SUCCESS) {
			logLibusbError(ret, "Error getting device descriptor");
			continue;
		}

		libusb_device_handle *handle;
		ret = libusb_open(device, &handle);
		if (ret != LIBUSB_SUCCESS) {
			logLibusbError(ret, "Error opening device");
			continue;
		}
		result.emplace_back(getDeviceInfo(device, handle, descriptor));
		libusb_close(handle);
	}

	libusb_free_device_list(devices, 1);

	return result;
}

static int hotplugCallback(struct libusb_context *ctx,
			   struct libusb_device *device,
			   libusb_hotplug_event event, void *user_data)
{
	auto devices = static_cast<std::vector<USBDeviceInfo> *>(user_data);

	static libusb_device_handle *handle = nullptr;
	struct libusb_device_descriptor descriptor;

	int ret = libusb_get_device_descriptor(device, &descriptor);
	logLibusbError(ret, "Error getting device descriptor");

	ret = libusb_open(device, &handle);
	if (ret != LIBUSB_SUCCESS) {
		logLibusbError(ret, "Error opening device");
		return 0;
	}
	const auto deviceInfo = getDeviceInfo(device, handle, descriptor);

	std::lock_guard<std::mutex> lock(mutex);
	if (LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED == event) {
		devices->emplace_back(deviceInfo);
	} else if (LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT == event) {
		devices->erase(std::find(devices->begin(), devices->end(),
					 deviceInfo));
	}

	libusb_close(handle);

	return 0;
}

static std::vector<USBDeviceInfo> getHotplugBasedDeviceList()
{
	static std::vector<USBDeviceInfo> devices;
	static bool hotplugSetupDone = false;

	if (!hotplugSetupDone) {
		hotplugSetupDone = true;
		devices = pollUSBDevices();

		int ret = libusb_hotplug_register_callback(
			nullptr,
			LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED |
				LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT,
			0, LIBUSB_HOTPLUG_MATCH_ANY, LIBUSB_HOTPLUG_MATCH_ANY,
			LIBUSB_HOTPLUG_MATCH_ANY, hotplugCallback, &devices,
			nullptr);
		if (ret != LIBUSB_SUCCESS) {
			hotplugsAreSupported = false;
		} else {
			blog(LOG_WARNING, LOG_PREFIX "hotplug supported!");
		}
	}

	std::lock_guard<std::mutex> lock(mutex);
	return devices;
}

static std::vector<USBDeviceInfo> getPollingBasedDeviceList()
{
	// Polling can be very expensive
	// Perform this operation at most once every 10 seconds
	static const int timeout = 10;
	static std::vector<USBDeviceInfo> deviceList = {};
	static std::chrono::high_resolution_clock::time_point lastUpdate = {};

	std::lock_guard<std::mutex> lock(mutex);
	auto now = std::chrono::high_resolution_clock::now();
	if (std::chrono::duration_cast<std::chrono::seconds>(now - lastUpdate)
		    .count() >= timeout) {
		deviceList = pollUSBDevices();
		lastUpdate = now;
	}

	return deviceList;
}

std::vector<USBDeviceInfo> GetUSBDevices()
{
	// Hotplug events do not seem to be firing consistently during testing
	// on Linux so for now only rely on polling based functionality instead
	return getPollingBasedDeviceList();

	/*
	if (hotplugsAreSupported) {
		return getHotplugBasedDeviceList();
	} else {
		return getPollingBasedDeviceList();
	}
	*/
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

bool USBDeviceInfo::operator==(const USBDeviceInfo &other)
{
	return vendorID == other.vendorID && productID == other.productID &&
	       busNumber == other.busNumber &&
	       deviceAddress == other.deviceAddress &&
	       vendorName == other.vendorName &&
	       productName == other.productName &&
	       serialNumber == other.serialNumber;
}

} // namespace advss
