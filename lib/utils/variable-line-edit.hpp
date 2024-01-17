#pragma once
#include "variable-string.hpp"

#include <QLineEdit>

namespace advss {

class VariableLineEdit : public QLineEdit {
	Q_OBJECT
public:
	EXPORT VariableLineEdit(QWidget *parent);
	EXPORT void setText(const QString &);
	EXPORT void setText(const StringVariable &);
	EXPORT void setToolTip(const QString &string);

private slots:
	void DisplayValidationMessages();

private:
};

} // namespace advss
