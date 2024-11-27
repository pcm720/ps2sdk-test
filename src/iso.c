#include "iso.h"
#include "common.h"
#include "devices.h"
#include "iso_cache.h"
#include "iso_title_id.h"
#include <errno.h>
#include <fcntl.h>
#include <ps2sdkapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int _findISO(DIR *directory, TargetList *result);
void insertIntoList(TargetList *result, Target *title);
void processTitleID(TargetList *result);

// Directories to skip when browsing for ISOs
const char *ignoredDirs[] = {
    "nhddl", "APPS", "ART", "CFG", "CHT", "LNG", "THM", "VMC", "XEBPLUS",
};

// Generates a list of launch candidates found on BDM devices
// Returns NULL if no targets were found or an error occurs
TargetList *findISO() {
  DIR *directory;
  TargetList *result = malloc(sizeof(TargetList));
  char mountpoint[] = MASS_PLACEHOLDER;
  result->total = 0;
  result->first = NULL;
  result->last = NULL;

  for (int i = 0; i < MAX_MASS_DEVICES; i++) {
    if (deviceModeMap[i].mode == MODE_ALL) {
      break;
    }
    mountpoint[4] = i + '0';

    directory = opendir(mountpoint);
    // Check if the directory can be opened
    if (directory == NULL) {
      logString("ERROR: Can't open %s\n", mountpoint);
      return NULL;
    }

    chdir(mountpoint);
    if (_findISO(directory, result)) {
      free(result);
      closedir(directory);
      return NULL;
    }
    closedir(directory);
  }
  processTitleID(result);

  // Set indexes for each title
  int idx = 0;
  Target *curTitle = result->first;
  while (curTitle != NULL) {
    curTitle->idx = idx;
    idx++;
    curTitle = curTitle->next;
  }

  return result;
}

// Searches rootpath and adds discovered ISOs to TargetList
int _findISO(DIR *directory, TargetList *result) {
  if (directory == NULL)
    return -ENOENT;

  // Read directory entries
  struct dirent *entry;
  char *fileext;
  char titlePath[PATH_MAX + 1];
  if (!getcwd(titlePath, PATH_MAX + 1)) { // Initialize titlePath with current working directory
    logString("ERROR: Failed to get cwd\n");
    return -ENOENT;
  }

  int cwdLen = strlen(titlePath); // Get the length of base path string
  while ((entry = readdir(directory)) != NULL) {
    // Check if the entry is a directory using d_type
    switch (entry->d_type) {
    case DT_DIR:
      // Ignore hidden, special and invalid directories (non-ASCII paths seem to return '?' and cause crashes when used with opendir)
      if ((entry->d_name[0] == '.') || (entry->d_name[0] == '$') || (entry->d_name[0] == '?'))
        continue;

      for (int i = 0; i < sizeof(ignoredDirs) / sizeof(char *); i++) {
        if (!strcmp(ignoredDirs[i], entry->d_name)) {
          goto skipDirectory;
        }
      }

      // Open dir and change cwd
      DIR *d = opendir(entry->d_name);
      chdir(entry->d_name);
      // Process inner directory recursively
      _findISO(d, result);
      // Return back to root directory
      chdir("..");
      closedir(d);
    skipDirectory:
      continue;
    default:
      if (entry->d_name[0] == '.') // Ignore .files (most likely macOS doubles)
        continue;

      // Make sure file has .iso extension
      fileext = strrchr(entry->d_name, '.');
      if ((fileext != NULL) && (!strcmp(fileext, ".iso") || !strcmp(fileext, ".ISO"))) {
        // Generate full path
        if (titlePath[cwdLen - 1] != '/')
          strcat(titlePath, "/");
        strcat(titlePath, entry->d_name);

        // Initialize target
        Target *title = calloc(sizeof(Target), 1);
        title->prev = NULL;
        title->next = NULL;
        title->fullPath = strdup(titlePath);
        title->deviceType = deviceModeMap[titlePath[4] - '0'].mode;

        // Get file name without the extension
        int nameLength = (int)(fileext - entry->d_name);
        title->name = calloc(sizeof(char), nameLength + 1);
        strncpy(title->name, entry->d_name, nameLength);

        // Increment title counter and update target list
        result->total++;
        if (result->first == NULL) {
          // If this is the first entry, update both pointers
          result->first = title;
          result->last = title;
        } else {
          insertIntoList(result, title);
        }
        titlePath[cwdLen] = '\0'; // reset titlePath by ending string on base path
      }
    }
  }

  return 0;
}

// Converts lowercase ASCII string into uppercase
void toUppercase(char *str) {
  for (int i = 0; i <= strlen(str); i++)
    if (str[i] >= 0x61 && str[i] <= 0x7A) {
      str[i] -= 32;
    }
}

