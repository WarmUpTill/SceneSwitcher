#include "non-modal-dialog.hpp"
#include "obs-module-helper.hpp"

#include <QMainWindow>
#include <QLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <obs-frontend-api.h>

namespace advss {

NonModalMessageDialog::NonModalMessageDialog(const QString &message,
					     bool question)
	: NonModalMessageDialog(message, question ? Type::QUESTION : Type::INFO)
{
}

NonModalMessageDialog::NonModalMessageDialog(const QString &message, Type type)
	: QDialog(static_cast<QMainWindow *>(obs_frontend_get_main_window())),
	  _type(type),
	  _answer(QMessageBox::No)
{
	setWindowTitle(obs_module_text("AdvSceneSwitcher.windowTitle"));
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	auto layout = new QVBoxLayout(this);
	layout->addWidget(new QLabel(message, this));

	switch (type) {
	case Type::INFO: {
		auto buttonbox = new QDialogButtonBox(QDialogButtonBox::Ok);
		connect(buttonbox, &QDialogButtonBox::accepted, this,
			&NonModalMessageDialog::YesClicked);
		layout->addWidget(buttonbox);
		break;
	}
	case Type::QUESTION: {
		auto buttonbox = new QDialogButtonBox(QDialogButtonBox::Yes |
						      QDialogButtonBox::No);
		connect(buttonbox, &QDialogButtonBox::accepted, this,
			&NonModalMessageDialog::YesClicked);
		connect(buttonbox, &QDialogButtonBox::rejected, this,
			&NonModalMessageDialog::NoClicked);
		layout->addWidget(buttonbox);
		break;
	}
	case Type::INPUT: {
		_inputEdit = new ResizingPlainTextEdit(this);
		connect(_inputEdit, &ResizingPlainTextEdit::textChanged, this,
			&NonModalMessageDialog::InputChanged);
		layout->addWidget(_inputEdit);
		auto buttonbox = new QDialogButtonBox(QDialogButtonBox::Ok |
						      QDialogButtonBox::Cancel);
		connect(buttonbox, &QDialogButtonBox::accepted, this,
			&NonModalMessageDialog::YesClicked);
		connect(buttonbox, &QDialogButtonBox::rejected, this,
			&NonModalMessageDialog::NoClicked);
		layout->addWidget(buttonbox);
		break;
	}
	}
	setLayout(layout);
}

QMessageBox::StandardButton NonModalMessageDialog::ShowMessage()
{
	show();
	exec();
	this->deleteLater();
	return _answer;
}

std::optional<std::string> NonModalMessageDialog::GetInput()
{
	show();

	// Trigger resize
	_inputEdit->setPlainText("");

	exec();
	this->deleteLater();
	if (_answer == QMessageBox::Yes) {
		return _input.toStdString();
	}
	return {};
}

void NonModalMessageDialog::YesClicked()
{
	_answer = QMessageBox::Yes;
	accept();
}

void NonModalMessageDialog::NoClicked()
{
	_answer = QMessageBox::No;
	accept();
}

void NonModalMessageDialog::InputChanged()
{
	_input = _inputEdit->toPlainText();
	adjustSize();
	updateGeometry();
}

bool CloseAllInputDialogs()
{
	auto window =
		static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!window) {
		return false;
	}

	bool closedAtLeastOneDialog = false;
	QList<QWidget *> widgets = window->findChildren<QWidget *>();
	for (auto &widget : widgets) {
		auto dialog = qobject_cast<NonModalMessageDialog *>(widget);
		if (!dialog) {
			continue;
		}
		if (dialog->GetType() != NonModalMessageDialog::Type::INPUT) {
			continue;
		}
		dialog->close();
		closedAtLeastOneDialog = true;
	}
	return closedAtLeastOneDialog;
}

} // namespace advss
