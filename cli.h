#pragma once

namespace Tasker {
namespace Cli {

class View;

class CliInterface {
public:
	virtual void newView(View *view) = 0;
	//virtual void replaceView(View *view) = 0;
	virtual void deleteView(View *view) = 0;
	virtual Backend::Project *getProject() = 0;
};

class View
{
public:
	virtual void render(CliInterface *parent) = 0;
};

class TaskView : public View
{
public:
	TaskView(Backend::Task *task);
	void render(CliInterface *parent) override;
private:
	Backend::Task *mTask;
};

class CreateTaskView : public View
{
public:
	void render(CliInterface *parent) override;
};

class TaskListView : public View
{
public:
	void render(CliInterface *parent) override;
};

class Main : public CliInterface
{
public:
	Main(int argc, char **argv);

	bool mainLoop();
	void newView(View *view) override;
	void deleteView(View *view) override;
	Backend::Project *getProject() override;
	View *getActiveView();
private:
	Backend::Project *project;
	TaskListView mListView;
	std::vector<View*> mViewStack;
};

};
};
