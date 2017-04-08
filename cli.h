#pragma once

enum ViewType
{
	QUIT,
	TASK_LIST_VIEW,
	TASK_VIEW
};

class CliInterface {
public:
	virtual void setView(ViewType type) = 0;
};

class TaskView
{
public:
	void render(CliInterface *parent);
};

class TaskListView
{
public:
	TaskListView(TaskList *list);
	void render(CliInterface *parent);
private:
	TaskList *mList;
};

class Cli : public CliInterface
{
public:
	Cli(int argc, char **argv);

	bool mainLoop();
	void setView(ViewType type);
private:
	TaskList mList;
	TaskListView mListView;
	TaskView mView;
	ViewType mActiveView;
};
