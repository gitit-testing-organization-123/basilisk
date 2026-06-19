#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "paths.h"

#include <dlfcn.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef BASILISK
#define BASILISK ""
#endif

#ifndef LIBDIR
#define LIBDIR ""
#endif

static char * executable_path = NULL;
static char * basilisk_root = NULL;
static char * libdir = NULL;
static char * ast_std_dir = NULL;

static char * xstrdup (const char * s)
{
  if (!s)
    return NULL;
  char * p = malloc (strlen (s) + 1);
  if (p)
    strcpy (p, s);
  return p;
}

char * qcc_path_join (const char * a, const char * b)
{
  if (!a || !*a)
    return xstrdup (b);
  if (!b || !*b)
    return xstrdup (a);

  size_t na = strlen (a), nb = strlen (b);
  char * out = malloc (na + 1 + nb + 1);
  if (!out)
    return NULL;

  strcpy (out, a);
  if (out[na - 1] != '/')
    strcat (out, "/");
  strcat (out, b);
  return out;
}

static char * dirname_dup (const char * path)
{
  char * out = xstrdup (path);
  if (!out)
    return NULL;

  char * slash = strrchr (out, '/');
  if (slash && slash != out)
    *slash = '\0';
  else if (slash)
    slash[1] = '\0';
  else
    strcpy (out, ".");
  return out;
}

static bool exists (const char * path)
{
  return path && access (path, F_OK) == 0;
}

static char * realpath_malloc (const char * path)
{
  char resolved[PATH_MAX];
  if (!path || !realpath (path, resolved))
    return NULL;
  return xstrdup (resolved);
}

static char * canonical_dir_if_exists (const char * path)
{
  return exists (path) ? realpath_malloc (path) : NULL;
}

static bool is_basilisk_root (const char * root)
{
  char * defaults = qcc_path_join (root, "ast/defaults.h");
  char * init = qcc_path_join (root, "ast/init_solver.h");
  char * common = qcc_path_join (root, "common.h");

  bool ok = exists (defaults) && exists (init) && exists (common);

  free (defaults);
  free (init);
  free (common);
  return ok;
}

static char * canonical_basilisk_root (const char * candidate)
{
  if (!candidate || !is_basilisk_root (candidate))
    return NULL;
  return realpath_malloc (candidate);
}

static char * executable_from_proc (void)
{
#ifdef __linux__
  char buf[PATH_MAX];
  ssize_t n = readlink ("/proc/self/exe", buf, sizeof buf - 1);
  if (n > 0) {
    buf[n] = '\0';
    return realpath_malloc (buf);
  }
#endif
  return NULL;
}

static char * executable_from_dladdr (void)
{
  Dl_info info;
  if (dladdr ((void *) &qcc_paths_init, &info) && info.dli_fname)
    return realpath_malloc (info.dli_fname);
  return NULL;
}

static char * executable_from_path (const char * argv0)
{
  const char * path_env = getenv ("PATH");
  if (!path_env)
    return NULL;

  char * path = xstrdup (path_env);
  if (!path)
    return NULL;

  char * saveptr = NULL;
  for (char * dir = strtok_r (path, ":", &saveptr);
       dir;
       dir = strtok_r (NULL, ":", &saveptr)) {
    char * candidate = qcc_path_join (*dir ? dir : ".", argv0);
    char * resolved = NULL;
    if (candidate && access (candidate, X_OK) == 0)
      resolved = realpath_malloc (candidate);
    free (candidate);
    if (resolved) {
      free (path);
      return resolved;
    }
  }

  free (path);
  return NULL;
}

static char * executable_from_argv0 (const char * argv0)
{
  if (!argv0 || !*argv0)
    return NULL;
  if (strchr (argv0, '/'))
    return realpath_malloc (argv0);
  return executable_from_path (argv0);
}

static char * find_executable (const char * argv0)
{
  char * exe = executable_from_proc ();
  if (exe)
    return exe;

  exe = executable_from_dladdr ();
  if (exe)
    return exe;

  return executable_from_argv0 (argv0);
}

static char * find_basilisk_near_executable (const char * exe)
{
  char * selfdir = dirname_dup (exe);
  if (!selfdir)
    return NULL;

  const char * suffixes[] = {
    ".",                       /* in-tree binary in src/ */
    "../../../src",            /* build/<config>/src/qcc */
    "../include/basilisk",     /* installed: prefix/bin/qcc */
    "../../include/basilisk",
    "../../usr/include/basilisk",
    NULL
  };

  for (int i = 0; suffixes[i]; i++) {
    char * candidate = qcc_path_join (selfdir, suffixes[i]);
    char * found = canonical_basilisk_root (candidate);
    free (candidate);
    if (found) {
      free (selfdir);
      return found;
    }
  }

  free (selfdir);
  return NULL;
}

static char * find_basilisk_root (const char * exe)
{
  const char * env = getenv ("BASILISK");
  char * root = canonical_basilisk_root (env);
  if (root)
    return root;

  root = find_basilisk_near_executable (exe);
  if (root)
    return root;

  return canonical_basilisk_root (BASILISK);
}

static char * find_libdir (void)
{
  const char * env = getenv ("BASILISK_LIBDIR");
  char * dir = canonical_dir_if_exists (env);
  if (dir)
    return dir;

  if (basilisk_root)
    return xstrdup (basilisk_root);

  dir = canonical_dir_if_exists (LIBDIR);
  if (dir)
    return dir;

  return xstrdup (LIBDIR);
}

void qcc_paths_init (const char * argv0)
{
  if (basilisk_root && libdir && ast_std_dir)
    return;

  if (!executable_path)
    executable_path = find_executable (argv0);

  if (!basilisk_root)
    basilisk_root = find_basilisk_root (executable_path);

  if (!libdir)
    libdir = find_libdir ();

  if (!ast_std_dir)
    ast_std_dir = qcc_path_join (basilisk_root, "ast/std");
}

const char * qcc_executable_path (void)
{
  qcc_paths_init (NULL);
  return executable_path;
}

const char * qcc_basilisk_root (void)
{
  qcc_paths_init (NULL);
  return basilisk_root;
}

const char * qcc_libdir (void)
{
  qcc_paths_init (NULL);
  return libdir;
}

const char * qcc_ast_std_dir (void)
{
  qcc_paths_init (NULL);
  return ast_std_dir;
}
