#pragma once

class View;

class CliInterface {
public:
	virtual void newView(View *view) = 0;
	//virtual void replaceView(View *view) = 0;
	virtual void deleteView(View *view) = 0;
};

class View
{
public:
	virtual void render(CliInterface *parent) = 0;
};

class TaskView : public View
{
public:
	void render(CliInterface *parent);
};

class CreateTaskView : public View
{
public:
	void render(CliInterface *parent);
};

class TaskListView : public View
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
	void newView(View *view);
	void deleteView(View *view);
	View *getActiveView();
private:
	TaskList mList;
	TaskListView mListView;
	std::vector<View*> mViewStack;
};
