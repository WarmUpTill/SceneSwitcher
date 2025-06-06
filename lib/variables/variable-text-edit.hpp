#pragma once
#include "resizing-text-edit.hpp"
#include "variable-string.hpp"

namespace advss {

class ADVSS_EXPORT VariableTextEdit : public ResizingPlainTextEdit {
	Q_OBJECT
public:
	VariableTextEdit(QWidget *parent, const int scrollAt = 10,
			 const int minLines = 3, const int paddingLines = 2);
	virtual ~VariableTextEdit(){};
	void setPlainText(const QString &);
	void setPlainText(const StringVariable &);
	void setToolTip(const QString &string);

private:
};

} // namespace advss
