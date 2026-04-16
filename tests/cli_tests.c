// screw-up-native - Native CLI that Git versioning metadata dumper/formatter.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under MIT.
// https://github.com/kekyo/screw-up-native/

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/cli.h"
#include "../src/util_str.h"
#include "version.h"
#include "test_utils.h"

#define ASSERT_TRUE(cond, msg) \
  do { \
    if (!(cond)) { \
      fprintf(stderr, "FAIL %s: %s\n", __func__, msg); \
      result = 1; \
      goto cleanup; \
    } \
  } while (0)

#define ASSERT_STR_EQ(actual, expected, label) \
  do { \
    if (!(actual) || strcmp((actual), (expected)) != 0) { \
      fprintf(stderr, "FAIL %s: %s expected '%s' got '%s'\n", \
              __func__, label, (expected), (actual) ? (actual) : "(null)"); \
      result = 1; \
      goto cleanup; \
    } \
  } while (0)

static int is_json_space(char ch) {
  return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}

static int json_contains_string_value(const char *json,
                                      const char *key,
                                      const char *expected) {
  char pattern[128];
  const char *cursor;
  size_t key_len;
  size_t value_len;

  if (!json || !key || !expected) {
    return 0;
  }

  key_len = strlen(key);
  value_len = strlen(expected);
  if (key_len == 0 || value_len == 0) {
    return 0;
  }

  if (snprintf(pattern, sizeof(pattern), "\"%s\"", key) < 0) {
    return 0;
  }

  cursor = json;
  while ((cursor = strstr(cursor, pattern)) != NULL) {
    const char *scan = cursor + strlen(pattern);
    while (*scan && is_json_space(*scan)) {
      scan++;
    }
    if (*scan != ':') {
      cursor = scan;
      continue;
    }
    scan++;
    while (*scan && is_json_space(*scan)) {
      scan++;
    }
    if (*scan != '"') {
      cursor = scan;
      continue;
    }
    scan++;
    if (strncmp(scan, expected, value_len) == 0 && scan[value_len] == '"') {
      return 1;
    }
    cursor = scan;
  }

  return 0;
}

static char *run_cli_with_input(const char *input,
                                int argc,
                                char **argv) {
  int saved_stdin = -1;
  int saved_stdout = -1;
  FILE *input_file = NULL;
  FILE *output_file = NULL;
  char *output = NULL;
  int exit_code;
  const char *payload = input ? input : "";

  saved_stdin = dup(STDIN_FILENO);
  saved_stdout = dup(STDOUT_FILENO);
  if (saved_stdin < 0 || saved_stdout < 0) {
    goto cleanup;
  }

  input_file = tmpfile();
  output_file = tmpfile();
  if (!input_file || !output_file) {
    goto cleanup;
  }

  fwrite(payload, 1, strlen(payload), input_file);
  fflush(input_file);
  rewind(input_file);

  fflush(stdout);
  if (dup2(fileno(input_file), STDIN_FILENO) < 0) {
    goto cleanup;
  }
  if (dup2(fileno(output_file), STDOUT_FILENO) < 0) {
    goto cleanup;
  }

  exit_code = screw_up_main(argc, argv);

  fflush(stdout);
  dup2(saved_stdin, STDIN_FILENO);
  dup2(saved_stdout, STDOUT_FILENO);

  if (exit_code != 0) {
    goto cleanup;
  }

  fseek(output_file, 0, SEEK_END);
  {
    long length = ftell(output_file);
    if (length < 0) {
      goto cleanup;
    }
    rewind(output_file);
    output = (char *)calloc((size_t)length + 1, 1);
    if (!output) {
      goto cleanup;
    }
    if (length > 0) {
      size_t read_size = fread(output, 1, (size_t)length, output_file);
      if (read_size != (size_t)length) {
        goto cleanup;
      }
      output[length] = '\0';
    }
  }

cleanup:
  if (saved_stdin >= 0) {
    dup2(saved_stdin, STDIN_FILENO);
    close(saved_stdin);
  }
  if (saved_stdout >= 0) {
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);
  }
  if (input_file) {
    fclose(input_file);
  }
  if (output_file) {
    fclose(output_file);
  }

  return output;
}

