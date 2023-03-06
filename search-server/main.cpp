#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <iostream>
#include <numeric>

using namespace std;

#define MAX_INACCURACY 1e-6
const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
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
        }
        else {
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
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status,
        const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status_query) const {
        return FindTopDocuments(raw_query, [status_query](int document_id, DocumentStatus status, int rating) { return status == status_query; });
    }

    template <typename Func>
    vector<Document> FindTopDocuments(const string& raw_query, Func func) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, func);

        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < MAX_INACCURACY) {
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

    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query,
        int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return { matched_words, documents_.at(document_id).status };
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

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

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return { text, is_minus, IsStopWord(text) };
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                }
                else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename Func>
    vector<Document> FindAllDocuments(const Query& query, Func func) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                if (func(document_id, documents_.at(document_id).status, documents_.at(document_id).rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back(
                { document_id, relevance, documents_.at(document_id).rating });
        }
        return matched_documents;
    }
};


/*
   ѕодставьте сюда вашу реализацию макросов
   ASSERT, ASSERT_EQUAL, ASSERT_EQUAL_HINT, ASSERT_HINT и RUN_TEST
*/
template <typename t, typename u>
void AssertEqual(t def1, u def2, const string& t_str, const string& u_str,
    const string& file_name, const int line_num, const string& func_name, const string& hint) {

    if (def1 != def2) {
        cerr << boolalpha;
        cerr << file_name << "("s << line_num << "): "s << func_name << ": "s;
        cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cerr << def1 << " != "s << def2 << "."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}
#define ASSERT_EQUAL(expr1, expr2) AssertEqual((expr1), (expr2), #expr1, #expr2, __FILE__, __LINE__, __FUNCTION__, "") 
#define ASSERT_EQUAL_HINT(expr1, expr2, hint) AssertEqual((expr1), (expr2), #expr1, #expr2, __FILE__, __LINE__, __FUNCTION__, (hint)) 

void AssertImpl(bool test, const string& t_str,
    const string& file_name, const int line_num, const string& func_name, const string& hint) {
    if (!test) {
        cerr << file_name << '(' << line_num << "): "s << func_name << ": ASSERT("s << t_str << ") failed."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}
#define ASSERT(expr) AssertImpl((expr), #expr, __FILE__, __LINE__, __FUNCTION__, "") 
#define ASSERT_HINT(expr, hint) AssertImpl((expr), #expr, __FILE__, __LINE__, __FUNCTION__, (hint)) 

template <typename t>
void RunTestImpl(t test, string str) {
    test();
    cerr << str << " OK\n";
}
#define RUN_TEST(func)  RunTestImpl(func, #func);

// -------- Ќачало модульных тестов поисковой системы ----------
void TestAddDocument() {
    SearchServer server;
    server.AddDocument(1, "Tirumala limniace is a large butterfly"s, DocumentStatus::ACTUAL, { 1 });
    server.AddDocument(2, "The upper side of the wing"s, DocumentStatus::ACTUAL, { 10 });
    server.AddDocument(3, "At the base of cells 2"s, DocumentStatus::ACTUAL, { 1, 2, 3, 4, 5 });
    server.AddDocument(4, ""s, DocumentStatus::ACTUAL, { 9, 1 });
    server.AddDocument(5, "top"s, DocumentStatus::ACTUAL, { });
    server.AddDocument(6, "top of the abdomen is dark"s, DocumentStatus::ACTUAL, { 0 });

    ASSERT_EQUAL(server.GetDocumentCount(), 6);
}

// “ест провер€ет, что поискова€ система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
            "Stop words must be excluded from documents"s);
    }
}

// ќстальные тесты

void TestOneNoMinusWord() {
    //“ест документа с одним словом и с одним рейтингом
    const int doc_id = 0;
    string content = "red";
    vector<int> ratings = { 1 };
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("red"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
    //“ест документа без слов и без рейтинга
    content = "";
    ratings.clear();
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("red"s).empty());
    }

    //“ест минус слов
    content = "cat in the city"s;
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("cat -city"s).empty(),
            "Documents with Minus_words must be excluded from the result"s);
    }


}

//Test sort relevance and matching document
void TestSortRelevance() {
    SearchServer server;
    server.AddDocument(1, "cat in the big city"s, DocumentStatus::ACTUAL, { 1 });
    server.AddDocument(2, "blue cat a tree cat"s, DocumentStatus::ACTUAL, { 10 });
    server.AddDocument(3, "monkey cat playing cat cat"s, DocumentStatus::ACTUAL, { 1, 2, 3, 4, 5 });
    server.AddDocument(4, "apple garden street map"s, DocumentStatus::ACTUAL, { 9, 1 });
    server.AddDocument(5, "truck driving by a cat and a catodog"s, DocumentStatus::ACTUAL, { 0, 0, 4 });

    const auto found_docs = server.FindTopDocuments("cat"s);


    //проверка убывани€ релевантности в выдачи
    ASSERT_HINT(found_docs[0].relevance >= found_docs[1].relevance &&
        found_docs[1].relevance >= found_docs[2].relevance &&
        found_docs[2].relevance >= found_docs[3].relevance, "Documents must be sorts in relevance decreasing order"s);
    
    //проверка соответсви€ сортировки и id документа; верный пор€док сортировки: 3-2-1-5
    ASSERT_EQUAL(found_docs[0].id, 3);
    ASSERT_EQUAL(found_docs[1].id, 2);
    ASSERT_EQUAL(found_docs[2].id, 1);
    ASSERT_EQUAL(found_docs[3].id, 5);


    const auto found_docs2 = server.FindTopDocuments("apple garden map street"s);
    //проверка нахождени€ всех слов из поискового запроса: 4 найденных уникальных слова в документе id=4 дадут релевантность log(5):
    //дл€ каждого слова idf = log(5)/1, tf = 1/4
    ASSERT_HINT(abs(found_docs2[0].relevance - log(5)) < MAX_INACCURACY, "All words from the query must be found");
    ASSERT_EQUAL(found_docs2[0].rating, 5);


}

