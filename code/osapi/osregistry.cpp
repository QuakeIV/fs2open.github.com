/*
* Copyright (C) Volition, Inc. 1999.  All rights reserved.
*
* All source code herein is the property of Volition, Inc. You may not sell
* or otherwise commercially exploit the source or things you created based on the
* source.
*
*/

#include "globalincs/pstypes.h"
#include "osapi/osregistry.h"
#include "osapi/osapi.h"
#include "cmdline/cmdline.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef WIN32
#include <windows.h>
// Stupid Microsoft is not able to fix a simple compile warning: https://connect.microsoft.com/VisualStudio/feedback/details/1342304/level-1-compiler-warnings-in-windows-sdk-shipped-with-visual-studio
#pragma warning(push)
#pragma warning(disable: 4091) // ignored on left of '' when no variable is declared
#include <shlobj.h>
#pragma warning(pop)
#include <sddl.h>
#endif

#define DEFAULT_SECTION "Default"

typedef struct KeyValue
{
  char *key;
  char *value;

  struct KeyValue *next;
} KeyValue;

typedef struct Section
{
  char *name;

  struct KeyValue *pairs;
  struct Section *next;
} Section;

typedef struct Profile
{
  struct Section *sections;
} Profile;

static Profile global_config_profile = {0};

// For string config functions
static char tmp_string_data[1024];

// This code is needed for compatibility with the old windows registry

static char *read_line_from_file(FILE *fp)
{
  char *buf, *buf_start;
  int buflen, eol;

  buflen = 80;
  buf = (char *)vm_malloc(buflen);
  buf_start = buf;
  eol = 0;

  do {
    if (buf == NULL) {
      return NULL;
    }

    if (fgets(buf_start, 80, fp) == NULL) {
      if (buf_start == buf) {
        vm_free(buf);
        return NULL;
      }
      else {
        *buf_start = 0;
        return buf;
      }
    }

    auto len = strlen(buf_start);

    if (buf_start[len - 1] == '\n') {
      buf_start[len - 1] = 0;
      eol = 1;
    }
    else {
      buflen += 80;

      buf = (char *)vm_realloc(buf, buflen);

      /* be sure to skip over the proper amount of nulls */
      buf_start = buf + (buflen - 80) - (buflen / 80) + 1;
    }
  } while (!eol);

  return buf;
}

static char *trim_string(char *str)
{
  char *ptr;

  if (str == NULL)
    return NULL;

  /* kill any comment */
  ptr = strchr(str, ';');
  if (ptr)
    *ptr = 0;
  ptr = strchr(str, '#');
  if (ptr)
    *ptr = 0;

  ptr = str;
  auto len = strlen(str);
  if (len > 0) {
    ptr += len - 1;
  }

  while ((ptr > str) && isspace(*ptr)) {
    ptr--;
  }

  if (*ptr) {
    ptr++;
    *ptr = 0;
  }

  ptr = str;
  while (*ptr && isspace(*ptr)) {
    ptr++;
  }

  return ptr;
}

static void profile_update(const char *section, const char *key, const char *value)
{
  KeyValue *kvp;

  Section **sp_ptr = &(global_config_profile.sections);
  Section *sp = global_config_profile.sections;

  while (sp != NULL) {
    if (strcmp(section, sp->name) == 0) {
      KeyValue **kvp_ptr = &(sp->pairs);
      kvp = sp->pairs;

      while (kvp != NULL) {
        if (strcmp(key, kvp->key) == 0) {
          vm_free(kvp->value);

          if (value == NULL) {
            *kvp_ptr = kvp->next;

            vm_free(kvp->key);
            vm_free(kvp);
          }
          else {
            kvp->value = vm_strdup(value);
          }

          /* all done */
          return;
        }

        kvp_ptr = &(kvp->next);
        kvp = kvp->next;
      }

      if (value != NULL) {
        /* key not found */
        kvp = (KeyValue *)vm_malloc(sizeof(KeyValue));
        kvp->next = NULL;
        kvp->key = vm_strdup(key);
        kvp->value = vm_strdup(value);
      }

      *kvp_ptr = kvp;

      /* all done */
      return;
    }

    sp_ptr = &(sp->next);
    sp = sp->next;
  }

  /* section not found */
  sp = (Section *)vm_malloc(sizeof(Section));
  sp->next = NULL;
  sp->name = vm_strdup(section);

  kvp = (KeyValue *)vm_malloc(sizeof(KeyValue));
  kvp->next = NULL;
  kvp->key = vm_strdup(key);
  kvp->value = vm_strdup(value);

  sp->pairs = kvp;

  *sp_ptr = sp;

  return;
}

static char *profile_get_value(const char *section, const char *key)
{
  Section *sp = global_config_profile.sections;

  while (sp != NULL) {
    if (stricmp(section, sp->name) == 0) {
      KeyValue *kvp = sp->pairs;

      while (kvp != NULL) {
        if (strcmp(key, kvp->key) == 0) {
          return kvp->value;
        }
        kvp = kvp->next;
      }
    }

    sp = sp->next;
  }

  /* not found */
  return NULL;
}

const char *os_config_read_string(const char *section, const char *name, const char *default_value)
{
  nprintf(("Registry", "os_config_read_string(): section = \"%s\", name = \"%s\", default value: \"%s\"\n",
    (section) ? section : DEFAULT_SECTION, name, (default_value) ? default_value : NOX("NULL")));

  if (section == NULL)
    section = DEFAULT_SECTION;

  char *ptr = profile_get_value(section, name);

  if (ptr != NULL) {
    strncpy(tmp_string_data, ptr, 1023);
    default_value = tmp_string_data;
  }

  return default_value;
}

unsigned int os_config_read_uint(const char *section, const char *name, unsigned int default_value)
{
  if (section == NULL)
    section = DEFAULT_SECTION;

  char *ptr = profile_get_value(section, name);

  if (ptr != NULL) {
    default_value = atoi(ptr);
  }

  return default_value;
}

void os_config_write_string(const char *section, const char *name, const char *value)
{
  if (section == NULL)
    section = DEFAULT_SECTION;

  profile_update(section, name, value);
}

void os_config_write_uint(const char *section, const char *name, unsigned int value)
{
  if (section == NULL)
    section = DEFAULT_SECTION;

  char buf[21];

  snprintf(buf, 20, "%u", value);

  profile_update(section, name, buf);
}

