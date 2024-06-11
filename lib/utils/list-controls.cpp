#include "list-controls.hpp"
#include "obs-module-helper.hpp"
#include "ui-helpers.hpp"

#include <QLayout>
#include <QToolButton>

namespace advss {

ListControls::ListControls(QWidget *parent, bool reorder) : QToolBar(parent)
{
	setObjectName("listControls");
	setStyleSheet("#listControls { background-color: transparent; }");

	setIconSize({16, 16});

	AddActionHelper("addIconSmall", "AdvSceneSwitcher.listControls.add",
			[this]() { Add(); });
	AddActionHelper("removeIconSmall",
			"AdvSceneSwitcher.listControls.remove",
			[this]() { Remove(); });

	if (reorder) {
		addSeparator();
		AddActionHelper("upArrowIconSmall",
				"AdvSceneSwitcher.listControls.up",
				[this]() { Up(); });
		AddActionHelper("downArrowIconSmall",
				"AdvSceneSwitcher.listControls.down",
				[this]() { Down(); });
	}
}

void ListControls::AddActionHelper(const char *theme, const char *tooltip,
				   const std::function<void()> &signal)
{
	auto button = new QToolButton(this);
	button->setToolTip(obs_module_text(tooltip));
	button->setProperty("themeID", QVariant(QString(theme)));
	auto action = addWidget(button);
	button->connect(button, &QToolButton::clicked, this, signal);
}

} // namespace advss
