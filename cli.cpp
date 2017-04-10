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
	Tasker::Cli::Main cli(argc, argv);

	return (int)!cli.mainLoop();
}

namespace Tasker {
namespace Cli {

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

Main::Main(int argc, char **argv)
	:mListView()
{
	project = Backend::Project::open("./");
	newView(&mListView);
}

void Main::newView(View *view)
{
	mViewStack.push_back(view);
}

void Main::deleteView(View *view)
{
	if (mViewStack.back() != view) {
		throw CliException("You can only delete the top item");
	}

	mViewStack.pop_back();
}

bool Main::mainLoop()
{
	while(!mViewStack.empty()) {
		View *activeView = getActiveView();
		activeView->render(this);
	}
	return true;
}

View *Main::getActiveView()
{
	return mViewStack.back();
}

Backend::Project *Main::getProject()
{
	return project;
}

/// TaskListView

void TaskListView::render(CliInterface *parent)
{
	auto *mList = parent->getProject()->getTaskList();
	std::cout << "Task list:\n";

	if (mList->getSize() == 0) {
		std::cout << "  [Empty]\n";
	}

	unsigned int i = 1;
	for (Backend::Task *t : mList->all()) {
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
		std::istringstream iss(args[0]);
		iss >> taskIndex;

		if (taskIndex < 1 && taskIndex > (int)mList->getSize()) {
			std::cerr << "Task index is out-of-bounds.\n";
			return;
		}
		parent->newView(new TaskView(mList->all()[taskIndex - 1]));
	} else if (command == "n" || command == "new") {
		parent->newView(new CreateTaskView());
	} else if (command == "t" || command == "type") {
		if (args.size() != 1) {
			std::cout << "USAGE: type NAME\n";
			return;
		}
		parent->newView(new ModifyTaskTypeView(args[0]));
	} else if (command == "q" || command == "quit") {
		parent->deleteView(this);
	}
}

/// ModifyTaskTypeView

ModifyTaskTypeView::ModifyTaskTypeView(std::string name)
{
	mName = name;
}

void ModifyTaskTypeView::render(CliInterface *parent)
{
	auto *type = parent->getProject()->getType(mName);

	if(!type) {
		std::cout << "Creating new type '" << mName << "'\n";
		type = new Backend::TaskType(parent->getProject(), mName);
		auto state = Backend::TaskState::create("not-started");
		type->setStartState(state);

		auto endState = Backend::TaskState::create("done");
		type->setEndStates({endState});

		type->setTransition(state, endState);
	} else {
		std::cout << "Opening existing type '" << mName << "'\n";
	}
	parent->deleteView(this);
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

	auto *prj = parent->getProject();
	auto *task_type = prj->getType(type);
	if(!task_type) {
		// TODO implement recomendations
		std::cerr << "Type is unknown.\n";
		parent->deleteView(this);
		return;
	}

	auto *task = new Backend::Task(prj, name);
	task->setType(task_type);

	parent->getProject()->getTaskList()->addTask(task);
	parent->deleteView(this);
	parent->newView(new TaskView(task));
}

//TaskView.cpp
TaskView::TaskView(Backend::Task *task)
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
	std::cout << "\n[" << mTask->getState()->getName() << "]\n";
	std::cout << "\n" << mTask->getDescription() << "\n";

	std::cout << "Command> ";

	std::string command;
	std::getline(std::cin, command);
	if(command == "e" || command == "exit") {
		parent->deleteView(this);
	}
}

};
};
