/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// utils
#include "BaseUtil.h"
// layout controllers
#include "BaseEngine.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "TwoWayStringSearch.h"

#define SkipWhitespace(c) for (; str::IsWs(*(c)); (c)++)
// ignore spaces between CJK glyphs but not between Latin, Greek, Cyrillic, etc. letters
// cf. http://code.google.com/p/sumatrapdf/issues/detail?id=959
#define isnoncjkwordchar(c) (isWordChar(c) && (unsigned short)(c) < 0x2E80)

static void markAllPagesNonSkip(std::vector<bool>& pagesToSkip) {
    for (size_t i = 0; i < pagesToSkip.size(); i++) {
        pagesToSkip[i] = false;
    }
}
TextSearch::TextSearch(BaseEngine *engine, PageTextCache *textCache) :
    TextSelection(engine, textCache)
{
    nPages = engine->PageCount();
    pagesToSkip.resize(nPages);
    markAllPagesNonSkip(pagesToSkip);
}

TextSearch::~TextSearch()
{
    Clear();
}

void TextSearch::Reset()
{
    pageText = nullptr;
    TextSelection::Reset();
}

void TextSearch::SetText(const WCHAR *text)
{
    // search text starting with a single space enables the 'Match word start'
    // and search text ending in a single space enables the 'Match word end' option
    // (that behavior already "kind of" exists without special treatment, but
    // usually is not quite what a user expects, so let's try to be cleverer)
    this->matchWordStart = text[0] == ' ' && text[1] != ' ';
    this->matchWordEnd = str::EndsWith(text, L" ") && !str::EndsWith(text, L"  ");

    if (text[0] == ' ')
        text++;

    // don't reset anything if the search text hasn't changed at all
    if (str::Eq(this->lastText, text))
        return;

    this->Clear();
    this->lastText = str::Dup(text);
    this->findText = str::Dup(text);

    // extract anchor string (the first word or the first symbol) for faster searching
    if (isnoncjkwordchar(*text)) {
        const WCHAR *end;
        for (end = text; isnoncjkwordchar(*end); end++)
            ;
        anchor = str::DupN(text, end - text);
    }
    // Adobe Reader also matches certain hard-to-type Unicode
    // characters when searching for easy-to-type homoglyphs
    // cf. http://forums.fofou.org/sumatrapdf/topic?id=2432337
    else if (*text == '-' || *text == '\'' || *text == '"')
        anchor = nullptr;
    else
        anchor = str::DupN(text, 1);

    if (str::Len(this->findText) >= INT_MAX)
        this->findText[(unsigned)INT_MAX - 1] = '\0';
    if (str::EndsWith(this->findText, L" "))
        this->findText[str::Len(this->findText) - 1] = '\0';

    markAllPagesNonSkip(pagesToSkip);
}

void TextSearch::SetSensitive(bool sensitive)
{
    if (caseSensitive == sensitive) {
        return;
    }
    this->caseSensitive = sensitive;

    markAllPagesNonSkip(pagesToSkip);
}

void TextSearch::SetDirection(TextSearchDirection direction)
{
    bool forward = FIND_FORWARD == direction;
    if (forward == this->forward)
        return;
    this->forward = forward;
    if (findText)
        findIndex += (int)str::Len(findText) * (forward ? 1 : -1);
}

void TextSearch::SetLastResult(TextSelection *sel)
{
    CopySelection(sel);

    AutoFreeW selection(ExtractText(L" "));
    str::NormalizeWS(selection);
    SetText(selection);

    findPage = std::min(startPage, endPage);
    findIndex = (findPage == startPage ? startGlyph : endGlyph) + (int)str::Len(findText);
#ifdef _NEW_TEXT_SEARCH_28EE9332
    pageText = textCache->GetData(findPage, &pageLen);
#else
    pageText = textCache->GetData(findPage);
#endif
    forward = true;
}

