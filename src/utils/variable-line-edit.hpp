#pragma once
#include "variable-string.hpp"

#include <QLineEdit>

namespace advss {

class VariableLineEdit : public QLineEdit {
	Q_OBJECT
public:
	VariableLineEdit(QWidget *parent);
	void setText(const QString &);
	void setText(const StringVariable &);
	void setToolTip(const QString &string);

private:
};

} // namespace advss
