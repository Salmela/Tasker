#pragma once

#include <string>
#include <iostream>

namespace FJson {

enum TokenType {
	STRING,
	BOOLEAN,
	NUL,
	INTEGER,
	REAL,
	OBJECT,
	ARRAY,
	SEPARATOR
};

struct Token {
	TokenType type;
	std::string string;
	union {
		bool boolean;
		long integer;
		double real;
	} value;
};

class Reader {
public:
	Reader(std::istream &stream);

	void read(bool &value);
	void read(int &value);
	void read(unsigned int &value);
	void read(float &value);
	void read(double &value);
	void read(std::string &value);

	void startObject();
	bool readObjectKey(std::string &key);

	void startArray();
	bool hasNextElement();
private:
	std::istream &mStream;
	Token mToken;

	void expect(int expected);
	int tokenize();
	long parseLong(int c);
	double fast10pow(long exp);
	void parseNumber();
	void parseString();
};

class Writer {
public:
	Writer(std::ostream &stream);

	void write(bool value);
	void write(int value);
	void write(unsigned int value);
	void write(float value);
	void write(double value);
	void write(std::string value);

	void startObject();
	void endObject();
	void writeObjectKey(std::string key);

	void startArray();
	void endArray();
private:
	std::ostream &mStream;
};

};
