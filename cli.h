#pragma once

namespace Tasker {
namespace Cli {

static std::string openEditor(std::string text);
static std::string trim(const std::string &str);

class View;

enum TextClass {
	TASK_ID,
	TASK_NAME,
	TASK_STATE,
	TASK_STATE_CLOSED,
	TASK_LIST_HEADER,
	SUB_TASK_HEADER,
	EVENT_HEADER,
};

class CliInterface {
public:
	virtual void newView(View *view) = 0;
	//virtual void replaceView(View *view) = 0;
	virtual void deleteView(View *view) = 0;
	virtual Backend::Project *getProject() = 0;
	virtual void quit() = 0;
	virtual bool hasColor() = 0;
	virtual std::string getText(TextClass klass, std::string text) = 0;
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
	void view(CliInterface *parent);
private:
	Backend::Task *mTask;
	bool mShowView;
};

class CreateTaskView : public View
{
public:
	CreateTaskView(Backend::Task *parent = NULL);
	void render(CliInterface *parent) override;
private:
	Backend::Task *mParent;
};

class TaskListView : public View
{
public:
	TaskListView();
	~TaskListView();
	void setFilter(Backend::TaskFilter *filter);
	void render(CliInterface *parent) override;
	void view(CliInterface *parent);
private:
	Backend::TaskFilter *mFilter;
	bool mShowView;
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
	void quit() override;
	View *getActiveView();

	static std::string getLine(std::string cmd);
	static void readline(std::string cmd, std::string &command, std::vector<std::string> &args);
	bool hasColor() {return mColors;};
	std::string getText(TextClass klass, std::string text);
private:
	Backend::Project *mProject;
	TaskListView mListView;
	std::vector<View*> mViewStack;
	bool mColors;

	const char *NORMAL;
	const char *BOLD;
	const char *UNDERLINE;
	const char *INVERT;
	const char *OVERLINE;
	const char *RED;
	const char *GREEN;
	const char *BLUE;
	const char *CYAN;
};

};
};
