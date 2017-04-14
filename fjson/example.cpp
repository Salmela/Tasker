/* FJson example code.
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
#include "fjson.h"

const char *json_code = "{\"cars\":[{\"name\": \"betty\", \"age\": 8]}";

void parseCar(FJson::Reader *reader)
{
	std::cout << "Car element";
	reader->startObject();

	std::string key;
	while(reader->readObjectKey(key)) {
		if(key == "name") {
			std::string name;
			reader->read(name);
			std::cout << "Name: " << name << "\n";
		} else if(key == "age") {
			int age;
			reader->read(age);
			std::cout << "Age: " << age << "\n";
		} else {
			std::cout << "Unknown\n";
		}
	}
}

void parseRootObject(FJson::Reader *reader)
{
	reader->startObject();
	std::string key;

	while(reader->readObjectKey(key)) {
		if(key == "cars") {
			std::cout << "Cars";
			reader->startArray();
			while(reader->hasNextElement()) {
				parseCar(reader);
			}
		} else {
			std::cout << "Unknown\n";
		}
	}
}

int main() {
	std::istringstream s(json_code);
	auto reader = FJson::Reader(s);
	parseRootObject(&reader);
}
