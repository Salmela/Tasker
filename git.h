#pragma once

#include <vector>
//#include <ostream>
#include <sstream>
#include <streambuf>

namespace Tasker {
namespace Backend {

class GitBackend;

class GitFileBuffer : public std::streambuf {
public:
	GitFileBuffer(GitBackend *backend, std::string file);
	~GitFileBuffer();

	//streamsize xsputn(const char* s, streamsize n) override;
};

class GitBackend {
public:
	GitBackend();
	~GitBackend();
	static GitBackend *open(std::string path);
	static GitBackend *create(std::string path);

	GitFileBuffer *fileStream(std::string file);
	void addFile(std::string file, std::string content);
	void commit();
private:
	struct git_commit *getHead();
	std::string getFile(std::string path);

	struct git_repository *mRepo;
	struct git_treebuilder *mTreeBuilder;
};

};
};