void TestRatingAndRelevanceCount() {
    SearchServer server;
    server.AddDocument(1, "cat cat cat cat cat"s, DocumentStatus::ACTUAL, { -1 });
    server.AddDocument(2, "blue cat cat cat cat"s, DocumentStatus::ACTUAL, { 12 });
    server.AddDocument(3, "monkey red cat playing cat cat"s, DocumentStatus::ACTUAL, { 1, 2, 3, 4, 8 });
    server.AddDocument(4, "cat garden cat map"s, DocumentStatus::ACTUAL, { -9, -1 });
    server.AddDocument(5, "truck driving by a cat"s, DocumentStatus::ACTUAL, { 10, -2, -2, -6 });
    server.AddDocument(6, "oak forest"s, DocumentStatus::ACTUAL, { 2, 6 });

    const auto found_docs = server.FindTopDocuments("cat"s);

    //calculated ratings for found documents
    vector<int> test_rating{ -1, 12, 3, -5, 0 };

    for (int i = 0; i < 5; ++i) {
        ASSERT_EQUAL_HINT(found_docs[i].rating, test_rating[i], "i = " + to_string(i));
    }

    //calculated relevences for found documents
    vector<double> test_rlv{ 0.182321557, 0.145857245, 0.091160778, 0.091160778, 0.036464311 };

    for (int i = 0; i < 5; ++i) {
        ASSERT(abs(found_docs[i].relevance - test_rlv[i]) < MAX_INACCURACY);
    }

}

void TestPredicateStatus() {
    SearchServer server;
    server.AddDocument(0, "cat in the big city"s, DocumentStatus::ACTUAL, { 9, 6 });
    server.AddDocument(1, "blue cat a tree cat"s, DocumentStatus::IRRELEVANT, { 1 });
    server.AddDocument(2, "monkey cat playing cat cat"s, DocumentStatus::ACTUAL, { 3 });
    server.AddDocument(3, "apple tree int the garden"s, DocumentStatus::BANNED, { 1 });
    server.AddDocument(4, "operates on values of the same type"s, DocumentStatus::BANNED, { 8 });
    server.AddDocument(5, "cat on values of the same type"s, DocumentStatus::REMOVED, { 4 });
    server.AddDocument(6, "cat like the underside  the big city"s, DocumentStatus::ACTUAL, { 9, 6 });
    server.AddDocument(7, "only  cat a tree cat"s, DocumentStatus::IRRELEVANT, { 1 });
    server.AddDocument(8, "almost  cat playing cat cat"s, DocumentStatus::REMOVED, { 3 });
    server.AddDocument(9, "pattern tree int the garden"s, DocumentStatus::BANNED, { 1 });

    //ѕроверка добавлени€ всех документов с разными статусами
    ASSERT_EQUAL_HINT(server.GetDocumentCount(), 10, "Documents with any status must be added");

    //¬ базовый поисковый запрос должен попадать только статус ACTUAL
    const auto document = server.FindTopDocuments("apple cat"s);
    ASSERT_EQUAL_HINT(document.size(), 3, "IRRELEVANT/BANNED/REMOVED docs must be excluded from the result"s);

    //ѕроверка поиска со статусом BANNED
    const auto document1 = server.FindTopDocuments("the apple cat garden"s, DocumentStatus::BANNED);
    ASSERT_EQUAL(document1.size(), 3);
    //ѕроверка поиска со статусом IRRELEVANT
    const auto document2 = server.FindTopDocuments("the apple cat"s, DocumentStatus::IRRELEVANT);
    ASSERT_EQUAL(document2.size(), 2);
    //ѕроверка поиска со статусом REMOVED
    const auto document3 = server.FindTopDocuments("the apple cat"s, DocumentStatus::REMOVED);
    ASSERT_EQUAL(document3.size(), 2);

    const auto document4 = server.FindTopDocuments("apple cat"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 6 == 0; });
    ASSERT_EQUAL_HINT(document4.size(), 2, "Error using predicate status - document_id % 6");

    const auto document5 = server.FindTopDocuments("apple cat"s, [](int document_id, DocumentStatus status, int rating) { return rating > 5; });
    ASSERT_EQUAL_HINT(document5.size(), 2, "Error using predicate status - rating > 5");
}

// ‘ункци€ TestSearchServer €вл€етс€ точкой входа дл€ запуска тестов
void TestSearchServer() {
    RUN_TEST(TestAddDocument);
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestOneNoMinusWord);
    RUN_TEST(TestSortRelevance);
    RUN_TEST(TestRatingAndRelevanceCount);
    RUN_TEST(TestPredicateStatus);
    
    // 
}


// --------- ќкончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // ≈сли вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}