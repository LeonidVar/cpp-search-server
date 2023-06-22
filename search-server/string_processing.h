#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <algorithm>
#include <string_view>

std::vector<std::string> SplitIntoWords(const std::string_view);
std::vector<std::string_view> SplitIntoWordsView(std::string_view str);

void RemoveDuplicateWords(std::vector<std::string_view>&);

template <typename ExecutionPolicy>
void RemoveDuplicateWords(ExecutionPolicy&& policy, std::vector<std::string_view>& vec, std::vector<std::string_view>::iterator it) {
    sort(policy, vec.begin(), it);
    vec.erase(unique(policy, vec.begin(), it), vec.end());
}

template <typename StringContainer>
std::set<std::string, std::less<>> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    std::set<std::string, std::less<>> non_empty_strings;
    for (const std::string_view str : strings) {
        if (!str.empty()) {
            non_empty_strings.insert(std::string{ str });
        }
    }
    return non_empty_strings;
}