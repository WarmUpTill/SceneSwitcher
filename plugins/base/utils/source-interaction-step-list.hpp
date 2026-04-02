#pragma once
#include "list-editor.hpp"
#include "source-interaction-step.hpp"

#include <vector>

namespace advss {

class SourceInteractionStepList : public ListEditor {
	Q_OBJECT
public:
	SourceInteractionStepList(QWidget *parent = nullptr);

	void SetSteps(const std::vector<SourceInteractionStep> &steps);
	void UpdateStep(int row, const SourceInteractionStep &step);
	int CurrentRow() const { return _list->currentRow(); }
	void SetCurrentRow(int row) { _list->setCurrentRow(row); }
	void Clear() const { _list->clear(); }
	void HideControls() const { _controls->hide(); }
	void Insert(const QString &value) const { _list->addItem(value); }
	void ScrollToBottom() const { _list->scrollToBottom(); }
	void AddControlWidget(QWidget *widget) { _controls->AddWidget(widget); }

signals:
	void StepsChanged(const std::vector<SourceInteractionStep> &);
	void RowSelected(int row);

private slots:
	void Add() override;
	void Remove() override;
	void Up() override;
	void Down() override;

private:
	std::vector<SourceInteractionStep> _steps;
};

} // namespace advss
