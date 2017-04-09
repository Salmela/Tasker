/* Commandline interface for tasker.
 *
 * Copyright (C) 2017 Aleksi Salmela
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#include <iostream>
#include <sstream>
#include <string>
#include <exception>

#include "backend.h"
#include "cli.h"

int main(int argc, char **argv)
{
	Cli cli(argc, argv);

	return (int)!cli.mainLoop();
}

class CliException : public std::exception {
public:
	CliException(std::string message) {
		mMessage = message.c_str();
	}
	CliException(const char *message) {
		mMessage = message;
	}

	const char *what() const throw() {
		return mMessage;
	}
private:
	const char *mMessage;
};

Cli::Cli(int argc, char **argv)
	:mListView(&mList)
{
	newView(&mListView);
}

void Cli::newView(View *view)
{
	mViewStack.push_back(view);
}

void Cli::deleteView(View *view)
{
	if (mViewStack.back() != view) {
		throw CliException("You can only delete the top item");
	}

	mViewStack.pop_back();
}

bool Cli::mainLoop()
{
	while(!mViewStack.empty()) {
		View *activeView = getActiveView();
		activeView->render(this);
	}
	return true;
}

View *Cli::getActiveView()
{
	return mViewStack.back();
}

TaskList *Cli::getTaskList()
{
	return &mList;
}

//TaskListView.cpp

TaskListView::TaskListView(TaskList *list)
{
	mList = list;
}

void TaskListView::render(CliInterface *parent)
{
	std::cout << "Task list:\n";

	if (mList->getSize() == 0) {
		std::cout << "  [Empty]\n";
	}

	unsigned int i = 1;
	for (Task *t : mList->all()) {
		std::cout << " " << i++ << ". " << t->getName() << "\n";
	}
	std::cout << "Command> ";

	std::string line;
	std::getline(std::cin, line);
	std::istringstream iss(line);

	std::string command;
	iss >> command;

	std::vector<std::string> args;
	std::string arg;
	while(iss >> arg) {
		args.push_back(arg);
	}

	if (command == "o" || command == "open") {
		int taskIndex;
		std::cin >> taskIndex;
		if (taskIndex < 1 && taskIndex > (int)mList->getSize()) {
			std::cerr << "Task index is out-of-bounds.\n";
			return;
		}
		parent->newView(new TaskView(mList->all()[taskIndex - 1]));
	} else if (command == "n" || command == "new") {
		parent->newView(new CreateTaskView());
	} else if (command == "q" || command == "quit") {
		parent->deleteView(this);
	}
}

/// CreateListView
void CreateTaskView::render(CliInterface *parent)
{
	std::string name, type;

	std::cout << "Name: ";
	std::getline(std::cin, name);

	std::cout << "Type: ";
	std::getline(std::cin, type);
	std::cout << "\n";
	//TODO handle type

	Task *task = new Task(name);
	parent->getTaskList()->addTask(task);
	parent->deleteView(this);
	parent->newView(new TaskView(task));
}

//TaskView.cpp
TaskView::TaskView(Task *task)
{
	mTask = task;
}

void TaskView::render(CliInterface *parent)
{
	std::string name = mTask->getName();
	std::cout << name << "\n";
	for(unsigned int i = 0; i < name.size(); i++) {
		std::cout << "=";
	}
	std::cout << "\n" << mTask->getDescription() << "\n";

	std::cout << "Command> ";

	std::string command;
	std::cin >> command;
	if (command == "e" || command == "exit") {
		parent->deleteView(this);
	}
}
