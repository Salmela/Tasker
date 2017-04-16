#pragma once

namespace Tasker {
namespace Cli {

static std::string openEditor(std::string text);
static std::string trim(const std::string &str);

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
	virtual ~View() {};
	virtual void render(CliInterface *parent) = 0;
};

class ModifyTaskTypeView : public View
{
public:
	ModifyTaskTypeView(std::string name);
	void render(CliInterface *parent) override;
private:
	std::string mName;
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
	Main();
	~Main();
	bool init(int argc, char **argv);

	bool mainLoop();
	void newView(View *view) override;
	void deleteView(View *view) override;
	Backend::Project *getProject() override;
	View *getActiveView();

	static void readline(std::string cmd, std::string &command, std::vector<std::string> &args);
private:
	Backend::Project *mProject;
	TaskListView mListView;
	std::vector<View*> mViewStack;
};

};
};
