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

#include "backend.h"
#include "cli.h"

Cli::Cli(int argc, char **argv)
	:mView(&mList)
{
}

bool Cli::mainLoop()
{
	while(mRunning) {
		//switch() {
			mView.render();
		//}
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
	std::cout << "Task list:";
}
