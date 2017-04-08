#pragma once

enum ViewType
{
	TASK_LIST_VIEW,
	TASK_VIEW
};

class TaskView
{
public:
	void render();
};

class TaskListView
{
public:
	TaskListView(TaskList *list);
	void render();
private:
	TaskList *mList;
};

class Cli
{
public:
	Cli(int argc, char **argv);

	bool mainLoop();
private:
	TaskList mList;
	TaskListView mListView;
	TaskView mView;
	ViewType mActiveView;
	bool mRunning;
};
