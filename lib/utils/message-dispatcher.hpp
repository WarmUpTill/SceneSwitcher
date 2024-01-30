#pragma once
#include "message-buffer.hpp"

#include <algorithm>
#include <memory>
#include <vector>

namespace advss {

template<class T> class MessageDispatcher {
public:
	[[nodiscard]] std::shared_ptr<MessageBuffer<T>> RegisterClient();
	void DispatchMessage(const T &message);

private:
	std::vector<std::weak_ptr<MessageBuffer<T>>> _clients;
	std::mutex _mutex;
};

template<class T>
inline std::shared_ptr<MessageBuffer<T>> MessageDispatcher<T>::RegisterClient()
{
	std::lock_guard<std::mutex> lock(_mutex);
	// Clear expired client buffers
	auto isExpired = [](const std::weak_ptr<MessageBuffer<T>> &ptr) {
		return ptr.expired();
	};
	_clients.erase(std::remove_if(_clients.begin(), _clients.end(),
				      isExpired),
		       _clients.end());
	// Prepare new buffer for client
	auto buffer = std::make_shared<MessageBuffer<T>>();
	_clients.emplace_back(buffer);
	return buffer;
}

template<class T>
inline void MessageDispatcher<T>::DispatchMessage(const T &message)
{
	std::lock_guard<std::mutex> lock(_mutex);
	for (auto &client_ : _clients) {
		auto client = client_.lock();
		if (!client) {
			continue;
		}
		client->AppendMessage(message);
	}
}

} // namespace advss
