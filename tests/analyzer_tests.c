// screw-up-native - Native CLI that Git versioning metadata dumper/formatter.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under MIT.
// https://github.com/kekyo/screw-up-native/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/analyzer.h"
#include "../src/logger.h"
#include "../src/util_vec.h"
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

#define ASSERT_STR_ARRAY(array, expected, count, label) \
  do { \
    if (!string_array_equals((array), (expected), (count))) { \
      size_t index; \
      fprintf(stderr, "FAIL %s: %s mismatch (len=%zu)\n", \
              __func__, label, (array)->length); \
      for (index = 0; index < (array)->length; index++) { \
        fprintf(stderr, "  actual[%zu]=%s\n", index, (array)->items[index]); \
      } \
      result = 1; \
      goto cleanup; \
    } \
  } while (0)

static Logger test_logger(void) {
  return logger_null();
}

static int test_tag_on_current_commit(void) {
  int result = 1;
  GitTestRepository repo = {0};
  GitMetadataFetcher *fetcher = NULL;
  Metadata *metadata = NULL;
  char *commit_hash = NULL;
  Logger logger = test_logger();

  if (git_test_repo_create(&repo) != 0) {
    goto cleanup;
  }

  git_test_repo_create_file(&repo, "README.md", "# Test Project");
  commit_hash = git_test_repo_commit(&repo, "Initial commit");
  git_test_repo_create_tag(&repo, "v1.2.3", NULL);

  fetcher = get_fetch_git_metadata(repo.path, 1, &logger);
  metadata = git_metadata_fetch(fetcher);

  ASSERT_TRUE(metadata && metadata->has_git, "metadata.git missing");
  ASSERT_STR_EQ(metadata->git.version, "1.2.3", "version");

  {
    const char *expected_tags[] = {"v1.2.3"};
    ASSERT_STR_ARRAY(&metadata->git.tags, expected_tags, 1, "tags");
  }

  ASSERT_STR_EQ(metadata->git.commit.hash, commit_hash, "commit hash");

  result = 0;

cleanup:
  free(commit_hash);
  if (fetcher) {
    git_metadata_fetcher_free(fetcher);
  }
  git_test_repo_cleanup(&repo);
  return result;
}

static int test_annotated_tag_on_current_commit(void) {
  int result = 1;
  GitTestRepository repo = {0};
  GitMetadataFetcher *fetcher = NULL;
  Metadata *metadata = NULL;
  char *commit_hash = NULL;
  Logger logger = test_logger();

  if (git_test_repo_create(&repo) != 0) {
    goto cleanup;
  }

  git_test_repo_create_file(&repo, "README.md", "# Test Project");
  commit_hash = git_test_repo_commit(&repo, "Initial commit");
  git_test_repo_create_annotated_tag(&repo,
                                     "v1.2.3",
                                     "Release version 1.2.3",
                                     NULL);

  fetcher = get_fetch_git_metadata(repo.path, 1, &logger);
  metadata = git_metadata_fetch(fetcher);

  ASSERT_TRUE(metadata && metadata->has_git, "metadata.git missing");
  ASSERT_STR_EQ(metadata->git.version, "1.2.3", "version");

  {
    const char *expected_tags[] = {"v1.2.3"};
    ASSERT_STR_ARRAY(&metadata->git.tags, expected_tags, 1, "tags");
  }

  ASSERT_STR_EQ(metadata->git.commit.hash, commit_hash, "commit hash");

  result = 0;

cleanup:
  free(commit_hash);
  if (fetcher) {
    git_metadata_fetcher_free(fetcher);
  }
  git_test_repo_cleanup(&repo);
  return result;
}

static int test_tag_in_parent_commit(void) {
  int result = 1;
  GitTestRepository repo = {0};
  GitMetadataFetcher *fetcher = NULL;
  Metadata *metadata = NULL;
  char *current_commit = NULL;
  Logger logger = test_logger();

  if (git_test_repo_create(&repo) != 0) {
    goto cleanup;
  }

  git_test_repo_create_file(&repo, "README.md", "# Test Project");
  git_test_repo_commit(&repo, "Initial commit");
  git_test_repo_create_tag(&repo, "v1.0.0", NULL);

  git_test_repo_create_file(&repo, "file.txt", "content");
  current_commit = git_test_repo_commit(&repo, "Add file");

  fetcher = get_fetch_git_metadata(repo.path, 1, &logger);
  metadata = git_metadata_fetch(fetcher);

  ASSERT_STR_EQ(metadata->git.version, "1.0.1", "version");
  ASSERT_TRUE(metadata->git.tags.length == 0, "tags should be empty");
  ASSERT_STR_EQ(metadata->git.commit.hash, current_commit, "commit hash");

  result = 0;

cleanup:
  free(current_commit);
  if (fetcher) {
    git_metadata_fetcher_free(fetcher);
  }
  git_test_repo_cleanup(&repo);
  return result;
}

static int test_annotated_tag_in_parent_commit(void) {
  int result = 1;
  GitTestRepository repo = {0};
  GitMetadataFetcher *fetcher = NULL;
  Metadata *metadata = NULL;
  char *current_commit = NULL;
  Logger logger = test_logger();

  if (git_test_repo_create(&repo) != 0) {
    goto cleanup;
  }

  git_test_repo_create_file(&repo, "README.md", "# Test Project");
  git_test_repo_commit(&repo, "Initial commit");
  git_test_repo_create_annotated_tag(&repo, "v1.0.0", "Initial release", NULL);

  git_test_repo_create_file(&repo, "file.txt", "content");
  current_commit = git_test_repo_commit(&repo, "Add file");

  fetcher = get_fetch_git_metadata(repo.path, 1, &logger);
  metadata = git_metadata_fetch(fetcher);

  ASSERT_STR_EQ(metadata->git.version, "1.0.1", "version");
  ASSERT_TRUE(metadata->git.tags.length == 0, "tags should be empty");
  ASSERT_STR_EQ(metadata->git.commit.hash, current_commit, "commit hash");

  result = 0;

cleanup:
  free(current_commit);
  if (fetcher) {
    git_metadata_fetcher_free(fetcher);
  }
  git_test_repo_cleanup(&repo);
  return result;
}

