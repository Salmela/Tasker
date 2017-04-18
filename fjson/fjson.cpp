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
#include <sstream>
#include <string.h>
#include "fjson.h"

namespace FJson {

Reader::Reader(std::istream &stream)
	:mStream(stream)
{
	afterStartBracket = false;
	tokenize();
}

#define isAsciiDigit(c) ((c) >= '0' && (c) <= '9')
#define MAX_INT ((long)(unsigned long)0x7fffffffffffffff)
#define MIN_INT ((long)(unsigned long)0x8000000000000000)

long Reader::parseLong(int c)
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

double Reader::fast10pow(long exp) {
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

void Reader::parseNumber()
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
		mToken.value.integer = value;
		mToken.type = INTEGER;
	} else {
		fValue *= sign;
		mToken.value.real = fValue;
		mToken.type = REAL;
	}
}

void Reader::generateUtf8(std::ostream &builder, int value)
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
		throw "Invalid unicode character";
	}
}

void Reader::parseString()
{
	int c = mStream.get();
	if(c != '\"') {
		throw "expected '\"'(quote)";
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
				throw "invalid escape sequence";
			}

			if(c != -1) {
				builder << (char)c;
			}
		} else {
			builder << (char)c;
		}
		c = mStream.get();
	}

	mToken.string = builder.str();

	if(c != '\"') {
		throw "expected '\"'(quote)";
	}
}

int Reader::tokenize()
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
		mToken.type = OBJECT;
		break;
	case '}':
		mToken.type = END_OBJECT;
		break;
	case '[':
		mToken.type = ARRAY;
		break;
	case ']':
		mToken.type = END_ARRAY;
		break;
	case ':':
		mToken.type = COLON;
		break;
	case '\"':
		mToken.type = STRING;
		mStream.unget();
		parseString();
		break;
	case ',':
		mToken.type = SEPARATOR;
		break;
	case 't':
		mToken.type = BOOLEAN;
		mToken.value.boolean = true;
		mStream.get(buf + 1, 4);
		if(memcmp(buf, "true", 4) != 0) {
			throw "invalid token";
		}
		break;
	case 'f':
		mToken.type = BOOLEAN;
		mToken.value.boolean = false;
		mStream.get(buf + 1, 5);
		if(memcmp(buf, "false", 5) != 0) {
			throw "invalid token";
		}
		break;
	case 'n':
		mToken.type = NUL;
		mStream.get(buf + 1, 4);
		if(memcmp(buf, "null", 4) != 0) {
			throw "invalid token";
		}
		break;
	case -1:
		mToken.type = END;
		break;
	default:
		if(c == '-' || isAsciiDigit(c)) {
			mStream.unget();
			parseNumber();
		} else {
			throw "invalid token";
		}
		break;
	}
	return 0;
}

void Reader::read(bool &value)
{
	if(mToken.type == BOOLEAN) {
		value = (int)mToken.value.boolean;
	} else {
		std::cerr << "Expected boolean.\n";
	}
	tokenize();
}

void Reader::read(int &value)
{
	if(mToken.type == INTEGER) {
		value = (int)mToken.value.integer;
	} else {
		std::cerr << "Expected integer.\n";
	}
	tokenize();
}

void Reader::read(unsigned int &value)
{
	if(mToken.type == INTEGER) {
		if(mToken.value.integer < 0) {
			std::cerr << "Expected positive integer.\n";
			value = 0;
		} else {
			value = (unsigned int)mToken.value.integer;
		}
	} else {
		std::cerr << "Expected positive integer.\n";
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
		std::cerr << "Expected floating point number.\n";
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
		std::cerr << "Expected string.\n";
	}
	tokenize();
}

void Reader::skipValue()
{
	std::string key;
	switch(mToken.type) {
		case STRING:
		case BOOLEAN:
		case NUL:
		case INTEGER:
		case REAL:
			tokenize();
			break;
		case OBJECT:
			startObject();
			while(readObjectKey(key)) {
				skipValue();
			}
			break;
		case ARRAY:
			startArray();
			while(hasNextElement()) {
				skipValue();
			}
			break;
		case END_OBJECT:
		case END_ARRAY:
		case SEPARATOR:
		case COLON:
		case END:
			throw "Invalid state";
			break;
	}
}

