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

// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
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

// Остальные тесты

void TestOneNoMinusWord() {
    //Тест документа с одним словом и с одним рейтингом
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
    //Тест документа без слов и без рейтинга
    content = "";
    ratings.clear();
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("red"s).empty());
    }

    //Тест минус слов
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

    //верный порядок сортировки: 3-2-1-5
    ASSERT_EQUAL(found_docs[0].id, 3);
    ASSERT_EQUAL(found_docs[1].id, 2);
    ASSERT_EQUAL(found_docs[2].id, 1);
    ASSERT_EQUAL(found_docs[3].id, 5);
    ASSERT_EQUAL(found_docs[0].rating, 3); // проверка правильности подсчета рейтинга для документа id=3
    ASSERT_EQUAL(found_docs[1].rating, 10); // проверка правильности подсчета рейтинга для документа id=2
    ASSERT((found_docs[1].relevance - log(5.0 / 4) * 2 / 5.0) < MAX_INACCURACY); // проверка подсчета релевантности
                                        // 5 - документов, 4 - со словом cat; 2 слова cat встречается, 5 слов всего)

    const auto found_docs2 = server.FindTopDocuments("apple garden map street"s);
    //проверка нахождения всех слов из поискового запроса: 4 найденных уникальных слова в документе id=4 дадут релевантность log(5):
    //для каждого слова idf = log(5)/1, tf = 1/4
    ASSERT_HINT(found_docs2[0].relevance - log(5) < MAX_INACCURACY, "All words from the query must be found");
    ASSERT_EQUAL(found_docs2[0].rating, 5);


}

void TestPredicatStatus() {
    SearchServer server;
    server.AddDocument(0, "cat in the big city"s, DocumentStatus::ACTUAL, { 9, 6 });
    server.AddDocument(1, "blue cat a tree cat"s, DocumentStatus::IRRELEVANT, { 1 });
    server.AddDocument(2, "monkey cat playing cat cat"s, DocumentStatus::ACTUAL, { 3 });
    server.AddDocument(3, "apple tree int the garden"s, DocumentStatus::BANNED, { 1 });
    server.AddDocument(4, "operates on values of the same type"s, DocumentStatus::BANNED, { 8 });
    server.AddDocument(5, "cat on values of the same type"s, DocumentStatus::REMOVED, { 4 });

    const auto document = server.FindTopDocuments("apple cat"s);
    ASSERT_EQUAL_HINT(document.size(), 2, "IRRELEVANT/BANNED/REMOVED docs must be excluded from the result"s);

    const auto document1 = server.FindTopDocuments("the apple cat"s, DocumentStatus::BANNED);
    ASSERT_EQUAL(document1.size(), 2);

    const auto document2 = server.FindTopDocuments("apple cat"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; });
    ASSERT_EQUAL_HINT(document2.size(), 2, "Error using predicat status - document_id % 2");

    const auto document3 = server.FindTopDocuments("apple cat"s, [](int document_id, DocumentStatus status, int rating) { return rating > 5; });
    ASSERT_EQUAL_HINT(document3.size(), 1, "Error using predicat status - rating > 5");
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestOneNoMinusWord);
    RUN_TEST(TestSortRelevance);
    RUN_TEST(TestPredicatStatus);
    // Не забудьте вызывать остальные тесты здесь
}


// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}
