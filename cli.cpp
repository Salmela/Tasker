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

Main::~Main()
{
	quit();
	delete mProject;
}

bool Main::init(int argc, char **argv)
{
	char *cwd_tmp = get_current_dir_name();
	std::string cwd = cwd_tmp;
	std::string dir = Backend::Config::guessProjectDir(cwd);
	free(cwd_tmp);

	if(argc >= 2) {
		dir = argv[1];
	}

	mProject = Backend::Project::open(dir);
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
	if(view != &mListView) {
		delete view;
	}
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

void Main::quit()
{
	for(View *view : mViewStack) {
		if(&mListView != view) {
			delete view;
		}
	}
	mViewStack.clear();
}

/// TaskListView

TaskListView::TaskListView()
	:mFilter(Backend::TaskFilter::isOpen(true))
{
}

TaskListView::~TaskListView()
{
	delete mFilter;
}

void TaskListView::setFilter(Backend::TaskFilter *filter)
{
	delete mFilter;
	mFilter = filter;
}

void TaskListView::render(CliInterface *parent)
{
	auto *mList = parent->getProject()->getTaskList();
	std::cout << "Task list:\n";

	if (mList->getSize() == 0) {
		std::cout << "  [Empty]\n";
	}

	auto sort = [](const Backend::Task *first, const Backend::Task *second) {
		Backend::Date firstDate = first->getCreationDate();
		Backend::Date secondDate = second->getCreationDate();
		if(!first->getEvents().empty())
			firstDate = first->getEvents().back()->getCreationDate();
		if(!second->getEvents().empty())
			secondDate = second->getEvents().back()->getCreationDate();
		return firstDate > secondDate;
	};

	auto taskList = mList->getFiltered(mFilter);
	std::sort(taskList.begin(), taskList.end(), sort);
	for (Backend::Task *t : taskList) {
		std::cout << " #" << t->getId() << " " << t->getName() << "\n";
	}

	std::string command;
	std::vector<std::string> args;

	Main::readline("Command>", command, args);

	if (command == "o" || command == "open") {
		int taskIndex;
		std::istringstream iss(args[0]);
		iss >> taskIndex;

		Backend::Task *task = mList->getTask(taskIndex);
		if (!task) {
			std::cerr << "Task index is out-of-bounds.\n";
			return;
		}
		parent->newView(new TaskView(task));
	} else if (command == "n" || command == "new") {
		parent->newView(new CreateTaskView());
	} else if (command == "s" || command == "search") {
		std::ostringstream oss;
		std::string separator = "";
		for(std::string str : args) {
			oss << separator << str;
			separator = " ";
		}
		try {
			auto *search = Backend::Search::create(oss.str());
			setFilter(search);
		} catch(...) {
			std::cout << "Syntax error in search string\n";
		}
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
		auto state = Backend::TaskState::create(type, "not-started");
		type->setStartState(state);

		auto endState = Backend::TaskState::create(type, "done");
		type->setEndStates({endState});

		type->setTransition(state, endState);
	} else {
		std::cout << "Opening existing type '" << mName << "'\n";
	}
	parent->deleteView(this);
}

/// CreateListView
CreateTaskView::CreateTaskView(Backend::Task *parent)
{
	mParent = parent;
}

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

	if(mParent) {
		mParent->addSubTask(task);
	} else {
		parent->getProject()->getTaskList()->addTask(task);
	}
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
	std::ostringstream stream;
	if(mTask->getParentTask()) {
		stream << "#" << mTask->getParentTask()->getId() << "." << mTask->getId()
			<< " " << mTask->getName();
	} else {
		stream << "#" << mTask->getId()
			<< " " << mTask->getName();
	}
	std::string name = stream.str();
	std::cout << name << "\n";
	for(unsigned int i = 0; i < name.size(); i++) {
		std::cout << "=";
	}
	std::cout << "\n[" << mTask->getState()->getName() << "], created at "
		<< mTask->getCreationDate().getFormattedTime("%d.%m.%Y %H:%M:%S") << "\n";
	if(mTask->getAssigned()) {
		std::cout << "assigned to: " << mTask->getAssigned()->getName() << "\n";
	}
	std::cout << "\n" << mTask->getDescription();

	auto subTasks = mTask->getSubTasks();

	if(!subTasks.empty()) {
		std::cout << "Sub-tasks:\n";

		unsigned int index = 1;
		for(auto *task : subTasks) {
			std::cout << " " << index++ << ". ";
			std::cout << task->getName() << "\n";
		}

		std::cout << "\n";
	}

	for(auto *event : mTask->getEvents()) {
		auto *comment = dynamic_cast<Backend::CommentEvent*>(event);
		auto *stateChange = dynamic_cast<Backend::StateChangeEvent*>(event);

		std::cout << event->getCreationDate().getFormattedTime("%d.%m.%Y")
		          << " by " << event->getUser()->getName() << "\n";
		if(comment) {
			std::cout << comment->getContent();
		} else if(stateChange) {
			std::cout << "State changed from " << stateChange->from()->getName()
			          << " to " << stateChange->to()->getName() << "\n";
		} else {
			std::cout << "Unknown event\n";
		}
		std::cout << "\n";
	}

	std::string command;
	std::vector<std::string> args;

	Main::readline("Command>", command, args);

	if(command == "e" || command == "exit") {
		parent->deleteView(this);
	} else if(command == "q" || command == "quit") {
		parent->quit();
	} else if(command == "n" || command == "new") {
		parent->newView(new CreateTaskView(mTask));
	} else if(command == "o" || command == "open") {
		int taskIndex;
		std::istringstream iss(args[0]);
		iss >> taskIndex;

		taskIndex--;
		if (taskIndex < 0 || (unsigned)taskIndex >= mTask->getSubTasks().size()) {
			std::cerr << "Task index is out-of-bounds.\n";
			return;
		}
		Backend::Task *task = mTask->getSubTasks()[taskIndex];
		parent->newView(new TaskView(task));
	} else if(command == "a" || command == "assign") {
		mTask->setAssigned(parent->getProject()->getDefaultUser());
	} else if(command == "c" || command == "comment") {
		auto *comment = new Backend::CommentEvent(mTask, trim(openEditor("")));
		mTask->addEvent(comment);
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
