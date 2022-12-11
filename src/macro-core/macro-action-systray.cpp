#include "macro-action-systray.hpp"
#include "advanced-scene-switcher.hpp"
#include "utility.hpp"

const std::string MacroActionSystray::id = "systray_notification";

bool MacroActionSystray::_registered = MacroActionFactory::Register(
	MacroActionSystray::id,
	{MacroActionSystray::Create, MacroActionSystrayEdit::Create,
	 "AdvSceneSwitcher.action.systray"});

bool MacroActionSystray::PerformAction()
{
	if (_msg.empty()) {
		return true;
	}
	DisplayTrayMessage(obs_module_text("AdvSceneSwitcher.pluginName"),
			   QString::fromStdString(_msg));
	return true;
}

void MacroActionSystray::LogAction() const
{
	vblog(LOG_INFO, "display systray message \"%s\"", _msg.c_str());
}

bool MacroActionSystray::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	obs_data_set_string(obj, "message", _msg.c_str());
	return true;
}

bool MacroActionSystray::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_msg = obs_data_get_string(obj, "message");
	return true;
}

MacroActionSystrayEdit::MacroActionSystrayEdit(
	QWidget *parent, std::shared_ptr<MacroActionSystray> entryData)
	: QWidget(parent)
{
	_msg = new QLineEdit();
	QWidget::connect(_msg, SIGNAL(editingFinished()), this,
			 SLOT(MessageChanged()));

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{message}}", _msg},
	};
	placeWidgets(obs_module_text("AdvSceneSwitcher.action.systray.entry"),
		     mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	_entryData = entryData;
	_msg->setText(QString::fromStdString(_entryData->_msg));
	_loading = false;
}

void MacroActionSystrayEdit::MessageChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_msg = _msg->text().toStdString();
}
