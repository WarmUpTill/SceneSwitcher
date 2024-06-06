#pragma once
#include "export-symbol-helper.hpp"

#include <QLabel>

namespace advss {

class ADVSS_EXPORT HelpIcon : public QLabel {
	Q_OBJECT

public:
	HelpIcon(const QString &tooltip = "", QWidget *parent = nullptr);
};

} // namespace advss
