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

/// TaskState
bool createTaskState() {
	TaskState *state = TaskState::create("test");
	state->ref();
	state->unref();
	bool res = (state && "test" == state->getName());
	state->free();
	return res;
}

bool createTaskStateAndDelete() {
	TaskState *state = TaskState::create("test");
	state->free();
	return true;
}

bool createRefTaskStateAndDelete() {
	TaskState *state = TaskState::create("test");
	state->ref();
	state->free();
	bool res = (state && "test" == state->getName());
	state->unref();
	return res;
}

/// TaskType

bool createTaskTypeAndDelete() {
	TaskType type("test");

	TaskState *state = TaskState::create("start");
	TaskState *endState = TaskState::create("end");

	type.setStartState(state);
	type.setEndStates({endState});
	type.setTransition(state, endState);

	bool res = type.canChange(state, endState);
	res &= !type.canChange(endState, state);

	auto ends = type.possibleChanges(state);
	res &= ends.size() == 1;
	res &= ends.find(endState) != ends.end();
	res &= type.isClosed(endState);

	delete state;
	delete endState;

	return res;
}

/// TaskList

bool addAndRemoveTask() {
	TaskList list;
	Task *task = new Task("test");

	list.addTask(task);
	list.removeTask(task);

	delete task;

	return list.getSize() == 0;
}

int main(int argc, char **argv)
{
	bool success;
	success = createTaskState();
	std::cout << (success ? "Success" : "Failure") << "\n";
	success = createTaskStateAndDelete();
	std::cout << (success ? "Success" : "Failure") << "\n";
	success = createRefTaskStateAndDelete();
	std::cout << (success ? "Success" : "Failure") << "\n";

	success = createTaskTypeAndDelete();
	std::cout << (success ? "Success" : "Failure") << "\n";

	success = addAndRemoveTask();
	std::cout << (success ? "Success" : "Failure") << "\n";
	return 0;
}
