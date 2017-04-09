#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>

class Task;

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
	TaskType(std::string name);
	void rename(std::string newName);
	std::string getName() const;
	void setStartState(TaskState *state);
	void setEndStates(std::set<TaskState*> states);
	void setTransition(TaskState *from, TaskState *to, bool create = true);

	bool canChange(TaskState *from, TaskState *to) const;
	const std::set<TaskState*> possibleChanges(TaskState *from) const;
	bool isClosed(TaskState *state) const;

private:
	std::string  mName;
	TaskState   *mStartState;
	std::set<TaskState*> mEndStates;
	std::map<TaskState*, std::set<TaskState*> > mStateMap;
};

class Task
{
public:
	Task(std::string name);

	void setName(std::string name);
	std::string getName() const;
	bool setState(TaskState *state);
	TaskState *getState() const;
	void setDescription(std::string text);
	std::string getDescription() const;

	bool isClosed();
private:
	std::string mName;
	std::string mDesc;
	TaskState *mState;
	bool mClosed;

	std::vector<Task> mSubTask;
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
