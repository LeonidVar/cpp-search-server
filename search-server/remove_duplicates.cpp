#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server) {
    for (const int document_id : search_server) {
        search_server.GetWordFrequencies(document_id)
        search_server.RemoveDocument(document_id);
    }
}