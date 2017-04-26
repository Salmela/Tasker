#pragma once

#include <string>
#include <vector>
#include <map>
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
	Token(TokenType t) {type = t;};
	TokenType type;
	std::string string;
	union {
		bool visible;
		bool boolean;
		long integer;
		double real;
	} value;
};

class TokenStream {
public:
	virtual ~TokenStream() {};
	virtual void next(Token *token) {};
	virtual bool isCache() {return false;};
};

class IstreamTokenStream : public TokenStream {
public:
	IstreamTokenStream(std::istream &stream);
	void next(Token *token) override;
private:
	std::istream &mStream;
	Token *mToken;

	void tokenize();
	long parseLong(int c);
	double fast10pow(long exp);
	void parseNumber();
	void parseString();
	void generateUtf8(std::ostream &builder, int value);
};

class TokenCache : public TokenStream {
public:
	TokenCache();
	void record(Token &token);
	void next(Token *token) override;
	bool isCache() override {return true;};
	void dump() const;
	std::vector<Token> getTokens() const;
private:
	std::vector<Token> mTokens;
	unsigned int mIndex;
};

class Reader {
public:
	Reader(std::istream &stream);
	Reader(TokenCache *cache);
	~Reader();

	void read(bool &value);
	void read(int &value);
	void read(unsigned int &value);
	void read(float &value);
	void read(double &value);
	void read(std::string &value);
	void skipValue(TokenCache *cache = NULL, bool isForeignKey = false);

	void startObject();
	bool readObjectKey(std::string &key);

	void startArray();
	bool hasNextElement();
private:
	Token mToken;
	TokenStream *mTokenizer;
	TokenCache *mCache;
	bool mAfterStartBracket;
	bool mHasInternalTokenizer;

	std::string mCurrentKey;
	std::vector<char> mStack;//list of '{' or '[' or 'A' or 'O' characters
	void tokenize();

	friend class AssocArray;
};

class AssocArray {
public:
	AssocArray(Reader *reader);
	~AssocArray();
	bool has(std::string key) const;
	TokenCache *get(std::string key);
	const std::map<std::string, TokenCache*> getValues();
private:
	void read();

	Reader *mReader;
	std::map<std::string, TokenCache*> mValues;
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
	void write(const TokenCache &cache);

	void startObject();
	void endObject();
	void writeObjectKey(std::string key);

	void startArray();
	void endArray();
	void startNextElement();
private:
	void doIndentation(bool lineFeed = false) const;
	void valueStateTransition();
	void writeToken(Token *token);

	bool mDoPrettyPrint;
	unsigned int mIndentWidth;
	std::vector<char> mStack;
	char mIndentChar;
	State mState;
	std::ostream &mStream;
};

};
