#include "backend.h"
#include <algorithm>

void TaskList::addTask(Task *task)
{
	tasks.push_back(task);
}

void TaskList::removeTask(Task *task)
{
	tasks.erase(std::remove(tasks.begin(), tasks.end(), task), tasks.end());
}

unsigned int TaskList::getSize()
{
	return tasks.size();
}