void Reader::startObject()
{
	if(mToken.type == NUL) {
		mStack.push_back('O');
		return;
	} else if(mToken.type == OBJECT) {
		mStack.push_back('{');
		afterStartBracket = true;
	} else {
		std::cerr << "Expected object.\n";
	}
	tokenize();
}

bool Reader::readObjectKey(std::string &key)
{
	char c = mStack.back();
	if(c == 'O') {
		return false;
	} else if(c != '{') {
		throw "Mismatching brackets";
	}
	bool res;
	if(mToken.type == END_OBJECT) {
		afterStartBracket = false;
		res = false;
		mStack.pop_back();
	} else if(afterStartBracket || mToken.type == SEPARATOR) {
		res = true;
		if(!afterStartBracket) {
			tokenize();
		}
		afterStartBracket = false;
		read(key);
		if(mToken.type != COLON) {
			throw "Expected ':'";
		}
	} else {
		throw "Expected '}' or ',' character.";
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
		afterStartBracket = true;
	} else {
		std::cerr << "Expected array.\n";
	}
	tokenize();
}

bool Reader::hasNextElement()
{
	char c = mStack.back();
	if(c == 'A') {
		return false;
	} else if(c != '[') {
		throw "Mismatching brackets";
	}
	bool res;
	if(mToken.type == ARRAY) {
		return true;
	} else if(mToken.type == END_ARRAY) {
		mStack.pop_back();
		res = false;
	} else if(mToken.type == SEPARATOR) {
		res = true;
	} else if(afterStartBracket) {
		afterStartBracket = false;
		return true;
	} else {
		throw "Expected ']' or ',' character.";
	}
	afterStartBracket = false;
	tokenize();
	return res;
}


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
	if(!mStack.empty()) throw "Some objects or arrays are not closed";
	if(mDoPrettyPrint) {
		mStream.put('\n');
	}
}

void Writer::valueStateTransition()
{
	if(mState != S_INIT && mState != S_SEPARATOR) throw "bad state";
	mState = S_VALUE;
}

void Writer::write(void *value)
{
	valueStateTransition();
	if(value) throw "Expected NULL";
	mStream << "null";
}

void Writer::write(bool value)
{
	valueStateTransition();
	mStream << (value ? "true" : "false");
}

void Writer::write(int value)
{
	valueStateTransition();
	mStream << value;
}

void Writer::write(unsigned int value)
{
	valueStateTransition();
	mStream << value;
}

void Writer::write(float value)
{
	valueStateTransition();
	mStream << value;
}

void Writer::write(double value)
{
	valueStateTransition();
	mStream << value;
}

void Writer::write(std::string value)
{
	valueStateTransition();
	mStream.put('"');
	mStream << value;
	mStream.put('"');
}

void Writer::startObject()
{
	if(mState == S_VALUE) throw "bad state";
	mStream.put('{');
	mState = S_START;
	mStack.push_back('{');
	doIndentation(true);
}

void Writer::endObject()
{
	if(mState == S_SEPARATOR) throw "bad state";
	if(mStack.empty() || mStack.back() != '{') throw "Mismatching api calls";
	mStack.pop_back();
	doIndentation(true);
	mStream.put('}');
	mState = S_VALUE;
}

void Writer::writeObjectKey(std::string key)
{
	if(mStack.empty() || mStack.back() != '{') throw "Allowed only in inside a object";
	if(mState == S_VALUE) {
		mStream.put(',');
		doIndentation(true);
	} else if(mState == S_SEPARATOR) {
		throw "bad state";
	}
	mState = S_SEPARATOR;
	write(key);
	mStream.put(':');
	if(mDoPrettyPrint) {
		mStream.put(' ');
	}
	mState = S_SEPARATOR;
}

void Writer::startArray()
{
	if(mState == S_VALUE) throw "bad state";
	mStream.put('[');
	mState = S_START;
	mStack.push_back('[');
}

void Writer::endArray()
{
	if(mState == S_SEPARATOR) throw "bad state";
	if(mStack.empty() || mStack.back() != '[') throw "Mismatching api calls";
	mStack.pop_back();
	if(mState != S_START) {
		doIndentation(true);
	}
	mStream.put(']');
	mState = S_VALUE;
}

void Writer::startNextElement()
{
	if(mState == S_SEPARATOR) throw "bad state";
	if(mStack.empty() || mStack.back() != '[') throw "Allowed only in inside a array";
	if(mState != S_START) {
		mStream.put(',');
	}
	mState = S_SEPARATOR;
	doIndentation(true);
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
