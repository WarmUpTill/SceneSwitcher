#pragma once
#include <QDialog>
#include <QComboBox>

namespace advss {

class Macro;

class NewMacroDialog : public QDialog {
	Q_OBJECT

public:
	enum class Type { BLANK_MACRO, COPY, TEMPLATE };
	static std::shared_ptr<Macro> AddNewMacro(QWidget *parent,
						  std::string &selectedName);
private slots:
	void TypeChanged(int);

private:
	NewMacroDialog(QWidget *parent);
	void Resize();

	QComboBox *_type;
};

} // namespace advss
