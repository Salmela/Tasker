#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>

namespace Tasker {
namespace Backend {

class Task;
class Project;

/* The task state should be feature of TaskType */
class TaskState
{
public:
	static TaskState *create(std::string name);

	void rename(std::string newName);
	std::string getName() const;
	void ref();
	void unref();
	void free();
private:
	TaskState(std::string name);

	std::string mName;
	int mRefCount;
	bool mIsDeleted;
};

class TaskType
{
public:
	TaskType(Project *project, std::string name);
	void rename(std::string newName);
	std::string getName() const;
	void setStartState(TaskState *state);
	TaskState *getStartState() const;
	void setEndStates(std::set<TaskState*> states);
	void setTransition(TaskState *from, TaskState *to, bool create = true);

	bool canChange(TaskState *from, TaskState *to) const;
	const std::set<TaskState*> possibleChanges(TaskState *from) const;
	bool isClosed(TaskState *state) const;
	bool isIncomplete() const;

private:
	Project *mProject;
	std::string mName;
	TaskState *mStartState;
	bool mIsDeleted;
	std::set<TaskState*> mEndStates;
	std::map<TaskState*, std::set<TaskState*> > mStateMap;
};

class Task
{
public:
	Task(Project *project, std::string name);

	void setName(std::string name);
	void setDescription(std::string text);
	void setType(TaskType *type);
	bool setState(TaskState *state);

	std::string getName() const;
	std::string getDescription() const;
	TaskType *getType() const;
	TaskState *getState() const;

	bool isClosed();
private:
	Project *mProject;
	std::string mName;
	std::string mDesc;
	TaskType *mType;
	TaskState *mState;
	bool mClosed;

	std::vector<Task*> mSubTask;
};

class TaskList
{
public:
	void addTask(Task *task);
	void removeTask(Task *task);
	const std::vector<Task*> all();
	unsigned int getSize() const;
private:
	std::vector<Task*> mTasks;
};

class Project
{
public:
	static Project *create(const char *dirname);
	static Project *open(const char *dirname);

	Project(); //< open for tests
	TaskType *getType(std::string name);
	TaskList *getTaskList();
private:
	std::map<std::string, TaskType*> mTypes;
	TaskList mList;

	friend TaskType;
};

};
};
