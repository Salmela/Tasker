#include "backend.h"
#include <algorithm>

//TaskList.cpp

void TaskList::addTask(Task *task)
{
	mTasks.push_back(task);
}

void TaskList::removeTask(Task *task)
{
	mTasks.erase(std::remove(mTasks.begin(), mTasks.end(), task),
		mTasks.end());
}

unsigned int TaskList::getSize()
{
	return mTasks.size();
}

//TaskListView.cpp


TaskListView::TaskListView(TaskList *list)
{
	mList = list;
}