static int test_tag_in_grandparent_commit(void) {
  int result = 1;
  GitTestRepository repo = {0};
  GitMetadataFetcher *fetcher = NULL;
  Metadata *metadata = NULL;
  char *current_commit = NULL;
  Logger logger = test_logger();

  if (git_test_repo_create(&repo) != 0) {
    goto cleanup;
  }

  git_test_repo_create_file(&repo, "README.md", "# Test Project");
  git_test_repo_commit(&repo, "Initial commit");
  git_test_repo_create_tag(&repo, "v2.1.0", NULL);

  git_test_repo_create_file(&repo, "file1.txt", "content1");
  git_test_repo_commit(&repo, "Add file1");

  git_test_repo_create_file(&repo, "file2.txt", "content2");
  current_commit = git_test_repo_commit(&repo, "Add file2");

  fetcher = get_fetch_git_metadata(repo.path, 1, &logger);
  metadata = git_metadata_fetch(fetcher);

  ASSERT_STR_EQ(metadata->git.version, "2.1.2", "version");
  ASSERT_STR_EQ(metadata->git.commit.hash, current_commit, "commit hash");

  result = 0;

cleanup:
  free(current_commit);
  if (fetcher) {
    git_metadata_fetcher_free(fetcher);
  }
  git_test_repo_cleanup(&repo);
  return result;
}

static int test_multiple_tags_same_commit(void) {
  int result = 1;
  GitTestRepository repo = {0};
  GitMetadataFetcher *fetcher = NULL;
  Metadata *metadata = NULL;
  Logger logger = test_logger();

  if (git_test_repo_create(&repo) != 0) {
    goto cleanup;
  }

  git_test_repo_create_file(&repo, "README.md", "# Test Project");
  git_test_repo_commit(&repo, "Initial commit");

  git_test_repo_create_tag(&repo, "v1.0.0", NULL);
  git_test_repo_create_tag(&repo, "v1.2.0", NULL);
  git_test_repo_create_tag(&repo, "v1.1.5", NULL);

  fetcher = get_fetch_git_metadata(repo.path, 1, &logger);
  metadata = git_metadata_fetch(fetcher);

  ASSERT_STR_EQ(metadata->git.version, "1.2.0", "version");
  {
    const char *expected_tags[] = {"v1.0.0", "v1.1.5", "v1.2.0"};
    ASSERT_STR_ARRAY(&metadata->git.tags, expected_tags, 3, "tags");
  }

  result = 0;

cleanup:
  if (fetcher) {
    git_metadata_fetcher_free(fetcher);
  }
  git_test_repo_cleanup(&repo);
  return result;
}

static int test_mixed_tags_same_commit(void) {
  int result = 1;
  GitTestRepository repo = {0};
  GitMetadataFetcher *fetcher = NULL;
  Metadata *metadata = NULL;
  Logger logger = test_logger();

  if (git_test_repo_create(&repo) != 0) {
    goto cleanup;
  }

  git_test_repo_create_file(&repo, "README.md", "# Test Project");
  git_test_repo_commit(&repo, "Initial commit");

  git_test_repo_create_tag(&repo, "v1.0.0", NULL);
  git_test_repo_create_annotated_tag(&repo, "v1.2.0", "Annotated release", NULL);
  git_test_repo_create_tag(&repo, "v1.1.0", NULL);

  fetcher = get_fetch_git_metadata(repo.path, 1, &logger);
  metadata = git_metadata_fetch(fetcher);

  ASSERT_STR_EQ(metadata->git.version, "1.2.0", "version");
  {
    const char *expected_tags[] = {"v1.0.0", "v1.1.0", "v1.2.0"};
    ASSERT_STR_ARRAY(&metadata->git.tags, expected_tags, 3, "tags");
  }

  result = 0;

cleanup:
  if (fetcher) {
    git_metadata_fetcher_free(fetcher);
  }
  git_test_repo_cleanup(&repo);
  return result;
}

static int test_ignore_non_version_tags(void) {
  int result = 1;
  GitTestRepository repo = {0};
  GitMetadataFetcher *fetcher = NULL;
  Metadata *metadata = NULL;
  Logger logger = test_logger();

  if (git_test_repo_create(&repo) != 0) {
    goto cleanup;
  }

  git_test_repo_create_file(&repo, "README.md", "# Test Project");
  git_test_repo_commit(&repo, "Initial commit");

  git_test_repo_create_tag(&repo, "release", NULL);
  git_test_repo_create_tag(&repo, "stable", NULL);
  git_test_repo_create_tag(&repo, "v1.5.2", NULL);
  git_test_repo_create_tag(&repo, "feature-branch", NULL);

  fetcher = get_fetch_git_metadata(repo.path, 1, &logger);
  metadata = git_metadata_fetch(fetcher);

  ASSERT_STR_EQ(metadata->git.version, "1.5.2", "version");
  {
    const char *expected_tags[] = {"feature-branch", "release", "stable", "v1.5.2"};
    ASSERT_STR_ARRAY(&metadata->git.tags, expected_tags, 4, "tags");
  }

  result = 0;

cleanup:
  if (fetcher) {
    git_metadata_fetcher_free(fetcher);
  }
  git_test_repo_cleanup(&repo);
  return result;
}

