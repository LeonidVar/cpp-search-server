#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <vector>
#include <map>
#include <cmath>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result = 0;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    int id;
    double relevance;
};

class SearchServer {
public:

    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document) {
        ++document_count_;

        const vector<string> words = SplitIntoWordsNoStop(document);
        int doc_words  = words.size(); // число слов в документе

        for (const string& word : words) {
            // для каждого слова заполняем пару id - TF
            documents_[word][document_id] += 1.0/doc_words;
        }
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        const Query query_words = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query_words);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 return lhs.relevance > rhs.relevance;
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

private:
    map<string, map<int, double>> documents_;
    set<string> stop_words_;

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    int document_count_ = 0;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }


    Query ParseQuery(const string& text) const {
        Query words;
        for (string& word : SplitIntoWordsNoStop(text)) {

            // слова с минусом в начале заносим в set minus, остальные в set plus
            if (word[0] == '-') {
                word = word.substr(1);
                words.minus_words.insert(word);
            } else {
                words.plus_words.insert(word);
            }
        }
        return words;
    }

    vector<Document> FindAllDocuments(const Query& query_words) const {
        map<int, double> matched_doc; //<id, tf*idf> для найденных документов

        for (const string& plus_word : query_words.plus_words) {
            if (documents_.count(plus_word)) {
                // расчет IDF: логарифм(всего документов / документов с искомым словом)
                const double idf = log(document_count_ * 1.0 / documents_.at(plus_word).size());

                for (auto& [id, tf] : documents_.at(plus_word)) {
                //расчет и заполнение релевантности
                matched_doc[id] +=  idf*tf;
                }
            }
        }

        // удаление документов с минус словом
        for (const string& minus_word : query_words.minus_words) {
            if (documents_.count(minus_word)) {
                for (const auto& [id, tf] : documents_.at(minus_word)) {
                matched_doc.erase(id);
                }
            }
        }

        vector<Document> matched_documents;
        for (const auto& [id, rlv] : matched_doc) {
                matched_documents.push_back({id, rlv});
        }
        return matched_documents;
    }
};

SearchServer CreateSearchServer() {
    SearchServer search_server;
    search_server.SetStopWords(ReadLine());

    const int document_count = ReadLineWithNumber();
    for (int document_id = 0; document_id < document_count; ++document_id) {
        search_server.AddDocument(document_id, ReadLine());
    }

    return search_server;
}

int main() {
    const SearchServer search_server = CreateSearchServer();

    const string query = ReadLine();
    for (const auto& [document_id, relevance] : search_server.FindTopDocuments(query)) {
        cout << "{ document_id = "s << document_id << ", "
             << "relevance = "s << relevance << " }"s << endl;
    }
}
