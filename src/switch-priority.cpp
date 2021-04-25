#include <algorithm>

#include "headers/advanced-scene-switcher.hpp"

void AdvSceneSwitcher::on_threadPriority_currentTextChanged(const QString &text)
{
	if (loading || ui->threadPriority->count() !=
			       (int)switcher->threadPriorities.size())
		return;

	std::lock_guard<std::mutex> lock(switcher->m);

	for (auto p : switcher->threadPriorities) {
		if (p.name == text.toUtf8().constData()) {
			switcher->threadPriority = p.value;
			break;
		}
	}
}

void AdvSceneSwitcher::on_priorityUp_clicked()
{
	int currentIndex = ui->priorityList->currentRow();
	if (currentIndex != -1 && currentIndex != 0) {
		ui->priorityList->insertItem(
			currentIndex - 1,
			ui->priorityList->takeItem(currentIndex));
		ui->priorityList->setCurrentRow(currentIndex - 1);
		std::lock_guard<std::mutex> lock(switcher->m);

		iter_swap(switcher->functionNamesByPriority.begin() +
				  currentIndex,
			  switcher->functionNamesByPriority.begin() +
				  currentIndex - 1);
	}
}

void AdvSceneSwitcher::on_priorityDown_clicked()
{
	int currentIndex = ui->priorityList->currentRow();
	if (currentIndex != -1 &&
	    currentIndex != ui->priorityList->count() - 1) {
		ui->priorityList->insertItem(
			currentIndex + 1,
			ui->priorityList->takeItem(currentIndex));
		ui->priorityList->setCurrentRow(currentIndex + 1);
		std::lock_guard<std::mutex> lock(switcher->m);

		iter_swap(switcher->functionNamesByPriority.begin() +
				  currentIndex,
			  switcher->functionNamesByPriority.begin() +
				  currentIndex + 1);
	}
}

bool SwitcherData::prioFuncsValid()
{
	auto fNBPCopy = functionNamesByPriority;

	std::sort(fNBPCopy.begin(), fNBPCopy.end());
	auto it = std::unique(fNBPCopy.begin(), fNBPCopy.end());
	bool wasUnique = (it == fNBPCopy.end());

	if (!wasUnique) {
		return false;
	}

	for (int p : functionNamesByPriority) {
		if (p < 0 || p > 10) {
			return false;
		}
	}
	return true;
}
