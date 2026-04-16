// screw-up-native - Native CLI that Git versioning metadata dumper/formatter.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under MIT.
// https://github.com/kekyo/screw-up-native/

#define _XOPEN_SOURCE 700

#include "test_utils.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../src/util_str.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static int run_command_capture(const char *working_dir,
                               char *const argv[],
                               char **output) {
  int pipefd[2];
  pid_t pid;
  int status;
  char buffer[512];
  ssize_t read_size;
  size_t total = 0;
  char *result = NULL;

  if (output && pipe(pipefd) != 0) {
    return -1;
  }

  pid = fork();
  if (pid < 0) {
    if (output) {
      close(pipefd[0]);
      close(pipefd[1]);
    }
    return -1;
  }

  if (pid == 0) {
    if (working_dir && chdir(working_dir) != 0) {
      _exit(127);
    }
    if (output) {
      dup2(pipefd[1], STDOUT_FILENO);
      close(pipefd[0]);
      close(pipefd[1]);
    }
    execvp(argv[0], argv);
    _exit(127);
  }

  if (output) {
    close(pipefd[1]);
    while ((read_size = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
      char *next = realloc(result, total + (size_t)read_size + 1);
      if (!next) {
        free(result);
        result = NULL;
        break;
      }
      result = next;
      memcpy(result + total, buffer, (size_t)read_size);
      total += (size_t)read_size;
      result[total] = '\0';
    }
    close(pipefd[0]);
  }

  if (waitpid(pid, &status, 0) < 0) {
    free(result);
    return -1;
  }

  if (output) {
    *output = result;
  }

  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }

  return -1;
}

static int run_git(GitTestRepository *repo,
                   char *const argv[],
                   char **output) {
  return run_command_capture(repo ? repo->path : NULL, argv, output);
}

int git_test_repo_create(GitTestRepository *repo) {
  char template_path[] = "/tmp/git-metadata-test-XXXXXX";
  char *dir;
  char *output = NULL;

  if (!repo) {
    return -1;
  }

  dir = mkdtemp(template_path);
  if (!dir) {
    return -1;
  }

  repo->path = su_strdup(dir);
  if (!repo->path) {
    return -1;
  }

  {
    char *const argv[] = {"git", "init", NULL};
    if (run_git(repo, argv, NULL) != 0) {
      return -1;
    }
  }

  {
    char *const argv[] = {"git", "config", "user.name", "Test User", NULL};
    if (run_git(repo, argv, NULL) != 0) {
      return -1;
    }
  }

  {
    char *const argv[] = {"git", "config", "user.email", "test@example.com", NULL};
    if (run_git(repo, argv, NULL) != 0) {
      return -1;
    }
  }

  {
    char *const argv[] = {"git", "checkout", "-b", "main", NULL};
    if (run_git(repo, argv, &output) != 0) {
      free(output);
      return -1;
    }
    free(output);
  }

  return 0;
}

int git_test_repo_create_worktree(GitTestRepository *repo,
                                  GitTestRepository *worktree,
                                  const char *ref_name) {
  char template_path[] = "/tmp/git-worktree-test-XXXXXX";
  char *dir;
  char *checkout_ref;

  if (!repo || !repo->path || !worktree) {
    return -1;
  }

  worktree->path = NULL;

  dir = mkdtemp(template_path);
  if (!dir) {
    return -1;
  }

  worktree->path = su_strdup(dir);
  if (!worktree->path) {
    char *const argv[] = {"rm", "-rf", dir, NULL};
    run_command_capture(NULL, argv, NULL);
    return -1;
  }

  checkout_ref = (char *)(ref_name ? ref_name : "HEAD");
  {
    char *const argv[] = {"git", "worktree", "add", worktree->path, checkout_ref, NULL};
    if (run_git(repo, argv, NULL) != 0) {
      git_test_repo_cleanup(worktree);
      return -1;
    }
  }

  return 0;
}

void git_test_repo_cleanup(GitTestRepository *repo) {
  if (!repo || !repo->path) {
    return;
  }

  {
    char *const argv[] = {"rm", "-rf", repo->path, NULL};
    run_command_capture(NULL, argv, NULL);
  }

  free(repo->path);
  repo->path = NULL;
}

