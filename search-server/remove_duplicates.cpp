#include "remove_duplicates.h"
using namespace std;

void RemoveDuplicates(SearchServer& search_server) {
    vector<int> remove_id_list;
    set<string> document_words_set;
    for (auto iter = search_server.begin(); iter != search_server.end(); ++iter) {
        string words;
        for (auto now : search_server.GetWordFrequencies(*iter)) {
            words += now.first;
        }
        if (!document_words_set.insert(words).second) {
            remove_id_list.push_back(*iter);
        }
    }
    for (int i : remove_id_list) {
        cout << "Found duplicate document id " << i << endl;
        search_server.RemoveDocument(i);
    }
}