/* Unit tests for the FJson.
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

#include <iostream>
#include <sstream>
#include <cmath>
#include "fjson.h"

using namespace FJson;

std::istringstream istream;
static Reader *json = NULL;

static void createReader(const char *str)
{
	istream.str(str);
	istream.clear();
	if(json) delete json;
	json = new Reader(istream);
}

static bool readInitialization()
{
	createReader("");
	return true;
}

static bool readInt()
{
	int val;
	bool res = true;

#define cmp(x, y) \
	createReader(x); \
	json->read(val); \
	res &= (val == (y));

	cmp("0", 0);
	cmp("1", 1);
	cmp("-1", -1);
	cmp("2", 2);
	cmp("10", 10);
	cmp("-10", -10);
	cmp("100", 100);
	cmp("1482984327", 1482984327);
#undef cmp

	return res;
}

static bool readFloat()
{
	bool res = true;
	float val;

#define cmp(x, y) \
	createReader(x); \
	json->read(val); \
	res &= (fabs(val - (y)) < 1e-8);

	cmp("0", 0.0);
	cmp("-0", 0.0);
	cmp("1", 1.0);
	cmp("-1", -1.0);
	cmp("0.5", 0.5);
	cmp("-0.5", -0.5);
	cmp("0.1", 0.1);
	cmp("1e0", 1e0);
	cmp("1e1", 1e1);
	cmp("1e-1", 1e-1);
	cmp("-1e1", -1e1);
	cmp("-1e-1", -1e-1);
	cmp("1e2", 1e2);
	cmp("1e-2", 1e-2);
	cmp("1e8", 1e8);
#undef cmp

	return res;
}

static bool readString()
{
	bool res = true;
	std::string val;

#define cmp(x, y) \
	createReader("\"" x "\""); \
	json->read(val); \
	res &= (val == y);

	cmp("a", "a");
	cmp("ä", "ä");
	cmp("\\n", "\n");
	cmp("\\t", "\t");
	cmp("\\\"", "\"");
	cmp("\\\\", "\\");
	cmp("a\\nb", "a\nb");
	cmp("ä\\nb", "ä\nb");
	cmp("a\\\"", "a\"");
	cmp("a\\\"b\\\"", "a\"b\"");
	cmp("\\u0034", "\u0034");
	cmp("\\u00e4", "\u00e4");
	cmp("\\u08e4", "\u08e4");
	cmp("\\u23e4", "\u23e4");
#undef cmp
	return res;
}

static bool readMisc()
{
	bool res = true;
	std::string val;
	bool valBool;

	createReader("null");
	json->read(val);
	res &= (val == "");

	createReader("false");
	json->read(valBool);
	res &= (valBool == false);

	createReader("true");
	json->read(valBool);
	res &= (valBool == true);

	return res;
}

static bool readArray()
{
	bool res = true;
	int value;

	createReader("null");
	json->startArray();
	res &= (json->hasNextElement() == false);

	createReader("[]");
	json->startArray();
	res &= (json->hasNextElement() == false);

	createReader("[1]");
	json->startArray();
	res &= (json->hasNextElement() == true);
	json->read(value);
	res &= (value == 1);
	res &= (json->hasNextElement() == false);

	createReader("[1,2]");
	json->startArray();
	res &= (json->hasNextElement() == true);
	json->read(value);
	res &= (value == 1);
	res &= (json->hasNextElement() == true);
	json->read(value);
	res &= (value == 2);
	res &= (json->hasNextElement() == false);

	return res;
}

static bool readObject()
{
	bool res = true;
	std::string key;

	createReader("null");
	json->startObject();
	res &= (json->readObjectKey(key) == false);

	createReader("{}");
	json->startObject();
	res &= (json->readObjectKey(key) == false);

	createReader("{\"test\":4}");
	json->startObject();
	res &= (json->readObjectKey(key) == true);
	res &= (key == "test");
	int val;
	json->read(val);
	res &= (val == 4);
	res &= (json->readObjectKey(key) == false);

	createReader("{\"test\":4, \"unit\": 2}");
	json->startObject();
	res &= (json->readObjectKey(key) == true);
	res &= (key == "test");
	json->read(val);
	res &= (val == 4);
	res &= (json->readObjectKey(key) == true);
	res &= (key == "unit");
	json->read(val);
	res &= (val == 2);
	res &= (json->readObjectKey(key) == false);

	return res;
}

static bool readMixed()
{
	std::string key;
	bool res = true;
	int val;

	createReader("{\"test\": [1, 2, 3], \"unit\": 2}");
	json->startObject();
	res &= (json->readObjectKey(key) == true);
	res &= (key == "test");
	json->startArray();
	res &= (json->hasNextElement() == true);
	json->read(val);
	res &= (val == 1);
	res &= (json->hasNextElement() == true);
	json->read(val);
	res &= (val == 2);
	res &= (json->hasNextElement() == true);
	json->read(val);
	res &= (val == 3);
	res &= (json->hasNextElement() == false);

	res &= (json->readObjectKey(key) == true);
	res &= (key == "unit");
	json->read(val);
	res &= (val == 2);
	res &= (json->readObjectKey(key) == false);

	return res;
}

std::ostringstream ostream;
static Writer *out = NULL;

static void createWriter()
{
	ostream.str("");
	ostream.clear();
	if(out) delete out;
	out = new Writer(ostream);
}

static bool writeInitialization()
{
	createWriter();
	return true;
}

static bool writeInt()
{
	bool res = true;
	createWriter();
	out->write(5);
	res &= ostream.str() == "5";

	createWriter();
	out->write(0);
	res &= ostream.str() == "0";

	createWriter();
	out->write(-5);
	res &= ostream.str() == "-5";

	return res;
}

static bool writeFloat()
{
	bool res = true;
	createWriter();
	out->write(0.0);
	res &= ostream.str() == "0";

	createWriter();
	out->write(0.2);
	res &= ostream.str() == "0.2";

	return res;
}

static bool writeMisc()
{
	bool res = true;
	createWriter();
	out->write(nullptr);
	res &= ostream.str() == "null";

	createWriter();
	out->write(false);
	res &= ostream.str() == "false";

	createWriter();
	out->write(true);
	res &= ostream.str() == "true";

	createWriter();
	std::string str = "hello";
	out->write(str);
	res &= ostream.str() == "\"hello\"";

	return res;
}

static bool writeArray()
{
	bool res = true;

	createWriter();
	out->startArray();
	out->endArray();
	res &= ostream.str() == "[]";

	createWriter();
	out->startArray();
	out->write(4);
	out->endArray();
	res &= ostream.str() == "[4]";

	createWriter();
	out->startArray();
	out->write(4);
	out->writeNextElement();
	out->write(1);
	out->endArray();
	res &= ostream.str() == "[4,1]";

	return res;
}

static bool writeObject()
{
	bool res = true;

	createWriter();
	out->startObject();
	out->writeObjectKey("test");
	out->write(4);
	out->endObject();
	res &= ostream.str() == "{\"test\":4}";

	createWriter();
	out->startObject();
	out->writeObjectKey("test");
	out->write(4);
	out->writeObjectKey("cool");
	out->write(7);
	out->endObject();
	res &= ostream.str() == "{\"test\":4,\"cool\":7}";

	return res;
}

int main(int argc, char **argv)
{
	bool success;

	std::cout << "Read tests\n";

	success = readInitialization();
	std::cout << (success ? "Success" : "Failure") << "\n";
	success = readInt();
	std::cout << (success ? "Success" : "Failure") << "\n";
	success = readFloat();
	std::cout << (success ? "Success" : "Failure") << "\n";
	success = readString();
	std::cout << (success ? "Success" : "Failure") << "\n";
	success = readMisc();
	std::cout << (success ? "Success" : "Failure") << "\n";
	success = readArray();
	std::cout << (success ? "Success" : "Failure") << "\n";
	success = readObject();
	std::cout << (success ? "Success" : "Failure") << "\n";
	success = readMixed();
	std::cout << (success ? "Success" : "Failure") << "\n";

	std::cout << "Write tests\n";
	success = writeInitialization();
	std::cout << (success ? "Success" : "Failure") << "\n";
	success = writeInt();
	std::cout << (success ? "Success" : "Failure") << "\n";
	success = writeFloat();
	std::cout << (success ? "Success" : "Failure") << "\n";
	success = writeMisc();
	std::cout << (success ? "Success" : "Failure") << "\n";
	success = writeArray();
	std::cout << (success ? "Success" : "Failure") << "\n";
	success = writeObject();
	std::cout << (success ? "Success" : "Failure") << "\n";

	delete json;
}
