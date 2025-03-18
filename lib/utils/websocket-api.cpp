#include "websocket-api.hpp"
#include "log-helper.hpp"
#include "obs-websocket-api.h"
#include "plugin-state-helpers.hpp"

#include <mutex>

namespace advss {

static std::vector<std::function<void(obs_data_t *, obs_data_t *)>> callbacks;

static constexpr char VendorName[] = "AdvancedSceneSwitcher";
static constexpr char VendorRequestStart[] = "AdvancedSceneSwitcherStart";
static constexpr char VendorRequestStop[] = "AdvancedSceneSwitcherStop";
static constexpr char VendorRequestStatus[] = "IsAdvancedSceneSwitcherRunning";
static obs_websocket_vendor vendor;

static void registerWebsocketVendor();

static bool setup();
static bool setupDone = setup();

bool setup()
{
	AddPluginPostLoadStep(registerWebsocketVendor);
	AddStartStep([]() {
		SendWebsocketVendorEvent("AdvancedSceneSwitcherStarted",
					 nullptr);
	});
	AddStopStep([]() {
		SendWebsocketVendorEvent("AdvancedSceneSwitcherStopped",
					 nullptr);
	});
	return true;
}

static void
registerWebsocketVendorRequest(const char *requestName,
			       obs_websocket_request_callback_function callback)
{
	if (!obs_websocket_vendor_register_request(vendor, requestName,
						   callback, NULL)) {
		blog(LOG_ERROR,
		     "Failed to register \"%s\" request with obs-websocket.",
		     requestName);
	}
}

static void registerWebsocketVendor()
{
	vendor = obs_websocket_register_vendor(VendorName);
	if (!vendor) {
		blog(LOG_ERROR,
		     "Vendor registration failed! (obs-websocket should have logged something if installed properly.)");
		return;
	}

	auto api_version = obs_websocket_get_api_version();
	if (api_version == 0) {
		blog(LOG_ERROR,
		     "Unable to fetch obs-websocket plugin API version.");
		return;
	} else if (api_version == 1) {
		blog(LOG_WARNING,
		     "Unsupported obs-websocket plugin API version for calling requests.");
		return;
	}

	registerWebsocketVendorRequest(VendorRequestStart,
				       [](obs_data_t *, obs_data_t *, void *) {
					       StartPlugin();
				       });
	registerWebsocketVendorRequest(VendorRequestStop,
				       [](obs_data_t *, obs_data_t *, void *) {
					       StopPlugin();
				       });
	registerWebsocketVendorRequest(
		VendorRequestStatus,
		[](obs_data_t *, obs_data_t *response, void *) {
			obs_data_set_bool(response, "isRunning",
					  PluginIsRunning());
		});
}

const char *GetWebsocketVendorName()
{
	return VendorName;
}

void RegisterWebsocketRequest(
	const std::string &name,
	const std::function<void(obs_data_t *, obs_data_t *)> &callback)
{
	static std::mutex m;
	std::lock_guard<std::mutex> lock(m);

	callbacks.emplace_back(callback);
	int index = callbacks.size() - 1;
	auto handleRequest = [](obs_data *requestData, obs_data *other,
				void *cbPtr) {
		auto cb = *(static_cast<
			    std::function<void(obs_data *, obs_data *)> *>(
			cbPtr));
		cb(requestData, other);
	};

	AddPluginPostLoadStep([name, handleRequest, index]() {
		if (!obs_websocket_vendor_register_request(
			    vendor, name.c_str(), handleRequest,
			    &callbacks.at(index))) {
			blog(LOG_ERROR,
			     "Failed to register \"%s\" request with obs-websocket.",
			     name.c_str());
		}
	});
}

void SendWebsocketVendorEvent(const std::string &eventName, obs_data_t *data)
{
	if (OBSIsShuttingDown()) {
		return;
	}
	obs_websocket_vendor_emit_event(vendor, eventName.c_str(), data);
}

} // namespace advss
