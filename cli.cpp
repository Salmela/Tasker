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
#include <string>
#include <exception>

#include "backend.h"
#include "cli.h"

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

	std::string command;
	std::cin >> command;

	if (command == "o" || command == "open") {
		int taskIndex;
		std::cin >> taskIndex;
		//parent->newView(new TaskView(taskIndex));
	} else if (command == "n" || command == "new") {
		parent->newView(new CreateTaskView());
	} else if (command == "q" || command == "quit") {
		parent->deleteView(this);
	}
}

//CreateListView.cpp
void CreateTaskView::render(CliInterface *parent)
{
	std::string name;
	std::cout << "Name: ";
	std::cin >> name;

	parent->getTaskList()->addTask(new Task(name));
	parent->deleteView(this);
}
