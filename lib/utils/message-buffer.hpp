#pragma once
#include <deque>
#include <mutex>
#include <optional>

namespace advss {

template<class T> class MessageBuffer {
public:
	bool Empty();
	void Clear();
	void AppendMessage(const T &);
	std::optional<T> ConsumeMessage();

private:
	std::deque<T> _buffer;
	std::mutex _mutex;
};

template<class T> inline bool MessageBuffer<T>::Empty()
{
	std::lock_guard<std::mutex> lock(_mutex);
	return _buffer.empty();
}

template<class T> inline void MessageBuffer<T>::Clear()
{
	std::lock_guard<std::mutex> lock(_mutex);
	_buffer.clear();
}

template<class T> inline void MessageBuffer<T>::AppendMessage(const T &message)
{
	std::lock_guard<std::mutex> lock(_mutex);
	_buffer.emplace_back(message);
}

template<class T> inline std::optional<T> MessageBuffer<T>::ConsumeMessage()
{
	std::lock_guard<std::mutex> lock(_mutex);
	if (_buffer.empty()) {
		return {};
	}
	T message = _buffer.at(0);
	_buffer.pop_front();
	return message;
}

} // namespace advss
