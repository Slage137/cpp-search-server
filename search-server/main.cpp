#include <algorithm>
#include <cassert>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <numeric>
#include <iostream>
#include <tuple>



using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;

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

/// Статусы документа
enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED
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
    //////////////// FindTopDocuments //////////////////
    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status = DocumentStatus::ACTUAL) const {
        return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus doc_status, int rating) { return status == doc_status; });
    }


    template<typename Predicate>
    vector<Document> FindTopDocuments(const string& raw_query,
        Predicate predicate) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, predicate);

        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < EPSILON) {
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
        return static_cast<int>(documents_.size());
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
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
    ////////////////////// FindAllDocuments //////////////////////
    template<typename Predicate>
    vector<Document> FindAllDocuments(const Query& query, Predicate predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const DocumentData& docData = documents_.at(document_id);
                if (predicate(document_id, docData.status, docData.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto& [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto& [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back(
                { document_id, relevance, documents_.at(document_id).rating });
        }
        return matched_documents;
    }
};

template <typename T>
void RunTestImpl(T& t, const string& t_str) {
    t();
    cerr << t_str << " OK" << endl;
}

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
    const string& func, unsigned line, const string& hint) {
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

void AssertImpl(bool t, const string& t_str, const string& file, const string& func, const int line, const string& hint) {
    if (t)
        return;
    cerr << boolalpha;
    cerr << file << "("s << line << "): "s << func << ": "s;
    if (hint.empty())
        cerr << "ASSERT("s << t_str << ") failed." << endl;
    else
        cerr << "ASSERT("s << t_str << ") failed. Hint: " << hint << endl;
    abort();
}

#define RUN_TEST(func)  RunTestImpl((func), (#func))
#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))
#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

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
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
    }
}

// Проверка поиска добавленного документа
void TestAddAndFindDocument() {
    const int doc_id = 1;
    const string content = "Hey class lets find this document"s;
    const vector<int> ratings = { 2, 1, -2 };

    SearchServer server;
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    vector<Document> res = server.FindTopDocuments("class find this document");
    ASSERT_EQUAL(res[0].id, doc_id);
}

// Проверка удаления стоп-слов из текста документа
void TestDeleteStopWordFromDocument() {
    const int doc_id = 2;
    const string content = "hey class lets delete stop words from this document"s;
    const vector<int> ratings = { 2, 1, -2 };
    {
        SearchServer server;
        server.SetStopWords("hey from this lets"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        //string res_words = "class delete stop words document"s;
        vector<string> res = SplitIntoWords("class delete document stop words"s);    // Алфавитный порядок плюс-слов
        vector<string> words_res;
        DocumentStatus status = DocumentStatus::ACTUAL;
        tie(words_res, status) = server.MatchDocument("class delete stop words document"s, doc_id);
        ASSERT_HINT(words_res == res, "Stop-words should be removed from the document");
    }

    // Проверка пустого списка стоп-слов
    {
        SearchServer server;
        server.SetStopWords(""s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        //string res_words = "class delete stop words document"s;
        vector<string> res = SplitIntoWords("class delete document from hey lets stop this words"s);    // Алфавитный порядок плюс-слов
        vector<string> words_res;
        DocumentStatus status = DocumentStatus::ACTUAL;
        tie(words_res, status) = server.MatchDocument("hey class lets delete stop words from this document"s, doc_id);
        ASSERT_HINT(words_res == res, "Empty stop-word string error");
    }

}

// Проверка удаления документов, содержащих минус-слова
void TestDeleteDocumentsWithMinusWords() {

    int doc_id = 3;
    string content = "hey class lets delete minus words from this document"s;
    vector<int> ratings = { 2, 1, -2 };
    string query = "class delete words -minus document"s;

    SearchServer server;
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

    doc_id = 1;
    content = "Hey class lets find this document"s;
    ratings = { 2, 1, -2 };

    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    ASSERT_EQUAL_HINT(server.FindTopDocuments(query).size(), 1, "Not removed document with one minus-word"s);
}

// Проверка матчинга документов
void TestMatchDocument() {
    const int doc_id = 1;
    const string content = "Hey class lets find this document"s;
    const vector<int> ratings = { 2, 1, -2 };

    SearchServer server;
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    vector<string> words_res;
    DocumentStatus status = DocumentStatus::ACTUAL;
    tie(words_res, status) = server.MatchDocument("class find document -this"s, doc_id);
    ASSERT_HINT(words_res.empty(), "Document should be deleted when minus-words found");  // Ожидаем пустой вектор из-за наличия минус-слова

    tie(words_res, status) = server.MatchDocument("class find document"s, doc_id);
    vector<string> res = { "class"s, "document"s, "find"s };    // алфавитный порядок
    ASSERT_HINT(words_res == res, "Match document error"s);
}

// Проверка правильности сортировки (по убыванию релевантности)
void TestCorrectSort() {
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);

    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });

    struct Res {
        int id;
        double rel;
    };
    Res res[3] = { {1, 0.693147}, {0, 0.346574}, {2, 0.346574} };
    const string query = "белый пушистый пёс"s;
    for (auto& doc : search_server.FindTopDocuments(query)) {
        static int i = 0;
        ASSERT_HINT(doc.id == res[i].id || doc.relevance == res[i].rel, "The sorting should be in descending order"s);
        i++;
    }
}

// Проверка вычисления среднего рейтинга
void TestCalculateAverage() {
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);

    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });

    vector<Document> out = search_server.FindTopDocuments("белый кот"s);
    vector<Document> res = { {0, 0, 2}, {1, 0, 5} }; // relevance = 0, так как не интересует

    ASSERT_HINT(equal(out.begin(), out.end(), res.begin(), res.end(), [](const Document& lhs, const Document& rhs) {
        return (lhs.id == rhs.id && lhs.rating == rhs.rating);
        }), "The average rating is calculated incorrectly"s);
}

// Фильтрация результатов поиска с использованием предиката, задаваемого пользователем
void TestPredicateFiltration() {
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);

    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });
    Document res[2] = { {0, 0.173287, 2 }, {2, 0.173287, -1} };

    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
        static int i = 0;
        ASSERT(document.id == res[i].id && document.relevance - res[i].relevance < EPSILON&& document.rating == res[i].rating);
        i++;
    }
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestAddAndFindDocument);
    RUN_TEST(TestDeleteStopWordFromDocument);
    RUN_TEST(TestDeleteDocumentsWithMinusWords);
    RUN_TEST(TestMatchDocument);
    RUN_TEST(TestCorrectSort);
    RUN_TEST(TestCalculateAverage);
    RUN_TEST(TestPredicateFiltration);
}

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}