#include "imgui-combo-filter.h"

#include <ctype.h>
#include <algorithm>

namespace ImStb
{
#include "imstb_textedit.h"
}

namespace ImGui
{

void SortFilterResultsDescending(ComboFilterSearchResults& filtered_items)
{
    std::sort(filtered_items.rbegin(), filtered_items.rend());
}

void SortFilterResultsAscending(ComboFilterSearchResults& filtered_items)
{
    std::sort(filtered_items.begin(), filtered_items.end());
}

namespace Internal
{

float CalcComboItemHeight(int item_count, float offset_multiplier)
{
    const ImGuiContext* g = GImGui;
    return item_count < 0 ? FLT_MAX : (g->FontSize + g->Style.ItemSpacing.y) * item_count - g->Style.ItemSpacing.y + (g->Style.WindowPadding.y * offset_multiplier);
}

void SetScrollToComboItemJump(ImGuiWindow* listbox_window, int index)
{
    const ImGuiContext& g = *GImGui;
    float spacing_y = ImMax(listbox_window->WindowPadding.y, g.Style.ItemSpacing.y);
    float temp_pos = (g.Font->FontSize + g.Style.ItemSpacing.y) * index;
    float new_pos = ImLerp(temp_pos - spacing_y, temp_pos + g.FontSize + g.Style.ItemSpacing.y + spacing_y, 0.5f) - listbox_window->Scroll.y;
    ImGui::SetScrollFromPosY(listbox_window, new_pos + 2.50f, 0.5f);
}

void SetScrollToComboItemUp(ImGuiWindow* listbox_window, int index)
{
    const ImGuiContext& g = *GImGui;
    float item_pos = (g.FontSize + g.Style.ItemSpacing.y) * index;
    float diff = item_pos - listbox_window->Scroll.y;
    if (diff < 0.0f)
        listbox_window->Scroll.y += diff - 1.0f;
}

void SetScrollToComboItemDown(ImGuiWindow* listbox_window, int index)
{
    const ImGuiContext& g = *GImGui;
    const float item_pos_lower = (g.FontSize + g.Style.ItemSpacing.y) * (index + 1);
    const float diff = item_pos_lower - listbox_window->Size.y - listbox_window->Scroll.y;
    if (diff > 0.0f)
        listbox_window->Scroll.y += diff + 1.0f;
}

void UpdateInputTextAndCursor(char* buf, int buf_capacity, const char* new_str)
{
    strncpy(buf, new_str, buf_capacity);

    ImGuiContext& g = *GImGui;
    ImGuiInputTextState& intxt_state = g.InputTextState;

    // 直接操作UTF-8缓冲区
    const int new_len = static_cast<int>(strlen(new_str));
    intxt_state.TextA.resize(new_len + 1);  // +1 for null terminator
    memcpy(intxt_state.TextA.Data, new_str, new_len + 1);
    
    // 设置光标位置
    intxt_state.Stb->cursor = new_len;
    intxt_state.Stb->select_end = new_len;
    intxt_state.CursorClamp();
}

// Adapted from https://github.com/forrestthewoods/lib_fts/blob/master/code/fts_fuzzy_match.h

static bool FuzzySearchRecursive(const char* pattern, const char* src, int& outScore, const char* strBegin, const unsigned char srcMatches[], unsigned char newMatches[], int maxMatches, int& nextMatch, int& recursionCount, int recursionLimit);

bool FuzzySearchEX(char const* pattern, char const* haystack, int& out_score, unsigned char matches[], int max_matches, int& out_matches)
{
	int recursionCount = 0;
	int recursionLimit = 10;
    out_matches = 0;
	return FuzzySearchRecursive(pattern, haystack, out_score, haystack, nullptr, matches, max_matches, out_matches, recursionCount, recursionLimit);
}

bool FuzzySearchEX(char const* pattern, char const* haystack, int& out_score)
{
    unsigned char matches[256];
	int match_count = 0;
	return FuzzySearchEX(pattern, haystack, out_score, matches, sizeof(matches), match_count);
}

static bool FuzzySearchRecursive(const char* pattern, const char* src, int& outScore, const char* strBegin, const unsigned char srcMatches[], unsigned char newMatches[], int maxMatches, int& nextMatch, int& recursionCount, int recursionLimit)
{
    // Count recursions
    ++recursionCount;
    if (recursionCount >= recursionLimit) {
        return false;
    }

    // Detect end of strings
    if (*pattern == '\0' || *src == '\0') {
        return false;
    }

    // Recursion params
    bool recursiveMatch = false;
    unsigned char bestRecursiveMatches[256];
    int bestRecursiveScore = 0;

    // Loop through pattern and str looking for a match
    bool firstMatch = true;
    while (*pattern != '\0' && *src != '\0') {
        // Found match
        if (tolower(*pattern) == tolower(*src)) {
            // Supplied matches buffer was too short
            if (nextMatch >= maxMatches) {
                return false;
            }

            // "Copy-on-Write" srcMatches into matches
            if (firstMatch && srcMatches) {
                memcpy(newMatches, srcMatches, nextMatch);
                firstMatch = false;
            }

            // Recursive call that "skips" this match
            unsigned char recursiveMatches[256];
            int recursiveScore;
            int recursiveNextMatch = nextMatch;
            if (FuzzySearchRecursive(pattern, src + 1, recursiveScore, strBegin, newMatches, recursiveMatches, sizeof(recursiveMatches), recursiveNextMatch, recursionCount, recursionLimit)) {
                // Pick the best recursive score
                if (!recursiveMatch || recursiveScore > bestRecursiveScore) {
                    memcpy(bestRecursiveMatches, recursiveMatches, 256);
                    bestRecursiveScore = recursiveScore;
                }
                recursiveMatch = true;
            }

            // Advance
            newMatches[nextMatch++] = (unsigned char)(src - strBegin);
            ++pattern;
        }
        ++src;
    }

    // Determine if full pattern was matched
    bool matched = *pattern == '\0';

    // Calculate score
    if (matched) {
        const int sequentialBonus = 15; // bonus for adjacent matches
        const int separatorBonus = 30; // bonus if match occurs after a separator
        const int camelBonus = 30; // bonus if match is uppercase and prev is lower
        const int firstLetterBonus = 15; // bonus if the first letter is matched

        const int leadingLetterPenalty = -5; // penalty applied for every letter in str before the first match
        const int maxLeadingLetterPenalty = -15; // maximum penalty for leading letters
        const int unmatchedLetterPenalty = -1; // penalty for every letter that doesn't matter

        // Iterate str to end
        while (*src != '\0') {
            ++src;
        }

        // Initialize score
        outScore = 100;

        // Apply leading letter penalty
        int penalty = leadingLetterPenalty * newMatches[0];
        if (penalty < maxLeadingLetterPenalty) {
            penalty = maxLeadingLetterPenalty;
        }
        outScore += penalty;

        // Apply unmatched penalty
        int unmatched = (int)(src - strBegin) - nextMatch;
        outScore += unmatchedLetterPenalty * unmatched;

        // Apply ordering bonuses
        for (int i = 0; i < nextMatch; ++i) {
            unsigned char currIdx = newMatches[i];

            if (i > 0) {
                unsigned char prevIdx = newMatches[i - 1];

                // Sequential
                if (currIdx == (prevIdx + 1))
                    outScore += sequentialBonus;
            }

            // Check for bonuses based on neighbor character value
            if (currIdx > 0) {
                // Camel case
                char neighbor = strBegin[currIdx - 1];
                char curr = strBegin[currIdx];
                if (::islower(neighbor) && ::isupper(curr)) {
                    outScore += camelBonus;
                }

                // Separator
                bool neighborSeparator = neighbor == '_' || neighbor == ' ';
                if (neighborSeparator) {
                    outScore += separatorBonus;
                }
            }
            else {
                // First letter
                outScore += firstLetterBonus;
            }
        }
    }

    // Return best result
    if (recursiveMatch && (!matched || bestRecursiveScore > outScore)) {
        // Recursive score is better than "this"
        memcpy(newMatches, bestRecursiveMatches, maxMatches);
        outScore = bestRecursiveScore;
        return true;
    }
    else if (matched) {
        // "this" score is better than recursive
        return true;
    }
    else {
        // no match
        return false;
    }
}

} // Internal namespace
} // ImGui namespace