// Inserts title in the list while keeping the alphabetical order
void inline insertIntoList(TargetList *result, Target *title) {
  // Traverse the list in reverse
  Target *curTitle = result->last;

  // Covert title name to uppercase
  char *curUppercase = strdup(title->name);
  toUppercase(curUppercase);

  // Overall, title name should not exceed PATH_MAX
  char lastUppercase[PATH_MAX];

  while (1) {
    // Reset string buffer
    lastUppercase[0] = '\0';
    // Convert name of the last title to uppercase
    strlcpy(lastUppercase, curTitle->name, PATH_MAX);
    toUppercase(lastUppercase);

    // Compare new title name and the current title name
    if (strcmp(curUppercase, lastUppercase) >= 0) {
      // First letter of the new title is after or the same as the current one
      // New title must be inserted after the current list element
      if (curTitle->next != NULL) {
        // Current title has a next title, update the next element
        curTitle->next->prev = title;
        title->next = curTitle->next;
      } else {
        // Current title has no next title (it's the last list element)
        result->last = title;
      }
      title->prev = curTitle;
      curTitle->next = title;
      break;
    }

    if (curTitle->prev == NULL) {
      // Current title is the first in this list
      // New title must be inserted at the beginning
      curTitle->prev = title;
      title->next = curTitle;
      result->first = title;
      break;
    }

    // Keep traversing the list
    curTitle = curTitle->prev;
  }
  free(curUppercase);
}

// Fills in title ID for every entry in the list
void processTitleID(TargetList *result) {
  if (result->total == 0)
    return;

  // Load title cache
  TitleIDCache *cache = malloc(sizeof(TitleIDCache));
  int isCacheUpdateNeeded = 0;
  if (loadTitleIDCache(cache)) {
    logString("Failed to load title ID cache, all ISOs will be rescanned\n");
    free(cache);
    cache = NULL;
  } else if (cache->total != result->total) {
    // Set flag if number of entries is different
    isCacheUpdateNeeded = 1;
  }

  // For every entry in target list, try to get title ID from cache
  // If cache doesn't have title ID for the path,
  // get it from ISO
  int cacheMisses = 0;
  char *titleID = NULL;
  Target *curTarget = result->first;
  while (curTarget != NULL) {
    // Try to get title ID from cache
    if (cache != NULL) {
      titleID = getCachedTitleID(curTarget->fullPath, cache);
    }

    if (titleID != NULL) {
      curTarget->id = strdup(titleID);
    } else { // Get title ID from ISO
      cacheMisses++;
      printf("Cache miss for %s\n", curTarget->fullPath);
      curTarget->id = getTitleID(curTarget->fullPath);
      if (curTarget->id == NULL) {
        printf("WARN: Removing '%s' from target list\n", curTarget->name);
        curTarget = freeTarget(curTarget);
        result->total -= 1;
      }
    }

    curTarget = curTarget->next;
  }
  freeTitleCache(cache);

  if ((cacheMisses > 0) || (isCacheUpdateNeeded)) {
    logString("Updating title ID cache\n");
    if (storeTitleIDCache(result)) {
      logString("Failed to save title ID cache\n");
    }
  }
}

// Completely frees Target and returns pointer to a previous argument in the list
Target *freeTarget(Target *target) {
  Target *prev = NULL;
  free(target->fullPath);
  free(target->name);
  free(target->id);
  if (target->prev != NULL) {
    prev = target->prev;
    if (target->next != NULL) {
      prev->next = target->next;
      target->next->prev = prev;
    }
  }
  free(target);
  return prev;
}

// Completely frees TargetList. Passed pointer will not be valid after this function executes
void freeTargetList(TargetList *result) {
  Target *target = result->last;
  while (target != NULL) {
    target = freeTarget(target);
  }
  result->first = NULL;
  result->last = NULL;
  result->total = 0;
  free(result);
}

// Finds target with given index in the list and returns a pointer to it
Target *getTargetByIdx(TargetList *targets, int idx) {
  Target *current = targets->first;
  while (1) {
    if (current->idx == idx) {
      return current;
    }

    if (current->next == NULL)
      break;

    current = current->next;
  }
  return NULL;
}

// Makes and returns a deep copy of src without prev/next pointers.
Target *copyTarget(Target *src) {
  Target *copy = calloc(sizeof(Target), 1);
  copy->idx = src->idx;

  copy->fullPath = strdup(src->fullPath);
  copy->name = strdup(src->name);
  copy->id = strdup(src->id);
  copy->deviceType = src->deviceType;

  return copy;
}