static int test_merge_commits_with_tags(void) {
  int result = 1;
  GitTestRepository repo = {0};
  GitMetadataFetcher *fetcher = NULL;
  Metadata *metadata = NULL;
  Logger logger = test_logger();

  if (git_test_repo_create(&repo) != 0) {
    goto cleanup;
  }

  git_test_repo_create_file(&repo, "README.md", "# Test Project");
  git_test_repo_commit(&repo, "Initial commit");
  git_test_repo_create_tag(&repo, "v1.0.0", NULL);

  git_test_repo_create_branch(&repo, "feature", NULL);
  git_test_repo_create_file(&repo, "feature.txt", "feature content");
  git_test_repo_commit(&repo, "Add feature");
  git_test_repo_create_tag(&repo, "v0.9.0", NULL);

  git_test_repo_checkout(&repo, "main");
  git_test_repo_merge(&repo, "feature", "Merge feature branch");

  fetcher = get_fetch_git_metadata(repo.path, 1, &logger);
  metadata = git_metadata_fetch(fetcher);

  ASSERT_STR_EQ(metadata->git.version, "1.0.1", "version");

  result = 0;

cleanup:
  if (fetcher) {
    git_metadata_fetcher_free(fetcher);
  }
  git_test_repo_cleanup(&repo);
  return result;
}

static int test_branch_without_tags(void) {
  int result = 1;
  GitTestRepository repo = {0};
  GitMetadataFetcher *fetcher = NULL;
  Metadata *metadata = NULL;
  char *current_commit = NULL;
  Logger logger = test_logger();

  if (git_test_repo_create(&repo) != 0) {
    goto cleanup;
  }

  git_test_repo_create_file(&repo, "README.md", "# Test Project");
  git_test_repo_commit(&repo, "Initial commit");

  git_test_repo_create_file(&repo, "file.txt", "content");
  current_commit = git_test_repo_commit(&repo, "Add file");

  fetcher = get_fetch_git_metadata(repo.path, 1, &logger);
  metadata = git_metadata_fetch(fetcher);

  ASSERT_STR_EQ(metadata->git.version, "0.0.2", "version");
  ASSERT_STR_EQ(metadata->git.commit.hash, current_commit, "commit hash");

  result = 0;

cleanup:
  free(current_commit);
  if (fetcher) {
    git_metadata_fetcher_free(fetcher);
  }
  git_test_repo_cleanup(&repo);
  return result;
}

static int test_revision_version_increment(void) {
  int result = 1;
  GitTestRepository repo = {0};
  GitMetadataFetcher *fetcher = NULL;
  Metadata *metadata = NULL;
  Logger logger = test_logger();

  if (git_test_repo_create(&repo) != 0) {
    goto cleanup;
  }

  git_test_repo_create_file(&repo, "README.md", "# Test Project");
  git_test_repo_commit(&repo, "Initial commit");
  git_test_repo_create_tag(&repo, "v1.2.3.4", NULL);

  git_test_repo_create_file(&repo, "file.txt", "content");
  git_test_repo_commit(&repo, "Add file");

  fetcher = get_fetch_git_metadata(repo.path, 1, &logger);
  metadata = git_metadata_fetch(fetcher);

  ASSERT_STR_EQ(metadata->git.version, "1.2.3.5", "version");

  result = 0;

cleanup:
  if (fetcher) {
    git_metadata_fetcher_free(fetcher);
  }
  git_test_repo_cleanup(&repo);
  return result;
}

static int test_version_increment_priority(void) {
  int result = 0;
  size_t index;
  struct {
    const char *tag;
    const char *expected;
  } cases[] = {
    {"v1.2.0.0", "1.2.0.1"},
    {"v1.2.3", "1.2.4"},
    {"v1.2", "1.3"},
  };

  for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index++) {
    GitTestRepository repo = {0};
    GitMetadataFetcher *fetcher = NULL;
    Metadata *metadata = NULL;
    Logger logger = test_logger();

    if (git_test_repo_create(&repo) != 0) {
      result = 1;
      goto cleanup_loop;
    }

    git_test_repo_create_file(&repo, "README.md", "# Test Project");
    git_test_repo_commit(&repo, "Initial commit");
    git_test_repo_create_tag(&repo, cases[index].tag, NULL);

    git_test_repo_create_file(&repo, "file.txt", "content");
    git_test_repo_commit(&repo, "Add file");

    fetcher = get_fetch_git_metadata(repo.path, 1, &logger);
    metadata = git_metadata_fetch(fetcher);

    if (!metadata || !metadata->git.version ||
        strcmp(metadata->git.version, cases[index].expected) != 0) {
      fprintf(stderr,
              "FAIL %s: case %zu expected %s got %s\n",
              __func__,
              index,
              cases[index].expected,
              metadata && metadata->git.version ? metadata->git.version : "(null)");
      result = 1;
    }

cleanup_loop:
    if (fetcher) {
      git_metadata_fetcher_free(fetcher);
    }
    git_test_repo_cleanup(&repo);
    if (result != 0) {
      return result;
    }
  }

  return result;
}

static int test_merge_with_different_depth_tags(void) {
  int result = 1;
  GitTestRepository repo = {0};
  GitMetadataFetcher *fetcher = NULL;
  Metadata *metadata = NULL;
  Logger logger = test_logger();

  if (git_test_repo_create(&repo) != 0) {
    goto cleanup;
  }

  git_test_repo_create_file(&repo, "README.md", "# Test Project");
  git_test_repo_commit(&repo, "Initial commit");
  git_test_repo_create_tag(&repo, "v1.0.0", NULL);

  git_test_repo_create_file(&repo, "main1.txt", "main content 1");
  git_test_repo_commit(&repo, "Main commit 1");
  git_test_repo_create_file(&repo, "main2.txt", "main content 2");
  git_test_repo_commit(&repo, "Main commit 2");

  git_test_repo_checkout(&repo, "HEAD~1");
  git_test_repo_create_branch(&repo, "feature", NULL);
  git_test_repo_create_file(&repo, "feature1.txt", "feature content 1");
  git_test_repo_commit(&repo, "Feature commit 1");
  git_test_repo_create_tag(&repo, "v2.0.0", NULL);
  git_test_repo_create_file(&repo, "feature2.txt", "feature content 2");
  git_test_repo_commit(&repo, "Feature commit 2");

  git_test_repo_checkout(&repo, "main");
  git_test_repo_merge(&repo, "feature", "Merge feature branch");

  fetcher = get_fetch_git_metadata(repo.path, 1, &logger);
  metadata = git_metadata_fetch(fetcher);

  ASSERT_STR_EQ(metadata->git.version, "2.0.2", "version");

  result = 0;

cleanup:
  if (fetcher) {
    git_metadata_fetcher_free(fetcher);
  }
  git_test_repo_cleanup(&repo);
  return result;
}

