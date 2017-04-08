#include "backend.h"
#include <iostream>

/// Task list

bool addAndRemoveTask() {
	TaskList list;
	Task *task = new Task();

	list.addTask(task);
	list.removeTask(task);

	delete task;

	return list.getSize() == 0;
}


int main(int argc, char **argv)
{
	bool success;
	success = addAndRemoveTask();
	std::cout << (success ? "Success" : "Failure") << "\n";
	return 0;
}
