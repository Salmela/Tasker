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
			value += mul * (c - '0');
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
		if(c == '+') {
			c = mStream.get();
		} else if(c == '-') {
			eSign = -1;
			c = mStream.get();
		}
		long exp = eSign * parseLong(c);
		fValue = fValue * fast10pow(exp);

		fValue *= sign;
	}

	if(!isFloat) {
		value *= sign;
		mToken.value.integer = value;
		mToken.type = INTEGER;
	} else {
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
	do {
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
	} while(c != '\"' && c != -1);

	mToken.string = builder.str();

	if(c != '\"') {
		throw "expected '\"'(quote)";
	}
}

int Reader::tokenize()
{
	//TODO skip whitespaces
	char buf[10];
	int c = mStream.get();
	buf[0] = c;

	switch(c) {
	case '{':
		mToken.type = OBJECT;
		break;
	case '}':
		break;
	case '[':
		mToken.type = ARRAY;
		break;
	case ']':
	case ':':
		return c;
	case '\"':
		mToken.type = STRING;
		mStream.unget();
		parseString();
		break;
	case ',':
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

void Reader::expect(int expected)
{
	int token = tokenize();
	if(token != expected) {
		std::cerr << "Expected '" << (char)expected << "', received '" << (char)token << "'\n";
		abort();//TODO throw exception
	}
}

void Reader::read(bool &value)
{
	tokenize();
	if(mToken.type == BOOLEAN) {
		value = (int)mToken.value.boolean;
	} else {
		std::cerr << "Expected boolean.\n";
	}
}

void Reader::read(int &value)
{
	tokenize();
	if(mToken.type == INTEGER) {
		value = (int)mToken.value.integer;
	} else {
		std::cerr << "Expected integer.\n";
	}
}

void Reader::read(unsigned int &value)
{
	value = 0;
}

void Reader::read(float &value)
{
	double tmp;
	read(tmp);
	value = tmp;
}

void Reader::read(double &value)
{
	tokenize();
	if(mToken.type == REAL) {
		value = mToken.value.real;
	} else if(mToken.type == INTEGER) {
		value = mToken.value.integer;
	} else {
		std::cerr << "Expected floating point number.\n";
	}
}

void Reader::read(std::string &value)
{
	tokenize();
	if(mToken.type == STRING) {
		value = mToken.string;
	} else if(mToken.type == NUL) {
		value = "";
	} else {
		std::cerr << "Expected string.\n";
	}
}

void Reader::startObject()
{
	expect('{');
}

bool Reader::readObjectKey(std::string &key)
{
	return false;
}

void Reader::startArray()
{
	expect('[');
}

bool Reader::hasNextElement()
{
	//TODO properly handle start element
	int token = tokenize();
	if(token == ']') {
		return false;
	} else if(token == ',') {
		return true;
	}
	return false;
}


Writer::Writer(std::ostream &stream)
	:mStream(stream)
{
}

void Writer::write(bool value)
{
	//mStream.put(value ? "true" : "false");
}

void Writer::write(int value)
{
	mStream.put(value);
}

void Writer::write(unsigned int value)
{
	mStream.put(value);
}

void Writer::write(float value)
{
	mStream.put(value);
}

void Writer::write(double value)
{
	mStream.put(value);
}

void Writer::write(std::string value)
{
	mStream.write(value.c_str(), value.size());
}

void Writer::startObject()
{
	mStream.put('{');
}

void Writer::endObject()
{
	mStream.put('}');
}

void Writer::writeObjectKey(std::string key)
{
	mStream.write(key.c_str(), key.size());
	mStream.put(':');
}

void Writer::startArray()
{
	mStream.put('[');
}

void Writer::endArray()
{
	mStream.put(']');
}

};