static int test_cli_format_replaces_placeholders(void) {
  int result = 1;
  GitTestRepository repo = {0};
  char *commit_hash = NULL;
  char short_hash[8];
  char expected[512];
  char *actual = NULL;
  char cwd[512];
  char *input =
    "Version {version}\n"
    "Commit {git.commit.shortHash}\n"
    "Tags {git.tags}\n"
    "Missing {nope}\n";
  char *argv[] = {"screw-up", "format", NULL};

  if (getcwd(cwd, sizeof(cwd)) == NULL) {
    goto cleanup;
  }

  if (git_test_repo_create(&repo) != 0) {
    goto cleanup;
  }

  git_test_repo_create_file(&repo, "README.md", "# Test Project");
  commit_hash = git_test_repo_commit(&repo, "Initial commit");
  git_test_repo_create_tag(&repo, "v1.2.3", NULL);

  ASSERT_TRUE(commit_hash != NULL, "commit hash missing");
  memcpy(short_hash, commit_hash, 7);
  short_hash[7] = '\0';

  if (chdir(repo.path) != 0) {
    goto cleanup;
  }

  actual = run_cli_with_input(input, 2, argv);

  if (chdir(cwd) != 0) {
    goto cleanup;
  }

  snprintf(expected,
           sizeof(expected),
           "Version 1.2.3\nCommit %s\nTags [\"v1.2.3\"]\nMissing {nope}\n",
           short_hash);

  ASSERT_STR_EQ(actual, expected, "formatted output");

  result = 0;

cleanup:
  if (chdir(cwd) != 0) {
    result = 1;
  }
  free(actual);
  free(commit_hash);
  git_test_repo_cleanup(&repo);
  return result;
}

static int test_cli_help_options(void) {
  int result = 1;
  char *output = NULL;
  char *argv_help[] = {"screw-up", "--help", NULL};
  char *argv_h[] = {"screw-up", "-h", NULL};
  char *argv_q[] = {"screw-up", "-?", NULL};

  output = run_cli_with_input("", 2, argv_help);
  ASSERT_TRUE(output != NULL, "help output missing");
  ASSERT_TRUE(strstr(output, "screw-up (native) [") != NULL, "help banner missing");
  ASSERT_TRUE(strstr(output, SCREW_UP_VERSION) != NULL, "version missing");
  free(output);
  output = NULL;

  output = run_cli_with_input("", 2, argv_h);
  ASSERT_TRUE(output != NULL, "short help output missing");
  ASSERT_TRUE(strstr(output, "Usage: screw-up <format|dump> [options]") != NULL, "usage missing");
  free(output);
  output = NULL;

  output = run_cli_with_input("", 2, argv_q);
  ASSERT_TRUE(output != NULL, "question help output missing");
  ASSERT_TRUE(strstr(output, "Options:") != NULL, "options missing");

  result = 0;

cleanup:
  free(output);
  return result;
}

static int test_cli_dump_outputs_metadata(void) {
  int result = 1;
  GitTestRepository repo = {0};
  char *commit_hash = NULL;
  char short_hash[8];
  char *output = NULL;
  char *trimmed = NULL;
  char cwd[512];
  char *argv[] = {"screw-up", "dump", NULL};

  if (getcwd(cwd, sizeof(cwd)) == NULL) {
    goto cleanup;
  }

  if (git_test_repo_create(&repo) != 0) {
    goto cleanup;
  }

  git_test_repo_create_file(&repo, "README.md", "# Test Project");
  commit_hash = git_test_repo_commit(&repo, "Initial commit");
  git_test_repo_create_tag(&repo, "v1.2.3", NULL);

  ASSERT_TRUE(commit_hash != NULL, "commit hash missing");
  memcpy(short_hash, commit_hash, 7);
  short_hash[7] = '\0';

  if (chdir(repo.path) != 0) {
    goto cleanup;
  }

  output = run_cli_with_input("", 2, argv);

  if (chdir(cwd) != 0) {
    goto cleanup;
  }

  ASSERT_TRUE(output != NULL, "dump output missing");
  trimmed = su_str_trim_copy(output);
  ASSERT_TRUE(trimmed != NULL, "trimmed output missing");
  ASSERT_TRUE(json_contains_string_value(trimmed, "version", "1.2.3"),
              "version missing");
  ASSERT_TRUE(json_contains_string_value(trimmed, "shortHash", short_hash),
              "short hash missing");

  result = 0;

cleanup:
  if (chdir(cwd) != 0) {
    result = 1;
  }
  free(trimmed);
  free(output);
  free(commit_hash);
  git_test_repo_cleanup(&repo);
  return result;
}

