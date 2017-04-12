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
			while (reader->hasNextElement()) {
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
