/* Temporary file file for backend code.
 *
 * Copyright (C) 2017 Aleksi Salmela
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#include <algorithm>
#include <exception>
#include <fstream>
#include <sstream>

#include "backend.h"

namespace Tasker {
namespace Backend {

// From https://stackoverflow.com/questions/236129/236803#236803
template<typename Out>
void split(const std::string &str, char delim, Out result) {
	std::stringstream ss;
	ss.str(str);
	std::string item;
	while(std::getline(ss, item, delim)) {
		*(result++) = item;
	}
}

std::vector<std::string> split(const std::string &str, char delim) {
	std::vector<std::string> elements;
	split(str, delim, std::back_inserter(elements));
	return elements;
}

/// TaskState
TaskState::TaskState(std::string name)
	:mName(name)
{
	mIsDeleted = false;
	mRefCount = 0;
}

TaskState *TaskState::create(std::string name)
{
	return new TaskState(name);
}

void TaskState::rename(std::string newName)
{
	mName = newName;
}

std::string TaskState::getName() const
{
	return mName;
}

void TaskState::ref()
{
	if(mIsDeleted) {
		throw std::exception();
	}
	mRefCount++;
}

void TaskState::unref()
{
	if(mRefCount <= 0) {
		throw std::exception();
	}
	mRefCount--;

	if(mIsDeleted && mRefCount == 0) {
		free();
	}
}

void TaskState::free()
{
	if(mRefCount == 0) {
		delete this;
		return;
	}
	mIsDeleted = true;
}

TaskState *TaskState::read(FJson::Reader &in)
{
	std::string name;
	in.read(name);
	auto state = new TaskState(name);
	return state;
}

void TaskState::write(FJson::Writer &out) const
{
	out.write(mName);
}

/// TaskType

TaskType::TaskType(Project *project, std::string name)
	:mProject(project), mName(name), mStartState(NULL)
{
	if(project) {
		project->mTypes[name] = this;
	}
}

void TaskType::rename(std::string newName)
{
	mName = newName;
}

std::string TaskType::getName() const
{
	return mName;
}

TaskState *TaskType::getStartState() const
{
	return mStartState;
}

void TaskType::setStartState(TaskState *state)
{
	if(mStartState == state) {
		return;
	}
	if(mStartState) {
		mStartState->unref();
	}
	state->ref();
	mStartState = state;
	if(getStateId(state) == -1) {
		mStates.push_back(state);
	}
}

void TaskType::setEndStates(std::set<TaskState*> states)
{
	for(TaskState *state : states) {
		state->ref();
		if(getStateId(state) == -1) {
			mStates.push_back(state);
		}
	}
	for(TaskState *state : mEndStates) {
		state->unref();
	}

	mEndStates = states;//TODO should we duplicate here
}

void TaskType::setTransition(TaskState *from, TaskState *to, bool create)
{
	auto iter = mStateMap.find(from);
	std::set<TaskState*> *set;
	if(iter == mStateMap.end()) {
		if(!create) {
			return;
		}
		set = &mStateMap[from];
		from->ref();
	} else {
		set = &iter->second;
	}

	if(create) {
		if(getStateId(from) == -1) {
			mStates.push_back(from);
		}
		if(getStateId(to) == -1) {
			mStates.push_back(to);
		}

		set->insert(to);
		to->ref();
	} else {
		set->erase(to);
		to->unref();
		if(set->empty()) {
			mStateMap.erase(from);
			from->unref();
		}
	}
}

bool TaskType::canChange(TaskState *from, TaskState *to) const
{
	std::set<TaskState*> states;
	states = possibleChanges(from);

	return states.find(to) != states.end();
}

const std::set<TaskState*> TaskType::possibleChanges(TaskState *from) const
{
	auto iter = mStateMap.find(from);
	if(iter == mStateMap.end()) {
		return std::set<TaskState*>();
	}
	return iter->second;
}

bool TaskType::isClosed(TaskState *state) const
{
	return mEndStates.find(state) != mEndStates.end();
}

bool TaskType::isIncomplete() const
{
	if(!mStartState) return true;
	if(mEndStates.size() == 0) return true;
	return false;
}

TaskType *TaskType::read(Project *project, FJson::Reader &in)
{
	TaskType *type = new TaskType(NULL, "");
	type->mProject = project;

	in.startObject();
	std::string key;

	int startState;
	std::vector<int> endStates;
	std::map<int, std::set<int> > stateMap;
	while(in.readObjectKey(key)) {
		if(key == "name") {
			in.read(type->mName);
		} else if(key == "deleted") {
			in.read(type->mIsDeleted);
		} else if(key == "start-state") {
			in.read(startState);
		} else if(key == "end-states") {
			in.startArray();
			while(in.hasNextElement()) {
				int state;
				in.read(state);
				endStates.push_back(state);
			}
		} else if(key == "state-map") {
			in.startArray();
			for(unsigned int i = 0; in.hasNextElement(); i++) {
				in.startArray();
				while(in.hasNextElement()) {
					int state;
					in.read(state);
					stateMap[i].insert(state);
				}
			}
		} else if(key == "states") {
			in.startArray();
			while(in.hasNextElement()) {
				auto state = TaskState::read(in);
				type->mStates.push_back(state);
			}
		} else {
			throw "Unknown task";
		}
	}
	type->mStartState = type->mStates[startState];
	for(auto state : endStates) {
		type->mEndStates.insert(type->mStates[state]);
	}
	for(auto toArray : stateMap) {
		std::set<TaskState*> set;
		for(int state : toArray.second) {
			set.insert(type->mStates[state]);
		}
		type->mStateMap[type->mStates[toArray.first]] = set;
	}
	return type;
}

void TaskType::write(FJson::Writer &out) const
{
	out.startObject();
	out.writeObjectKey("name");
	out.write(mName);
	out.writeObjectKey("deleted");
	out.write(mIsDeleted);
	out.writeObjectKey("start-state");
	out.write(getStateId(mStartState));

	out.writeObjectKey("end-states");
	out.startArray();
	for(const auto state : mEndStates) {
		out.startNextElement();
		out.write(getStateId(state));
	}
	out.endArray();

	out.writeObjectKey("state-map");

	out.startArray();
	for(unsigned int i = 0; i < mStates.size(); i++) {
		out.startNextElement();
		out.startArray();
		TaskState *from = mStates[i];
		auto iter = mStateMap.find(from);

		if(iter != mStateMap.end()) {
			for(const auto state : iter->second) {
				out.startNextElement();
				out.write(getStateId(state));
			}
		}
		out.endArray();
	}
	out.endArray();

	out.writeObjectKey("states");

	out.startArray();
	for(const auto state : mStates) {
		out.startNextElement();
		state->write(out);
	}
	out.endArray();

	out.endObject();
}

int TaskType::getStateId(TaskState *state) const
{
	auto iter = std::find(mStates.begin(), mStates.end(), state);

	if(iter == mStates.end()) {
		return -1;
	}
	return iter - mStates.begin();
}

TaskState *TaskType::getStateById(unsigned int index) const
{
	if(index >= mStates.size()) {
		return NULL;
	}
	return mStates[index];
}

/// Task

Task::Task(Project *project, std::string name)
	:mProject(project), mName(name), mType(NULL),
	mState(NULL), mClosed(false)
{
}

void Task::setName(std::string newName)
{
	mName = newName;
}

std::string Task::getName() const
{
	return mName;
}

void Task::setDescription(std::string text)
{
	mDesc = text;
}

std::string Task::getDescription() const
{
	return mDesc;
}

void Task::setType(TaskType *type)
{
	if(type->isIncomplete()) {
		throw "Incomplete type";
	}
	mType = type;
	mState = type->getStartState();
}

TaskType *Task::getType() const
{
	return mType;
}

bool Task::setState(TaskState *newState)
{
	if(!mType->canChange(mState, newState)) {
		return false;
	}
	mState = newState;
	return true;
}

TaskState *Task::getState() const
{
	return mState;
}

bool Task::isClosed() const
{
	return mType->isClosed(mState);
}

Task *Task::read(Project *project, FJson::Reader &in)
{
	auto *task = new Task(project, "");
	int state;

	in.startObject();
	std::string key;

	while(in.readObjectKey(key)) {
		if(key == "name") {
			in.read(task->mName);
		} else if(key == "desc") {
			in.startArray();
			std::ostringstream desc;
			while(in.hasNextElement()) {
				std::string str;
				in.read(str);
				desc << str;
			}
			task->mDesc = desc.str();
		} else if(key == "type") {
			std::string type;
			in.read(type);
			task->mType = project->getType(type);
		} else if(key == "state") {
			in.read(state);
		} else if(key == "closed") {
			in.read(task->mClosed);
		} else {
			std::cout << "Unknown\n";
		}
	}
	task->mState = task->mType->getStateById(state);
	return task;
}

void Task::write(FJson::Writer &out) const
{
	out.startObject();
	out.writeObjectKey("name");
	out.write(mName);
	out.writeObjectKey("desc");

	out.startArray();
	std::vector<std::string> lines = split(mDesc, '\n');
	for(auto line : lines) {
		out.startNextElement();
		out.write(line + "\\n");
	}
	out.endArray();

	out.writeObjectKey("type");
	out.write(mType->getName());
	out.writeObjectKey("state");
	out.write(mType->getStateId(mState));
	out.writeObjectKey("closed");
	out.write(mClosed);
	out.endObject();
}

/// TaskFilter

TaskFilter TaskFilter::notOf(TaskFilter *a)
{
	TaskFilter f(NOT_OF);
	f.mFirst = a;
	return f;
}

TaskFilter TaskFilter::orOf(TaskFilter *a, TaskFilter *b)
{
	TaskFilter f(OR_OF);
	f.mFirst = a;
	f.mSecond = b;
	return f;
}

TaskFilter TaskFilter::andOf(TaskFilter *a, TaskFilter *b)
{
	TaskFilter f(AND_OF);
	f.mFirst = a;
	f.mSecond = b;
	return f;
}

TaskFilter TaskFilter::isOpen(bool open)
{
	TaskFilter f(IS_OPEN);
	f.mIsOpen = open;
	return f;
}

TaskFilter TaskFilter::hasState(TaskState *state)
{
	TaskFilter f(HAS_STATE);
	f.mState = state;
	return f;
}

TaskFilter TaskFilter::search(std::string query)
{
	TaskFilter f(SEARCH);
	f.mQuery = query;
	return f;
}

TaskFilter::TaskFilter(FilterType type)
{
	mType = type;
}

bool TaskFilter::getValue(const Task *task)
{
	switch(mType) {
		case NOT_OF: {
			return !mFirst->getValue(task);
		}
		case OR_OF:
			return mFirst->getValue(task) ||
				mSecond->getValue(task);
		case AND_OF:
			return mFirst->getValue(task) &&
				mSecond->getValue(task);
		case IS_OPEN:
			return !task->isClosed();
		case HAS_STATE:
			return task->getState() == mState;
		case SEARCH:
			return false;//TODO implement
	}
	return true;
}

bool TaskFilter::operator()(const Task *task)
{
	return getValue(task);
}

/// TaskList

void TaskList::addTask(Task *task)
{
	mTasks.push_back(task);
}

void TaskList::removeTask(Task *task)
{
	mTasks.erase(std::remove(mTasks.begin(), mTasks.end(), task),
		mTasks.end());
}

const std::vector<Task*> TaskList::all() const
{
	return mTasks;
}

const std::vector<Task*> TaskList::getFiltered(TaskFilter filter) const
{
	std::vector<Task*> newList(std::min((int)mTasks.size(), 256));
	auto it = std::copy_if(mTasks.begin(), mTasks.end(), newList.begin(), filter);
	newList.resize(std::distance(newList.begin(), it));
	return newList;
}

unsigned int TaskList::getSize() const
{
	return mTasks.size();
}

/// Project

Project *Project::create(const char *dirname)
{
	auto project = new Project();
	project->mDirname = dirname;
	return project;
}

Project *Project::open(const char *dirname)
{
	auto project = new Project();
	project->mDirname = Config::getTaskerData(dirname);
	if(project->mDirname.empty()) {
		project->mDirname = dirname;
	}
	if(project->read()) {
		return project;
	} else {
		delete project;
		return NULL;
	}
}

Project::Project()
{
}

Project::~Project()
{
	for(const auto &entry : mTypes) {
		delete entry.second;
	}
}

TaskType *Project::getType(std::string name)
{
	auto iter = mTypes.find(name);
	return (iter != mTypes.end()) ? iter->second : NULL;
}

TaskList *Project::getTaskList()
{
	return &mList;
}

void Project::write()
{
	if(mDirname.empty()) return;

	std::ofstream stream(mDirname + "/tasker.conf");

	FJson::Writer out(stream, true);
	out.startObject();

	out.writeObjectKey("types");
	out.startObject();
	for(const auto type : mTypes) {
		out.writeObjectKey(type.first);
		type.second->write(out);
	}
	out.endObject();

	out.writeObjectKey("tasks");
	out.startArray();
	for(const auto task : mList.all()) {
		out.startNextElement();
		task->write(out);
	}
	out.endArray();

	out.endObject();
}

bool Project::read()
{
	if(mDirname.empty()) return false;

	//create lock file

	std::ifstream stream(mDirname + "/tasker.conf");
	if(!stream.is_open()) return false;

	FJson::Reader in(stream);
	in.startObject();
	std::string key;

	while(in.readObjectKey(key)) {
		if(key == "types") {
			in.startObject();
			std::string name;
			while(in.readObjectKey(name)) {
				auto type = TaskType::read(this, in);
				mTypes[name] = type;
			}
		} else if(key == "tasks") {
			in.startArray();
			while(in.hasNextElement()) {
				auto *task = Task::read(this, in);
				mList.addTask(task);
			}
		} else {
			std::cout << "Unknown\n";
		}
	}
	return true;
}

/// Config

Config Config::mConfig;

Config::Config() {
	this->readHomeConfig();
}

static bool startsWith(std::string &prefix, std::string &value) {
	if(value.size() < prefix.size()) return false;
	return std::equal(prefix.begin(), prefix.end(), value.begin());
}

std::string Config::getTaskerData(std::string path)
{
	//TODO what if there is nested project, we should choose the deepest match
	for(auto *repository : Config::mConfig.mRepositories) {
		if(startsWith(repository->source, path)) {
			return repository->data;
		}
	}
	return "";
}

void Config::setTaskerData(std::string path, std::string source)
{
	mConfig.addRepository(source, path);

	//TODO lock the taskerconf file somehow
	std::ofstream stream(std::string(getenv("HOME")) + "/.taskerconf");
	FJson::Writer out(stream, true);

	out.startObject();
	out.writeObjectKey("repositories");
	out.startArray();
	for(const auto *repo : Config::mConfig.mRepositories) {
		out.startObject();
		out.writeObjectKey("source");
		out.write(repo->source);
		out.writeObjectKey("data");
		out.write(repo->data);
		out.endObject();
	}
	out.endArray();
	out.endObject();
}

void Config::readHomeConfig() {
	std::ifstream stream(std::string(getenv("HOME")) + "/.taskerconf");
	if(!stream.is_open()) return;
	FJson::Reader in(stream);
	std::string key;

	in.startObject();
	while(in.readObjectKey(key)) {
		if(key == "repositories") {
			readRepository(in);
		} else {
			std::cout << "unknown\n";
		}
	}
}

void Config::readRepository(FJson::Reader &in) {
	in.startArray();

	while(in.hasNextElement()) {
		std::string key, source, data;
		in.startObject();
		while(in.readObjectKey(key)) {
			if(key == "source") {
				in.read(source);
			} else if(key == "data") {
				in.read(data);
			} else {
				std::cout << "unknown\n";
			}
		}
		addRepository(source, data);
	}
}

void Config::addRepository(std::string source, std::string data)
{
	auto *repo = new Repository;
	repo->data = data;
	repo->source = source;
	mRepositories.push_back(repo);
}

};
};
