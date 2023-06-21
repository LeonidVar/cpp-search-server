#pragma once
#include "string_processing.h"
#include "document.h"
#include <map>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <execution>
#include <mutex>

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double MAX_INACCURACY = 1e-6;
constexpr int MAP_BASKET_COUNT = 100;

template <typename Key, typename Value>
class ConcurrentMap {
private:
    struct Bucket {
        std::mutex mutex;
        std::map<Key, Value> map;
    };

public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys");

    struct Access {
        std::lock_guard<std::mutex> guard;
        Value& ref_to_value;

        Access(const Key& key, Bucket& bucket)
            : guard(bucket.mutex)
            , ref_to_value(bucket.map[key]) {
        }
    };

    explicit ConcurrentMap(size_t bucket_count)
        : buckets_(bucket_count) {
    }

    Access operator[](const Key& key) {
        auto& bucket = buckets_[static_cast<uint64_t>(key) % buckets_.size()];
        return { key, bucket };
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> result;
        for (auto& [mutex, map] : buckets_) {
            std::lock_guard g(mutex);
            result.insert(map.begin(), map.end());
        }
        return result;
    }

    void erase(Key key) {
        buckets_[static_cast<uint64_t>(key) % buckets_.size()].map.erase(key);
    }

private:
    std::vector<Bucket> buckets_;
};

class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);

    explicit SearchServer(const std::string& stop_words_text);
    explicit SearchServer(const std::string_view& stop_words_text);
   
    std::set<int>::iterator begin();
    std::set<int>::iterator end();

    const std::map<std::string, double>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);

    template<typename ExecutionPolicy>
    void RemoveDocument(ExecutionPolicy&& policy, int document_id);

    void AddDocument(int document_id, const std::string_view& document, DocumentStatus status,
        const std::vector<int>& ratings);   

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string_view& raw_query,
        DocumentPredicate document_predicate) const;
    template <typename DocumentPredicate, typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, const std::string_view& raw_query,
        DocumentPredicate document_predicate) const;

    std::vector<Document> FindTopDocuments(const std::string_view& raw_query, DocumentStatus status) const;
    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, const std::string_view& raw_query, DocumentStatus status) const;

    std::vector<Document> FindTopDocuments(const std::string_view& raw_query) const;
    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, const std::string_view& raw_query) const;

    int GetDocumentCount() const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string_view& raw_query,
        int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::parallel_policy&, const std::string_view& raw_query,
        int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::sequenced_policy&, const std::string_view& raw_query,
        int document_id) const;

    std::map<int, std::set<std::string>> GetDocumentWords(int doc_id);

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    const std::set<std::string, std::less<>> stop_words_;
    //map(слово, map(документ, частота))
    std::map<std::string, std::map<int, double>> word_to_document_freqs_;
    //map(документ, map(слово, частота))
    std::map<int, std::map<std::string, double>> document_to_word_freqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> document_ids_;

    bool IsStopWord(const std::string_view& word) const;

    static bool IsValidWord(const std::string_view& word);

    std::vector<std::string_view> SplitIntoWordsNoStop(const std::string_view& text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(std::string_view& text) const;

    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    Query ParseQuery(const std::string_view& text, bool sort) const;


    // Existence required
    double ComputeWordInverseDocumentFreq(const std::string_view& word) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query,
        DocumentPredicate document_predicate) const;
    template <typename DocumentPredicate, typename ExecutionPolicy>
    std::vector<Document> FindAllDocuments(ExecutionPolicy&& policy, const Query& query,
        DocumentPredicate document_predicate) const;
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words))  // Extract non-empty stop words
{
    if (!std::all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
        using namespace std::literals::string_literals;
        throw std::invalid_argument("Some of stop words are invalid"s);
    }
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::string_view& raw_query,
    DocumentPredicate document_predicate) const {
    const auto query = ParseQuery(raw_query, true);

    auto matched_documents = FindAllDocuments(query, document_predicate);

    sort(matched_documents.begin(), matched_documents.end(),
        [](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < MAX_INACCURACY) {
                return lhs.rating > rhs.rating;
            }
            else {
                return lhs.relevance > rhs.relevance;
            }
        });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    return matched_documents;
}