static int test_cli_dump_directory_argument(void) {
  int result = 1;
  GitTestRepository repo = {0};
  char *commit_hash = NULL;
  char short_hash[8];
  char *output = NULL;
  char *trimmed = NULL;
  char *argv[] = {"screw-up", "dump", NULL, NULL};

  if (git_test_repo_create(&repo) != 0) {
    goto cleanup;
  }

  git_test_repo_create_file(&repo, "README.md", "# Test Project");
  commit_hash = git_test_repo_commit(&repo, "Initial commit");
  git_test_repo_create_tag(&repo, "v2.0.0", NULL);

  ASSERT_TRUE(commit_hash != NULL, "commit hash missing");
  memcpy(short_hash, commit_hash, 7);
  short_hash[7] = '\0';

  argv[2] = repo.path;
  output = run_cli_with_input("", 3, argv);

  ASSERT_TRUE(output != NULL, "dump output missing");
  trimmed = su_str_trim_copy(output);
  ASSERT_TRUE(trimmed != NULL, "trimmed output missing");
  ASSERT_TRUE(json_contains_string_value(trimmed, "version", "2.0.0"),
              "version missing");
  ASSERT_TRUE(json_contains_string_value(trimmed, "shortHash", short_hash),
              "short hash missing");

  result = 0;

cleanup:
  free(trimmed);
  free(output);
  free(commit_hash);
  git_test_repo_cleanup(&repo);
  return result;
}

static int test_cli_dump_worktree_directory_argument(void) {
  int result = 1;
  GitTestRepository repo = {0};
  GitTestRepository worktree = {0};
  char *commit_hash = NULL;
  char short_hash[8];
  char *output = NULL;
  char *trimmed = NULL;
  char *argv[] = {"screw-up", "dump", NULL, NULL};

  if (git_test_repo_create(&repo) != 0) {
    goto cleanup;
  }

  git_test_repo_create_file(&repo, "README.md", "# Test Project");
  commit_hash = git_test_repo_commit(&repo, "Initial commit");
  git_test_repo_create_tag(&repo, "v1.2.3", NULL);

  if (git_test_repo_create_worktree(&repo, &worktree, "HEAD") != 0) {
    goto cleanup;
  }

  ASSERT_TRUE(commit_hash != NULL, "commit hash missing");
  memcpy(short_hash, commit_hash, 7);
  short_hash[7] = '\0';

  argv[2] = worktree.path;
  output = run_cli_with_input("", 3, argv);

  ASSERT_TRUE(output != NULL, "dump output missing");
  trimmed = su_str_trim_copy(output);
  ASSERT_TRUE(trimmed != NULL, "trimmed output missing");
  ASSERT_TRUE(json_contains_string_value(trimmed, "version", "1.2.3"),
              "version missing");
  ASSERT_TRUE(json_contains_string_value(trimmed, "hash", commit_hash),
              "commit hash missing");
  ASSERT_TRUE(json_contains_string_value(trimmed, "shortHash", short_hash),
              "short hash missing");

  result = 0;

cleanup:
  free(trimmed);
  free(output);
  free(commit_hash);
  git_test_repo_cleanup(&worktree);
  git_test_repo_cleanup(&repo);
  return result;
}

