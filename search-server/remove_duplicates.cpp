#include "remove_duplicates.h"
using namespace std;

void RemoveDuplicates(SearchServer& search_server) {
    vector<int> remove_id_list;
    for (auto iter = --search_server.end(); iter != search_server.begin(); --iter) {
        auto map1 = search_server.GetWordFrequencies(*iter);
        for (auto iter2 = search_server.begin(); iter2 != iter; ++iter2) {
            auto map2 = search_server.GetWordFrequencies(*iter2);
            if (map1.size() == map2.size()
                && equal(map1.begin(), map1.end(), map2.begin(), [](auto a, auto b)
                    { return a.first == b.first; })) {
                remove_id_list.push_back(*iter);
                break;
            }
        }
    }
    for (int i : remove_id_list) {
        cout << "Found duplicate document id " << i << endl;
        search_server.RemoveDocument(i);
    }
}