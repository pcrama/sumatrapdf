#ifndef _TWO_WAY_STRING_SEARCH_
#define _TWO_WAY_STRING_SEARCH_

/* To enable the experimental different text search functionality,
 * #define this special macro.  The random part at the end of the name
 * has no specific signification, it is just there to make the
 * identifier unique */
#define _NEW_TEXT_SEARCH_28EE9332

LPCWSTR wrapTwoWayStringSearch(LPCWSTR text, size_t text_len, LPCWSTR needle, size_t needle_len);

#endif
