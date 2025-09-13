#include "macro-add-dialog.hpp"
#include "macro.hpp"

namespace advss {

NewMacroDialog::NewMacroDialog(QWidget *parent)
	: QDialog(parent),
	  _type(new QComboBox(this))
{
	setModal(true);
	setWindowModality(Qt::WindowModality::WindowModal);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	auto buttonbox = new QDialogButtonBox(QDialogButtonBox::Ok |
					      QDialogButtonBox::Cancel);
	buttonbox->setCenterButtons(true);
	connect(buttonbox, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttonbox, &QDialogButtonBox::rejected, this, &QDialog::reject);

	auto dialogLayout = new QVBoxLayout();
	dialogLayout->addWidget(_type);
	dialogLayout->addWidget(buttonbox);
	setLayout(dialogLayout);

	Resize();
}

void NewMacroDialog::TypeChanged(int value) {}

void NewMacroDialog::Resize() {}

std::shared_ptr<Macro> NewMacroDialog::AddNewMacro(QWidget *parent,
						   std::string &selectedName)
{
	NewMacroDialog dialog(parent);
	dialog.setWindowTitle(obs_module_text("AdvSceneSwitcher.windowTitle"));
	if (dialog.exec() != DialogCode::Accepted) {
		return {};
	}

	// TODO ...
	return {};
}

} // namespace advss
