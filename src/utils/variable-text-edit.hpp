#pragma once
#include "resizing-text-edit.hpp"
#include "variable-string.hpp"

namespace advss {

class VariableTextEdit : public ResizingPlainTextEdit {
	Q_OBJECT
public:
	VariableTextEdit(QWidget *parent);
	void setPlainText(const QString &);
	void setPlainText(const StringVariable &);

private:
};

} // namespace advss
