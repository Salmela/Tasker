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
#include <unistd.h>

#include "backend.h"

namespace Tasker {

bool dateTest() {
	bool res = true;

	Backend::Date d("2017-04-23T14:51:00Z");
	res &= d.getMachineTime() == "2017-04-23T14:51:00Z";

	res &= d.getFormattedTime("Year %Y") == "Year 2017";

	return res;
}

bool createProject() {
	char file[] = "/tmp/tasker-create-project-XXXXXX";
	if(!mkdtemp(file)) return false;
	auto *project = Backend::Project::create(file);
	bool res = project->getTaskList();
	res &= !project->getType("xyz");
	delete project;

	rmdir(file);

	return res;//the dir must be empty
}

bool openProject()
{
	char file[] = "/tmp/tasker-open-project-XXXXXX";
	if(!mkdtemp(file)) return false;
	auto *project = Backend::Project::create(file);
	bool res = project->getTaskList();
	project->write();
	delete project;

	project = Backend::Project::open(file);
	res &= project && project->getTaskList();
	delete project;

	rmdir(file);//the dir must be empty

	return res;
}

/// TaskState
bool createTaskState() {
	auto *state = Backend::TaskState::create(NULL, "test");
	state->ref();
	state->unref();
	bool res = (state && "test" == state->getName());
	state->free();
	return res;
}

bool createTaskStateAndDelete() {
	auto *state = Backend::TaskState::create(NULL, "test");
	state->free();
	return true;
}

bool createRefTaskStateAndDelete() {
	auto *state = Backend::TaskState::create(NULL, "test");
	state->ref();
	state->free();
	bool res = (state && "test" == state->getName());
	state->unref();
	return res;
}

/// TaskType

bool createTaskTypeAndDelete() {
	Backend::TaskType type(NULL, "test");

	auto *state = Backend::TaskState::create(&type, "start");
	auto *endState = Backend::TaskState::create(&type, "end");

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
	Backend::Project project;
	Backend::TaskList *list = project.getTaskList();

	Backend::TaskType type(NULL, "type");

	auto *state = Backend::TaskState::create(&type, "start");
	auto *endState = Backend::TaskState::create(&type, "end");

	type.setStartState(state);
	type.setEndStates({endState});
	type.setTransition(state, endState);

	auto *task = new Backend::Task(&project, "test");
	task->setType(&type);
	list->addTask(task);

	task = new Backend::Task(&project, "closed");
	task->setType(&type);
	task->setState(endState);
	list->addTask(task);

	bool res = true;

	auto *filter = Backend::TaskFilter::hasState("start");
	auto tasks = list->getFiltered(filter);
	res &= tasks.size() == 1;
	delete filter;

	filter = Backend::TaskFilter::isOpen(false);
	tasks = list->getFiltered(filter);
	res &= tasks.size() == 1;
	delete filter;

	auto *f1 = Backend::TaskFilter::isOpen(true);
	auto *f2 = Backend::TaskFilter::hasState("end");
	auto *filter2 = Backend::TaskFilter::orOf(f1, f2);
	tasks = list->getFiltered(filter2);
	res &= tasks.size() == 2;

	filter = Backend::TaskFilter::andOf(f1->clone(), f2->clone());
	tasks = list->getFiltered(filter);
	res &= tasks.size() == 0;
	delete filter;

	auto f3 = Backend::TaskFilter::notOf(f1->clone());
	filter = Backend::TaskFilter::andOf(f3, f2->clone());
	tasks = list->getFiltered(filter);
	res &= tasks.size() == 1;
	delete filter;
	delete filter2;

	return res;
}

bool searchTasks() {
	Backend::Project project;
	Backend::TaskList *list = project.getTaskList();

	auto *type = new Backend::TaskType(&project, "type");

	auto *state = Backend::TaskState::create(type, "start");
	auto *endState = Backend::TaskState::create(type, "end");

	type->setStartState(state);
	type->setEndStates({endState});
	type->setTransition(state, endState);

	auto *task = new Backend::Task(&project, "test lol");
	task->setType(type);
	list->addTask(task);

	auto *task2 = new Backend::Task(&project, "closed lol");
	task2->setType(type);
	task2->setState(endState);
	list->addTask(task2);

	auto *task3 = new Backend::Task(&project, "closed");
	task3->setType(type);
	task3->setState(endState);
	list->addTask(task3);

	bool res = true;

	auto search = Backend::Search::create("\"test\"");
	auto tasks = list->getFiltered(search);
	res &= tasks.size() == 1;
	res &= tasks[0] == task;
	delete search;

	search = Backend::Search::create("- \"test\"");
	tasks = list->getFiltered(search);
	res &= tasks.size() == 2;
	res &= tasks[0] == task2;
	res &= tasks[1] == task3;
	delete search;

	search = Backend::Search::create("\"test\" and \"closed\"");
	tasks = list->getFiltered(search);
	res &= tasks.size() == 0;
	delete search;

	search = Backend::Search::create("\"test\" or \"closed\"");
	tasks = list->getFiltered(search);
	res &= tasks.size() == 3;
	delete search;

	search = Backend::Search::create("\"lol\" and not \"test\"");
	tasks = list->getFiltered(search);
	res &= tasks.size() == 1;
	res &= tasks[0] == task2;
	delete search;

	search = Backend::Search::create("not \"test\" and \"lol\"");
	tasks = list->getFiltered(search);
	res &= tasks.size() == 1;
	res &= tasks[0] == task2;
	delete search;

	search = Backend::Search::create("not (\"test\" and \"lol\")");
	tasks = list->getFiltered(search);
	res &= tasks.size() == 2;
	res &= tasks[0] == task2;
	res &= tasks[1] == task3;
	delete search;

	search = Backend::Search::create("open");
	tasks = list->getFiltered(search);
	res &= tasks.size() == 1;
	res &= tasks[0] == task;
	delete search;

	search = Backend::Search::create("closed");
	tasks = list->getFiltered(search);
	res &= tasks.size() == 2;
	res &= tasks[0] == task2;
	res &= tasks[1] == task3;
	delete search;

	search = Backend::Search::create("state(\"end\")");
	tasks = list->getFiltered(search);
	res &= tasks.size() == 2;
	res &= tasks[0] == task2;
	res &= tasks[1] == task3;
	delete search;

	return res;
}

bool taskEvents()
{
	Backend::Project project;
	Backend::TaskList *list = project.getTaskList();
	bool res = true;

	auto *type = new Backend::TaskType(&project, "type");

	auto *state = Backend::TaskState::create(type, "start");
	auto *endState = Backend::TaskState::create(type, "end");

	type->setStartState(state);
	type->setEndStates({endState});
	type->setTransition(state, endState);

	auto *task = new Backend::Task(&project, "test");
	task->setType(type);
	list->addTask(task);

	auto *event = new Backend::CommentEvent(task, "Hello");
	task->addEvent(event);

	std::vector<Backend::TaskEvent*> events = task->getEvents();
	res &= events.size() == 1;
	auto *e = dynamic_cast<Backend::CommentEvent*>(events[0]);
	if(!e) return false;
	res &= e->getContent() == "Hello";

	//state change
	task = new Backend::Task(&project, "test2");
	task->setType(type);
	list->addTask(task);

	task->setState(endState);

	events = task->getEvents();
	res &= events.size() == 1;
	auto *e2 = dynamic_cast<Backend::StateChangeEvent*>(events[0]);
	if(!e2) return false;
	res &= e2->from() == state;
	res &= e2->to() == endState;

	return res;
}

bool openProjectPerf()
{
	auto *project = Backend::Project::open("/home/me/sources/tasker");
	bool res = project && project->getTaskList();
	return res;
}

int testMain()
{
//#define PERF
#ifdef PERF
	return !openProjectPerf();
#else
	bool success;

	success = dateTest();
	std::cout << (success ? "Success" : "Failure") << "\n";

	success = createProject();
	std::cout << (success ? "Success" : "Failure") << "\n";

	success = openProject();
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

	success = taskEvents();
	std::cout << (success ? "Success" : "Failure") << "\n";
	return 0;
#endif
}

};

int main(int argc, char **argv)
{
	return Tasker::testMain();
}
