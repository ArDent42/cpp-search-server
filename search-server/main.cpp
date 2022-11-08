#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <numeric>
#include <cassert>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPS = 1e-6;

template<typename T>
void RunTestImpl(T test, const string &name) {
    test();
    cerr << name << " OK"s << endl;
}

template<typename T, typename U>
void AssertEqualImpl(const T &t, const U &u, const string &t_str, const string &u_str, const string &file,
        const string &func, unsigned line, const string &hint) {
    if (t != u) {
        cerr << boolalpha;
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

void AssertImpl(bool value, const string &expr_str, const string &file, const string &func, unsigned line,
        const string &hint) {
    if (!value) {
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "Assert("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s);
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint));
#define ASSERT(a) AssertImpl((a), #a, __FILE__, __FUNCTION__, __LINE__, ""s);
#define ASSERT_HINT(a, hint) AssertImpl((a), #a, __FILE__, __FUNCTION__, __LINE__, (hint));
#define RUN_TEST(func)  RunTestImpl(func, #func);

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

vector<string> SplitIntoWords(const string &text) {
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
    void SetStopWords(const string &text) {
        for (const string &word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string &document, DocumentStatus status,
            const vector<int> &ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string &word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData { ComputeAverageRating(ratings), status });
    }

    vector<Document> FindTopDocuments(const string &raw_query, DocumentStatus Status = DocumentStatus::ACTUAL) const {
        return FindTopDocuments(raw_query, [Status](int document_id, DocumentStatus status, int rating) {
            return Status == status;
        });
    }

    template<typename Predicate>
    vector<Document> FindTopDocuments(const string &raw_query,
            Predicate predicate) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, predicate);

        sort(matched_documents.begin(), matched_documents.end(),
                [](const Document &lhs, const Document &rhs) {
                    if (abs(lhs.relevance - rhs.relevance) < EPS) {
                        return lhs.rating > rhs.rating;
                    } else {
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

    tuple<vector<string>, DocumentStatus> MatchDocument(const string &raw_query,
            int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string &word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string &word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return {matched_words, documents_.at(document_id).status};
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    bool IsStopWord(const string &word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string &text) const {
        vector<string> words;
        for (const string &word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int> &ratings) {
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
        return {text, is_minus, IsStopWord(text)};
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string &text) const {
        Query query;
        for (const string &word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                } else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    // Existence required
    double ComputeWordInverseDocumentFreq(const string &word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template<typename Predicate>
    vector<Document> FindAllDocuments(const Query &query, Predicate predicate) const {
        map<int, double> document_to_relevance;
        for (const string &word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                if (predicate(document_id, documents_.at(document_id).status, documents_.at(document_id).rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string &word : query.minus_words) {
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

SearchServer CreateSearchServer() {
    SearchServer server;
    const vector<int> doc_id { 0, 1, 2 };
    const vector<string> content { "a white cat with long tail is found near the red square"s,
            "a black cat with long furry tail"s,
            "dog with three legs and short tail"s };
    const vector<vector<int>> ratings = { { 1, 2, 3 }, { 2, 6, 1 }, { 5, 8 } };
    server.SetStopWords("in the a and with is at was near"s);
    server.AddDocument(doc_id[0], content[0], DocumentStatus::ACTUAL, ratings[0]);
    server.AddDocument(doc_id[1], content[1], DocumentStatus::BANNED, ratings[1]);
    server.AddDocument(doc_id[2], content[2], DocumentStatus::ACTUAL, ratings[2]);
    return server;
}
// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    // Сначала убеждаемся, что поиск слова, не входящего в список стоп-слов,
    // находит нужный документ
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document &doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    // Затем убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
    // возвращает пустой результат
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in"s).empty());
    }
}

void TestingMinusWords() {
    SearchServer server;
    server = CreateSearchServer();
    {
        string query = "cat -tail"s;
        auto found_docs = server.FindTopDocuments(query);
        ASSERT(found_docs.empty());
    }
    {
        string query = "cat with furry tail"s;
        auto found_docs = server.FindTopDocuments(query);
        ASSERT_EQUAL(found_docs.size(), 2);
    }
}

void TestingMatching() {
    SearchServer server;
    server = CreateSearchServer();
    {
        string query = "cat furry tail"s;
        const auto [words, status] = server.MatchDocument(query, 0);
        ASSERT_EQUAL(words.size(), 2);
    }
    {
        string query = "cat furry tail"s;
        const auto [words, status] = server.MatchDocument(query, 1);
        ASSERT_EQUAL(words.size(), 3);
    }

    {
        string query = "cat furry tail -dog"s;
        const auto [words, status] = server.MatchDocument(query, 2);
        ASSERT(words.empty());
    }
}

void TestingSortRelevance() {
    SearchServer server;
    server = CreateSearchServer();
    string query = "cat with furry tail black"s;
    const auto found_docs = server.FindTopDocuments(query);
    bool is_sort = true;
    for (size_t i = 1; i < found_docs.size(); ++i) {
        if (found_docs[i - 1].relevance < found_docs[i].relevance) {
            is_sort = false;
        }
    }
    ASSERT(is_sort);
}

void TestingAverageRating() {
    SearchServer server;
    server = CreateSearchServer();
    map<int, int> id_rating { { 0, 2 }, { 1, 3 }, { 2, 6 } };
    const string query = "cat furry tail";
    const auto found_docs = server.FindTopDocuments(query);
    for (size_t i = 0; i < found_docs.size(); ++i) {
        ASSERT_EQUAL(found_docs[i].rating, id_rating.at(found_docs[i].id));
    }
}

void TestingPredicate() {
    SearchServer server;
    server = CreateSearchServer();
    const string query = "cat furry tail";
    {
        const auto found_docs = server.FindTopDocuments(query, [](int document_id, DocumentStatus status, int rating) {
            return rating > 4;
        });
        ASSERT_EQUAL(found_docs.size(), 1);
    }
}

void TestingStatus() {
    SearchServer server;
    server = CreateSearchServer();
    const string query = "cat furry tail";
    {
        const auto found_docs = server.FindTopDocuments(query, DocumentStatus::BANNED);
        ASSERT_EQUAL(found_docs.size(), 1);
    }
    {
        const auto found_docs = server.FindTopDocuments(query, DocumentStatus::REMOVED);
        ASSERT(found_docs.empty());
    }
}

void TestingRelevance() {
    SearchServer server;
    server.AddDocument(43, "cat furry tail"s, DocumentStatus::ACTUAL, { 1 });
    const string query = "cat furry tail";
    const auto found_docs = server.FindTopDocuments(query, DocumentStatus::ACTUAL);
    ASSERT_EQUAL(found_docs[0].relevance, 0);
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestingMinusWords);
    RUN_TEST(TestingMatching);
    RUN_TEST(TestingSortRelevance);
    RUN_TEST(TestingAverageRating);
    RUN_TEST(TestingPredicate);
    RUN_TEST(TestingStatus);
    RUN_TEST(TestingRelevance);
    // Не забудьте вызывать остальные тесты здесь
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();

}