static int test_cli_dump_no_wds_option(void) {
  int result = 1;
  GitTestRepository repo = {0};
  char *output_default = NULL;
  char *output_no_wds = NULL;
  char *trimmed_default = NULL;
  char *trimmed_no_wds = NULL;
  char cwd[512];
  char *argv_default[] = {"screw-up", "dump", NULL};
  char *argv_no_wds[] = {"screw-up", "dump", "--no-wds", NULL};

  if (getcwd(cwd, sizeof(cwd)) == NULL) {
    goto cleanup;
  }

  if (git_test_repo_create(&repo) != 0) {
    goto cleanup;
  }

  git_test_repo_create_file(&repo, "README.md", "# Test Project");
  git_test_repo_commit(&repo, "Initial commit");
  git_test_repo_create_tag(&repo, "v1.0.0", NULL);

  git_test_repo_create_file(&repo, "README.md", "# Modified Project");

  if (chdir(repo.path) != 0) {
    goto cleanup;
  }

  output_default = run_cli_with_input("", 2, argv_default);
  output_no_wds = run_cli_with_input("", 3, argv_no_wds);

  if (chdir(cwd) != 0) {
    goto cleanup;
  }

  ASSERT_TRUE(output_default != NULL, "default dump output missing");
  ASSERT_TRUE(output_no_wds != NULL, "no-wds dump output missing");
  trimmed_default = su_str_trim_copy(output_default);
  trimmed_no_wds = su_str_trim_copy(output_no_wds);
  ASSERT_TRUE(trimmed_default != NULL, "trimmed default output missing");
  ASSERT_TRUE(trimmed_no_wds != NULL, "trimmed no-wds output missing");
  ASSERT_TRUE(json_contains_string_value(trimmed_default, "version", "1.0.1"),
              "default wds version missing");
  ASSERT_TRUE(json_contains_string_value(trimmed_no_wds, "version", "1.0.0"),
              "no-wds version missing");

  result = 0;

cleanup:
  if (chdir(cwd) != 0) {
    result = 1;
  }
  free(trimmed_default);
  free(trimmed_no_wds);
  free(output_default);
  free(output_no_wds);
  git_test_repo_cleanup(&repo);
  return result;
}

static int test_cli_dump_help_option(void) {
  int result = 1;
  char *output = NULL;
  char *argv_dump_help[] = {"screw-up", "dump", "--help", NULL};

  output = run_cli_with_input("", 3, argv_dump_help);
  ASSERT_TRUE(output != NULL, "dump help output missing");
  ASSERT_TRUE(strstr(output, "Usage: screw-up dump") != NULL, "dump usage missing");

  result = 0;

cleanup:
  free(output);
  return result;
}

static int test_cli_format_input_option(void) {
  int result = 1;
  GitTestRepository repo = {0};
  char *commit_hash = NULL;
  char short_hash[8];
  char template_path[] = "/tmp/screwup-format-XXXXXX";
  char *actual = NULL;
  char cwd[512];
  char *argv[] = {"screw-up", "format", "--input", template_path, NULL};
  int template_fd = -1;

  if (getcwd(cwd, sizeof(cwd)) == NULL) {
    goto cleanup;
  }

  if (git_test_repo_create(&repo) != 0) {
    goto cleanup;
  }

  git_test_repo_create_file(&repo, "README.md", "# Test Project");
  commit_hash = git_test_repo_commit(&repo, "Initial commit");
  git_test_repo_create_tag(&repo, "v1.0.0", NULL);

  ASSERT_TRUE(commit_hash != NULL, "commit hash missing");
  memcpy(short_hash, commit_hash, 7);
  short_hash[7] = '\0';

  template_fd = mkstemp(template_path);
  if (template_fd < 0) {
    goto cleanup;
  }
  {
    FILE *file = fdopen(template_fd, "w");
    if (!file) {
      goto cleanup;
    }
    fputs("Version {version}\nCommit {git.commit.shortHash}\n", file);
    fclose(file);
    template_fd = -1;
  }

  if (chdir(repo.path) != 0) {
    goto cleanup;
  }

  actual = run_cli_with_input("", 4, argv);

  if (chdir(cwd) != 0) {
    goto cleanup;
  }

  {
    char expected[256];
    snprintf(expected,
             sizeof(expected),
             "Version 1.0.0\nCommit %s\n",
             short_hash);
    ASSERT_STR_EQ(actual, expected, "formatted output");
  }

  result = 0;

cleanup:
  if (chdir(cwd) != 0) {
    result = 1;
  }
  if (template_fd >= 0) {
    close(template_fd);
  }
  unlink(template_path);
  free(actual);
  free(commit_hash);
  git_test_repo_cleanup(&repo);
  return result;
}

