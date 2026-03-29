/*
 * aria_libc_fs.c — Filesystem extras for aria-libc
 *
 * Covers: access, chmod, symlink, readlink, hard links, truncate,
 *         realpath, mkdtemp, mkstemp, fnmatch, glob (handle-based).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <glob.h>
#include <fnmatch.h>
#include <limits.h>

/* ── AriaString helpers ─────────────────────────────────────────── */

typedef struct { char *data; int64_t length; } AriaString;

static AriaString make_string(const char *s) {
    if (!s || !*s) return (AriaString){ "", 0 };
    int64_t len = (int64_t)strlen(s);
    char *buf = malloc((size_t)len + 1);
    if (!buf) return (AriaString){ "", 0 };
    memcpy(buf, s, (size_t)len + 1);
    return (AriaString){ buf, len };
}

/* ── Error state ────────────────────────────────────────────────── */

static int64_t last_errno = 0;

int64_t aria_libc_fs_errno(void) { return last_errno; }

AriaString aria_libc_fs_strerror(int64_t e) {
    return make_string(strerror((int)e));
}

/* ── Access-check constants ─────────────────────────────────────── */

int64_t aria_libc_fs_F_OK(void) { return (int64_t)F_OK; }
int64_t aria_libc_fs_R_OK(void) { return (int64_t)R_OK; }
int64_t aria_libc_fs_W_OK(void) { return (int64_t)W_OK; }
int64_t aria_libc_fs_X_OK(void) { return (int64_t)X_OK; }

/* ── fnmatch flag constants ─────────────────────────────────────── */

int64_t aria_libc_fs_FNM_PATHNAME(void) { return (int64_t)FNM_PATHNAME; }
int64_t aria_libc_fs_FNM_PERIOD(void)   { return (int64_t)FNM_PERIOD; }
int64_t aria_libc_fs_FNM_NOESCAPE(void) { return (int64_t)FNM_NOESCAPE; }

/* ── Permission-bit constants ───────────────────────────────────── */

int64_t aria_libc_fs_S_IRWXU(void) { return (int64_t)S_IRWXU; }
int64_t aria_libc_fs_S_IRUSR(void) { return (int64_t)S_IRUSR; }
int64_t aria_libc_fs_S_IWUSR(void) { return (int64_t)S_IWUSR; }
int64_t aria_libc_fs_S_IXUSR(void) { return (int64_t)S_IXUSR; }
int64_t aria_libc_fs_S_IRWXG(void) { return (int64_t)S_IRWXG; }
int64_t aria_libc_fs_S_IRWXO(void) { return (int64_t)S_IRWXO; }

/* ── Filesystem operations ──────────────────────────────────────── */

/* access(path, mode) → 0 on success, -1 on failure */
int64_t aria_libc_fs_access(const char *path, int64_t mode) {
    int rc = access(path, (int)mode);
    if (rc < 0) last_errno = errno;
    return (int64_t)rc;
}

/* chmod(path, mode) → 0 on success, -1 on failure */
int64_t aria_libc_fs_chmod(const char *path, int64_t mode) {
    int rc = chmod(path, (mode_t)mode);
    if (rc < 0) last_errno = errno;
    return (int64_t)rc;
}

/* symlink(target, linkpath) → 0 on success, -1 on failure */
int64_t aria_libc_fs_symlink(const char *target, const char *linkpath) {
    int rc = symlink(target, linkpath);
    if (rc < 0) last_errno = errno;
    return (int64_t)rc;
}

/* readlink(path) → target string (empty on error) */
AriaString aria_libc_fs_readlink(const char *path) {
    char buf[PATH_MAX];
    ssize_t len = readlink(path, buf, sizeof(buf) - 1);
    if (len < 0) {
        last_errno = errno;
        return (AriaString){ "", 0 };
    }
    buf[len] = '\0';
    return make_string(buf);
}

/* hardlink(target, linkpath) → 0 on success, -1 on failure */
int64_t aria_libc_fs_hardlink(const char *target, const char *linkpath) {
    int rc = link(target, linkpath);
    if (rc < 0) last_errno = errno;
    return (int64_t)rc;
}

/* truncate_file(path, length) → 0 on success, -1 on failure */
int64_t aria_libc_fs_truncate_file(const char *path, int64_t length) {
    int rc = truncate(path, (off_t)length);
    if (rc < 0) last_errno = errno;
    return (int64_t)rc;
}

