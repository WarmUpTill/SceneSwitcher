#include "macro-action-systray.hpp"
#include "layout-helpers.hpp"
#include "ui-helpers.hpp"

#include <obs-frontend-api.h>
#include <util/config-file.h>

namespace advss {

const std::string MacroActionSystray::id = "systray_notification";

bool MacroActionSystray::_registered = MacroActionFactory::Register(
	MacroActionSystray::id,
	{MacroActionSystray::Create, MacroActionSystrayEdit::Create,
	 "AdvSceneSwitcher.action.systray"});

bool MacroActionSystray::PerformAction()
{
	if (_lastPath != std::string(_iconPath)) {
		_lastPath = _iconPath;
		_icon = QIcon(QString::fromStdString(_iconPath));
	}
	DisplayTrayMessage(QString::fromStdString(_title),
			   QString::fromStdString(_message), _icon);
	return true;
}

void MacroActionSystray::LogAction() const
{
	ablog(LOG_INFO, "display systray message \"%s\":\n%s", _title.c_str(),
	      _message.c_str());
}

bool MacroActionSystray::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	_message.Save(obj, "message");
	_title.Save(obj, "title");
	_iconPath.Save(obj, "icon");
	obs_data_set_int(obj, "version", 1);
	return true;
}

bool MacroActionSystray::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_message.Load(obj, "message");
	_title.Load(obj, "title");
	_iconPath.Load(obj, "icon");
	if (!obs_data_has_user_value(obj, "version")) {
		_title = obs_module_text("AdvSceneSwitcher.pluginName");
	}
	return true;
}

std::shared_ptr<MacroAction> MacroActionSystray::Create(Macro *m)
{
	return std::make_shared<MacroActionSystray>(m);
}

std::shared_ptr<MacroAction> MacroActionSystray::Copy() const
{
	return std::make_shared<MacroActionSystray>(*this);
}

void MacroActionSystray::ResolveVariablesToFixedValues()
{
	_message.ResolveVariables();
	_title.ResolveVariables();
	_iconPath.ResolveVariables();
}

MacroActionSystrayEdit::MacroActionSystrayEdit(
	QWidget *parent, std::shared_ptr<MacroActionSystray> entryData)
	: QWidget(parent),
	  _message(new VariableLineEdit(this)),
	  _title(new VariableLineEdit(this)),
	  _iconPath(new FileSelection()),
	  _trayDisableWarning(
		  new QLabel("AdvSceneSwitcher.action.systray.disabled"))
{
	_iconPath->setToolTip(
		obs_module_text("AdvSceneSwitcher.action.systray.iconHint"));

	QWidget::connect(_message, SIGNAL(editingFinished()), this,
			 SLOT(MessageChanged()));
	QWidget::connect(_title, SIGNAL(editingFinished()), this,
			 SLOT(TitleChanged()));
	QWidget::connect(_iconPath, SIGNAL(PathChanged(const QString &)), this,
			 SLOT(IconPathChanged(const QString &)));

	auto layout = new QGridLayout();
	int row = 0;
	layout->addWidget(new QLabel(obs_module_text(
				  "AdvSceneSwitcher.action.systray.title")),
			  row, 0);
	layout->addWidget(_title, ++row, 1);
	layout->addWidget(new QLabel(obs_module_text(
				  "AdvSceneSwitcher.action.systray.message")),
			  row, 0);
	layout->addWidget(_message, ++row, 1);
	layout->addWidget(new QLabel(obs_module_text(
				  "AdvSceneSwitcher.action.systray.icon")),
			  row, 0);
	layout->addWidget(_iconPath, ++row, 1);

	auto mainlayout = new QVBoxLayout();
	mainlayout->addLayout(layout);
	mainlayout->addWidget(_trayDisableWarning);
	setLayout(mainlayout);

	_entryData = entryData;
	_message->setText(_entryData->_message);
	_title->setText(_entryData->_title);
	_iconPath->SetPath(_entryData->_iconPath);
	_loading = false;

	CheckIfTrayIsDisabled();
	QWidget::connect(&_checkTrayDisableTimer, SIGNAL(timeout()), this,
			 SLOT(CheckIfTrayIsDisabled()));
	_checkTrayDisableTimer.start(1000);
}

void MacroActionSystrayEdit::TitleChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_title = _title->text().toStdString();
}

void MacroActionSystrayEdit::IconPathChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_iconPath = text.toStdString();
}

void MacroActionSystrayEdit::CheckIfTrayIsDisabled()
{
	auto config = obs_frontend_get_global_config();
	if (!config) {
		return;
	}

	_trayDisableWarning->setVisible(
		!config_get_bool(config, "BasicWindow", "SysTrayEnabled"));
	adjustSize();
	updateGeometry();
}

void MacroActionSystrayEdit::MessageChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_message = _message->text().toStdString();
}

} // namespace advss
