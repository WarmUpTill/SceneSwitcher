#pragma once
#include "resizing-text-edit.hpp"

#include <QDialog>
#include <QMessageBox>
#include <optional>

namespace advss {

bool CloseAllInputDialogs();

class NonModalMessageDialog : public QDialog {
	Q_OBJECT

public:
	enum class Type { INFO, QUESTION, INPUT };

	NonModalMessageDialog(const QString &message, Type);
	NonModalMessageDialog(const QString &message, bool question);
	QMessageBox::StandardButton ShowMessage();
	std::optional<std::string> GetInput();
	Type GetType() const { return _type; }
	void SetInput(const QString &);

private slots:
	void YesClicked();
	void NoClicked();
	void InputChanged();

private:
	const Type _type;
	QString _input = "";
	ResizingPlainTextEdit *_inputEdit = nullptr;
	QMessageBox::StandardButton _answer;
};

} // namespace advss
