/*
 * aria_libc_regex.c — POSIX regex wrappers for aria-libc
 *
 * Handle-based pool of compiled regex patterns.
 * Provides: compile, release, is_match, exec (with groups),
 *           group_start/end/string, replace_first, replace_all,
 *           count_matches, error reporting.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <regex.h>

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

static char last_error[512] = "";

int64_t aria_libc_regex_errno(void) {
    return last_error[0] ? 1 : 0;
}

AriaString aria_libc_regex_strerror(int64_t e) {
    (void)e;
    return make_string(last_error);
}

/* Convenience: store the last compile error */
static void set_error(const char *msg) {
    strncpy(last_error, msg, sizeof(last_error) - 1);
    last_error[sizeof(last_error) - 1] = '\0';
}

static void clear_error(void) {
    last_error[0] = '\0';
}

/* ── Constants ──────────────────────────────────────────────────── */

int64_t aria_libc_regex_REG_EXTENDED(void) { return (int64_t)REG_EXTENDED; }
int64_t aria_libc_regex_REG_ICASE(void)    { return (int64_t)REG_ICASE; }
int64_t aria_libc_regex_REG_NEWLINE(void)  { return (int64_t)REG_NEWLINE; }
int64_t aria_libc_regex_REG_NOSUB(void)    { return (int64_t)REG_NOSUB; }

/* ── Handle pool ────────────────────────────────────────────────── */

#define MAX_REGEX 32
#define MAX_GROUPS 32

static struct {
    regex_t compiled;
    int active;
    regmatch_t matches[MAX_GROUPS];
    int last_nmatch;     /* groups from last exec */
} regex_pool[MAX_REGEX];

/* ── Compile / Release ──────────────────────────────────────────── */

/* compile(pattern, flags) → handle (-1 on error, check strerror) */
int64_t aria_libc_regex_compile(const char *pattern, int64_t flags) {
    clear_error();

    for (int i = 0; i < MAX_REGEX; i++) {
        if (!regex_pool[i].active) {
            int rc = regcomp(&regex_pool[i].compiled, pattern, (int)flags);
            if (rc != 0) {
                char buf[256];
                regerror(rc, &regex_pool[i].compiled, buf, sizeof(buf));
                set_error(buf);
                return -1;
            }
            regex_pool[i].active = 1;
            regex_pool[i].last_nmatch = 0;
            return (int64_t)i;
        }
    }
    set_error("regex pool exhausted (max 32)");
    return -1;
}

/* release(handle) → 0 on success, -1 on bad handle */
int64_t aria_libc_regex_release(int64_t handle) {
    if (handle < 0 || handle >= MAX_REGEX || !regex_pool[handle].active)
        return -1;
    regfree(&regex_pool[handle].compiled);
    regex_pool[handle].active = 0;
    regex_pool[handle].last_nmatch = 0;
    return 0;
}

/* ── Matching ───────────────────────────────────────────────────── */

/* is_match(handle, text) → 1 if text matches, 0 if not */
int64_t aria_libc_regex_is_match(int64_t handle, const char *text) {
    if (handle < 0 || handle >= MAX_REGEX || !regex_pool[handle].active)
        return 0;
    return regexec(&regex_pool[handle].compiled, text, 0, NULL, 0) == 0 ? 1 : 0;
}

/* exec(handle, text) → number of groups matched (0 = no match). 
 * Group 0 = entire match, groups 1+ = captures.
 * Stores matches for group_start/group_end/group_string access.
 */
int64_t aria_libc_regex_exec(int64_t handle, const char *text) {
    if (handle < 0 || handle >= MAX_REGEX || !regex_pool[handle].active)
        return 0;

    /* Zero out matches to ensure clean state */
    memset(regex_pool[handle].matches, 0, sizeof(regex_pool[handle].matches));

    int rc = regexec(&regex_pool[handle].compiled, text,
                     MAX_GROUPS, regex_pool[handle].matches, 0);
    if (rc != 0) {
        regex_pool[handle].last_nmatch = 0;
        return 0;
    }

    /* Use re_nsub + 1 (POSIX standard) instead of scanning for rm_so == -1,
     * which is unreliable across libc implementations (glibc vs musl). */
    int count = (int)regex_pool[handle].compiled.re_nsub + 1;
    if (count > MAX_GROUPS) count = MAX_GROUPS;
    regex_pool[handle].last_nmatch = count;
    return (int64_t)count;
}

/* group_start(handle, group) → byte offset of group start (-1 if invalid) */
int64_t aria_libc_regex_group_start(int64_t handle, int64_t group) {
    if (handle < 0 || handle >= MAX_REGEX || !regex_pool[handle].active) return -1;
    if (group < 0 || group >= regex_pool[handle].last_nmatch) return -1;
    return (int64_t)regex_pool[handle].matches[group].rm_so;
}

/* group_end(handle, group) → byte offset past group end (-1 if invalid) */
int64_t aria_libc_regex_group_end(int64_t handle, int64_t group) {
    if (handle < 0 || handle >= MAX_REGEX || !regex_pool[handle].active) return -1;
    if (group < 0 || group >= regex_pool[handle].last_nmatch) return -1;
    return (int64_t)regex_pool[handle].matches[group].rm_eo;
}

