#include "search_server.h"
#include <algorithm>
using namespace std;

SearchServer::SearchServer(const string& stop_words_text)
    : SearchServer::SearchServer(SplitIntoWords(stop_words_text))  // Invoke delegating constructor from string container
{
}

SearchServer::SearchServer(const string_view& stop_words_text)
        : SearchServer::SearchServer(SplitIntoWords(stop_words_text))  // Invoke delegating constructor from string container
    {
    }

set<int>::iterator SearchServer::begin() {
    return  document_ids_.begin();
}

set<int>::iterator SearchServer::end() {
    return document_ids_.end();
}

const map<string, double>& SearchServer::GetWordFrequencies(int document_id) const {
    return document_to_word_freqs_.at(document_id);
}

void SearchServer::RemoveDocument(int document_id) {
    documents_.erase(document_id);
    document_ids_.erase(document_id);
    document_to_word_freqs_.erase(document_id);
    for (auto& word : word_to_document_freqs_) {
        word.second.erase(document_id);
    }
}


void SearchServer::AddDocument(int document_id, const string_view& document, DocumentStatus status,
    const vector<int>& ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw invalid_argument("Invalid document_id"s);
    }
    const auto words = SplitIntoWordsNoStop(document);

    const double inv_word_count = 1.0 / words.size();
    for (const string_view& word : words) {
        word_to_document_freqs_[string{ word }][document_id] += inv_word_count;
        document_to_word_freqs_[document_id][string{ word }] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    document_ids_.insert(document_id);
}

vector<Document> SearchServer::FindTopDocuments(const string_view& raw_query, DocumentStatus status) const {
    return SearchServer::FindTopDocuments(
        raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
        });
}

vector<Document> SearchServer::FindTopDocuments(const string_view& raw_query) const {
    return SearchServer::FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}


int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const string_view& raw_query,
    int document_id) const {
    const auto query = ParseQuery(raw_query, true);
    vector<string_view> matched_words;
    //matched_words.clear();
    

    for (const string_view& word : query.minus_words) {
        if (word_to_document_freqs_.count(string{ word }) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(string{ word }).count(document_id)) {
            return { {}, documents_.at(document_id).status};
        }
    }

    //matched_words.reserve(query.plus_words.size());

    for (const string_view& word : query.plus_words) {
        if (word_to_document_freqs_.count(string{ word }) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(string{ word }).count(document_id)) {
            matched_words.push_back(word);
        }
    }

    vector<string_view> result{ matched_words.begin(), matched_words.end() };
    return { result, documents_.at(document_id).status };
}

bool SearchServer::IsStopWord(const string_view& word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(const string_view& word) {
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}

vector<string_view> SearchServer::SplitIntoWordsNoStop(const string_view& text) const{
    vector<string_view> words;
    for (const string_view& word : SplitIntoWordsView(text)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("Word is invalid"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = 0;
    for (const int rating : ratings) {
        rating_sum += rating;
    }
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(string_view& text) const {
    if (text.empty()) {
        throw invalid_argument("Query word is empty"s);
    }
    //string word{ text.begin(), text.end() };
    bool is_minus = false;
    if (text[0] == '-') {
        is_minus = true;
        text = text.substr(1);
    }
    if (text.empty() || text[0] == '-' || !IsValidWord(text)) {
        throw invalid_argument("Query word is invalid");
    }

    return { text, is_minus, IsStopWord(text) };
}



SearchServer::Query SearchServer::ParseQuery(const string_view& text, bool sort) const {
    Query result;
    for (string_view& word : SplitIntoWordsView(text)) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.push_back(query_word.data);
            }
            else {
                result.plus_words.push_back(query_word.data);
            }
        }
    }

    if (sort)
    {
        RemoveDuplicateWords(result.minus_words);
        RemoveDuplicateWords(result.plus_words);
    }
    return result;
}

// Existence required
double SearchServer::ComputeWordInverseDocumentFreq(const string_view& word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(std::string{ word }).size());
}


std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::execution::parallel_policy&, const std::string_view& raw_query,
    int document_id) const {
    if (document_ids_.count(document_id) == 0) {
        throw std::out_of_range("Invalid document_id.");
    }
    auto query = ParseQuery(raw_query, false);
    std::vector<std::string_view> matched_words;

    if (std::none_of(std::execution::par, query.minus_words.begin(), query.minus_words.end(), [this, document_id](const std::string_view& word)
        {return /*word_to_document_freqs_.count(word) != 0 &&*/ word_to_document_freqs_.at(std::string{ word }).count(document_id); })) {

        matched_words.resize(query.plus_words.size());
        auto it = std::copy_if(std::execution::par, query.plus_words.begin(), query.plus_words.end(), matched_words.begin(),
            [this, document_id](const std::string_view& word)
            {return /*word_to_document_freqs_.count(word) != 0 &&*/ word_to_document_freqs_.at(std::string{ word }).count(document_id); });
        RemoveDuplicateWords(std::execution::par, matched_words, it);

    }

    vector<string_view> result{ matched_words.begin(), matched_words.end() };
    return { result, documents_.at(document_id).status };
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::execution::sequenced_policy&, const std::string_view& raw_query,
    int document_id) const {
    return MatchDocument(raw_query, document_id);
}