#pragma once
#include "variable.hpp"

#include <QLineEdit>

class VariableLineEdit : public QLineEdit {
	Q_OBJECT
public:
	VariableLineEdit(QWidget *parent);
	void setText(const QString &);
	void setText(const VariableResolvingString &);

private:
};
