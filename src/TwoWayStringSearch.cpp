#include "BaseUtil.h"

// Algorithm suggested in comment to https://stackoverflow.com/a/16088595
// Copt-pasted from http://www-igm.univ-mlv.fr/~lecroq/string/node26.html, adapted to WCHAR*

/* Computing of the maximal suffix for <= */
int maxSuf(LPCWSTR x, int m, int *p) {
    int ms, j, k;
    WCHAR a, b;

    ms = -1;
    j = 0;
    k = *p = 1;
    while (j + k < m) {
        a = x[j + k];
        b = x[ms + k];
        if (a < b) {
            j += k;
            k = 1;
            *p = j - ms;
        }
        else
            if (a == b)
                if (k != *p)
                    ++k;
                else {
                    j += *p;
                    k = 1;
                }
            else { /* a > b */
                ms = j;
                j = ms + 1;
                k = *p = 1;
            }
    }
    return(ms);
}

/* Computing of the maximal suffix for >= */
int maxSufTilde(LPCWSTR x, int m, int *p) {
    int ms, j, k;
    WCHAR a, b;

    ms = -1;
    j = 0;
    k = *p = 1;
    while (j + k < m) {
        a = x[j + k];
        b = x[ms + k];
        if (a > b) {
            j += k;
            k = 1;
            *p = j - ms;
        }
        else
            if (a == b)
                if (k != *p)
                    ++k;
                else {
                    j += *p;
                    k = 1;
                }
            else { /* a < b */
                ms = j;
                j = ms + 1;
                k = *p = 1;
            }
    }
    return(ms);
}

#define MAX(x, y) (((x)>(y))?(x):(y))

/* Two Way string matching algorithm.
x: pattern
m: pattern length
n: text length
y: text */
int TW(LPCWSTR x, int m, LPCWSTR y, int n) {
    int i, j, ell, memory, p, per, q;

    /* Preprocessing */
    i = maxSuf(x, m, &p);
    j = maxSufTilde(x, m, &q);
    if (i > j) {
        ell = i;
        per = p;
    }
    else {
        ell = j;
        per = q;
    }

    /* Searching */
    if (memcmp(x, x + per, ell + 1) == 0) {
        j = 0;
        memory = -1;
        while (j <= n - m) {
            i = MAX(ell, memory) + 1;
            while (i < m && x[i] == y[i + j])
                ++i;
            if (i >= m) {
                i = ell;
                while (i > memory && x[i] == y[i + j])
                    --i;
                if (i <= memory)
                    return j;
                j += per;
                memory = m - per - 1;
            }
            else {
                j += (i - ell);
                memory = -1;
            }
        }
    }
    else {
        per = MAX(ell + 1, m - ell - 1) + 1;
        j = 0;
        while (j <= n - m) {
            i = ell + 1;
            while (i < m && x[i] == y[i + j])
                ++i;
            if (i >= m) {
                i = ell;
                while (i >= 0 && x[i] == y[i + j])
                    --i;
                if (i < 0)
                    return j;
                j += per;
            }
            else
                j += (i - ell);
        }
    }
    return -1;
}

LPCWSTR wrapTwoWayStringSearch(LPCWSTR text, size_t text_len, LPCWSTR needle, size_t needle_len) {
    int x = TW(needle, needle_len, text, text_len);
    return (0 <= x) ? (text + x) : (LPWSTR)NULL;
}
