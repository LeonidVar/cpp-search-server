#include "process_queries.h"
using namespace std;

vector<vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const vector<string>& queries) {
    vector<vector<Document>> documents_lists(queries.size());
   /*     for (const string& query : queries) {
        documents_lists.push_back(search_server.FindTopDocuments(query));
    }*/
    transform(execution::par, queries.begin(), queries.end(), documents_lists.begin(),
             [&search_server](const string& query) {return search_server.FindTopDocuments(query);});
    return documents_lists;
}

list<Document> ProcessQueriesJoined(
    const SearchServer& search_server,
    const vector<string>& queries) {
    list<Document> result;

    for (auto& now : ProcessQueries(search_server, queries)) {
        for (auto& docs : now) {
            result.push_back(docs);
        }
    }

    return result;
}