static int test_merge_without_tags(void) {
  int result = 1;
  GitTestRepository repo = {0};
  GitMetadataFetcher *fetcher = NULL;
  Metadata *metadata = NULL;
  Logger logger = test_logger();

  if (git_test_repo_create(&repo) != 0) {
    goto cleanup;
  }

  git_test_repo_create_file(&repo, "README.md", "# Test Project");
  git_test_repo_commit(&repo, "Initial commit");

  git_test_repo_create_file(&repo, "main.txt", "main content");
  git_test_repo_commit(&repo, "Main commit");

  git_test_repo_create_branch(&repo, "feature", NULL);
  git_test_repo_create_file(&repo, "feature.txt", "feature content");
  git_test_repo_commit(&repo, "Feature commit");

  git_test_repo_checkout(&repo, "main");
  git_test_repo_merge(&repo, "feature", "Merge feature branch");

  fetcher = get_fetch_git_metadata(repo.path, 1, &logger);
  metadata = git_metadata_fetch(fetcher);

  ASSERT_STR_EQ(metadata->git.version, "0.0.4", "version");

  result = 0;

cleanup:
  if (fetcher) {
    git_metadata_fetcher_free(fetcher);
  }
  git_test_repo_cleanup(&repo);
  return result;
}

static int test_multiple_level_branch_merges(void) {
  int result = 1;
  GitTestRepository repo = {0};
  GitMetadataFetcher *fetcher = NULL;
  Metadata *metadata = NULL;
  Logger logger = test_logger();

  if (git_test_repo_create(&repo) != 0) {
    goto cleanup;
  }

  git_test_repo_create_file(&repo, "README.md", "# Test Project");
  git_test_repo_commit(&repo, "Initial commit");
  git_test_repo_create_tag(&repo, "v1.0.0", NULL);

  git_test_repo_create_file(&repo, "main1.txt", "main content 1");
  git_test_repo_commit(&repo, "Main commit 1");
  git_test_repo_create_file(&repo, "main2.txt", "main content 2");
  git_test_repo_commit(&repo, "Main commit 2");

  git_test_repo_create_branch(&repo, "dev", NULL);
  git_test_repo_create_tag(&repo, "v1.1.0", NULL);
  git_test_repo_create_file(&repo, "dev1.txt", "dev content 1");
  git_test_repo_commit(&repo, "Dev commit 1");
  git_test_repo_create_file(&repo, "dev2.txt", "dev content 2");
  git_test_repo_commit(&repo, "Dev commit 2");

  git_test_repo_create_branch(&repo, "feature", NULL);
  git_test_repo_create_file(&repo, "feature1.txt", "feature content 1");
  git_test_repo_commit(&repo, "Feature commit 1");
  git_test_repo_create_tag(&repo, "v2.0.0", NULL);
  git_test_repo_create_file(&repo, "feature2.txt", "feature content 2");
  git_test_repo_commit(&repo, "Feature commit 2");

  git_test_repo_checkout(&repo, "dev");
  git_test_repo_merge(&repo, "feature", "Merge feature to dev");

  git_test_repo_checkout(&repo, "main");
  git_test_repo_merge(&repo, "dev", "Merge dev to main");

  fetcher = get_fetch_git_metadata(repo.path, 1, &logger);
  metadata = git_metadata_fetch(fetcher);

  ASSERT_STR_EQ(metadata->git.version, "2.0.3", "version");

  result = 0;

cleanup:
  if (fetcher) {
    git_metadata_fetcher_free(fetcher);
  }
  git_test_repo_cleanup(&repo);
  return result;
}

static int test_same_version_tags_on_branches(void) {
  int result = 1;
  GitTestRepository repo = {0};
  GitMetadataFetcher *fetcher = NULL;
  Metadata *metadata = NULL;
  Logger logger = test_logger();

  if (git_test_repo_create(&repo) != 0) {
    goto cleanup;
  }

  git_test_repo_create_file(&repo, "README.md", "# Test Project");
  git_test_repo_commit(&repo, "Initial commit");

  git_test_repo_create_file(&repo, "main.txt", "main content");
  git_test_repo_commit(&repo, "Main commit");
  git_test_repo_create_tag(&repo, "v1.5.0", NULL);

  git_test_repo_create_branch(&repo, "feature", NULL);
  git_test_repo_create_file(&repo, "feature.txt", "feature content");
  git_test_repo_commit(&repo, "Feature commit");
  git_test_repo_create_tag(&repo, "v1.4.0", NULL);

  git_test_repo_checkout(&repo, "main");
  git_test_repo_merge(&repo, "feature", "Merge feature branch");

  fetcher = get_fetch_git_metadata(repo.path, 1, &logger);
  metadata = git_metadata_fetch(fetcher);

  ASSERT_STR_EQ(metadata->git.version, "1.5.1", "version");

  result = 0;

cleanup:
  if (fetcher) {
    git_metadata_fetcher_free(fetcher);
  }
  git_test_repo_cleanup(&repo);
  return result;
}

