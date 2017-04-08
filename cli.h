#pragma once

class View;

class CliInterface {
public:
	virtual TaskList *getTaskList() = 0;
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
	void render(CliInterface *parent) override;
};

class CreateTaskView : public View
{
public:
	void render(CliInterface *parent) override;
};

class TaskListView : public View
{
public:
	TaskListView(TaskList *list);
	void render(CliInterface *parent) override;
private:
	TaskList *mList;
};

class Cli : public CliInterface
{
public:
	Cli(int argc, char **argv);

	bool mainLoop();
	void newView(View *view) override;
	void deleteView(View *view) override;
	View *getActiveView();

	TaskList *getTaskList() override;
private:
	TaskList mList;
	TaskListView mListView;
	std::vector<View*> mViewStack;
};
