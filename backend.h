#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>

#include "fjson/fjson.h"

namespace Tasker {
namespace Backend {

class Task;
class TaskType;
class Project;
class GitBackend;

class User
{
public:
	static User *ANONYMOUS;

	User(std::string name) :mName(name) {};
	std::string getName() const;
	std::string getEmail() const;

	static User *read(FJson::Reader &in);
	void write(FJson::Writer &out) const;
private:
	std::string mName;
	std::string mEmail;
	std::vector<std::string> mNickNames;
	static User ANONYMOUS_VALUE;
	FJson::TokenCache mForeignKeys;
};

class TaskState
{
public:
	static TaskState *create(TaskType *type, std::string name);
	static const unsigned int INVALID_ID = 0xffffffff;

	void ownedBy(const TaskType *type);
	void rename(std::string newName);
	std::string getName() const;
	unsigned int getId() const;
	void ref();
	void unref();
	void free();

	static TaskState *read(TaskType *type, FJson::Reader &in);
	void write(FJson::Writer &out) const;
private:
	TaskState(TaskType *type, std::string name);
	TaskState(TaskType *type, std::string name, unsigned int id);

	TaskType *mType;
	std::string mName;
	unsigned int mId;
	int mRefCount;
	bool mIsDeleted;
	FJson::TokenCache mForeignKeys;
};

class TaskType
{
public:
	TaskType(Project *project, std::string name);
	~TaskType();
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

	static TaskType *read(Project *project, FJson::Reader &in);
	void write(FJson::Writer &out) const;
	TaskState *getStateById(unsigned int index) const;

	unsigned int useNextStateId(TaskState *state);
	unsigned int useStateId(TaskState *state, unsigned int id);

private:
	Project *mProject;
	std::string mName;
	bool mIsDeleted;
	TaskState *mStartState;
	std::set<TaskState*> mEndStates;
	std::map<TaskState*, std::set<TaskState*> > mStateMap;
	std::vector<TaskState*> mStates;
	FJson::TokenCache mForeignKeys;
};

class Date
{
public:
	Date(std::string date);
	Date(const Date &date);
	Date();
	~Date();

	std::string getMachineTime() const;
	std::string getFormattedTime(std::string format) const;
	int cmp(const Date &other) const;
	Date &operator=(const Date &other);
	bool operator==(const Date &other) {return cmp(other) == 0;};
	bool operator!=(const Date &other) {return cmp(other) != 0;};
	bool operator<(const Date &other) {return cmp(other) < 0;};
	bool operator>(const Date &other) {return cmp(other) > 0;};
	bool operator<=(const Date &other) {return cmp(other) <= 0;};
	bool operator>=(const Date &other) {return cmp(other) >= 0;};
private:
	struct tm *mTime;//TODO smart pointer?
};

//class TaskEvent;
//typedef TaskEvent *(*TaskConstructor)();

class TaskEvent
{
public:
	virtual ~TaskEvent();
	static TaskEvent *read(Project *project, FJson::Reader &in);
	void write(FJson::Writer &out) const;
	const Date &getCreationDate() const;
	void setUser(User *user);
	User *getUser() const;
	void setTask(Task *task);

	virtual std::string getName() const {return "unknown";};
protected:
	TaskEvent();
	TaskEvent(Task *task);
	Task *getTask() const;
private:
	virtual bool readInternal(FJson::Reader &in, std::string key) {return false;};
	virtual void writeEvent(FJson::Writer &out) const {};

	User *mUser;
	Task *mTask;
	Date mDate;
	FJson::TokenCache mForeignKeys;
};

class StateChangeEvent : public TaskEvent
{
public:
	StateChangeEvent() {};
	StateChangeEvent(Task *task, TaskState *from, TaskState *to);

	TaskState *from() const;
	TaskState *to() const;
private:
	std::string getName() const override { return "STATE_CHANGE"; }
	bool readInternal(FJson::Reader &in, std::string key) override;
	void writeEvent(FJson::Writer &out) const override;
	unsigned int mFromState, mToState;
};

class CommentEvent : public TaskEvent
{
public:
	CommentEvent() {};
	CommentEvent(Task *task, std::string content);
	const std::string getContent() const {return mContent;};
private:
	std::string getName() const override { return "COMMENT"; }
	bool readInternal(FJson::Reader &in, std::string key) override;
	void writeEvent(FJson::Writer &out) const override;

	std::string mContent;
};

class ReferenceEvent : public TaskEvent
{
private:
	std::string getName() const override { return "TASK_REF"; }
	bool readInternal(FJson::Reader &in, std::string key) override {return false;};
	void writeEvent(FJson::Writer &out) const override {};
};

class CommitEvent : public TaskEvent
{
private:
	std::string getName() const override { return "COMMIT_REF"; }
	bool readInternal(FJson::Reader &in, std::string key) override;
	void writeEvent(FJson::Writer &out) const override;
	std::string mCommit;
};

class Task
{
public:
	Task(Project *project, std::string name);
	~Task();