static int test_mixed_version_formats_across_branches(void) {
  int result = 1;
  GitTestRepository repo = {0};
  GitMetadataFetcher *fetcher = NULL;
  Metadata *metadata = NULL;
  Logger logger = test_logger();

  if (git_test_repo_create(&repo) != 0) {
    goto cleanup;
  }

  git_test_repo_create_file(&repo, "README.md", "# Test Project");
  git_test_repo_commit(&repo, "Initial commit");
  git_test_repo_create_tag(&repo, "v1.0", NULL);

  git_test_repo_create_branch(&repo, "feature", NULL);
  git_test_repo_create_file(&repo, "feature.txt", "feature content");
  git_test_repo_commit(&repo, "Feature commit");
  git_test_repo_create_tag(&repo, "v1.2.3.4", NULL);

  git_test_repo_checkout(&repo, "main");
  git_test_repo_merge(&repo, "feature", "Merge feature branch");

  fetcher = get_fetch_git_metadata(repo.path, 1, &logger);
  metadata = git_metadata_fetch(fetcher);

  ASSERT_STR_EQ(metadata->git.version, "1.2.3.5", "version");

  result = 0;

cleanup:
  if (fetcher) {
    git_metadata_fetcher_free(fetcher);
  }
  git_test_repo_cleanup(&repo);
  return result;
}

static int test_feature_branch_merged_after_main_progress(void) {
  int result = 1;
  GitTestRepository repo = {0};
  GitMetadataFetcher *fetcher = NULL;
  Metadata *metadata = NULL;
  Logger logger = test_logger();

  if (git_test_repo_create(&repo) != 0) {
    goto cleanup;
  }

  git_test_repo_create_file(&repo, "README.md", "# Test Project");
  git_test_repo_commit(&repo, "Initial commit");
  git_test_repo_create_tag(&repo, "v1.0.0", NULL);

  git_test_repo_create_branch(&repo, "feature", NULL);

  git_test_repo_checkout(&repo, "main");
  git_test_repo_create_file(&repo, "main1.txt", "main content 1");
  git_test_repo_commit(&repo, "Main commit 1");
  git_test_repo_create_tag(&repo, "v1.1.0", NULL);
  git_test_repo_create_file(&repo, "main2.txt", "main content 2");
  git_test_repo_commit(&repo, "Main commit 2");

  git_test_repo_checkout(&repo, "feature");
  git_test_repo_create_file(&repo, "feature1.txt", "feature content 1");
  git_test_repo_commit(&repo, "Feature commit 1");
  git_test_repo_create_tag(&repo, "v2.0.0", NULL);
  git_test_repo_create_file(&repo, "feature2.txt", "feature content 2");
  git_test_repo_commit(&repo, "Feature commit 2");

  git_test_repo_checkout(&repo, "main");
  git_test_repo_merge(&repo, "feature", "Merge feature branch");

  fetcher = get_fetch_git_metadata(repo.path, 1, &logger);
  metadata = git_metadata_fetch(fetcher);

  ASSERT_STR_EQ(metadata->git.version, "2.0.2", "version");

  result = 0;

cleanup:
  if (fetcher) {
    git_metadata_fetcher_free(fetcher);
  }
  git_test_repo_cleanup(&repo);
  return result;
}

static int test_branch_without_tags_merged_into_tagged(void) {
  int result = 1;
  GitTestRepository repo = {0};
  GitMetadataFetcher *fetcher = NULL;
  Metadata *metadata = NULL;
  Logger logger = test_logger();

  if (git_test_repo_create(&repo) != 0) {
    goto cleanup;
  }

  git_test_repo_create_file(&repo, "README.md", "# Test Project");
  git_test_repo_commit(&repo, "Initial commit");
  git_test_repo_create_tag(&repo, "v1.0.0", NULL);

  git_test_repo_create_file(&repo, "main.txt", "main content");
  git_test_repo_commit(&repo, "Main commit");
  git_test_repo_create_tag(&repo, "v1.1.0", NULL);

  git_test_repo_create_branch(&repo, "feature", NULL);
  git_test_repo_create_file(&repo, "feature1.txt", "feature content 1");
  git_test_repo_commit(&repo, "Feature commit 1");
  git_test_repo_create_file(&repo, "feature2.txt", "feature content 2");
  git_test_repo_commit(&repo, "Feature commit 2");

  git_test_repo_checkout(&repo, "main");
  git_test_repo_merge(&repo, "feature", "Merge feature branch");

  fetcher = get_fetch_git_metadata(repo.path, 1, &logger);
  metadata = git_metadata_fetch(fetcher);

  ASSERT_STR_EQ(metadata->git.version, "1.1.3", "version");

  result = 0;

cleanup:
  if (fetcher) {
    git_metadata_fetcher_free(fetcher);
  }
  git_test_repo_cleanup(&repo);
  return result;
}

static int test_deeply_nested_branch_structure(void) {
  int result = 1;
  GitTestRepository repo = {0};
  GitMetadataFetcher *fetcher = NULL;
  Metadata *metadata = NULL;
  Logger logger = test_logger();

  if (git_test_repo_create(&repo) != 0) {
    goto cleanup;
  }

  git_test_repo_create_file(&repo, "README.md", "# Test Project");
  git_test_repo_commit(&repo, "Initial commit");
  git_test_repo_create_tag(&repo, "v1.0.0", NULL);

  git_test_repo_create_branch(&repo, "dev1", NULL);
  git_test_repo_create_file(&repo, "dev1.txt", "dev1 content");
  git_test_repo_commit(&repo, "Dev1 commit");
  git_test_repo_create_tag(&repo, "v1.1.0", NULL);

  git_test_repo_create_branch(&repo, "dev2", NULL);
  git_test_repo_create_file(&repo, "dev2.txt", "dev2 content");
  git_test_repo_commit(&repo, "Dev2 commit");
  git_test_repo_create_tag(&repo, "v1.2.0", NULL);

  git_test_repo_create_branch(&repo, "feature", NULL);
  git_test_repo_create_file(&repo, "feature.txt", "feature content");
  git_test_repo_commit(&repo, "Feature commit");
  git_test_repo_create_tag(&repo, "v2.0.0", NULL);

  git_test_repo_checkout(&repo, "dev2");
  git_test_repo_merge(&repo, "feature", "Merge feature to dev2");

  git_test_repo_checkout(&repo, "dev1");
  git_test_repo_merge(&repo, "dev2", "Merge dev2 to dev1");

  git_test_repo_checkout(&repo, "main");
  git_test_repo_merge(&repo, "dev1", "Merge dev1 to main");

  fetcher = get_fetch_git_metadata(repo.path, 1, &logger);
  metadata = git_metadata_fetch(fetcher);

  ASSERT_STR_EQ(metadata->git.version, "2.0.3", "version");

  result = 0;

cleanup:
  if (fetcher) {
    git_metadata_fetcher_free(fetcher);
  }
  git_test_repo_cleanup(&repo);
  return result;
}

