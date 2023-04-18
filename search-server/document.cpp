#include "document.h"
#include <iostream>

using namespace std;

Document::Document() = default;

Document::Document(int id, double relevance, int rating)
    : id(id)
    , relevance(relevance)
    , rating(rating) {
}

ostream& operator<<(ostream& out, Document doc) {
    out << "{ document_id = "s << doc.id << ", relevance = "s << doc.relevance << ", rating = "s << doc.rating << " }"s;
    return out;
}
