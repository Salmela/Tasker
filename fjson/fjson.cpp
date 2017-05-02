/* FJson reader and writer.
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
#include <sstream>
#include <string.h>
#include "fjson.h"

namespace FJson {

class Exception : public std::exception {
public:
	Exception(std::string str) {mStr = str.c_str();};
	Exception(const char *str) {mStr = str;};
	const char *what() const noexcept override {return mStr;};
private:
	const char *mStr;
};

class ApiException : public std::exception {
public:
	ApiException(std::string str) {mStr = str.c_str();};
	ApiException(const char *str) {mStr = str;};
	const char *what() const noexcept override {return mStr;};
private:
	const char *mStr;
};

IstreamTokenStream::IstreamTokenStream(std::istream &stream)
	:mStream(stream)
{
}

void IstreamTokenStream::next(Token *token)
{
	mToken = token;
	tokenize();
}

#define isAsciiDigit(c) ((c) >= '0' && (c) <= '9')
#define MAX_INT ((long)(unsigned long)0x7fffffffffffffff)
#define MIN_INT ((long)(unsigned long)0x8000000000000000)

long IstreamTokenStream::parseLong(int c)
{
	long value = 0;
	do {
		value += c - '0';
		c = mStream.get();
		if(!isAsciiDigit(c)) {
			mStream.unget();
			break;
		}
		if(value >= MAX_INT / 90) {
			return (value < 0) ? MIN_INT : MAX_INT;
		}
		value *= 10;
	} while(true);
	return value;
}

double IstreamTokenStream::fast10pow(long exp) {
	//modular exponiation algorithm
	double res = 1.0;
	double base = (exp > 0) ? 10.0 : 0.1;
	exp = abs(exp);

	while(exp > 0) {
		if((exp & 1) == 1) {
			res = res * base;
		}
		exp >>= 1;
		base *= base;
	}

	return res;
}

void IstreamTokenStream::parseNumber()
{
	int c = mStream.get();
	long value = 0;
	int sign = 1;

	if(c == '-') {
		sign = -1;
		c = mStream.get();
	}

	if(c != '0') {
		value = parseLong(c);
	}
	c = mStream.get();

	double mul = 0.1;
	double fValue;
	bool isFloat = false;
	if(c == '.') {
		isFloat = true;
		fValue = value;
		c = mStream.get();
		while(isAsciiDigit(c)) {
			fValue += mul * (c - '0');
			c = mStream.get();
			if(!isAsciiDigit(c))
				break;
			if(mul == 0)
				break;
			mul *= 0.1;
		}
	}

	if(c == 'e' || c == 'E') {
		int eSign = 1;
		if(!isFloat) {
			fValue = value;
		}
		isFloat = true;
		c = mStream.get();
		if(c == '+') {
			c = mStream.get();
		} else if(c == '-') {
			eSign = -1;
			c = mStream.get();
		}
		long exp = eSign * parseLong(c);
		fValue = fValue * fast10pow(exp);
	} else {
		mStream.unget();
	}

	if(!isFloat) {
		value *= sign;
		mToken->value.integer = value;
		mToken->type = INTEGER;
	} else {
		fValue *= sign;
		mToken->value.real = fValue;
		mToken->type = REAL;
	}
}

void IstreamTokenStream::generateUtf8(std::ostream &builder, int value)
{
	if(value < 0x80) {
		builder << (char)value;
	} else if(value < 0x800) {
		builder << (char)(0xc0 | (value >> 6 & 0x1f));
		builder << (char)(0x80 | (value & 0x3f));
	} else if(value < 0x10000) {
		builder << (char)(0xe0 | (value >> 12 & 0x0f));
		builder << (char)(0x80 | (value >> 6 & 0x3f));
		builder << (char)(0x80 | (value & 0x3f));
	} else if(value < 0x110000) {
		builder << (char)(0xf0 | (value >> 18 & 0x07));
		builder << (char)(0x80 | (value >> 12 & 0x3f));
		builder << (char)(0x80 | (value >> 6 & 0x3f));
		builder << (char)(0x80 | (value & 0x3f));
	} else {
		throw Exception("Invalid unicode character.");
	}
}

void IstreamTokenStream::parseString()
{
	int c = mStream.get();
	if(c != '\"') {
		throw Exception("expected '\"'(quote).");
	}

	std::ostringstream builder;
	c = mStream.get();
	while(c != '\"' && c != -1) {
		//TODO we should validate utf8 multi-byte characters
		if(c == '\\') {
			c = mStream.get();
			switch(c) {
			case '\"':
			case '\\':
			case '/':
				break;
			case 'b':
				c = '\b';
				break;
			case 'f':
				c = '\f';
				break;
			case 'n':
				c = '\n';
				break;
			case 'r':
				c = '\r';
				break;
			case 't':
				c = '\t';
				break;
			case 'u': {
				char buf[5];
				mStream.get(buf, 5);
				int val = strtol(buf, NULL, 16);
				generateUtf8(builder, val);
				c = -1;
				break;
			}
			default:
				throw Exception("invalid escape sequence.");
			}

			if(c != -1) {
				builder << (char)c;
			}
		} else {
			builder << (char)c;
		}
		c = mStream.get();
	}

	mToken->string = builder.str();

	if(c != '\"') {
		throw Exception("expected '\"'(quote).");
	}
}

void IstreamTokenStream::tokenize()
{
	//TODO skip whitespaces
	char buf[10];
	int c;
	do {
		c = mStream.get();
	} while(c == ' ' || c == '\t' || c == '\n' || c == '\r');
	buf[0] = c;

	switch(c) {
	case '{':
		mToken->type = OBJECT;
		break;
	case '}':
		mToken->type = END_OBJECT;
		break;
	case '[':
		mToken->type = ARRAY;
		break;
	case ']':
		mToken->type = END_ARRAY;
		break;
	case ':':
		mToken->type = COLON;
		break;
	case '\"':
		mToken->type = STRING;
		mStream.unget();
		parseString();
		break;
	case ',':
		mToken->type = SEPARATOR;
		mToken->value.boolean = true;
		break;
	case 't':
		mToken->type = BOOLEAN;
		mToken->value.boolean = true;
		mStream.get(buf + 1, 4);
		if(memcmp(buf, "true", 4) != 0) {
			throw Exception("invalid token");
		}
		break;
	case 'f':
		mToken->type = BOOLEAN;
		mToken->value.boolean = false;
		mStream.get(buf + 1, 5);
		if(memcmp(buf, "false", 5) != 0) {
			throw Exception("invalid token");
		}
		break;
	case 'n':
		mToken->type = NUL;
		mStream.get(buf + 1, 4);
		if(memcmp(buf, "null", 4) != 0) {
			throw Exception("invalid token");
		}
		break;
	case -1:
		mToken->type = END;
		break;
	default:
		if(c == '-' || isAsciiDigit(c)) {
			mStream.unget();
			parseNumber();
		} else {
			throw Exception("invalid token");
		}
		break;
	}
}

/// TokenCache

TokenCache::TokenCache()
{
	mIndex = 0;
}

void TokenCache::record(Token &token)
{
	mTokens.push_back(token);
}

void TokenCache::next(Token *token)
{
	if(mIndex >= mTokens.size()) {
		token->type = END;
		return;
	}
	*token = mTokens[mIndex];
	mIndex++;
}

std::vector<Token> TokenCache::getTokens() const
{
	return mTokens;
}

void TokenCache::dump() const
{
	for(Token t : mTokens) {
		std::cout << "Token " << t.type << "\n";
	}
}

/// Reader

Reader::Reader(std::istream &stream)
	:mToken(NUL)
{
	mAfterStartBracket = false;
	mHasInternalTokenizer = true;
	mTokenizer = new IstreamTokenStream(stream);
	mCache = NULL;
	tokenize();
}

Reader::Reader(TokenCache *cache)
	:mToken(NUL)
{
	mAfterStartBracket = false;
	mHasInternalTokenizer = false;
	mTokenizer = cache;
	mCache = NULL;
	tokenize();
}

Reader::~Reader()
{
	if(mHasInternalTokenizer) {
		delete mTokenizer;
	}
}

void Reader::tokenize()
{
	if(mCache) {
		mCache->record(mToken);
	}
	mTokenizer->next(&mToken);
}

void Reader::read(bool &value)
{
	if(mToken.type == BOOLEAN) {
		value = (int)mToken.value.boolean;
	} else {
		throw Exception("Expected boolean.");
	}
	tokenize();
}

void Reader::read(int &value)
{
	if(mToken.type == INTEGER) {
		value = (int)mToken.value.integer;
	} else {
		throw Exception("Expected integer.");
	}
	tokenize();
}

void Reader::read(unsigned int &value)
{
	if(mToken.type == INTEGER) {
		if(mToken.value.integer < 0) {
			throw Exception("Expected positive integer.");
			value = 0;
		} else {
			value = (unsigned int)mToken.value.integer;
		}
	} else {
		throw Exception("Expected positive integer.");
	}
	tokenize();
}

void Reader::read(float &value)
{
	double tmp;
	read(tmp);
	value = tmp;
}

void Reader::read(double &value)
{
	if(mToken.type == REAL) {
		value = mToken.value.real;
	} else if(mToken.type == INTEGER) {
		value = mToken.value.integer;
	} else {
		throw Exception("Expected floating point number.");
	}
	tokenize();
}

void Reader::read(std::string &value)
{
	if(mToken.type == STRING) {
		value = mToken.string;
	} else if(mToken.type == NUL) {
		value = "";
	} else {
		throw Exception("Expected string.");
	}
	tokenize();
}

void Reader::skipValue(TokenCache *cache, bool isForeignKey)
{
	std::string key;

	if(cache) {
		mCache = cache;
	}

	if(cache && isForeignKey) {
		if(!mStack.empty() && mStack.back() == '{') {
			Token t(SEPARATOR);
			mCache->record(t);

			Token t2(STRING);
			t2.string = mCurrentKey;
			mCache->record(t2);

			Token t3(COLON);
			mCache->record(t3);
		}
	}

	switch(mToken.type) {
		case STRING:
		case BOOLEAN:
		case INTEGER:
		case REAL:
		case NUL:
			tokenize();
			break;
		case OBJECT:
			startObject();
			if(mCache && mToken.type != END_OBJECT) {
				Token t(SEPARATOR);
				mCache->record(t);
			}
			while(readObjectKey(key)) {
				skipValue();
			}
			break;
		case ARRAY:
			startArray();
			if(mCache && mToken.type != END_ARRAY) {
				Token t(SEPARATOR);
				mCache->record(t);
			}
			while(hasNextElement()) {
				skipValue();
			}
			break;
		case END_OBJECT:
		case END_ARRAY:
		case SEPARATOR:
		case COLON:
		case END:
			throw ApiException("Invalid state.");
			break;
	}

	if(cache) {
		mCache = NULL;
	}
}

void Reader::startObject()
{
	if(mToken.type == NUL) {
		mStack.push_back('O');
		return;
	} else if(mToken.type == OBJECT) {
		mStack.push_back('{');
		mAfterStartBracket = true;
	} else {
		throw Exception("Expected object.");
	}
	tokenize();
}

bool Reader::readObjectKey(std::string &key)
{
	char c = mStack.back();
	if(c == 'O') {
		return false;
	} else if(c != '{') {
		throw Exception("Mismatching brackets.");
	}
	bool res;
	if(mToken.type == END_OBJECT) {
		mAfterStartBracket = false;
		res = false;
		mStack.pop_back();
	} else if(mAfterStartBracket || mToken.type == SEPARATOR) {
		res = true;
		if(mTokenizer->isCache() || !mAfterStartBracket) {
			tokenize();
		}
		mAfterStartBracket = false;
		read(key);
		mCurrentKey = key;
		if(mToken.type != COLON) {
			throw Exception("Expected ':'.");
		}
	} else {
		throw Exception("Expected '}' or ',' character.");
	}
	tokenize();
	return res;
}

void Reader::startArray()
{
	if(mToken.type == NUL) {
		mStack.push_back('A');
		return;
	} else if(mToken.type == ARRAY) {
		mStack.push_back('[');
		mAfterStartBracket = true;
	} else {
		throw Exception("Expected array.");
	}
	tokenize();
}

bool Reader::hasNextElement()
{
	char c = mStack.back();
	if(c == 'A') {
		return false;
	} else if(c != '[') {
		throw Exception("Mismatching brackets.");
	}
	bool res;
	if(mToken.type == ARRAY) {
		return true;
	} else if(mToken.type == END_ARRAY) {
		mStack.pop_back();
		res = false;
	} else if(mToken.type == SEPARATOR) {
		res = true;
	} else if(mAfterStartBracket) {
		mAfterStartBracket = false;
		return true;
	} else {
		throw Exception("Expected ']' or ',' character.");
	}
	mAfterStartBracket = false;
	tokenize();
	return res;
}

AssocArray::AssocArray(Reader *reader)
{
	mReader = reader;
	read();
}

AssocArray::~AssocArray()
{
	for(const auto &entry : mValues) {
		delete entry.second;
	}
}

void AssocArray::read()
{
	if(mReader->mToken.type != OBJECT) {
		throw ApiException("AssocArray is supported only for objects");
	}
	mReader->startObject();
	std::string key;
	while(mReader->readObjectKey(key)) {
		TokenCache *cache = new TokenCache;
		mReader->skipValue(cache);
		mValues[key] = cache;
	}
}

bool AssocArray::has(std::string key) const
{
	return mValues.find(key) != mValues.end();
}

TokenCache *AssocArray::get(std::string key)
{
	return mValues[key];
}

const std::map<std::string, TokenCache*> AssocArray::getValues()
{
	return mValues;
}

/// Writer

Writer::Writer(std::ostream &stream, bool doPretty)
	:mStream(stream)
{
	mState = S_INIT;
	mDoPrettyPrint = doPretty;
	mIndentWidth = 1;
	mIndentChar = '\t';
}

Writer::~Writer()
{
	if(!mStack.empty()) throw ApiException("Some objects or arrays are not closed.");
}

void Writer::valueStateTransition()
{
	if(mState != S_INIT && mState != S_SEPARATOR) throw ApiException("bad state");
	mState = S_VALUE;
}

void Writer::write(void *value)
{
	valueStateTransition();
	if(value) throw ApiException("Expected NULL.");
	Token t(NUL);
	writeToken(&t);
}

void Writer::write(bool value)
{
	valueStateTransition();
	Token t(BOOLEAN);
	t.value.boolean = value;
	writeToken(&t);
}

void Writer::write(int value)
{
	valueStateTransition();
	Token t(INTEGER);
	t.value.integer = value;
	writeToken(&t);
}

void Writer::write(unsigned int value)
{
	valueStateTransition();
	Token t(INTEGER);
	t.value.integer = value;
	writeToken(&t);
}

void Writer::write(float value)
{
	valueStateTransition();
	Token t(REAL);
	t.value.real = value;
	writeToken(&t);
}

void Writer::write(double value)
{
	valueStateTransition();
	Token t(REAL);
	t.value.real = value;
	writeToken(&t);
}

void Writer::write(std::string value)
{
	valueStateTransition();
	Token t(STRING);
	t.string = value;
	writeToken(&t);
}

void Writer::write(const TokenCache &cache)
{
	for(Token token : cache.getTokens()) {
		writeToken(&token);
	}
}

void Writer::startObject()
{
	if(mState == S_VALUE) throw ApiException("bad state");
	Token t(OBJECT);
	writeToken(&t);
}

void Writer::endObject()
{
	if(mState == S_SEPARATOR) throw ApiException("bad state");
	if(mStack.empty() || mStack.back() != '{') throw ApiException("Mismatching api calls.");
	Token t(END_OBJECT);
	writeToken(&t);

	if(mStack.empty()) {
		Token t(END);
		writeToken(&t);
	}
}

void Writer::writeObjectKey(std::string key)
{
	if(mStack.empty() || mStack.back() != '{') throw ApiException("Allowed only in inside a object.");
	if(mState == S_SEPARATOR) {
		throw ApiException("bad state");
	}
	Token t(SEPARATOR);
	writeToken(&t);

	Token t2(STRING);
	t2.string = key;
	writeToken(&t2);

	Token t3(COLON);
	writeToken(&t3);
}

void Writer::startArray()
{
	if(mState == S_VALUE) throw ApiException("bad state");
	Token t(ARRAY);
	writeToken(&t);
}

void Writer::endArray()
{
	if(mState == S_SEPARATOR) throw ApiException("bad state");
	if(mStack.empty() || mStack.back() != '[') throw ApiException("Mismatching api calls.");
	Token t(END_ARRAY);
	writeToken(&t);

	if(mStack.empty()) {
		Token t(END);
		writeToken(&t);
	}
}

void Writer::startNextElement()
{
	if(mStack.empty() || mStack.back() != '[') throw ApiException("Allowed only in inside a array.");
	if(mState == S_SEPARATOR) {
		throw ApiException("bad state");
	}
	Token t(SEPARATOR);
	writeToken(&t);
}

void Writer::writeToken(Token *token)
{
	State newState = S_VALUE;
	switch(token->type) {
		case STRING:
			mStream.put('"');
			mStream << token->string;
			mStream.put('"');
			break;
		case BOOLEAN:
			mStream << (token->value.boolean ? "true" : "false");
			break;
		case NUL:
			mStream << "null";
			break;
		case INTEGER:
			mStream << token->value.integer;
			break;
		case REAL:
			mStream << token->value.real;
			break;
		case OBJECT:
			mStream.put('{');
			newState = S_START;
			mStack.push_back('{');
			break;
		case ARRAY:
			mStream.put('[');
			newState = S_START;
			mStack.push_back('[');
			break;
		case END_OBJECT:
			mStack.pop_back();
			doIndentation(true);
			mStream.put('}');
			newState = S_VALUE;
			break;
		case END_ARRAY:
			mStack.pop_back();
			if(mState != S_START) {
				doIndentation(true);
			}
			mStream.put(']');
			newState = S_VALUE;
			break;
		case SEPARATOR:
			if(mState != S_START) {
				mStream.put(',');
				doIndentation(true);
			} else {
				doIndentation(true);
			}
			newState = S_SEPARATOR;
			break;
		case COLON:
			mStream.put(':');
			if(mDoPrettyPrint) {
				mStream.put(' ');
			}
			newState = S_SEPARATOR;
			break;
		case END:
			if(mDoPrettyPrint) {
				mStream.put('\n');
			}
			break;
	}
	mState = newState;
}

void Writer::doIndentation(bool lineFeed) const
{
	if(!mDoPrettyPrint) {
		return;
	}
	if(lineFeed) {
		mStream << "\n";
	}
	mStream << std::string(mStack.size() * mIndentWidth, mIndentChar);
}

};
