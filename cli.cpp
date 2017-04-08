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
	mRunning = true;
	mActiveView = TASK_LIST_VIEW;
}

bool Cli::mainLoop()
{
	while(mRunning) {
		switch(mActiveView) {
		case TASK_LIST_VIEW:
			mListView.render();
			mRunning = false;
			break;
		case TASK_VIEW:
			//mView.render();
			break;
		default:
			throw CliException("Invalid view.");
		}
	}
	return true;
}

//TaskListView.cpp


TaskListView::TaskListView(TaskList *list)
{
	mList = list;
}

void TaskListView::render()
{
	std::cout << "Task list:\n";

	if (mList->getSize() == 0) {
		std::cout << "  [Empty]\n";
	}
}
