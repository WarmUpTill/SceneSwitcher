#include "source-interaction-step-list.hpp"
#include "obs-module-helper.hpp"

#include <QTimer>

namespace advss {

SourceInteractionStepList::SourceInteractionStepList(QWidget *parent)
	: ListEditor(parent)
{
	_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
	connect(_list, &QListWidget::currentRowChanged, this,
		&SourceInteractionStepList::RowSelected);
	SetPlaceholderText(obs_module_text(
		"AdvSceneSwitcher.action.sourceInteraction.record.placeholder"));
	SetMaxListHeight(350);
}

void SourceInteractionStepList::SetSteps(
	const std::vector<SourceInteractionStep> &steps)
{
	_steps = steps;

	_list->clear();
	for (const auto &step : _steps) {
		_list->addItem(QString::fromStdString(step.ToString()));
	}

	UpdateListSize();

	_list->setCurrentRow(-1);
}

void SourceInteractionStepList::UpdateStep(int row,
					   const SourceInteractionStep &step)
{
	if (row < 0 || row >= (int)_steps.size()) {
		return;
	}
	_steps[row] = step;
	_list->item(row)->setText(QString::fromStdString(step.ToString()));
}

void SourceInteractionStepList::Add()
{
	_steps.emplace_back();
	_list->addItem(QString::fromStdString(_steps.back().ToString()));
	QTimer::singleShot(0, this, [this]() { UpdateListSize(); });
	emit StepsChanged(_steps);
	_list->setCurrentRow((int)_steps.size() - 1);
}

void SourceInteractionStepList::Remove()
{
	const QList<QListWidgetItem *> selected = _list->selectedItems();
	if (selected.isEmpty()) {
		return;
	}
	std::vector<int> rows;
	rows.reserve(selected.size());
	for (const auto *item : selected) {
		rows.push_back(_list->row(item));
	}
	std::sort(rows.begin(), rows.end(), std::greater<int>());
	for (int row : rows) {
		if (row >= 0 && row < (int)_steps.size()) {
			_steps.erase(_steps.begin() + row);
			delete _list->takeItem(row);
		}
	}
	QTimer::singleShot(0, this, [this]() { UpdateListSize(); });
	emit StepsChanged(_steps);
}

void SourceInteractionStepList::Up()
{
	int row = _list->currentRow();
	if (row <= 0 || row >= (int)_steps.size()) {
		return;
	}
	std::swap(_steps[row], _steps[row - 1]);
	_list->insertItem(row - 1, _list->takeItem(row));
	_list->setCurrentRow(row - 1);
	emit StepsChanged(_steps);
}

void SourceInteractionStepList::Down()
{
	int row = _list->currentRow();
	if (row < 0 || row >= (int)_steps.size() - 1) {
		return;
	}
	std::swap(_steps[row], _steps[row + 1]);
	_list->insertItem(row + 1, _list->takeItem(row));
	_list->setCurrentRow(row + 1);
	emit StepsChanged(_steps);
}

} // namespace advss
