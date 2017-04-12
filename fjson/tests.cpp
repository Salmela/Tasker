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
#include "fjson.h"

using namespace FJson;

std::istringstream stream;
static Reader *json = NULL;

static void createReader(const char *str)
{
	stream.str(str);
	stream.clear();
	if(json) delete json;
	json = new Reader(stream);
}

static bool initialization()
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
	res &= (abs(val - (y)) < 1e-323);

	cmp("0", 0.0);
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

int main(int argc, char **argv)
{
	bool success;

	success = initialization();
	std::cout << (success ? "Success" : "Failure") << "\n";
	success = readInt();
	std::cout << (success ? "Success" : "Failure") << "\n";
	success = readFloat();
	std::cout << (success ? "Success" : "Failure") << "\n";
	success = readString();
	std::cout << (success ? "Success" : "Failure") << "\n";
	success = readMisc();
	std::cout << (success ? "Success" : "Failure") << "\n";

	delete json;
}