template <typename DocumentPredicate, typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, const std::string_view& raw_query,
    DocumentPredicate document_predicate) const {
    const auto query = ParseQuery(raw_query, true);

    auto matched_documents = FindAllDocuments(policy, query, document_predicate);

    sort(policy, matched_documents.begin(), matched_documents.end(),
        [](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < MAX_INACCURACY) {
                return lhs.rating > rhs.rating;
            }
            else {
                return lhs.relevance > rhs.relevance;
            }
        });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    return matched_documents;
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, const std::string_view& raw_query, DocumentStatus status) const {
    return SearchServer::FindTopDocuments(policy,
        raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
        });
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, const std::string_view& raw_query) const {
    return SearchServer::FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query,
    DocumentPredicate document_predicate) const {
    std::map<int, double> document_to_relevance;

    for (const std::string_view& word : query.plus_words) {
        if (word_to_document_freqs_.count(std::string{ word }) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(std::string{ word })) {
            const auto& document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }

    for (const std::string_view& word : query.minus_words) {
        if (word_to_document_freqs_.count(std::string{ word }) == 0) {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(std::string{ word })) {
            document_to_relevance.erase(document_id);
        }
    }



    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back(
            { document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;
}

template <typename DocumentPredicate, typename ExecutionPolicy>
std::vector<Document> SearchServer::FindAllDocuments(ExecutionPolicy&& policy, const Query& query,
    DocumentPredicate document_predicate) const {
    ConcurrentMap<int, double> document_to_relevance(MAP_BASKET_COUNT);

    std::for_each(policy, query.plus_words.begin(), query.plus_words.end(),
        [&document_to_relevance, &document_predicate, this](const std::string_view& word) {
            if (word_to_document_freqs_.count(std::string{ word })) {
                const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
                for (const auto [document_id, term_freq] : word_to_document_freqs_.at(std::string{ word })) {
                    const auto& document_data = documents_.at(document_id);
                    if (document_predicate(document_id, document_data.status, document_data.rating)) {
                        document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                    }
                }
            }
        });
    //for (const std::string_view& word : query.plus_words) {
    //    //если слова нет в словаре - продолжаем
    //    if (word_to_document_freqs_.count(std::string{ word }) == 0) {
    //        continue;
    //    }
    //    ;
    //    auto cur_map = word_to_document_freqs_.at(std::string{ word });

    //    for_each(policy, cur_map.begin(), cur_map.end(),
    //        [&document_to_relevance, &document_predicate, inverse_document_freq, this]
    //    (const auto pair_docid_freq) {
    //            const auto& document_data = documents_.at(pair_docid_freq.first);
    //            if (document_predicate(pair_docid_freq.first, document_data.status, document_data.rating)) {
    //                document_to_relevance[pair_docid_freq.first].ref_to_value += pair_docid_freq.second * inverse_document_freq;
    //            }
    //        }
    //    );

    //}

    std::for_each(policy, query.minus_words.begin(), query.minus_words.end(),
        [&document_to_relevance, this](const std::string_view& word) {
            if (word_to_document_freqs_.count(std::string{ word })) {
                for (const auto [document_id, _] : word_to_document_freqs_.at(std::string{ word })) {
                    document_to_relevance.erase(document_id);
                }
            }
        });

    //for (const std::string_view& word : query.minus_words) {
    //    auto cur_map = word_to_document_freqs_.at(std::string{ word });
    //    if (word_to_document_freqs_.count(std::string{ word }) == 0) {
    //        continue;
    //    }
    //    for_each(policy, cur_map.begin(), cur_map.end(),
    //        [&document_to_relevance](const auto pair_docid_freq) {
    //            document_to_relevance.erase(pair_docid_freq.first);
    //        });
    //    //for (const auto [document_id, _] : word_to_document_freqs_.at(std::string{ word })) {
    //    //    document_to_relevance.erase(document_id);
    //    //}
    //}

    auto result = document_to_relevance.BuildOrdinaryMap();
    std::vector<Document> matched_documents;
    matched_documents.reserve(result.size());
    for (const auto [document_id, relevance] : result) {
        matched_documents.push_back(
            { document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;
}

template<typename ExecutionPolicy>
void SearchServer::RemoveDocument(ExecutionPolicy&& policy, int document_id) {

    if (document_ids_.count(document_id) == 0) {
        throw std::invalid_argument("Invalid document_id.");
    }

    std::vector<std::string> tmp(document_to_word_freqs_[document_id].size());
    std::transform(policy, document_to_word_freqs_[document_id].cbegin(), document_to_word_freqs_[document_id].cend(),
        tmp.begin(), [](const auto& word) {return word.first; });
    //std::transform(policy, word_to_document_freqs_.cbegin(), word_to_document_freqs_.cend(), 
    //    tmp.begin(), [](const auto& word) {return word.first; });
    for_each(policy, tmp.begin(), tmp.end(), 
        [this, document_id](const auto& word) {word_to_document_freqs_[word].erase(document_id); });

    documents_.erase(document_id);
    document_ids_.erase(document_id);
    document_to_word_freqs_.erase(document_id);
}

