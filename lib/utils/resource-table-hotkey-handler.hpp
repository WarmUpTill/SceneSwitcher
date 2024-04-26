#pragma once
#include "export-symbol-helper.hpp"

#include <map>
#include <mutex>
#include <QEvent>
#include <QObject>

namespace advss {

EXPORT void RegisterHotkeyFunction(QWidget *, Qt::Key,
				   std::function<void()> func);
EXPORT void DeregisterHotkeyFunctions(QWidget *);

class ResourceTabHotkeyHandler : public QObject {
	Q_OBJECT
public:
	static ResourceTabHotkeyHandler *Instance();
	void RegisterHandler(QWidget *, Qt::Key, std::function<void()> func);
	void Deregister(QWidget *);

protected:
	bool eventFilter(QObject *obj, QEvent *event) override;

private:
	struct CallbackData {
		Qt::Key key;
		std::function<void()> func;
	};

	ResourceTabHotkeyHandler() : QObject(){};

	std::multimap<QObject *, CallbackData> _callbacks;
	std::mutex _mutex;
};

} // namespace advss
