#include "value-input.hpp"
#include "file-selection.hpp"
#include "obs-module-helper.hpp"
#include "scene-selection.hpp"
#include "selection-helpers.hpp"
#include "source-helpers.hpp"
#include "source-selection.hpp"
#include "variable.hpp"

#include <memory>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLineEdit>

namespace advss {

void PopulateValueInputTypeSelection(QComboBox *list)
{
	static const std::vector<std::pair<ValueInputType, const char *>>
		types = {
			{ValueInputType::File,
			 "AdvSceneSwitcher.valueInputType.file"},
			{ValueInputType::Folder,
			 "AdvSceneSwitcher.valueInputType.folder"},
			{ValueInputType::Scene,
			 "AdvSceneSwitcher.valueInputType.scene"},
			{ValueInputType::Source,
			 "AdvSceneSwitcher.valueInputType.source"},
			{ValueInputType::String,
			 "AdvSceneSwitcher.valueInputType.string"},
			{ValueInputType::Variable,
			 "AdvSceneSwitcher.valueInputType.variable"},
			{ValueInputType::Selection,
			 "AdvSceneSwitcher.valueInputType.selection"},
		};
	for (const auto &[type, name] : types) {
		list->addItem(obs_module_text(name), static_cast<int>(type));
	}
}

static QStringList populateSources()
{
	auto sources = GetSourceNames();
	sources.sort();
	return sources;
}

ValueInputWidget::ValueInputWidget(ValueInputType type,
				   const std::string &defaultValue,
				   const QStringList &options, QWidget *parent)
	: QWidget(parent)
{
	auto layout = new QHBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);

	switch (type) {
	case ValueInputType::File:
	case ValueInputType::Folder: {
		auto widget =
			new FileSelection(type == ValueInputType::Folder
						  ? FileSelection::Type::FOLDER
						  : FileSelection::Type::READ,
					  this);
		widget->SetPath(QString::fromStdString(defaultValue));
		layout->addWidget(widget);
		_getValue = [widget]() {
			return widget->GetPath().toStdString();
		};
		break;
	}
	case ValueInputType::Scene: {
		auto widget = new SceneSelectionWidget(this);
		auto current = std::make_shared<std::string>();
		auto scene = GetWeakSourceByName(defaultValue.c_str());
		OBSSourceAutoRelease source = obs_weak_source_get_source(scene);
		if (source && obs_source_is_scene(source)) {
			SceneSelection selection;
			selection.SetScene(scene);
			widget->SetScene(selection);
			*current = defaultValue;
		}
		connect(widget, &SceneSelectionWidget::SceneChanged, this,
			[current](const SceneSelection &s) {
				*current = s.ToString();
			});
		layout->addWidget(widget);
		_getValue = [current]() {
			return *current;
		};
		break;
	}
	case ValueInputType::Source: {
		auto widget =
			new SourceSelectionWidget(this, populateSources, false);
		auto current = std::make_shared<std::string>();
		auto source = GetWeakSourceByName(defaultValue.c_str());
		if (source) {
			SourceSelection selection;
			selection.SetSource(source);
			widget->SetSource(selection);
			*current = defaultValue;
		}
		connect(widget, &SourceSelectionWidget::SourceChanged, this,
			[current](const SourceSelection &s) {
				*current = s.ToString();
			});
		layout->addWidget(widget);
		_getValue = [current]() {
			return *current;
		};
		break;
	}
	case ValueInputType::Selection: {
		auto widget = new QComboBox(this);
		widget->addItems(options);
		widget->setCurrentIndex(
			widget->findText(QString::fromStdString(defaultValue)));
		layout->addWidget(widget);
		_getValue = [widget]() {
			return widget->currentText().toStdString();
		};
		break;
	}
	case ValueInputType::Variable: {
		auto widget = new VariableSelection(this);
		widget->SetVariable(defaultValue);
		layout->addWidget(widget);
		_getValue = [widget]() {
			auto item = widget->GetCurrentItem();
			return item ? item->Name() : "";
		};
		break;
	}
	default: // ValueInputType::String
	{
		auto widget = new QLineEdit(
			QString::fromStdString(defaultValue), this);
		layout->addWidget(widget);
		_getValue = [widget]() {
			return widget->text().toStdString();
		};
		break;
	}
	}
}

std::string ValueInputWidget::Value() const
{
	return _getValue();
}

} // namespace advss
