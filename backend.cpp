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

TaskState *TaskState::read(FJson::Reader *reader)
{
	std::string name;
	reader->read(name);
	auto state = new TaskState(name);
	return state;
}

void TaskState::write(FJson::Writer *writer) const
{
	writer->write(mName);
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

TaskType *TaskType::read(Project *project, FJson::Reader *reader)
{
	TaskType *type = new TaskType(project, "");
	reader->startObject();
	std::string key;

	int startState;
	std::vector<int> endStates;
	std::map<int, std::set<int> > stateMap;
	while(reader->readObjectKey(key)) {
		if(key == "name") {
			reader->read(type->mName);
		} else if(key == "deleted") {
			reader->read(type->mIsDeleted);
		} else if(key == "start-state") {
			reader->read(startState);
		} else if(key == "end-states") {
			reader->startArray();
			while(reader->hasNextElement()) {
				int state;
				reader->read(state);
				endStates.push_back(state);
			}
		} else if(key == "state-map") {
			reader->startArray();
			for(unsigned int i = 0; reader->hasNextElement(); i++) {
				reader->startArray();
				while(reader->hasNextElement()) {
					int state;
					reader->read(state);
					stateMap[i].insert(state);
				}
			}
		} else if(key == "states") {
			reader->startArray();
			while(reader->hasNextElement()) {
				auto state = TaskState::read(reader);
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

void TaskType::write(FJson::Writer *writer) const
{
	writer->startObject();
	writer->writeObjectKey("name");
	writer->write(mName);
	writer->writeObjectKey("deleted");
	writer->write(mIsDeleted);
	writer->writeObjectKey("start-state");
	writer->write(getStateId(mStartState));

	writer->writeObjectKey("end-states");
	writer->startArray();
	for(const auto state : mEndStates) {
		writer->startNextElement();
		writer->write(getStateId(state));
	}
	writer->endArray();

	writer->writeObjectKey("state-map");

	writer->startArray();
	for(unsigned int i = 0; i < mStates.size(); i++) {
		writer->startNextElement();
		writer->startArray();
		TaskState *from = mStates[i];
		auto iter = mStateMap.find(from);

		if(iter != mStateMap.end()) {
			for(const auto state : iter->second) {
				writer->startNextElement();
				writer->write(getStateId(state));
			}
		}
		writer->endArray();
	}
	writer->endArray();

	writer->writeObjectKey("states");

	writer->startArray();
	for(const auto state : mStates) {
		writer->startNextElement();
		state->write(writer);
	}
	writer->endArray();

	writer->endObject();
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

Task *Task::read(Project *project, FJson::Reader *reader)
{
	auto *task = new Task(project, "");
	int state;

	reader->startObject();
	std::string key;

	while(reader->readObjectKey(key)) {
		if(key == "name") {
			reader->read(task->mName);
		} else if(key == "desc") {
			reader->startArray();
			std::ostringstream desc;
			while(reader->hasNextElement()) {
				std::string str;
				reader->read(str);
				desc << str;
			}
			task->mDesc = desc.str();
		} else if(key == "type") {
			std::string type;
			reader->read(type);
			task->mType = project->getType(type);
		} else if(key == "state") {
			reader->read(state);
		} else if(key == "closed") {
			reader->read(task->mClosed);
		} else {
			std::cout << "Unknown\n";
		}
	}
	task->mState = task->mType->getStateById(state);
	return task;
}

void Task::write(FJson::Writer *writer) const
{
	writer->startObject();
	writer->writeObjectKey("name");
	writer->write(mName);
	writer->writeObjectKey("desc");

	writer->startArray();
	std::vector<std::string> lines = split(mDesc, '\n');
	for(auto line : lines) {
		writer->startNextElement();
		writer->write(line);
	}
	writer->endArray();

	writer->writeObjectKey("type");
	writer->write(mType->getName());
	writer->writeObjectKey("state");
	writer->write(mType->getStateId(mState));
	writer->writeObjectKey("closed");
	writer->write(mClosed);
	writer->endObject();
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

const std::vector<Task*> TaskList::all()
{
	return mTasks;
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

	auto writer = new FJson::Writer(stream, true);
	writer->startObject();

	writer->writeObjectKey("types");
	writer->startObject();
	for(const auto type : mTypes) {
		writer->writeObjectKey(type.first);
		type.second->write(writer);
	}
	writer->endObject();

	writer->writeObjectKey("tasks");
	writer->startArray();
	for(const auto task : mList.all()) {
		writer->startNextElement();
		task->write(writer);
	}
	writer->endArray();

	writer->endObject();
}

bool Project::read()
{
	if(mDirname.empty()) return false;

	//create lock file

	std::ifstream stream(mDirname + "/tasker.conf");
	if(!stream.is_open()) return false;

	auto reader = new FJson::Reader(stream);
	reader->startObject();
	std::string key;

	while(reader->readObjectKey(key)) {
		if(key == "types") {
			reader->startObject();
			std::string name;
			while(reader->readObjectKey(name)) {
				auto type = TaskType::read(this, reader);
				mTypes[name] = type;
			}
		} else if(key == "tasks") {
			reader->startArray();
			while(reader->hasNextElement()) {
				auto *task = Task::read(this, reader);
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
	FJson::Writer writer(stream, true);

	writer.startObject();
	writer.writeObjectKey("repositories");
	writer.startArray();
	for(const auto *repo : Config::mConfig.mRepositories) {
		writer.startObject();
		writer.writeObjectKey("source");
		writer.write(repo->source);
		writer.writeObjectKey("data");
		writer.write(repo->data);
		writer.endObject();
	}
	writer.endArray();
	writer.endObject();
}

void Config::readHomeConfig() {
	std::ifstream stream(std::string(getenv("HOME")) + "/.taskerconf");
	if(!stream.is_open()) return;
	FJson::Reader reader(stream);
	std::string key;

	reader.startObject();
	while(reader.readObjectKey(key)) {
		if(key == "repositories") {
			readRepository(reader);
		} else {
			std::cout << "unknown\n";
		}
	}
}

void Config::readRepository(FJson::Reader &reader) {
	reader.startArray();

	while(reader.hasNextElement()) {
		std::string key, source, data;
		reader.startObject();
		while(reader.readObjectKey(key)) {
			if(key == "source") {
				reader.read(source);
			} else if(key == "data") {
				reader.read(data);
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
