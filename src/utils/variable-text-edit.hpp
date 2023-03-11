#pragma once
#include "resizing-text-edit.hpp"
#include "variable-string.hpp"

class VariableTextEdit : public ResizingPlainTextEdit {
	Q_OBJECT
public:
	VariableTextEdit(QWidget *parent);
	void setPlainText(const QString &);
	void setPlainText(const StringVariable &);

private:
};