static int test_cli_format_bracket_option(void) {
  int result = 1;
  GitTestRepository repo = {0};
  char *commit_hash = NULL;
  char short_hash[8];
  char template_path[512];
  char *actual = NULL;
  char cwd[512];
  char *argv[] = {"screw-up", "format", "-i", template_path, "-b", "#{,}#", NULL};

  if (getcwd(cwd, sizeof(cwd)) == NULL) {
    goto cleanup;
  }

  if (git_test_repo_create(&repo) != 0) {
    goto cleanup;
  }

  git_test_repo_create_file(&repo, "README.md", "# Test Project");
  commit_hash = git_test_repo_commit(&repo, "Initial commit");

  ASSERT_TRUE(commit_hash != NULL, "commit hash missing");
  memcpy(short_hash, commit_hash, 7);
  short_hash[7] = '\0';

  snprintf(template_path, sizeof(template_path), "%s/template-bracket.txt", repo.path);
  {
    FILE *file = fopen(template_path, "w");
    if (!file) {
      goto cleanup;
    }
    fputs("Commit #{git.commit.shortHash}#", file);
    fclose(file);
  }

  if (chdir(repo.path) != 0) {
    goto cleanup;
  }

  actual = run_cli_with_input("", 6, argv);

  if (chdir(cwd) != 0) {
    goto cleanup;
  }

  {
    char expected[128];
    snprintf(expected, sizeof(expected), "Commit %s", short_hash);
    ASSERT_STR_EQ(actual, expected, "formatted output");
  }

  result = 0;

cleanup:
  if (chdir(cwd) != 0) {
    result = 1;
  }
  free(actual);
  free(commit_hash);
  git_test_repo_cleanup(&repo);
  return result;
}

static int test_cli_format_no_wds_option(void) {
  int result = 1;
  GitTestRepository repo = {0};
  char template_path[512];
  char *actual_default = NULL;
  char *actual_no_wds = NULL;
  char cwd[512];
  char *argv_default[] = {"screw-up", "format", "--input", template_path, NULL};
  char *argv_no_wds[] = {"screw-up", "format", "--input", template_path, "--no-wds", NULL};

  if (getcwd(cwd, sizeof(cwd)) == NULL) {
    goto cleanup;
  }

  if (git_test_repo_create(&repo) != 0) {
    goto cleanup;
  }

  git_test_repo_create_file(&repo, "README.md", "# Test Project");
  git_test_repo_commit(&repo, "Initial commit");
  git_test_repo_create_tag(&repo, "v1.0.0", NULL);

  snprintf(template_path, sizeof(template_path), "%s/template-wds.txt", repo.path);
  {
    FILE *file = fopen(template_path, "w");
    if (!file) {
      goto cleanup;
    }
    fputs("Version {version}", file);
    fclose(file);
  }

  git_test_repo_create_file(&repo, "README.md", "# Modified Project");

  if (chdir(repo.path) != 0) {
    goto cleanup;
  }

  actual_default = run_cli_with_input("", 4, argv_default);
  actual_no_wds = run_cli_with_input("", 5, argv_no_wds);

  if (chdir(cwd) != 0) {
    goto cleanup;
  }

  ASSERT_STR_EQ(actual_default, "Version 1.0.1", "default wds output");
  ASSERT_STR_EQ(actual_no_wds, "Version 1.0.0", "no-wds output");

  result = 0;

cleanup:
  if (chdir(cwd) != 0) {
    result = 1;
  }
  free(actual_default);
  free(actual_no_wds);
  git_test_repo_cleanup(&repo);
  return result;
}

typedef int (*TestFn)(void);

typedef struct {
  const char *name;
  TestFn fn;
} TestCase;

int main(void) {
  TestCase tests[] = {
    {"cli format replaces placeholders", test_cli_format_replaces_placeholders},
    {"cli format input option", test_cli_format_input_option},
    {"cli format bracket option", test_cli_format_bracket_option},
    {"cli format no-wds option", test_cli_format_no_wds_option},
    {"cli help options", test_cli_help_options},
    {"cli dump outputs metadata", test_cli_dump_outputs_metadata},
    {"cli dump directory argument", test_cli_dump_directory_argument},
    {"cli dump worktree directory argument", test_cli_dump_worktree_directory_argument},
    {"cli dump no-wds option", test_cli_dump_no_wds_option},
    {"cli dump help option", test_cli_dump_help_option},
  };
  size_t index;
  int failures = 0;

  for (index = 0; index < sizeof(tests) / sizeof(tests[0]); index++) {
    if (tests[index].fn() != 0) {
      fprintf(stderr, "Test failed: %s\n", tests[index].name);
      failures++;
    } else {
      printf("PASS: %s\n", tests[index].name);
    }
  }

  if (failures > 0) {
    fprintf(stderr, "%d test(s) failed.\n", failures);
    return 1;
  }

  printf("All tests passed.\n");
  return 0;
}
