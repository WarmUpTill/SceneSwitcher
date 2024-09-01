#pragma once
#include "export-symbol-helper.hpp"

#include <functional>
#include <obs-data.h>
#include <string>

namespace advss {

EXPORT const char *GetWebsocketVendorName();
EXPORT void RegisterWebsocketRequest(
	const std::string &name,
	const std::function<void(obs_data_t *, obs_data_t *)> &callback);
EXPORT void SendWebsocketVendorEvent(const std::string &eventName,
				     obs_data_t *data);

} // namespace advss
