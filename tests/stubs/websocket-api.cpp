#include "websocket-api.hpp"

#include <obs.hpp>

namespace advss {

namespace {

int g_eventCount = 0;
std::string g_lastEventName;
OBSData g_lastEventData;

} // namespace

void ResetStubWebsocketEvents()
{
	g_eventCount = 0;
	g_lastEventName.clear();
	g_lastEventData = nullptr;
}

int GetStubWebsocketEventCount()
{
	return g_eventCount;
}

std::string GetStubLastWebsocketEventName()
{
	return g_lastEventName;
}

obs_data_t *GetStubLastWebsocketEventData()
{
	return g_lastEventData;
}

const char *GetWebsocketVendorName()
{
	return "AdvancedSceneSwitcher";
}

void RegisterWebsocketRequest(
	const std::string &,
	const std::function<void(obs_data_t *, obs_data_t *)> &)
{
}

void SendWebsocketVendorEvent(const std::string &eventName, obs_data_t *data)
{
	g_eventCount++;
	g_lastEventName = eventName;
	g_lastEventData = data;
}

} // namespace advss
