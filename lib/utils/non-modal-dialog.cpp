#include "non-modal-dialog.hpp"
#include "obs-module-helper.hpp"

#include <atomic>
#include <mutex>
#include <obs-frontend-api.h>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLayout>
#include <QMainWindow>
#include <QCoreApplication>

namespace advss {

static std::mutex mutex;
static std::vector<NonModalMessageDialog *> dialogs;

QWidget *GetSettingsWindow();

NonModalMessageDialog::NonModalMessageDialog(const QString &message,
					     bool question,
					     bool focusAdvssWindow)
	: NonModalMessageDialog(message, question ? Type::QUESTION : Type::INFO,
				focusAdvssWindow)
{
}

NonModalMessageDialog::~NonModalMessageDialog()
{
	std::lock_guard<std::mutex> lock(mutex);
	dialogs.erase(std::remove(dialogs.begin(), dialogs.end(), this),
		      dialogs.end());
}

NonModalMessageDialog::NonModalMessageDialog(const QString &message, Type type,
					     bool focusAdvssWindow)
	: QDialog(focusAdvssWindow && GetSettingsWindow()
			  ? GetSettingsWindow()
			  : static_cast<QMainWindow *>(
				    obs_frontend_get_main_window())),
	  _type(type),
	  _answer(QMessageBox::No)
{
	setWindowTitle(obs_module_text("AdvSceneSwitcher.windowTitle"));
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	auto layout = new QVBoxLayout(this);
	auto label = new QLabel(message, this);
	label->setTextInteractionFlags(Qt::TextBrowserInteraction);
	label->setOpenExternalLinks(true);
	layout->addWidget(label);

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
		_inputEdit->setTabChangesFocus(true);
		_inputEdit->setFocus();
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

	std::lock_guard<std::mutex> lock(mutex);
	dialogs.emplace_back(this);
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
	_inputEdit->setPlainText(_inputEdit->toPlainText());

	exec();
	deleteLater();

	if (_answer == QMessageBox::Yes) {
		return _input.toStdString();
	}
	return {};
}

void NonModalMessageDialog::SetInput(const QString &input)
{
	assert(_type == Type::INPUT);
	_inputEdit->setPlainText(input);
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

void CloseAllInputDialogs()
{
	std::lock_guard<std::mutex> lock(mutex);
	if (dialogs.empty()) {
		return;
	}
	obs_queue_task(
		OBS_TASK_UI,
		[](void *) {
			for (const auto dialog : dialogs) {
				if (dialog->GetType() ==
				    NonModalMessageDialog::Type::INPUT) {
					dialog->close();
				}
			}
		},
		nullptr, true);
}

} // namespace advss