/* group_string(handle, group, original_text) → matched substring */
AriaString aria_libc_regex_group_string(int64_t handle, int64_t group,
                                         const char *text) {
    if (handle < 0 || handle >= MAX_REGEX || !regex_pool[handle].active)
        return (AriaString){ "", 0 };
    if (group < 0 || group >= regex_pool[handle].last_nmatch)
        return (AriaString){ "", 0 };

    regoff_t start = regex_pool[handle].matches[group].rm_so;
    regoff_t end   = regex_pool[handle].matches[group].rm_eo;
    int64_t len = (int64_t)(end - start);
    if (len <= 0) return (AriaString){ "", 0 };

    char *buf = malloc((size_t)len + 1);
    if (!buf) return (AriaString){ "", 0 };
    memcpy(buf, text + start, (size_t)len);
    buf[len] = '\0';
    return (AriaString){ buf, len };
}

/* ── Replace ────────────────────────────────────────────────────── */

/* replace_first(handle, text, replacement) → new string with first match replaced */
AriaString aria_libc_regex_replace_first(int64_t handle, const char *text,
                                          const char *replacement) {
    if (handle < 0 || handle >= MAX_REGEX || !regex_pool[handle].active)
        return make_string(text);

    regmatch_t match;
    int rc = regexec(&regex_pool[handle].compiled, text, 1, &match, 0);
    if (rc != 0) return make_string(text);

    size_t text_len  = strlen(text);
    size_t repl_len  = strlen(replacement);
    size_t match_len = (size_t)(match.rm_eo - match.rm_so);
    size_t result_len = text_len - match_len + repl_len;

    char *buf = malloc(result_len + 1);
    if (!buf) return make_string(text);

    memcpy(buf, text, (size_t)match.rm_so);
    memcpy(buf + match.rm_so, replacement, repl_len);
    memcpy(buf + match.rm_so + repl_len, text + match.rm_eo, text_len - (size_t)match.rm_eo);
    buf[result_len] = '\0';

    return (AriaString){ buf, (int64_t)result_len };
}

/* replace_all(handle, text, replacement) → new string with all matches replaced */
AriaString aria_libc_regex_replace_all(int64_t handle, const char *text,
                                        const char *replacement) {
    if (handle < 0 || handle >= MAX_REGEX || !regex_pool[handle].active)
        return make_string(text);

    size_t repl_len = strlen(replacement);
    size_t text_len = strlen(text);

    /* Dynamic output buffer */
    size_t buf_cap = text_len * 2 + 256;
    char *buf = malloc(buf_cap);
    if (!buf) return make_string(text);
    size_t buf_pos = 0;

    const char *cursor = text;
    regmatch_t match;

    while (regexec(&regex_pool[handle].compiled, cursor, 1, &match, 0) == 0) {
        size_t pre_len = (size_t)match.rm_so;

        /* Grow buffer if needed */
        size_t needed = buf_pos + pre_len + repl_len + strlen(cursor + match.rm_eo) + 1;
        if (needed > buf_cap) {
            buf_cap = needed * 2;
            char *tmp = realloc(buf, buf_cap);
            if (!tmp) { free(buf); return make_string(text); }
            buf = tmp;
        }

        /* Copy text before match */
        memcpy(buf + buf_pos, cursor, pre_len);
        buf_pos += pre_len;

        /* Copy replacement */
        memcpy(buf + buf_pos, replacement, repl_len);
        buf_pos += repl_len;

        /* Advance cursor past match (guard against zero-length matches) */
        if (match.rm_eo == 0) {
            if (*cursor) {
                if (buf_pos + 1 >= buf_cap) {
                    buf_cap *= 2;
                    char *tmp = realloc(buf, buf_cap);
                    if (!tmp) { free(buf); return make_string(text); }
                    buf = tmp;
                }
                buf[buf_pos++] = *cursor;
                cursor++;
            } else {
                break;
            }
        } else {
            cursor += match.rm_eo;
        }
    }

    /* Copy remaining text */
    size_t rest = strlen(cursor);
    if (buf_pos + rest + 1 > buf_cap) {
        char *tmp = realloc(buf, buf_pos + rest + 1);
        if (!tmp) { free(buf); return make_string(text); }
        buf = tmp;
    }
    memcpy(buf + buf_pos, cursor, rest);
    buf_pos += rest;
    buf[buf_pos] = '\0';

    return (AriaString){ buf, (int64_t)buf_pos };
}

/* ── Count ──────────────────────────────────────────────────────── */

/* count_matches(handle, text) → number of non-overlapping matches */
int64_t aria_libc_regex_count_matches(int64_t handle, const char *text) {
    if (handle < 0 || handle >= MAX_REGEX || !regex_pool[handle].active)
        return 0;

    const char *cursor = text;
    int64_t count = 0;
    regmatch_t match;

    while (regexec(&regex_pool[handle].compiled, cursor, 1, &match, 0) == 0) {
        count++;
        if (match.rm_eo == 0) {
            if (*cursor) cursor++;
            else break;
        } else {
            cursor += match.rm_eo;
        }
    }
    return count;
}

/* last_error_message() → string describing last compile error */
AriaString aria_libc_regex_last_error(void) {
    return make_string(last_error);
}