// try to match "findText" from "start" with whitespace tolerance
// (ignore all whitespace except after alphanumeric characters)
int TextSearch::MatchLen(const WCHAR *start) const
{
    const WCHAR *match = findText, *end = start;

    if (matchWordStart && start > pageText && isWordChar(start[-1]) && isWordChar(start[0]))
        return -1;

    if (!match)
        return -1;

    while (*match) {
        if (!*end)
            return -1;
        if (caseSensitive ? *match == *end : CharLower((LPWSTR)LOWORD(*match)) == CharLower((LPWSTR)LOWORD(*end)))
            /* characters are identical */;
        else if (str::IsWs(*match) && str::IsWs(*end))
            /* treat all whitespace as identical */;
        // TODO: Adobe Reader seems to have a more extensive list of
        //       normalizations - is there an easier way?
        else if (*match == '-' && (0x2010 <= *end && *end <= 0x2014))
            /* make HYPHEN-MINUS also match HYPHEN, NON-BREAKING HYPHEN,
               FIGURE DASH, EN DASH and EM DASH (but not the other way around) */;
        else if (*match == '\'' && (0x2018 <= *end && *end <= 0x201b))
            /* make APOSTROPHE also match LEFT/RIGHT SINGLE QUOTATION MARK */;
        else if (*match == '"' && (0x201c <= *end && *end <= 0x201f))
            /* make QUOTATION MARK also match LEFT/RIGHT DOUBLE QUOTATION MARK */;
        else
            return -1;
        match++;
        end++;
        // treat "??" and "? ?" differently, since '?' could have been a word
        // character that's just missing an encoding (and '?' is the replacement
        // character); cf. http://code.google.com/p/sumatrapdf/issues/detail?id=1574
        if (*match && !isnoncjkwordchar(*(match - 1)) && (*(match - 1) != '?' || *match != '?') ||
            str::IsWs(*(match - 1)) && str::IsWs(*(end - 1))) {
            SkipWhitespace(match);
            SkipWhitespace(end);
        }
    }

    if (matchWordEnd && end > pageText && isWordChar(end[-1]) && isWordChar(end[0]))
        return -1;

    return (int)(end - start);
}

static const WCHAR *GetNextIndex(const WCHAR *base, int offset, bool forward)
{
    const WCHAR *c = base + offset + (forward ? 0 : -1);
    if (c < base || !*c)
        return nullptr;
    return c;
}

bool TextSearch::FindTextInPage(int pageNo)
{
    if (str::IsEmpty(findText))
        return false;
    if (!pageNo)
        pageNo = findPage;
    findPage = pageNo;
#ifdef _NEW_TEXT_SEARCH_28EE9332
    size_t needle_size = wcslen(anchor);
#endif
    const WCHAR *found;
    int length;
    do {
        if (!anchor)
            found = GetNextIndex(pageText, findIndex, forward);
        else if (forward) {
#ifndef _NEW_TEXT_SEARCH_28EE9332
            found = (caseSensitive ? StrStr : StrStrI)(pageText + findIndex, anchor);
#else
            if (caseSensitive || 1) {
                found = wrapTwoWayStringSearch(pageText + findIndex, pageLen - findIndex,
                    anchor, needle_size);
            } else {
                found = StrStrI(pageText + findIndex, anchor);
            }
#endif
        } else {
#ifndef _NEW_TEXT_SEARCH_28EE9332
            found = StrRStrI(pageText, pageText + findIndex, anchor);
#else
            found = StrRStrI(pageText, pageText + findIndex, anchor);
#endif
        }
        if (!found)
            return false;
        findIndex = (int)(found - pageText) + (forward ? 1 : 0);
        length = MatchLen(found);
    } while (length <= 0);

    int offset = (int)(found - pageText);
    StartAt(pageNo, offset);
    SelectUpTo(pageNo, offset + length);
    findIndex = offset + (forward ? length : 0);

    // try again if the found text is completely outside the page's mediabox
    if (result.len == 0)
        return FindTextInPage(pageNo);

    return true;
}

bool TextSearch::FindStartingAtPage(int pageNo, ProgressUpdateUI *tracker)
{
    if (str::IsEmpty(findText))
        return false;

    int next = forward ? 1 : -1;
    while (1 <= pageNo && pageNo <= nPages && (!tracker || !tracker->WasCanceled())) {
        if (tracker) {
            tracker->UpdateProgress(pageNo, nPages);
        }

        if (pagesToSkip[pageNo - 1]) {
            pageNo += next;
            continue;
        }

        Reset();

        pageText = textCache->GetData(pageNo, &findIndex);
#ifdef _NEW_TEXT_SEARCH_28EE9332
        pageLen = findIndex;
#endif
        if (pageText) {
            if (forward) {
                findIndex = 0;
            }
            if (FindTextInPage(pageNo)) {
                return true;
            }
            pagesToSkip[pageNo - 1] = true;
        }

        pageNo += next;
    }

    // allow for the first/last page to be included in the next search
    findPage = forward ? nPages + 1 : 0;

    return false;
}

TextSel *TextSearch::FindFirst(int page, const WCHAR *text, ProgressUpdateUI *tracker)
{
    SetText(text);

    if (FindStartingAtPage(page, tracker))
        return &result;
    return nullptr;
}

TextSel *TextSearch::FindNext(ProgressUpdateUI *tracker)
{
    CrashIf(!findText);
    if (!findText)
        return nullptr;

    if (tracker) {
        if (tracker->WasCanceled()) {
            return nullptr;
        }
        tracker->UpdateProgress(findPage, nPages);
    }

    if (FindTextInPage()) {
        return &result;
    }

    auto next = forward ? 1 : -1;
    if (FindStartingAtPage(findPage + next, tracker)) {
        return &result;
    }
    return nullptr;
}
