#pragma once

#include <string>
#include <vector>

class TaskState
{
public:
	TaskState(std::string name);
	void rename(std::string newName);
	std::string getName();
private:
	std::string mName;
};

class Task
{
public:
	Task(std::string name);

	void setName(std::string name);
	std::string getName() const;
	void setState(TaskState *state);
	TaskState *getState() const;
	void setDescription(std::string text);
	std::string getDescription() const;
private:
	std::string mName;
	std::string mDesc;
	TaskState *mState;
};

class TaskList
{
public:
	void addTask(Task *task);
	void removeTask(Task *task);
	const std::vector<Task*> all();
	unsigned int getSize();
private:
	std::vector<Task*> mTasks;
};

