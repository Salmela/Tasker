#pragma once

#include <vector>
#include <sstream>
#include <streambuf>

namespace Tasker {
namespace Backend {

class GitBackend;

class GitFileBuffer : public std::streambuf {
public:
	GitFileBuffer(GitBackend *backend, std::string file);
	~GitFileBuffer();

	void close(struct git_oid *oid);

	//streamsize xsputn(const char* s, streamsize n) override;
	int overflow(int c) override;
private:
	std::string mBuf;
	std::string mFile;
	GitBackend *mBackend;
};

class GitBackend {
public:
	GitBackend();
	~GitBackend();
	static GitBackend *open(std::string path);
	static GitBackend *create(std::string path);

	GitFileBuffer *addFile(std::string file);
	std::streambuf *getFile(std::string path);
	void commit();
private:
	struct git_commit *getHead();
	std::string getNextCommitMessage(struct git_commit *head);

	struct git_repository *mRepo;
	struct git_treebuilder *mTreeBuilder;
	static int refs_to_lib;

	friend GitFileBuffer::~GitFileBuffer();
};

};
};
