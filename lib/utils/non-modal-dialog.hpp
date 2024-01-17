#pragma once
#include "resizing-text-edit.hpp"

#include <QDialog>
#include <QMessageBox>
#include <optional>

namespace advss {

void CloseAllInputDialogs();

class NonModalMessageDialog : public QDialog {
	Q_OBJECT

public:
	enum class Type { INFO, QUESTION, INPUT };

	NonModalMessageDialog(const QString &message, Type,
			      bool focusAdvssWindow = false);
	NonModalMessageDialog(const QString &message, bool question,
			      bool focusAdvssWindow = false);
	~NonModalMessageDialog();
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
