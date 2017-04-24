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
#include <cstring>
#include <ctime>

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

User User::ANONYMOUS_VALUE("anonymous");
User *User::ANONYMOUS = &User::ANONYMOUS_VALUE;

/// TaskState
TaskState::TaskState(TaskType *type, std::string name)
	:mName(name)
{
	mType = type;
	mId = type ? type->useNextStateId(this) : TaskState::INVALID_ID;
	mIsDeleted = false;
	mRefCount = 0;
}

TaskState *TaskState::create(TaskType *type, std::string name)
{
	return new TaskState(type, name);
}

void TaskState::ownedBy(const TaskType *type)
{
	if(type != mType) {
		throw "bad task type";
	}
}

void TaskState::rename(std::string newName)
{
	mName = newName;
}

std::string TaskState::getName() const
{
	return mName;
}

unsigned int TaskState::getId() const
{
	return mId;
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

TaskState *TaskState::read(TaskType *type, FJson::Reader &in)
{
	std::string name;
	in.read(name);
	auto state = new TaskState(type, name);
	return state;
}

void TaskState::write(FJson::Writer &out) const
{
	out.write(mName);
}

/// TaskType

TaskType::TaskType(Project *project, std::string name)
	:mProject(project), mName(name), mNextStateId(0), mStartState(NULL)
{
	if(project) {
		project->mTypes[name] = this;
	}
}

TaskType::~TaskType()
{
	for(auto state : mStates) {
		delete state;
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
	state->ownedBy(this);
	if(mStartState) {
		mStartState->unref();
	}
	state->ref();
	mStartState = state;
}

void TaskType::setEndStates(std::set<TaskState*> states)
{
	for(TaskState *state : states) {
		state->ownedBy(this);
		state->ref();
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
	from->ownedBy(this);
	to->ownedBy(this);
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
				TaskState::read(type, in);
			}
		} else {
			in.skipValue(&type->mForeignKeys);
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
	out.write(mStartState->getId());

	out.writeObjectKey("end-states");
	out.startArray();
	for(const auto state : mEndStates) {
		out.startNextElement();
		out.write(state->getId());
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
				out.write(state->getId());
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

TaskState *TaskType::getStateById(unsigned int index) const
{
	if(index >= mStates.size()) {
		return NULL;
	}
	return mStates[index];
}

unsigned int TaskType::useNextStateId(TaskState *state)
{
	mStates.push_back(state);
	return mNextStateId++;
}

/// Date

Date::Date(std::string date)
{
	mTime = new struct tm;
	*mTime = {0};
	strptime(date.c_str(), "%Y-%m-%dT%H:%M:%SZ", mTime);
}

Date::Date()
{
	time_t t = time(NULL);
	mTime = new struct tm;
	gmtime_r(&t, mTime);
}

Date::~Date()
{
	delete mTime;
}

std::string Date::getMachineTime() const
{
	char buffer[24];
	strftime(buffer, 24, "%G-%m-%dT%H:%M:%SZ", mTime);
	return buffer;
}

std::string Date::getFormattedTime(std::string format) const
{
	char buffer[80];
	time_t val = mktime(mTime);
	strftime(buffer, 80, format.c_str(), localtime(&val));
	return buffer;
}

Date &Date::operator=(const Date &other)
{
	*mTime = *other.mTime;
	return *this;
}

/// TaskEvent

TaskEvent::TaskEvent()
	:mUser(User::ANONYMOUS), mTask(NULL)
{
}

TaskEvent::TaskEvent(Task *task)
	:mUser(User::ANONYMOUS), mTask(task)
{
}

TaskEvent::~TaskEvent() {}

TaskEvent *TaskEvent::read(Project *project, FJson::Reader &in)
{
	std::string typeStr;

	FJson::AssocArray obj(&in);
	FJson::Reader type(obj.get("type"));
	type.read(typeStr);

	TaskEvent *event;
	//TODO use factory
	if(typeStr == "STATE_CHANGE") {
		event = new StateChangeEvent();
	} else if(typeStr == "COMMENT") {
		event = new CommentEvent();
	} else if(typeStr == "TASK_REF") {
		event = new ReferenceEvent();
	} else if(typeStr == "COMMIT_REF") {
		event = new CommitEvent();
	} else {
		throw "Unknown event type";
	}

	for(auto pair : obj.getValues()) {
		auto key = pair.first;
		FJson::Reader value(pair.second);
		if(key == "type") {
			continue;
		} else if(key == "user") {
			std::string name;
			value.read(name);
			event->mUser = project->getUser(name);
		} else if(key == "date") {
			std::string time;
			value.read(time);
			event->mDate = Date(time);
		} else if(!event->readInternal(value, key)) {
			value.skipValue(&event->mForeignKeys);
		}
	}
	return event;
}

void TaskEvent::write(FJson::Writer &out) const
{
	out.startObject();
	out.writeObjectKey("type");
	out.write(getName());

	out.writeObjectKey("date");
	out.write(mDate.getMachineTime());

	if(mUser != User::ANONYMOUS) {
		out.writeObjectKey("user");
		out.write(mUser->getName());
	}

	writeEvent(out);
	out.endObject();
}

const Date *TaskEvent::getCreationTime() const
{
	return &mDate;
}

void TaskEvent::setUser(User *user)
{
	if(mUser != User::ANONYMOUS) {
		throw "User can't be set twice.";
	}
	mUser = user;
}

User *TaskEvent::getUser() const
{
	return mUser;
}

void TaskEvent::setTask(Task *task)
{
	if(mTask) {
		throw "Task can't be set twice.";
	}
	mTask = task;
}

Task *TaskEvent::getTask() const
{
	return mTask;
}

StateChangeEvent::StateChangeEvent(Task *task, TaskState *from, TaskState *to)
	:TaskEvent(task)
{
	mFromState = from->getId();
	mToState = to->getId();
}

TaskState *StateChangeEvent::from() const
{
	TaskType *type = getTask()->getType();
	return type->getStateById(mFromState);
}

TaskState *StateChangeEvent::to() const
{
	TaskType *type = getTask()->getType();
	return type->getStateById(mToState);
}

bool StateChangeEvent::readInternal(FJson::Reader &in, std::string key)
{
	if(key == "from") {
		in.read(mFromState);
		return true;
	} else if(key == "to") {
		in.read(mToState);
		return true;
	}
	return false;
}

void StateChangeEvent::writeEvent(FJson::Writer &out) const
{
	out.writeObjectKey("from");
	out.write(mFromState);
	out.writeObjectKey("to");
	out.write(mToState);
}

CommentEvent::CommentEvent(Task *task, std::string content)
	:TaskEvent(task), mContent(content)
{
}

bool CommentEvent::readInternal(FJson::Reader &in, std::string key)
{
	if(key == "content") {
		mContent = Project::readText(in);
		return true;
	}
	return false;
}

void CommentEvent::writeEvent(FJson::Writer &out) const
{
	out.writeObjectKey("content");
	Project::writeText(out, mContent);
}

bool CommitEvent::readInternal(FJson::Reader &in, std::string key)
{
	if(key == "commit") {
		in.read(mCommit);
		return true;
	}
	return false;
}
void CommitEvent::writeEvent(FJson::Writer &out) const
{
	out.writeObjectKey("commit");
	out.write(mCommit);
}

/// Task

Task::Task(Project *project, std::string name)
	:mProject(project), mName(name), mType(NULL),
	mState(NULL), mClosed(false)
{
}

Task::~Task()
{
	for(auto event : mEvents) {
		delete event;
	}
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
	addEvent(new StateChangeEvent(this, mState, newState));
	mState = newState;
	return true;
}

TaskState *Task::getState() const
{
	return mState;
}

void Task::addSubTask(Task *task)
{
	mSubTasks.push_back(task);
}

const std::vector<Task*> Task::getSubTasks() const
{
	return mSubTasks;
}

void Task::addEvent(TaskEvent *event)
{
	event->setUser(mProject->getMyUser());
	mEvents.push_back(event);
}

const std::vector<TaskEvent*> Task::getEvents() const
{
	return mEvents;
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
			task->mDesc = Project::readText(in);
		} else if(key == "type") {
			std::string type;
			in.read(type);
			task->mType = project->getType(type);
		} else if(key == "state") {
			in.read(state);
		} else if(key == "closed") {
			in.read(task->mClosed);
		} else if(key == "events") {
			in.startArray();
			while(in.hasNextElement()) {
				auto *event = TaskEvent::read(project, in);
				task->mEvents.push_back(event);
			}
		} else {
			in.skipValue(&task->mForeignKeys);
		}
	}
	task->mState = task->mType->getStateById(state);
	for(auto event : task->mEvents) {
		event->setTask(task);
	}
	return task;
}

void Task::write(FJson::Writer &out) const
{
	out.startObject();
	out.writeObjectKey("name");
	out.write(mName);
	out.writeObjectKey("desc");

	Project::writeText(out, mDesc);

	out.writeObjectKey("type");
	out.write(mType->getName());
	out.writeObjectKey("state");
	out.write(mState->getId());
	out.writeObjectKey("closed");
	out.write(mClosed);
	out.writeObjectKey("events");
	out.startArray();
	for(auto event : mEvents) {
		out.startNextElement();
		event->write(out);
	}
	out.endArray();
	out.write(mForeignKeys);
	out.endObject();
}

/// TaskFilter

TaskFilter::TaskFilter(FilterType type)
{
	mType = type;
	mFirst = NULL;
	mSecond = NULL;
}

TaskFilter::~TaskFilter()
{
	if(mFirst) delete mFirst;
	if(mSecond) delete mSecond;
}

TaskFilter *TaskFilter::notOf(TaskFilter *a)
{
	auto *f = new TaskFilter(NOT_OF);
	f->mFirst = a;
	return f;
}

TaskFilter *TaskFilter::orOf(TaskFilter *a, TaskFilter *b)
{
	auto *f = new TaskFilter(OR_OF);
	f->mFirst = a;
	f->mSecond = b;
	return f;
}

TaskFilter *TaskFilter::andOf(TaskFilter *a, TaskFilter *b)
{
	auto *f = new TaskFilter(AND_OF);
	f->mFirst = a;
	f->mSecond = b;
	return f;
}

TaskFilter *TaskFilter::isOpen(bool open)
{
	auto *f = new TaskFilter(IS_OPEN);
	f->mIsOpen = open;
	return f;
}

TaskFilter *TaskFilter::hasState(std::string state)
{
	auto *f = new TaskFilter(HAS_STATE);
	f->mState = state;
	return f;
}

TaskFilter *TaskFilter::search(std::string query)
{
	auto *f = new TaskFilter(SEARCH);
	f->mQuery = lower(query);
	return f;
}

TaskFilter *TaskFilter::clone() const
{
	auto *f = new TaskFilter(this->mType);
	*f = *this;
	if(f->mFirst) f->mFirst = f->mFirst->clone();
	if(f->mSecond) f->mSecond = f->mSecond->clone();
	return f;
}

TaskFilter::TaskFilterWrapper TaskFilter::wrap()
{
	TaskFilter::TaskFilterWrapper wrap;
	wrap.mFilter = this;
	return wrap;
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
			return mIsOpen == !task->isClosed();
		case HAS_STATE:
			return task->getState()->getName() == mState;
		case SEARCH:
			if(lower(task->getName()).find(mQuery) != std::string::npos) {
				return true;
			}
			if(lower(task->getDescription()).find(mQuery) != std::string::npos) {
				return true;
			}
			return false;
	}
	return true;
}

std::string TaskFilter::lower(std::string str)
{
	std::string copy = str;
	for(auto p = copy.begin(); p != copy.end(); p++) {
		*p = tolower(*p);//TODO warning this will fail on unicode characters!!!!
	}
	return copy;
}

bool TaskFilter::TaskFilterWrapper::operator()(const Task *task)
{
	return mFilter->getValue(task);
}

/// Search

std::string Search::parseString(std::istream &stream)
{
	int val = stream.get();
	if(val != '"') {
		throw "Expected quote";
	}

	std::ostringstream ostream;
	while((val = stream.get()) != -1) {
		if(val == '\\') {
			val = stream.get();
			if(val == -1) {
				break;
			} else if(val == '"') {
				ostream.put('"');
			} else if(val == '\\') {
				ostream.put('\\');
			} else {
				throw "Invalid escape sequence";
			}
			continue;
		} else if(val == '"') {
			break;
		}
		ostream.put((char)val);
	}

	if(val != '"') {
		throw "Expected quote";
	}
	return ostream.str();
}

TaskFilter *Search::readTerm(std::istream &stream)
{
	int val = stream.get();
	char buf[256];
	buf[0] = val;

	TaskFilter *filter = NULL;;

	switch(val) {
		case 'o':
			stream.get(buf + 1, 4);
			if(strcmp(buf, "open") == 0) {
				filter = TaskFilter::isOpen(true);
			} else {
				throw "Unknown is keyword";
			}
			break;
		case 'c':
			stream.get(buf + 1, 6);
			if(strcmp(buf, "closed") == 0) {
				filter = TaskFilter::isOpen(false);
			} else {
				throw "Unknown is keyword";
			}
			break;
		case 's':
			stream.get(buf + 1, 5);
			if(strcmp(buf, "state") == 0) {
				if(stream.get() != '(') {
					throw "Expected '('";
				}
				std::string str = Search::parseString(stream);
				filter = TaskFilter::hasState(str);
				if(stream.get() != ')') {
					throw "Expected ')'";
				}
			}
			break;
		default:
			throw "Unknown keyword";
			break;
	}
	if(!filter) {
		throw "inalid state";
	}
	return filter;
}

void Search::processStack(Data *data, char nextOperator)
{
	unsigned char precedence[128] = {0};
	precedence['('] = 0;
	precedence['='] = 1;
	precedence['!'] = 2;
	precedence['|'] = 3;
	precedence['&'] = 4;
	precedence[')'] = 5;
	precedence['\0'] = 6;

	// TODO WIP improve error handling!

	if(data->mOpStack.empty()) {
		data->mOpStack.push_back(nextOperator);
		return;
	}

	int current = data->mOpStack.back();

	while(current != '(' && precedence[(int)nextOperator] > precedence[current]) {
		TaskFilter *filter = NULL, *a, *b;
#define popValue() data->mValueStack.back(); data->mValueStack.pop_back()
		switch(current) {
			case ')':
				break;
			case '(':
				filter = popValue();
				break;
			case '!':
				a = popValue();
				filter = TaskFilter::notOf(a);
				break;
			case '|':
				a = popValue();
				b = popValue();
				filter = TaskFilter::orOf(a, b);
				break;
			case '&':
				a = popValue();
				b = popValue();
				filter = TaskFilter::andOf(a, b);
				break;
			default:
				throw "invalid operation";
				break;
		}
		if(!filter) {
			break;
		}
		data->mOpStack.pop_back();
		data->mValueStack.push_back(filter);
		if(data->mOpStack.empty()) {
			break;
		}
		current = data->mOpStack.back();
	}
	if(nextOperator == ')') {
		if(data->mOpStack.back() != '(') {
			throw "Mismatched parenthesis\n";
		}
		data->mOpStack.pop_back();
		return;
	}
	data->mOpStack.push_back(nextOperator);
}

TaskFilter *Search::create(std::string query)
{
	std::istringstream stream(query);
	struct Data data;
	int val;

	std::vector<char> opStack;
	std::vector<TaskFilter*> valueStack;

	while((val = stream.get()) != -1) {
		char buf[4];
		buf[0] = val;
		switch(val) {
			case '"': {
				stream.unget();
				std::string value = parseString(stream);
				TaskFilter *filter = TaskFilter::search(value);
				data.mValueStack.push_back(filter);
			} break;
			case '(':
			case ')':
			case '|'://or
				processStack(&data, val);
				break;
			case ',':
			case '&':
				processStack(&data, '&');
				break;
			case '!':
			case '-':
				data.mOpStack.push_back('!');
				break;
			case 'a':
			case 'A':
				stream.get(buf + 1, 3);
				if(strcmp(buf, "and") == 0 ||
				   strcmp(buf, "AND") == 0) {
					processStack(&data, '&');
					break;
				}
				stream.putback(buf[2]);
				stream.putback(buf[1]);
				stream.putback(buf[0]);
				data.mValueStack.push_back(readTerm(stream));
				break;
			case 'o':
			case 'O':
				stream.get(buf + 1, 2);
				if(strcmp(buf, "or") == 0 ||
				   strcmp(buf, "OR") == 0) {
					processStack(&data, '|');
					break;
				}
				stream.putback(buf[1]);
				stream.putback(buf[0]);
				data.mValueStack.push_back(readTerm(stream));
				break;
			case 'n':
			case 'N':
				stream.get(buf + 1, 3);
				if(strcmp(buf, "not") == 0 ||
				   strcmp(buf, "NOT") == 0) {
					data.mOpStack.push_back('!');
					break;
				}
				stream.putback(buf[2]);
				stream.putback(buf[1]);
				stream.putback(buf[0]);
				data.mValueStack.push_back(readTerm(stream));
				break;
			case ' ':
				break;
			case '\t':
				break;
			default:
				stream.unget();
				data.mValueStack.push_back(readTerm(stream));
				break;
		}
	}

	processStack(&data, '\0');
	data.mOpStack.pop_back();

	if(data.mValueStack.size() != 1) {
		std::cerr << "Too many values in stack\n";
		throw "";
	}
	if(!data.mOpStack.empty()) {
		std::cerr << "Unprocessed operators left\n";
		throw "";
	}

	return data.mValueStack[0];
}

/// TaskList

TaskList::~TaskList()
{
	for(auto task : mTasks) {
		delete task;
	}
}

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

const std::vector<Task*> TaskList::getFiltered(TaskFilter *filter) const
{
	std::vector<Task*> newList(std::min((int)mTasks.size(), 256));
	auto it = std::copy_if(mTasks.begin(), mTasks.end(), newList.begin(), filter->wrap());
	newList.resize(std::distance(newList.begin(), it));
	return newList;
}

unsigned int TaskList::getSize() const
{
	return mTasks.size();
}

/// Project

Project *Project::create(std::string dirname)
{
	auto project = new Project();
	project->mDirname = dirname;
	return project;
}

Project *Project::open(std::string dirname)
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
	:mMyUser(NULL)
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

User *Project::getMyUser()
{
	if(!mMyUser) {
		return User::ANONYMOUS;
	}
	return mMyUser;
}

User *Project::getUser(std::string name)
{
	auto iter = mUsers.find(name);
	if(iter != mUsers.end()) {
		return iter->second;
	} else {
		User *user = new User(name);
		mUsers[name] = user;
		return user;
	}
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

	if(mTaskFile.empty()) {
		mTaskFile = "tasks.json";
	}

	out.writeObjectKey("task-path");
	out.write(mTaskFile);
	out.write(mForeignKeys);
	out.endObject();

	writeTasks();
}

void Project::writeTasks()
{
	std::ofstream stream(mDirname + "/" + mTaskFile);

	FJson::Writer out(stream, true);

	out.startArray();
	for(const auto task : mList.all()) {
		out.startNextElement();
		task->write(out);
	}
	out.endArray();
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
		} else if(key == "task-path") {
			in.read(mTaskFile);
		} else {
			in.skipValue(&mForeignKeys);
		}
	}

	if(!mTaskFile.empty()) {
		std::ifstream stream(mDirname + "/" + mTaskFile);
		if(!stream.is_open()) return false;
		FJson::Reader in(stream);

		in.startArray();
		while(in.hasNextElement()) {
			auto *task = Task::read(this, in);
			mList.addTask(task);
		}
	}
	return true;
}

std::string Project::readText(FJson::Reader &in)
{
	in.startArray();
	std::ostringstream text;
	while(in.hasNextElement()) {
		std::string str;
		in.read(str);
		text << str;
	}
	return text.str();
}

void Project::writeText(FJson::Writer &out, std::string text)
{
	out.startArray();
	std::vector<std::string> lines = split(text, '\n');
	for(auto line : lines) {
		out.startNextElement();
		out.write(line + "\\n");
	}
	out.endArray();
}

/// Config

Config Config::mConfig;

Config::Config() {
	this->readHomeConfig();
}

Config::~Config() {
	for(auto repo : mRepositories) {
		delete repo;
	}
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
	out.write(Config::mConfig.mForeignKeys);
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
			in.skipValue(&Config::mConfig.mForeignKeys);
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