	void setId(unsigned int id);
	void setName(std::string name);
	void setDescription(std::string text);
	void setAssigned(User *user);
	void setType(TaskType *type);
	bool setState(TaskState *state);

	int getId() const;
	std::string getName() const;
	std::string getDescription() const;
	User *getAssigned() const;
	TaskType *getType() const;
	TaskState *getState() const;
	const Date &getCreationDate() const;
	Task *getParentTask() const;

	void addSubTask(Task *task);
	const std::vector<Task*> getSubTasks() const;

	void addEvent(TaskEvent *event);
	const std::vector<TaskEvent*> getEvents() const;

	bool isClosed() const;
	static Task *read(Project *project, FJson::Reader &in);
	void write(FJson::Writer &out) const;

private:
	Project *mProject;
	int mId;
	std::string mName;
	std::string mDesc;
	User *mAssigned;
	TaskType *mType;
	TaskState *mState;
	Date mCreationDate;
	Task *mParent;
	FJson::TokenCache mForeignKeys;

	std::vector<TaskEvent*> mEvents;
	std::vector<Task*> mSubTasks;
};

class TaskFilter
{
public:
	static TaskFilter *notOf(TaskFilter *a);
	static TaskFilter *orOf(TaskFilter *a, TaskFilter *b);
	static TaskFilter *andOf(TaskFilter *a, TaskFilter *b);
	static TaskFilter *isOpen(bool open);
	static TaskFilter *hasState(std::string state);
	static TaskFilter *search(std::string query);

	struct TaskFilterWrapper {
		bool operator()(const Task *task);
		TaskFilter *mFilter;
	};

	~TaskFilter();
	TaskFilter *clone() const;
	TaskFilter::TaskFilterWrapper wrap();
	bool getValue(const Task *task);
private:
	static std::string lower(std::string str);

	enum FilterType {
		NOT_OF,
		OR_OF,
		AND_OF,
		IS_OPEN,
		HAS_STATE,
		SEARCH
	};
	TaskFilter(FilterType type);
	FilterType mType;

	bool mIsOpen;
	std::string mQuery;
	TaskFilter *mFirst, *mSecond;
	std::string mState;
};

class Search
{
public:
	static TaskFilter *create(std::string query);
private:
	struct Data {
		std::vector<char> mOpStack;
		std::vector<TaskFilter*> mValueStack;
	};
	static std::string parseString(std::istream &stream);
	static TaskFilter *readTerm(std::istream &stream);
	static void processStack(Data *data, char nextOperator);
};

class TaskList
{
public:
	~TaskList();
	void addTask(Task *task);
	void removeTask(Task *task);
	Task *getTask(unsigned int id);
	const std::vector<Task*> all() const;
	const std::vector<Task*> getFiltered(TaskFilter *filter) const;
	unsigned int getSize() const;
private:
	std::vector<Task*> mTasks;
	FJson::TokenCache mForeignKeys;
	void getTaskId(Task *task, unsigned int id);
};

class Project
{
public:
	static Project *create(std::string dirname);
	static Project *open(std::string dirname);

	Project(); //< open for tests
	~Project();
	User *getDefaultUser();
	User *getUser(std::string name);
	TaskType *getType(std::string name);
	TaskList *getTaskList();

	static std::string readText(FJson::Reader &in);
	static void writeText(FJson::Writer &out, std::string text);

	void write();//< TODO make private
private:
	User *mDefaultUser;
	std::string mDirname;
	std::string mTaskFile;
	std::map<std::string, TaskType*> mTypes;
	std::map<std::string, User*> mUsers;
	TaskList mList;
	FJson::TokenCache mForeignKeys;

	GitBackend *mSrcStorage, *mTaskStorage;

	bool read();
	void writeMain();
	void writeTasks();
	std::streambuf *getOutStream(std::string path);
	std::streambuf *getInStream(std::string path);

	friend TaskType;
};

class Config//< TODO this object should be thread safe
{
public:
	static User *getDefaultUser();
	static std::string guessProjectDir(std::string currentWorkDir);
	static std::string getSourceDir(std::string taskerPath);
	static std::string getTaskerData(std::string path, std::string *source = NULL);
	static void setTaskerData(std::string path, std::string source);
private:
	Config();
	~Config();
	static Config mConfig;

	struct Repository {
		std::string data;
		std::string source;
	};
	FJson::TokenCache mForeignKeys;
	std::vector<Repository*> mRepositories;
	User *mDefaultUser;

	void readHomeConfig();
	void readRepository(FJson::Reader &in);

	void addRepository(std::string source, std::string data);
};

};
};