static int test_git_metadata_extraction(void) {
  int result = 1;
  GitTestRepository repo = {0};
  GitMetadataFetcher *fetcher = NULL;
  Metadata *metadata = NULL;
  char *commit_hash = NULL;
  Logger logger = test_logger();

  if (git_test_repo_create(&repo) != 0) {
    goto cleanup;
  }

  git_test_repo_create_file(&repo, "README.md", "# Test Project");
  commit_hash = git_test_repo_commit(&repo, "Initial commit");
  git_test_repo_create_tag(&repo, "v1.0.0", NULL);

  fetcher = get_fetch_git_metadata(repo.path, 1, &logger);
  metadata = git_metadata_fetch(fetcher);

  ASSERT_STR_EQ(metadata->git.version, "1.0.0", "version");

  {
    const char *expected_tags[] = {"v1.0.0"};
    ASSERT_STR_ARRAY(&metadata->git.tags, expected_tags, 1, "tags");
  }

  ASSERT_STR_EQ(metadata->git.commit.hash, commit_hash, "commit hash");
  if (commit_hash) {
    char short_hash[8];
    memcpy(short_hash, commit_hash, 7);
    short_hash[7] = '\0';
    ASSERT_STR_EQ(metadata->git.commit.short_hash, short_hash, "short hash");
  }
  ASSERT_TRUE(metadata->git.commit.date != NULL, "commit date");
  ASSERT_STR_EQ(metadata->git.commit.message, "Initial commit", "commit message");

  {
    const char *expected_branches[] = {"main"};
    ASSERT_STR_ARRAY(&metadata->git.branches, expected_branches, 1, "branches");
  }

  result = 0;

cleanup:
  free(commit_hash);
  if (fetcher) {
    git_metadata_fetcher_free(fetcher);
  }
  git_test_repo_cleanup(&repo);
  return result;
}

static int test_git_metadata_extraction_from_worktree(void) {
  int result = 1;
  GitTestRepository repo = {0};
  GitTestRepository worktree = {0};
  GitMetadataFetcher *fetcher = NULL;
  Metadata *metadata = NULL;
  char *commit_hash = NULL;
  Logger logger = test_logger();

  if (git_test_repo_create(&repo) != 0) {
    goto cleanup;
  }

  git_test_repo_create_file(&repo, "README.md", "# Test Project");
  commit_hash = git_test_repo_commit(&repo, "Initial commit");
  git_test_repo_create_tag(&repo, "v1.2.3", NULL);

  if (git_test_repo_create_worktree(&repo, &worktree, "HEAD") != 0) {
    goto cleanup;
  }

  fetcher = get_fetch_git_metadata(worktree.path, 1, &logger);
  metadata = git_metadata_fetch(fetcher);

  ASSERT_TRUE(metadata != NULL, "metadata");
  ASSERT_TRUE(metadata->has_git, "has git");
  ASSERT_STR_EQ(metadata->git.version, "1.2.3", "version");

  {
    const char *expected_tags[] = {"v1.2.3"};
    ASSERT_STR_ARRAY(&metadata->git.tags, expected_tags, 1, "tags");
  }

  ASSERT_STR_EQ(metadata->git.commit.hash, commit_hash, "commit hash");

  {
    const char *expected_branches[] = {"main"};
    ASSERT_STR_ARRAY(&metadata->git.branches, expected_branches, 1, "branches");
  }

  result = 0;

cleanup:
  free(commit_hash);
  if (fetcher) {
    git_metadata_fetcher_free(fetcher);
  }
  git_test_repo_cleanup(&worktree);
  git_test_repo_cleanup(&repo);
  return result;
}

static int test_detect_modified_files(int check_status) {
  int result = 1;
  GitTestRepository repo = {0};
  GitMetadataFetcher *fetcher = NULL;
  Metadata *metadata = NULL;
  Logger logger = test_logger();

  if (git_test_repo_create(&repo) != 0) {
    goto cleanup;
  }

  git_test_repo_create_file(&repo, "README.md", "# Test Project");
  git_test_repo_commit(&repo, "Initial commit");

  git_test_repo_create_file(&repo, "README.md", "# Modified Project");

  fetcher = get_fetch_git_metadata(repo.path, check_status, &logger);
  metadata = git_metadata_fetch(fetcher);

  if (check_status) {
    ASSERT_STR_EQ(metadata->git.version, "0.0.2", "version");
  } else {
    ASSERT_STR_EQ(metadata->git.version, "0.0.1", "version");
  }

  result = 0;

cleanup:
  if (fetcher) {
    git_metadata_fetcher_free(fetcher);
  }
  git_test_repo_cleanup(&repo);
  return result;
}