int git_test_repo_create_file(GitTestRepository *repo,
                              const char *filename,
                              const char *content) {
  char path[PATH_MAX];
  char dir_path[PATH_MAX];
  size_t index;
  FILE *file;

  if (!repo || !repo->path || !filename) {
    return -1;
  }

  snprintf(path, sizeof(path), "%s/%s", repo->path, filename);
  snprintf(dir_path, sizeof(dir_path), "%s", path);
  for (index = 1; dir_path[index] != '\0'; index++) {
    if (dir_path[index] == '/') {
      dir_path[index] = '\0';
      mkdir(dir_path, 0755);
      dir_path[index] = '/';
    }
  }

  file = fopen(path, "w");
  if (!file) {
    return -1;
  }

  if (content) {
    fputs(content, file);
  }

  fclose(file);
  return 0;
}

char *git_test_repo_commit(GitTestRepository *repo, const char *message) {
  char *output = NULL;

  if (!repo) {
    return NULL;
  }

  {
    char *const argv[] = {"git", "add", ".", NULL};
    if (run_git(repo, argv, NULL) != 0) {
      return NULL;
    }
  }

  {
    char *const argv[] = {"git", "commit", "-m", (char *)message, NULL};
    if (run_git(repo, argv, NULL) != 0) {
      return NULL;
    }
  }

  {
    char *const argv[] = {"git", "rev-parse", "HEAD", NULL};
    if (run_git(repo, argv, &output) != 0) {
      free(output);
      return NULL;
    }
  }

  if (output) {
    char *trimmed = su_str_trim_copy(output);
    free(output);
    return trimmed;
  }

  return NULL;
}

int git_test_repo_create_tag(GitTestRepository *repo,
                             const char *tag_name,
                             const char *commit_hash) {
  if (!repo || !tag_name) {
    return -1;
  }

  if (commit_hash) {
    char *const argv[] = {"git", "tag", (char *)tag_name, (char *)commit_hash, NULL};
    return run_git(repo, argv, NULL);
  }

  {
    char *const argv[] = {"git", "tag", (char *)tag_name, NULL};
    return run_git(repo, argv, NULL);
  }
}

int git_test_repo_create_annotated_tag(GitTestRepository *repo,
                                       const char *tag_name,
                                       const char *message,
                                       const char *commit_hash) {
  if (!repo || !tag_name || !message) {
    return -1;
  }

  if (commit_hash) {
    char *const argv[] = {"git",
                          "tag",
                          "-a",
                          (char *)tag_name,
                          "-m",
                          (char *)message,
                          (char *)commit_hash,
                          NULL};
    return run_git(repo, argv, NULL);
  }

  {
    char *const argv[] = {"git",
                          "tag",
                          "-a",
                          (char *)tag_name,
                          "-m",
                          (char *)message,
                          NULL};
    return run_git(repo, argv, NULL);
  }
}

int git_test_repo_create_branch(GitTestRepository *repo,
                                const char *branch_name,
                                const char *start_point) {
  if (!repo || !branch_name) {
    return -1;
  }

  if (start_point) {
    char *const argv[] = {"git", "checkout", "-b", (char *)branch_name, (char *)start_point, NULL};
    return run_git(repo, argv, NULL);
  }

  {
    char *const argv[] = {"git", "checkout", "-b", (char *)branch_name, NULL};
    return run_git(repo, argv, NULL);
  }
}

int git_test_repo_checkout(GitTestRepository *repo, const char *ref_name) {
  if (!repo || !ref_name) {
    return -1;
  }
  {
    char *const argv[] = {"git", "checkout", (char *)ref_name, NULL};
    return run_git(repo, argv, NULL);
  }
}

int git_test_repo_merge(GitTestRepository *repo,
                        const char *branch_name,
                        const char *message) {
  if (!repo || !branch_name || !message) {
    return -1;
  }

  {
    char *const argv[] = {"git",
                          "merge",
                          "--no-ff",
                          "-m",
                          (char *)message,
                          (char *)branch_name,
                          NULL};
    return run_git(repo, argv, NULL);
  }
}

char *git_test_repo_current_commit(GitTestRepository *repo) {
  char *output = NULL;

  if (!repo) {
    return NULL;
  }

  {
    char *const argv[] = {"git", "rev-parse", "HEAD", NULL};
    if (run_git(repo, argv, &output) != 0) {
      free(output);
      return NULL;
    }
  }

  if (output) {
    char *trimmed = su_str_trim_copy(output);
    free(output);
    return trimmed;
  }

  return NULL;
}
