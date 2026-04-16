// screw-up-native - Native CLI that Git versioning metadata dumper/formatter.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under MIT.
// https://github.com/kekyo/screw-up-native/

#ifndef SCREW_UP_TEST_UTILS_H
#define SCREW_UP_TEST_UTILS_H

#include <stddef.h>

typedef struct {
  char *path;
} GitTestRepository;

int git_test_repo_create(GitTestRepository *repo);
int git_test_repo_create_worktree(GitTestRepository *repo,
                                  GitTestRepository *worktree,
                                  const char *ref_name);
void git_test_repo_cleanup(GitTestRepository *repo);

int git_test_repo_create_file(GitTestRepository *repo,
                              const char *filename,
                              const char *content);

char *git_test_repo_commit(GitTestRepository *repo, const char *message);

int git_test_repo_create_tag(GitTestRepository *repo,
                             const char *tag_name,
                             const char *commit_hash);

int git_test_repo_create_annotated_tag(GitTestRepository *repo,
                                       const char *tag_name,
                                       const char *message,
                                       const char *commit_hash);

int git_test_repo_create_branch(GitTestRepository *repo,
                                const char *branch_name,
                                const char *start_point);

int git_test_repo_checkout(GitTestRepository *repo, const char *ref_name);

int git_test_repo_merge(GitTestRepository *repo,
                        const char *branch_name,
                        const char *message);

char *git_test_repo_current_commit(GitTestRepository *repo);

#endif
