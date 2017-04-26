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

GitBackend::GitBackend()
	:mRepo(NULL), mTreeBuilder(NULL)
{
}

GitBackend::~GitBackend()
{
	if(mTreeBuilder) {
		git_treebuilder_free(mTreeBuilder);
	}
	if(mRepo) {
		git_repository_free(mRepo);
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
	return backend;
}

void GitBackend::addFile(std::string file, std::string content)
{
	git_oid oid;
	git_blob *blob;

	if(!mTreeBuilder) {
		int ret = git_treebuilder_create(&mTreeBuilder, NULL);
		if(ret) {
			throw "Failed to create tree builder";
		}
	}

	//TODO git_blob_create_fromchunks
	if(git_blob_create_frombuffer(&oid, mRepo, content.c_str(), content.size())) {
		throw "Failed to create file from string";
	}
	if(git_blob_lookup(&blob, mRepo, &oid)) {
		throw "Failed to find new blob";
	}
	if(git_treebuilder_insert(NULL, mTreeBuilder, file.c_str(), &oid, GIT_FILEMODE_BLOB)) {
		throw "Can't insert blob to tree";
	}
	git_blob_free(blob);
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

	if(git_treebuilder_write(&oid_tree, mRepo, mTreeBuilder)) {
		throw "Write failed";
	}

	if(git_tree_lookup(&tree, mRepo, &oid_tree)) {
		throw "Tree lookup failed";
	}

	git_signature_default(&author, mRepo);
	git_commit *head = getHead();
	git_commit_create(&oid, mRepo, "HEAD", author, author, "UTF-8",
		"test msg", tree, 1, (const git_commit**)head);

	git_treebuilder_free(mTreeBuilder);
	mTreeBuilder = NULL;
}

git_commit *GitBackend::getHead()
{
	git_reference *ref;
	if(git_repository_head(&ref, mRepo)) {
		throw "get file failed";
	}
	const git_oid *id = git_reference_target_peel(ref);
	git_commit *commit;
	git_commit_lookup(&commit, mRepo, id);
	git_reference_free(ref);
	return commit;
}

std::string GitBackend::getFile(std::string path)
{
	//git_commit *head = getHead();
	//git_tree *tree;
	//git_commit_tree(&tree, commit);
	return "";
}

GitFileBuffer::GitFileBuffer(GitBackend *backend, std::string file)
{
}

GitFileBuffer::~GitFileBuffer()
{

}

};
};
