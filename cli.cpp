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
#include <cctype>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <exception>
#include <unistd.h>

#include "backend.h"
#include "cli.h"

int main(int argc, char **argv)
{
	Tasker::Cli::Main cli;

	if(!cli.init(argc, argv)) {
		return 1;
	}

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

static std::string openEditor(std::string text) {
	const char *c_editor = getenv("EDITOR");
	std::string editor;
	if(!c_editor || !*c_editor) {
		editor = "sensible-editor";
	} else {
		editor = c_editor;
	}
	char filename[] = "/tmp/tasker.XXXXXX";
	if(mkstemp(filename) == -1) {
		std::cerr << "Unable to create tmp file.\n";
		return text;
	}
	std::ofstream stream(filename);
	stream << text;
	stream.close();

	system((editor + " " + filename).c_str());

	std::ifstream istream(filename);
	std::string content((std::istreambuf_iterator<char>(istream)),
		(std::istreambuf_iterator<char>()));
	istream.close();

	unlink(filename);
	return content;
}

// From https://stackoverflow.com/questions/ TODO find it
static std::string trim(const std::string &str)
{
	auto wsFront = std::find_if_not(str.begin(), str.end(),
		(int(&)(int))std::isspace);
	auto wsBack = std::find_if_not(str.rbegin(), str.rend(),
		(int(&)(int))std::isspace);

	return (wsBack.base() <= wsFront ?
		std::string() :
		std::string(wsFront, wsBack.base()));
}

Main::Main()
	:mListView()
{
	newView(&mListView);
}

bool Main::init(int argc, char **argv)
{
	const char *cwd = get_current_dir_name();
	mProject = Backend::Project::open(cwd);
	if(mProject) {
		return true;
	}
	std::cerr << "Tasker data not found.\n";
	while(true) {
		std::cout << "Create a new tasker repository? [y/n] ";
		std::string line;
		std::getline(std::cin, line);
		if(line == "y") {
			break;
		} else if(line == "n") {
			return false;
		}
	}
	mProject = Backend::Project::create("./");
	std::cout << "Where is the project folder? ";
	std::string src;
	std::getline(std::cin, src);

	Backend::Config::setTaskerData(cwd, src);
	return true;
}

void Main::readline(std::string cmd, std::string &command, std::vector<std::string> &args)
{
	std::cout << cmd << " ";

	std::string line;
	std::getline(std::cin, line);
	std::istringstream iss(line);

	iss >> command;

	args.clear();
	std::string arg;
	while(iss >> arg) {
		args.push_back(arg);
	}
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
	return mProject;
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
	auto taskList = mList->getFiltered(Backend::TaskFilter::isOpen(true));
	for (Backend::Task *t : taskList) {
		std::cout << " " << i++ << ". " << t->getName() << "\n";
	}

	std::string command;
	std::vector<std::string> args;

	Main::readline("Command>", command, args);

	if (command == "o" || command == "open") {
		int taskIndex;
		std::istringstream iss(args[0]);
		iss >> taskIndex;

		if (taskIndex < 1 && taskIndex > (int)taskList.size()) {
			std::cerr << "Task index is out-of-bounds.\n";
			return;
		}
		parent->newView(new TaskView(taskList[taskIndex - 1]));
	} else if (command == "n" || command == "new") {
		parent->newView(new CreateTaskView());
	} else if (command == "t" || command == "type") {
		if (args.size() != 1) {
			std::cout << "USAGE: type NAME\n";
			return;
		}
		parent->newView(new ModifyTaskTypeView(args[0]));
	} else if (command == "w" || command == "write") {
		parent->getProject()->write();
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

	std::string command;
	std::vector<std::string> args;

	Main::readline("Command>", command, args);

	if(command == "e" || command == "exit") {
		parent->deleteView(this);
	} else if(command == "c" || command == "comment") {
		//Comment
		std::cout << "Commenting not implemented yet";
	} else if(command == "s" || command == "state") {
		//Change the state
		if(args.size() != 1) {
			std::cout << "Usage: state NEW_STATE\n";
			return;
		}
		auto possibles = mTask->getType()->possibleChanges(mTask->getState());
		for(auto *state : possibles) {
			if(state->getName() == args[0]) {
				mTask->setState(state);
				return;
			}
		}
		std::cerr << "No possible state " << args[0] << "\n";
		std::cout << "Allowed next states are:\n";
		for(auto *state : possibles) {
			std::cout << "  " << state->getName() << "\n";
		}
	} else if(command == "r" || command == "rename") {
		mTask->setName(trim(openEditor(mTask->getName())));
		//Rename the task
	} else if(command == "d" || command == "description") {
		//Change description
		std::string text = openEditor(mTask->getDescription());
		mTask->setDescription(trim(text));
	}
}

};
};