static int test_gitignore_ignored_files(void) {
  int result = 1;
  GitTestRepository repo = {0};
  GitMetadataFetcher *fetcher = NULL;
  Metadata *metadata = NULL;
  Logger logger = test_logger();

  if (git_test_repo_create(&repo) != 0) {
    goto cleanup;
  }

  git_test_repo_create_file(&repo, "README.md", "# Test Project");
  git_test_repo_commit(&repo, "Initial commit");
  git_test_repo_create_tag(&repo, "v1.0.0", NULL);

  git_test_repo_create_file(&repo, ".gitignore", "temp.txt\n*.log\nnode_modules/\n");
  git_test_repo_commit(&repo, "Add .gitignore");

  git_test_repo_create_file(&repo, "temp.txt", "temporary content");
  git_test_repo_create_file(&repo, "debug.log", "log content");

  fetcher = get_fetch_git_metadata(repo.path, 1, &logger);
  metadata = git_metadata_fetch(fetcher);

  ASSERT_STR_EQ(metadata->git.version, "1.0.1", "version");

  result = 0;

cleanup:
  if (fetcher) {
    git_metadata_fetcher_free(fetcher);
  }
  git_test_repo_cleanup(&repo);
  return result;
}

static int test_gitignore_all_ignored_files(void) {
  int result = 1;
  GitTestRepository repo = {0};
  GitMetadataFetcher *fetcher = NULL;
  Metadata *metadata = NULL;
  Logger logger = test_logger();

  if (git_test_repo_create(&repo) != 0) {
    goto cleanup;
  }

  git_test_repo_create_file(&repo, "README.md", "# Test Project");
  git_test_repo_commit(&repo, "Initial commit");
  git_test_repo_create_tag(&repo, "v1.0.0", NULL);

  git_test_repo_create_file(&repo, ".gitignore", "test.txt\n*.log\ntemp/\n");
  git_test_repo_commit(&repo, "Add .gitignore");

  git_test_repo_create_file(&repo, "debug.log", "log content");
  git_test_repo_create_file(&repo, "test.txt", "test content");

  fetcher = get_fetch_git_metadata(repo.path, 1, &logger);
  metadata = git_metadata_fetch(fetcher);

  ASSERT_STR_EQ(metadata->git.version, "1.0.1", "version");

  result = 0;

cleanup:
  if (fetcher) {
    git_metadata_fetcher_free(fetcher);
  }
  git_test_repo_cleanup(&repo);
  return result;
}

static int test_gitignore_non_ignored_files(void) {
  int result = 1;
  GitTestRepository repo = {0};
  GitMetadataFetcher *fetcher = NULL;
  Metadata *metadata = NULL;
  Logger logger = test_logger();

  if (git_test_repo_create(&repo) != 0) {
    goto cleanup;
  }

  git_test_repo_create_file(&repo, "README.md", "# Test Project");
  git_test_repo_commit(&repo, "Initial commit");
  git_test_repo_create_tag(&repo, "v1.0.0", NULL);

  git_test_repo_create_file(&repo, ".gitignore", "*.log\ntemp/\n");
  git_test_repo_commit(&repo, "Add .gitignore");

  git_test_repo_create_file(&repo, "debug.log", "log content");
  git_test_repo_create_file(&repo, "important.txt", "important content");

  fetcher = get_fetch_git_metadata(repo.path, 1, &logger);
  metadata = git_metadata_fetch(fetcher);

  ASSERT_STR_EQ(metadata->git.version, "1.0.2", "version");

  result = 0;

cleanup:
  if (fetcher) {
    git_metadata_fetcher_free(fetcher);
  }
  git_test_repo_cleanup(&repo);
  return result;
}

static int test_gitignore_subdirectory(void) {
  int result = 1;
  GitTestRepository repo = {0};
  GitMetadataFetcher *fetcher = NULL;
  Metadata *metadata = NULL;
  Logger logger = test_logger();

  if (git_test_repo_create(&repo) != 0) {
    goto cleanup;
  }

  git_test_repo_create_file(&repo, "README.md", "# Test Project");
  git_test_repo_commit(&repo, "Initial commit");
  git_test_repo_create_tag(&repo, "v1.0.0", NULL);

  git_test_repo_create_file(&repo, "src/.gitignore", "*.tmp\ndebug/\n");
  git_test_repo_commit(&repo, "Add src/.gitignore");

  git_test_repo_create_file(&repo, "src/temp.tmp", "temp content");
  git_test_repo_create_file(&repo, "src/code.js", "console.log(\"hello\");");

  fetcher = get_fetch_git_metadata(repo.path, 1, &logger);
  metadata = git_metadata_fetch(fetcher);

  ASSERT_STR_EQ(metadata->git.version, "1.0.2", "version");

  result = 0;

cleanup:
  if (fetcher) {
    git_metadata_fetcher_free(fetcher);
  }
  git_test_repo_cleanup(&repo);
  return result;
}

static int test_gitignore_complex_patterns(void) {
  int result = 1;
  GitTestRepository repo = {0};
  GitMetadataFetcher *fetcher = NULL;
  Metadata *metadata = NULL;
  Logger logger = test_logger();

  const char *gitignore_content =
    "# Logs\n"
    "*.log\n"
    "logs/\n\n"
    "# Dependencies\n"
    "node_modules/\n"
    "bower_components/\n\n"
    "# Build outputs\n"
    "dist/\n"
    "build/\n"
    "*.min.js\n\n"
    "# IDE files\n"
    ".vscode/\n"
    ".idea/\n"
    "*.swp\n\n"
    "# OS files\n"
    ".DS_Store\n"
    "Thumbs.db\n\n"
    "# Temporary files\n"
    "*.tmp\n"
    ".cache/\n\n"
    "# But include some exceptions\n"
    "!important.log\n"
    "!dist/index.html\n";

  if (git_test_repo_create(&repo) != 0) {
    goto cleanup;
  }

  git_test_repo_create_file(&repo, "README.md", "# Test Project");
  git_test_repo_commit(&repo, "Initial commit");
  git_test_repo_create_tag(&repo, "v1.0.0", NULL);

  git_test_repo_create_file(&repo, ".gitignore", gitignore_content);
  git_test_repo_commit(&repo, "Add comprehensive .gitignore");

  git_test_repo_create_file(&repo, "debug.log", "debug content");
  git_test_repo_create_file(&repo, "important.log", "important content");
  git_test_repo_create_file(&repo, "app.min.js", "minified js");
  git_test_repo_create_file(&repo, "regular.js", "regular js");
  git_test_repo_create_file(&repo, "temp.tmp", "temp");

  fetcher = get_fetch_git_metadata(repo.path, 1, &logger);
  metadata = git_metadata_fetch(fetcher);

  ASSERT_STR_EQ(metadata->git.version, "1.0.2", "version");

  result = 0;

cleanup:
  if (fetcher) {
    git_metadata_fetcher_free(fetcher);
  }
  git_test_repo_cleanup(&repo);
  return result;
}