/* realpath(path) → resolved absolute path (empty on error) */
AriaString aria_libc_fs_realpath(const char *path) {
    char *resolved = realpath(path, NULL);
    if (!resolved) {
        last_errno = errno;
        return (AriaString){ "", 0 };
    }
    AriaString result = make_string(resolved);
    free(resolved);
    return result;
}

/* ── Temporary files/dirs ───────────────────────────────────────── */

/* mkdtemp(template) → created dir path (empty on error)
 * Template must end with XXXXXX, e.g. "/tmp/aria-XXXXXX"
 */
AriaString aria_libc_fs_mkdtemp(const char *tmpl) {
    char *copy = strdup(tmpl);
    if (!copy) {
        last_errno = ENOMEM;
        return (AriaString){ "", 0 };
    }
    char *result = mkdtemp(copy);
    if (!result) {
        last_errno = errno;
        free(copy);
        return (AriaString){ "", 0 };
    }
    AriaString s = make_string(result);
    free(copy);
    return s;
}

/* mkstemp(template) → fd (-1 on error), stores path for mkstemp_path()
 * Template must end with XXXXXX, e.g. "/tmp/aria-XXXXXX"
 */
static char last_mkstemp_path[PATH_MAX] = "";

int64_t aria_libc_fs_mkstemp(const char *tmpl) {
    char *copy = strdup(tmpl);
    if (!copy) {
        last_errno = ENOMEM;
        return -1;
    }
    int fd = mkstemp(copy);
    if (fd < 0) {
        last_errno = errno;
        free(copy);
        return -1;
    }
    strncpy(last_mkstemp_path, copy, PATH_MAX - 1);
    last_mkstemp_path[PATH_MAX - 1] = '\0';
    free(copy);
    return (int64_t)fd;
}

/* mkstemp_path() → path of last successful mkstemp */
AriaString aria_libc_fs_mkstemp_path(void) {
    return make_string(last_mkstemp_path);
}

/* ── fnmatch ────────────────────────────────────────────────────── */

/* fnmatch(pattern, string, flags) → 0 if match, FNM_NOMATCH otherwise */
int64_t aria_libc_fs_fnmatch(const char *pattern, const char *str, int64_t flags) {
    return (int64_t)fnmatch(pattern, str, (int)flags);
}

/* ── Glob (handle-based pool) ───────────────────────────────────── */

#define MAX_GLOBS 16

static struct {
    glob_t g;
    int active;
} glob_pool[MAX_GLOBS];

/* glob_open(pattern) → handle (-1 on error) */
int64_t aria_libc_fs_glob_open(const char *pattern) {
    for (int i = 0; i < MAX_GLOBS; i++) {
        if (!glob_pool[i].active) {
            memset(&glob_pool[i].g, 0, sizeof(glob_t));
            int rc = glob(pattern, GLOB_NOSORT, NULL, &glob_pool[i].g);
            if (rc == GLOB_NOMATCH) {
                /* Valid result — zero matches */
                glob_pool[i].active = 1;
                return (int64_t)i;
            }
            if (rc != 0) {
                last_errno = errno;
                return -1;
            }
            glob_pool[i].active = 1;
            return (int64_t)i;
        }
    }
    last_errno = ENOMEM;
    return -1;
}

/* glob_count(handle) → number of matches */
int64_t aria_libc_fs_glob_count(int64_t handle) {
    if (handle < 0 || handle >= MAX_GLOBS || !glob_pool[handle].active) return 0;
    return (int64_t)glob_pool[handle].g.gl_pathc;
}

/* glob_path(handle, index) → matched path string */
AriaString aria_libc_fs_glob_path(int64_t handle, int64_t index) {
    if (handle < 0 || handle >= MAX_GLOBS || !glob_pool[handle].active)
        return (AriaString){ "", 0 };
    if (index < 0 || index >= (int64_t)glob_pool[handle].g.gl_pathc)
        return (AriaString){ "", 0 };
    return make_string(glob_pool[handle].g.gl_pathv[index]);
}

/* glob_close(handle) → 0 on success */
int64_t aria_libc_fs_glob_close(int64_t handle) {
    if (handle < 0 || handle >= MAX_GLOBS || !glob_pool[handle].active) return -1;
    globfree(&glob_pool[handle].g);
    glob_pool[handle].active = 0;
    return 0;
}
