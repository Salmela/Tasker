#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>

#include "fjson/fjson.h"

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

	static TaskState *read(FJson::Reader *reader);
	void write(FJson::Writer *writer) const;
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

	static TaskType *read(Project *project, FJson::Reader *reader);
	void write(FJson::Writer *writer) const;
	int getStateId(TaskState *state) const;
	TaskState *getStateById(unsigned int index) const;

private:
	Project *mProject;
	std::string mName;
	bool mIsDeleted;
	TaskState *mStartState;
	std::set<TaskState*> mEndStates;
	std::map<TaskState*, std::set<TaskState*> > mStateMap;
	std::vector<TaskState*> mStates;
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
	static Task *read(Project *project, FJson::Reader *reader);
	void write(FJson::Writer *writer) const;

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
	~Project();
	TaskType *getType(std::string name);
	TaskList *getTaskList();

	void write();//< TODO make private
private:
	std::string mDirname;
	std::map<std::string, TaskType*> mTypes;
	TaskList mList;

	bool read();

	friend TaskType;
};

class Config//< TODO this object should be thread safe
{
public:
	static std::string getTaskerData(std::string path);
	static void setTaskerData(std::string path, std::string source);
private:
	Config();
	static Config mConfig;

	struct Repository {
		std::string data;
		std::string source;
	};

	void readHomeConfig();
	void readRepository(FJson::Reader &reader);

	void addRepository(std::string source, std::string data);

	std::vector<Repository*> mRepositories;
};

};
};
