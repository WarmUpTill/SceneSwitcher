#include "resource-table-hotkey-handler.hpp"

#include <QKeyEvent>
#include <QWidget>
#include <unordered_map>

namespace advss {

void RegisterHotkeyFunction(QWidget *widget, Qt::Key key,
			    std::function<void()> func)
{
	ResourceTabHotkeyHandler::Instance()->RegisterHandler(widget, key,
							      func);
}

void DeregisterHotkeyFunctions(QWidget *widget)
{
	ResourceTabHotkeyHandler::Instance()->Deregister(widget);
}

ResourceTabHotkeyHandler *ResourceTabHotkeyHandler::Instance()
{
	static ResourceTabHotkeyHandler handler;
	return &handler;
}

void ResourceTabHotkeyHandler::RegisterHandler(QWidget *widget, Qt::Key key,
					       std::function<void()> func)
{
	std::lock_guard<std::mutex> lock(_mutex);
	_callbacks.emplace(widget, CallbackData{key, func});
	widget->installEventFilter(this);
}

void ResourceTabHotkeyHandler::Deregister(QWidget *widget)
{
	std::lock_guard<std::mutex> lock(_mutex);
	_callbacks.erase(widget);
}

bool ResourceTabHotkeyHandler::eventFilter(QObject *obj, QEvent *event)
{
	if (event->type() != QEvent::KeyPress) {
		return QObject::eventFilter(obj, event);
	}
	QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
	auto pressedKey = keyEvent->key();

	std::lock_guard<std::mutex> lock(_mutex);
	auto it = _callbacks.find(obj);
	if (it == _callbacks.end()) {
		return QObject::eventFilter(obj, event);
	}

	auto current = it;
	while (current != _callbacks.end() && current->first == it->first) {
		auto &[_, cbData] = *current;
		if (pressedKey != cbData.key) {
			++current;
			continue;
		}
		cbData.func();
		++current;
		continue;
	}

	return QObject::eventFilter(obj, event);
}

} // namespace advss
