#pragma once
#include "search_server.h"
#include <deque>

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);

    // сделаем "обёртки" для всех методов поиска, чтобы сохранять результаты для нашей статистики
    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
        query_result_.docs = FindDoc(search_server_.FindTopDocuments(raw_query, document_predicate));
        requests_.push_back(query_result_);
        return query_result_.docs;

    }
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);
    std::vector<Document> AddFindRequest(const std::string& raw_query);
    int GetNoResultRequests() const;

private:
    struct QueryResult {
        std::vector<Document> docs;
    };
    QueryResult query_result_;
    std::deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    // возможно, здесь вам понадобится что-то ещё
    const SearchServer& search_server_;
    uint64_t  cur_time;
    int empty_results;

    std::vector<Document> FindDoc(std::vector<Document> find_docs);
};