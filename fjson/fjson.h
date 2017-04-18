#pragma once

#include <string>
#include <vector>
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
	END_OBJECT,
	END_ARRAY,
	SEPARATOR,
	COLON,
	END
};

enum State {
	S_INIT,//< initial state
	S_START,//< after Object or Array start token
	S_VALUE,//< after Value is written
	S_SEPARATOR//< after Object or Array separator
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
	void skipValue();

	void startObject();
	bool readObjectKey(std::string &key);

	void startArray();
	bool hasNextElement();
private:
	std::istream &mStream;
	Token mToken;
	bool afterStartBracket;

	std::vector<char> mStack;//list of '{' or '[' or 'A' or 'O' characters

	int tokenize();
	long parseLong(int c);
	double fast10pow(long exp);
	void parseNumber();
	void parseString();
	void generateUtf8(std::ostream &builder, int value);
};

class Writer {
public:
	Writer(std::ostream &stream, bool doPretty = false);
	~Writer();

	void write(void *value);
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
	void startNextElement();
private:
	void doIndentation(bool lineFeed = false) const;
	void valueStateTransition();

	bool mDoPrettyPrint;
	unsigned int mIndentWidth;
	std::vector<char> mStack;
	char mIndentChar;
	State mState;
	std::ostream &mStream;
};

};
