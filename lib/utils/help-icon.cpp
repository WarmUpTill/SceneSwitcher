#include "help-icon.hpp"
#include "ui-helpers.hpp"

namespace advss {

HelpIcon::HelpIcon(const QString &tooltip, QWidget *parent) : QLabel(parent)
{
	auto path = GetThemeTypeName() == "Light"
			    ? ":/res/images/help.svg"
			    : ":/res/images/help_light.svg";
	QIcon icon(path);
	QPixmap pixmap = icon.pixmap(QSize(16, 16));
	setPixmap(pixmap);
	if (!tooltip.isEmpty()) {
		setToolTip(tooltip);
	}
}

} // namespace advss