static int test_gitignore_nested_rules(void) {
  int result = 1;
  GitTestRepository repo = {0};
  GitMetadataFetcher *fetcher = NULL;
  Metadata *metadata = NULL;
  Logger logger = test_logger();

  if (git_test_repo_create(&repo) != 0) {
    goto cleanup;
  }

  git_test_repo_create_file(&repo, "README.md", "# Test Project");
  git_test_repo_commit(&repo, "Initial commit");
  git_test_repo_create_tag(&repo, "v1.0.0", NULL);

  git_test_repo_create_file(&repo, ".gitignore", "*.log\ntemp/\n");
  git_test_repo_create_file(&repo, "src/.gitignore", "*.tmp\n!important.tmp\n");
  git_test_repo_create_file(&repo, "src/utils/.gitignore", "*.cache\n");
  git_test_repo_commit(&repo, "Add nested .gitignore files");

  git_test_repo_create_file(&repo, "app.log", "app logs");
  git_test_repo_create_file(&repo, "config.txt", "config");
  git_test_repo_create_file(&repo, "src/temp.tmp", "temp");
  git_test_repo_create_file(&repo, "src/important.tmp", "important");
  git_test_repo_create_file(&repo, "src/code.js", "code");
  git_test_repo_create_file(&repo, "src/utils/data.cache", "cache");
  git_test_repo_create_file(&repo, "src/utils/helper.js", "helper");

  fetcher = get_fetch_git_metadata(repo.path, 1, &logger);
  metadata = git_metadata_fetch(fetcher);

  ASSERT_STR_EQ(metadata->git.version, "1.0.2", "version");

  result = 0;

cleanup:
  if (fetcher) {
    git_metadata_fetcher_free(fetcher);
  }
  git_test_repo_cleanup(&repo);
  return result;
}

static int test_gitignore_with_status_disabled(void) {
  int result = 1;
  GitTestRepository repo = {0};
  GitMetadataFetcher *fetcher = NULL;
  Metadata *metadata = NULL;
  Logger logger = test_logger();

  if (git_test_repo_create(&repo) != 0) {
    goto cleanup;
  }

  git_test_repo_create_file(&repo, "README.md", "# Test Project");
  git_test_repo_commit(&repo, "Initial commit");
  git_test_repo_create_tag(&repo, "v1.0.0", NULL);

  git_test_repo_create_file(&repo, ".gitignore", "*.tmp\n");
  git_test_repo_commit(&repo, "Add .gitignore");

  git_test_repo_create_file(&repo, "temp.tmp", "temp content");
  git_test_repo_create_file(&repo, "important.txt", "important content");

  fetcher = get_fetch_git_metadata(repo.path, 0, &logger);
  metadata = git_metadata_fetch(fetcher);

  ASSERT_STR_EQ(metadata->git.version, "1.0.1", "version");

  result = 0;

cleanup:
  if (fetcher) {
    git_metadata_fetcher_free(fetcher);
  }
  git_test_repo_cleanup(&repo);
  return result;
}

static int test_detect_modified_files_enabled(void) {
  return test_detect_modified_files(1);
}

static int test_detect_modified_files_disabled(void) {
  return test_detect_modified_files(0);
}

typedef int (*TestFn)(void);

typedef struct {
  const char *name;
  TestFn fn;
} TestCase;

int main(void) {
  TestCase tests[] = {
    {"tag on current commit", test_tag_on_current_commit},
    {"annotated tag on current commit", test_annotated_tag_on_current_commit},
    {"tag in parent commit", test_tag_in_parent_commit},
    {"annotated tag in parent commit", test_annotated_tag_in_parent_commit},
    {"tag in grandparent commit", test_tag_in_grandparent_commit},
    {"multiple tags same commit", test_multiple_tags_same_commit},
    {"mixed tags same commit", test_mixed_tags_same_commit},
    {"ignore non-version tags", test_ignore_non_version_tags},
    {"merge commits with tags", test_merge_commits_with_tags},
    {"branch without tags", test_branch_without_tags},
    {"revision version increment", test_revision_version_increment},
    {"version increment priority", test_version_increment_priority},
    {"merge with different depth tags", test_merge_with_different_depth_tags},
    {"merge without tags", test_merge_without_tags},
    {"multiple level branch merges", test_multiple_level_branch_merges},
    {"same version tags on branches", test_same_version_tags_on_branches},
    {"mixed version formats", test_mixed_version_formats_across_branches},
    {"feature branch after main progress", test_feature_branch_merged_after_main_progress},
    {"branch without tags merged", test_branch_without_tags_merged_into_tagged},
    {"deeply nested branch structure", test_deeply_nested_branch_structure},
    {"git metadata extraction", test_git_metadata_extraction},
    {"git metadata extraction from worktree", test_git_metadata_extraction_from_worktree},
    {"detect modified files enabled", test_detect_modified_files_enabled},
    {"detect modified files disabled", test_detect_modified_files_disabled},
    {"gitignore ignored files", test_gitignore_ignored_files},
    {"gitignore all ignored files", test_gitignore_all_ignored_files},
    {"gitignore non-ignored files", test_gitignore_non_ignored_files},
    {"gitignore subdirectory", test_gitignore_subdirectory},
    {"gitignore complex patterns", test_gitignore_complex_patterns},
    {"gitignore nested rules", test_gitignore_nested_rules},
    {"gitignore with status disabled", test_gitignore_with_status_disabled},
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
