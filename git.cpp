/* Backend for libgit
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

#include <string>
#include <git2.h>
#include "git.h"

namespace Tasker {
namespace Backend {

int GitBackend::refs_to_lib = 0;

GitBackend::GitBackend()
	:mRepo(NULL), mTreeBuilder(NULL)
{
	if(GitBackend::refs_to_lib == 0) {
		git_libgit2_init();
	}
	GitBackend::refs_to_lib++;
}

GitBackend::~GitBackend()
{
	if(mTreeBuilder) {
		git_treebuilder_free(mTreeBuilder);
	}
	if(mRepo) {
		git_repository_free(mRepo);
	}
	GitBackend::refs_to_lib--;
	if(GitBackend::refs_to_lib == 0) {
		git_libgit2_shutdown();
	}
}

GitBackend *GitBackend::open(std::string path)
{
	GitBackend *backend = new GitBackend();
	//git_repository_open_bare ?
	if(git_repository_open(&backend->mRepo, path.c_str())) {
		throw "repo open failed";
	}
	return backend;
}

GitBackend *GitBackend::create(std::string path)
{
	GitBackend *backend = new GitBackend();
	bool is_bare = false;//no working dir
	//git_repository_init_init_options ?
	if(git_repository_init(&backend->mRepo, path.c_str(), is_bare)) {
		throw "repo init failed";
	}
	if(git_treebuilder_new(&backend->mTreeBuilder, backend->mRepo, NULL)) {
		throw "Failed to create initial tree for repo";
	}
	try {
		backend->commit();
	} catch(...) {
		throw "Failed to create initial commit";
	}
	return backend;
}

GitFileBuffer *GitBackend::addFile(std::string file)
{
	if(!mTreeBuilder) {
		int ret = git_treebuilder_new(&mTreeBuilder, mRepo, NULL);
		if(ret) {
			throw "Failed to create tree builder";
		}
	}

	return new GitFileBuffer(this, file);
}

void GitBackend::commit()
{
	//TODO implement
	git_oid oid, oid_tree;
	git_tree *tree;
	git_signature *author;

	if(!mTreeBuilder) {
		throw "No changes";
	}

	if(git_treebuilder_write(&oid_tree, mTreeBuilder)) {
		throw "Write failed";
	}

	if(git_tree_lookup(&tree, mRepo, &oid_tree)) {
		throw "Tree lookup failed";
	}

	if(git_signature_default(&author, mRepo)) {
		throw "No default user";
	}
	git_commit *head = getHead();
	std::string msg = getNextCommitMessage(head);
	if(git_commit_create(&oid, mRepo, "HEAD", author, author, "UTF-8",
		msg.c_str(), tree, head ? 1 : 0, (const git_commit**)&head)) {
		throw "Failed to create commit";
	}
	git_commit_free(head);
	git_signature_free(author);
	git_tree_free(tree);

	git_treebuilder_free(mTreeBuilder);
	mTreeBuilder = NULL;
}

git_commit *GitBackend::getHead()
{
	git_reference *ref;
	if(git_repository_head(&ref, mRepo)) {
		return NULL;
	}
	const git_oid *id = git_reference_target(ref);
	git_commit *commit;
	git_commit_lookup(&commit, mRepo, id);
	git_reference_free(ref);
	return commit;
}

std::string GitBackend::getNextCommitMessage(struct git_commit *head)
{
	if(!head) return "0";
	const char *msg = git_commit_summary(head);

	//lets hope that nobody manually commits
	int index = atoi(msg) + 1;

	return std::to_string(index);
}

std::streambuf *GitBackend::getFile(std::string path)
{
	git_tree *root;
	git_commit *head = getHead();
	git_commit_tree(&root, head);
	git_commit_free(head);

	git_tree_entry *entry;
	if(git_tree_entry_bypath(&entry, root, path.c_str())) {
		git_tree_free(root);
		return NULL;
	}
	git_tree_free(root);
	if(git_tree_entry_type(entry) != GIT_OBJ_BLOB) {
		throw "entry is not a file";
	}
	git_blob *blob;
	git_tree_entry_to_object((git_object**)&blob, mRepo, entry);
	//TODO we should create InputStream so we would not need duplicate the content
	std::string buf((const char*)git_blob_rawcontent(blob), 0, git_blob_rawsize(blob));
	git_blob_free(blob);

	git_tree_entry_free(entry);
	return new std::basic_stringbuf<char>(buf);
}

GitFileBuffer::GitFileBuffer(GitBackend *backend, std::string file)
{
	mFile = file;
	mBackend = backend;
}

GitFileBuffer::~GitFileBuffer()
{
	git_oid oid;
	git_blob *blob;

	if(git_blob_create_frombuffer(&oid, mBackend->mRepo, mBuf.c_str(), mBuf.size())) {
		throw "Failed to create file from string";
	}

	if(git_blob_lookup(&blob, mBackend->mRepo, &oid)) {
		throw "Failed to find new blob";
	}

	if(git_treebuilder_insert(NULL, mBackend->mTreeBuilder, mFile.c_str(), &oid, GIT_FILEMODE_BLOB)) {
		throw "Can't insert blob to tree";
	}
	git_blob_free(blob);
}

int GitFileBuffer::overflow(int c)
{
	//TODO use the git blob streaming api
	mBuf += c;
	return 1;
}

};
};
