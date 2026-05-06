#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cctype>
#include <cstdint>

extern "C" {

int string_length(const char* s) {
    if (!s) return 0;
    return strlen(s);
}

const char* string_concat(const char* s1, const char* s2) {
    if (!s1 && !s2) return "";
    if (!s1) return s2;
    if (!s2) return s1;
    char* res = (char*)malloc(strlen(s1) + strlen(s2) + 1);
    strcpy(res, s1);
    strcat(res, s2);
    return res;
}

const char* string_substring(const char* s, int start, int len) {
    if (!s) return "";
    int l = strlen(s);
    if (start < 0 || start >= l || len <= 0) return "";
    if (start + len > l) len = l - start;
    char* res = (char*)malloc(len + 1);
    strncpy(res, s + start, len);
    res[len] = '\0';
    return res;
}

const char* string_toUpper(const char* s) {
    if (!s) return "";
    int l = strlen(s);
    char* res = (char*)malloc(l + 1);
    for(int i = 0; i < l; ++i) res[i] = toupper(s[i]);
    res[l] = '\0';
    return res;
}

const char* string_toLower(const char* s) {
    if (!s) return "";
    int l = strlen(s);
    char* res = (char*)malloc(l + 1);
    for(int i = 0; i < l; ++i) res[i] = tolower(s[i]);
    res[l] = '\0';
    return res;
}

int string_indexOf(const char* s, const char* sub) {
    if (!s || !sub) return -1;
    const char* p = strstr(s, sub);
    if (p) return p - s;
    return -1;
}

bool string_contains(const char* s, const char* sub) {
    return string_indexOf(s, sub) >= 0;
}

bool string_startsWith(const char* s, const char* prefix) {
    if (!s || !prefix) return false;
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

bool string_endsWith(const char* s, const char* suffix) {
    if (!s || !suffix) return false;
    int ls = strlen(s), lsu = strlen(suffix);
    if (ls < lsu) return false;
    return strcmp(s + ls - lsu, suffix) == 0;
}

const char* string_trim(const char* s) {
    if (!s) return "";
    while(isspace(*s)) s++;
    if(*s == 0) {
        char* res = (char*)malloc(1);
        res[0] = 0;
        return res;
    }
    const char* end = s + strlen(s) - 1;
    while(end > s && isspace(*end)) end--;
    int len = end - s + 1;
    char* res = (char*)malloc(len + 1);
    strncpy(res, s, len);
    res[len] = 0;
    return res;
}

const char* string_replace(const char* s, const char* old_str, const char* new_str) {
    if (!s || !old_str || !new_str) return s;
    int old_len = strlen(old_str);
    if (old_len == 0) return s;
    int new_len = strlen(new_str);
    int count = 0;
    const char* p = s;
    while ((p = strstr(p, old_str))) {
        count++;
        p += old_len;
    }
    char* res = (char*)malloc(strlen(s) + count * (new_len - old_len) + 1);
    p = s;
    char* r = res;
    while (*p) {
        if (strncmp(p, old_str, old_len) == 0) {
            strcpy(r, new_str);
            r += new_len;
            p += old_len;
        } else {
            *r++ = *p++;
        }
    }
    *r = 0;
    return res;
}

const char* string_from_int(int64_t v) {
    char* res = (char*)malloc(32);
    snprintf(res, 32, "%lld", (long long)v);
    return res;
}

const char* string_from_float(double v) {
    char* res = (char*)malloc(64);
    snprintf(res, 64, "%g", v);
    return res;
}

const char* string_from_char(char v) {
    char* res = (char*)malloc(2);
    res[0] = v;
    res[1] = '\0';
    return res;
}

void luv_panic(const char* msg) {
    fprintf(stderr, "\033[1;31m[runtime panic]\033[0m %s\n", msg);
    exit(1);
}

void luv_bounds_check(int64_t index, int64_t len) {
    if (index < 0 || index >= len) {
        luv_panic("Index out of bounds");
    }
}

void luv_null_check(const void* ptr, const char* msg) {
    if (!ptr) {
        luv_panic(msg);
    }
}

void luv_div_zero_check(int64_t val) {
    if (val == 0) {
        luv_panic("Division by zero");
    }
}

}
