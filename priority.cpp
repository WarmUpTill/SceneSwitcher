#include <algorithm>
#include "advanced-scene-switcher.hpp"

void SceneSwitcher::on_priorityUp_clicked()
{
	int currentIndex = ui->priorityList->currentRow();
	if (currentIndex != -1 && currentIndex != 0)
	{
		ui->priorityList->insertItem(currentIndex - 1 ,ui->priorityList->takeItem(currentIndex));
		ui->priorityList->setCurrentRow(currentIndex -1);
		lock_guard<mutex> lock(switcher->m);

		iter_swap(switcher->functionNamesByPriority.begin() + currentIndex, switcher->functionNamesByPriority.begin() + currentIndex - 1);
	}
}

void SceneSwitcher::on_priorityDown_clicked()
{
	int currentIndex = ui->priorityList->currentRow();
	if (currentIndex != -1 && currentIndex != ui->priorityList->count() - 1)
	{
		ui->priorityList->insertItem(currentIndex + 1, ui->priorityList->takeItem(currentIndex));
		ui->priorityList->setCurrentRow(currentIndex + 1);
		lock_guard<mutex> lock(switcher->m);

		iter_swap(switcher->functionNamesByPriority.begin() + currentIndex, switcher->functionNamesByPriority.begin() + currentIndex + 1);
	}
}

bool prioStrValid(string s)
{
	if (s == WINDOW_TITLE_FUNC)
		return true;
	if (s == SCREEN_REGION_FUNC)
		return true;
	if (s == ROUND_TRIP_FUNC)
		return true;
	if (s == (IDLE_FUNC))
		return true;
	if (s == EXE_FUNC)
		return true;
	if (s == READ_FILE_FUNC)
		return true;
	return false;
}


bool SwitcherData::prioFuncsValid()
{
	for (string f : functionNamesByPriority)
	{
		if (!prioStrValid(f))
			return false;
	}
	return true;
}

