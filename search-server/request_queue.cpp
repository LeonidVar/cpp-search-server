#include "request_queue.h"
using namespace std;

RequestQueue::RequestQueue(const SearchServer& search_server)
    : search_server_(search_server), cur_time(0), empty_results(0) {
}

// сделаем "обёртки" для всех методов поиска, чтобы сохранять результаты для нашей статистики
vector<Document> RequestQueue::AddFindRequest(const string& raw_query, DocumentStatus status) {
    query_result_.docs = FindDoc(search_server_.FindTopDocuments(raw_query, status));
    requests_.push_back(query_result_);
    return query_result_.docs;
}
vector<Document> RequestQueue::AddFindRequest(const string& raw_query) {
    query_result_.docs = FindDoc(search_server_.FindTopDocuments(raw_query));
    requests_.push_back(query_result_);
    return query_result_.docs;
}
int RequestQueue::GetNoResultRequests() const {
    return empty_results;
}

vector<Document> RequestQueue::FindDoc(vector<Document> find_docs) {
    bool empty_result = find_docs.empty();
    ++cur_time;
    if (cur_time > min_in_day_) {
        if (requests_.front().docs.empty()) {
            --empty_results;
        }
        requests_.pop_front();
    }
    if (empty_result) {
        ++empty_results;
    }
    return find_docs;
}
