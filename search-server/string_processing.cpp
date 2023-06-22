#include "string_processing.h"
using namespace std;

vector<string> SplitIntoWords(const string_view text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        }
        else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}


vector<string_view> SplitIntoWordsView(string_view str) {
    vector<string_view> result;

    str.remove_prefix(std::min(str.find_first_not_of(" "), str.size()));;
    while (!str.empty()) {

        int64_t space = str.find(' ');
        if (static_cast<size_t>(space) == str.npos)
        {
            result.push_back(str);
            //str.remove_prefix(std::min(static_cast<size_t>(space), str.size()));
            str = str.substr(0, 0);
        }
        else {
            result.push_back(str.substr(0, space));
            //str.remove_prefix(std::min(static_cast<size_t>(space), str.size()));
            str.remove_prefix(space);
        }
        str.remove_prefix(std::min(str.find_first_not_of(" "), str.size()));;
    }

    return result;
}


void RemoveDuplicateWords(vector<string_view>& vec) {
    sort(vec.begin(), vec.end());
    vec.erase(unique(vec.begin(), vec.end()), vec.end());
}