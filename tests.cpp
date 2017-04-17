/* Unit tests for the backend.
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

#include "backend.h"

namespace Tasker {

bool createProject() {
	auto *project = Backend::Project::create("./");
	bool res = project->getTaskList();
	res &= !project->getType("xyz");
	delete project;

	return res;
}

/// TaskState
bool createTaskState() {
	auto *state = Backend::TaskState::create("test");
	state->ref();
	state->unref();
	bool res = (state && "test" == state->getName());
	state->free();
	return res;
}

bool createTaskStateAndDelete() {
	auto *state = Backend::TaskState::create("test");
	state->free();
	return true;
}

bool createRefTaskStateAndDelete() {
	auto *state = Backend::TaskState::create("test");
	state->ref();
	state->free();
	bool res = (state && "test" == state->getName());
	state->unref();
	return res;
}

/// TaskType

bool createTaskTypeAndDelete() {
	Backend::TaskType type(NULL, "test");

	auto *state = Backend::TaskState::create("start");
	auto *endState = Backend::TaskState::create("end");

	type.setStartState(state);
	type.setEndStates({endState});
	type.setTransition(state, endState);

	bool res = type.canChange(state, endState);
	res &= !type.canChange(endState, state);

	auto ends = type.possibleChanges(state);
	res &= ends.size() == 1;
	res &= ends.find(endState) != ends.end();
	res &= type.isClosed(endState);
	res &= !type.isIncomplete();

	return res;
}

bool createTaskTypeForProject() {
	Backend::Project project;

	auto *type = new Backend::TaskType(&project, "test");

	bool res = project.getType("test") == type;

	return res;
}

/// TaskList

bool addAndRemoveTask() {
	Backend::TaskList list;
	auto *task = new Backend::Task(NULL, "test");

	list.addTask(task);
	list.removeTask(task);

	delete task;

	return list.getSize() == 0;
}

bool filterTasks() {
	Backend::TaskList list;

	Backend::TaskType type(NULL, "type");

	auto *state = Backend::TaskState::create("start");
	auto *endState = Backend::TaskState::create("end");

	type.setStartState(state);
	type.setEndStates({endState});
	type.setTransition(state, endState);

	auto *task = new Backend::Task(NULL, "test");
	task->setType(&type);
	list.addTask(task);

	task = new Backend::Task(NULL, "closed");
	task->setType(&type);
	task->setState(endState);
	list.addTask(task);

	bool res = list.getSize() == 2;

	auto tasks = list.getFiltered(Backend::TaskFilter::hasState("start"));
	res &= tasks.size() == 1;

	tasks = list.getFiltered(Backend::TaskFilter::isOpen(false));
	res &= tasks.size() == 1;

	auto f1 = Backend::TaskFilter::isOpen(true);
	auto f2 = Backend::TaskFilter::hasState("end");
	tasks = list.getFiltered(Backend::TaskFilter::orOf(&f1, &f2));
	res &= tasks.size() == 2;

	tasks = list.getFiltered(Backend::TaskFilter::andOf(&f1, &f2));
	res &= tasks.size() == 0;

	auto f3 = Backend::TaskFilter::notOf(&f1);
	tasks = list.getFiltered(Backend::TaskFilter::andOf(&f3, &f2));
	res &= tasks.size() == 1;

	return res;
}

bool searchTasks() {
	Backend::TaskList list;

	Backend::TaskType type(NULL, "type");

	auto *state = Backend::TaskState::create("start");
	auto *endState = Backend::TaskState::create("end");

	type.setStartState(state);
	type.setEndStates({endState});
	type.setTransition(state, endState);

	auto *task = new Backend::Task(NULL, "test lol");
	task->setType(&type);
	list.addTask(task);

	auto *task2 = new Backend::Task(NULL, "closed lol");
	task2->setType(&type);
	task2->setState(endState);
	list.addTask(task2);

	auto *task3 = new Backend::Task(NULL, "closed");
	task3->setType(&type);
	task3->setState(endState);
	list.addTask(task3);

	bool res = true;

	auto search = Backend::Search::create("\"test\"");
	auto tasks = list.getFiltered(search);
	res &= tasks.size() == 1;
	res &= tasks[0] == task;

	search = Backend::Search::create("- \"test\"");
	tasks = list.getFiltered(search);
	res &= tasks.size() == 2;
	res &= tasks[0] == task2;
	res &= tasks[1] == task3;

	search = Backend::Search::create("\"test\" and \"closed\"");
	tasks = list.getFiltered(search);
	res &= tasks.size() == 0;

	search = Backend::Search::create("\"test\" or \"closed\"");
	tasks = list.getFiltered(search);
	res &= tasks.size() == 3;

	search = Backend::Search::create("\"lol\" and not \"test\"");
	tasks = list.getFiltered(search);
	res &= tasks.size() == 1;
	res &= tasks[0] == task2;

	search = Backend::Search::create("not \"test\" and \"lol\"");
	tasks = list.getFiltered(search);
	res &= tasks.size() == 1;
	res &= tasks[0] == task2;

	search = Backend::Search::create("not (\"test\" and \"lol\")");
	tasks = list.getFiltered(search);
	res &= tasks.size() == 2;
	res &= tasks[0] == task2;
	res &= tasks[1] == task3;

	search = Backend::Search::create("open");
	tasks = list.getFiltered(search);
	res &= tasks.size() == 1;
	res &= tasks[0] == task;

	search = Backend::Search::create("closed");
	tasks = list.getFiltered(search);
	res &= tasks.size() == 2;
	res &= tasks[0] == task2;
	res &= tasks[1] == task3;

	search = Backend::Search::create("state(\"end\")");
	tasks = list.getFiltered(search);
	res &= tasks.size() == 2;
	res &= tasks[0] == task2;
	res &= tasks[1] == task3;

	return res;
}

int testMain()
{
	bool success;

	success = createProject();
	std::cout << (success ? "Success" : "Failure") << "\n";

	success = createTaskState();
	std::cout << (success ? "Success" : "Failure") << "\n";
	success = createTaskStateAndDelete();
	std::cout << (success ? "Success" : "Failure") << "\n";
	success = createRefTaskStateAndDelete();
	std::cout << (success ? "Success" : "Failure") << "\n";

	success = createTaskTypeAndDelete();
	std::cout << (success ? "Success" : "Failure") << "\n";
	success = createTaskTypeForProject();
	std::cout << (success ? "Success" : "Failure") << "\n";

	success = addAndRemoveTask();
	std::cout << (success ? "Success" : "Failure") << "\n";
	success = filterTasks();
	std::cout << (success ? "Success" : "Failure") << "\n";
	success = searchTasks();
	std::cout << (success ? "Success" : "Failure") << "\n";
	return 0;
}

};

int main(int argc, char **argv)
{
	return Tasker::testMain();
}
