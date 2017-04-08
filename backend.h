#pragma once

#include <string>
#include <vector>

class TaskState
{
public:
	TaskState(std::string name);
	void rename(std::string name);
private:
	std::string name;
};

class Task
{
public:
	void setName(std::string name);
	std::string getName() const;
	void setState(TaskState *state);
	TaskState *getState() const;
	void setDescription(std::string text);
	std::string getDescription() const;
private:
	std::string name;
	std::string desc;
	TaskState *state;
};

class TaskList
{
public:
	void addTask(Task *task);
	void removeTask(Task *task);
	unsigned int getSize();
private:
	std::vector<Task*> tasks;
};


class TaskListView
{
public:
	void render();
};

class TaskView
{
public:
	void render();
};
