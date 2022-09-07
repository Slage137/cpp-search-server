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
#include <stdexcept>



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

template <typename StringContainer>
set<string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    set<string> non_empty_strings;
    for (const string& str : strings) {
        if (!str.empty()) {
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}

/// Статусы документа
enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED
};


class SearchServer {
public:
    // Defines an invalid document id
    // You can refer this constant as SearchServer::INVALID_DOCUMENT_ID
    inline static constexpr int INVALID_DOCUMENT_ID = -1;

    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words)
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
        for (auto& word : stop_words_) {
            if (!IsValidWord(word))
                throw invalid_argument("Incorrect stop-words");
        }
    }

    SearchServer() = default;

    explicit SearchServer(const string& stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text))  // Invoke delegating constructor from string container
    {
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status,
        const vector<int>& ratings) {
        if (document_id < 0) {
            throw invalid_argument("Incorrect document id. Id < 0");
        }
        if (documents_.count(document_id) > 0)
            throw invalid_argument("Document with this id already exists");

        const auto words = SplitIntoWordsNoStop(document);

        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
        document_ids_.push_back(document_id);
    }

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const {
        const Query query = ParseQuery(raw_query);

        auto matched_documents = FindAllDocuments(query, document_predicate);

        sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
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

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
            });
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    int GetDocumentCount() const {
        return static_cast<int>(documents_.size());
    }

    int GetDocumentId(int index) const {
        if (index >= 0 && index < GetDocumentCount()) {
            return document_ids_[index];
        }
        else
            throw out_of_range("Incorrect document index in GetDocumentId");
        return INVALID_DOCUMENT_ID;
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
    const set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;
    vector<int> document_ids_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    static bool IsValidWord(const string& word) {
        // A valid word must not contain special characters
        return none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c < ' ';
            });
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsValidWord(word)) {
                throw invalid_argument("Incorrent word in document");
            }
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
        if (text.empty()) {
            throw invalid_argument("Empty query word");
        }
        bool is_minus = false;
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        if (text.empty() || text[0] == '-' || !IsValidWord(text)) {
            throw invalid_argument("Invalid query word");
        }

        return QueryWord{ text, is_minus, IsStopWord(text) };
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query result;
        for (const string& word : SplitIntoWords(text)) {
            const auto query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    result.minus_words.insert(query_word.data);
                }
                else {
                    result.plus_words.insert(query_word.data);
                }
            }
        }
        return result;
    }

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
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
            matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
        }
        return matched_documents;
    }
};

template <typename TestFunc>
void RunTestImpl(TestFunc& func, const string& t_str) {
    func();
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

void AssertImpl(bool condition, const string& t_str, const string& file, const string& func, const int line, const string& hint) {
    if (condition)
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
        SearchServer server("in the"s);
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
    int doc_cnt = server.GetDocumentCount();
    ASSERT(doc_cnt == 0); // Количество документов должно быть 0 изначально
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    vector<Document> res = server.FindTopDocuments("class find this document");
    doc_cnt = server.GetDocumentCount();
    if (!res.empty())
        ASSERT(res[0].id == doc_id && doc_cnt == 1);    // Проверяем, что документ появился и он нужный нам
}

// Проверка удаления стоп-слов из текста документа
void TestDeleteStopWordFromDocument() {
    const int doc_id = 2;
    const string content = "hey class lets delete stop words from this document"s;
    const vector<int> ratings = { 2, 1, -2 };
    {
        SearchServer server("hey from this lets"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        vector<string> res = SplitIntoWords("class delete document stop words"s);    // Алфавитный порядок плюс-слов
        auto [words_res, status] = server.MatchDocument("class delete stop words document"s, doc_id);
        ASSERT_HINT((words_res == res && status == DocumentStatus::ACTUAL), "Stop-words should be removed from the document");
    }

    // Проверка пустого списка стоп-слов
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        vector<string> res = SplitIntoWords("class delete document from hey lets stop this words"s);    // Алфавитный порядок плюс-слов
        auto [words_res, status] = server.MatchDocument("hey class lets delete stop words from this document"s, doc_id);
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
    auto res = server.FindTopDocuments(query);
    auto [words, status] = server.MatchDocument(query, doc_id);
    vector<string> words_res = { "class", "document" };
    if (!res.empty()) // Проверяем, что результат не пуст и содержимое корректно
        ASSERT_HINT(res.at(0).id == 1 && words == words_res && res.size() == 1,
            "Not removed document with one minus-word"s);
}

// Проверка матчинга документов
void TestMatchDocument() {
    int doc_id = 1;
    string content = "Hey class lets find this document"s;
    vector<int> ratings = { 2, 1, -2 };

    SearchServer server;
    server.AddDocument(doc_id, content, DocumentStatus::BANNED, ratings);
    {
        auto [words_res, status] = server.MatchDocument("class find document -this"s, doc_id);
        ASSERT_HINT(words_res.empty(), "Document should be deleted when minus-words found");  // Ожидаем пустой вектор из-за наличия минус-слова

    }
    {
        auto [words_res, status] = server.MatchDocument("class find document"s, doc_id);
        vector<string> res = { "class"s, "document"s, "find"s };    // алфавитный порядок
        ASSERT_HINT(words_res == res, "Match document error"s);
    }
    {
        doc_id = 2;
        content = "Hey class lets pass the test with minus words"s;
        ratings = { 1, 2, -4 };
        server.AddDocument(doc_id, content, DocumentStatus::BANNED, ratings);
        auto [words_res, status] = server.MatchDocument("pass test -this"s, doc_id);
        //ASSERT( (status == DocumentStatus::BANNED && words_res == {"pass", "test"} ));
        vector<string> res = { "pass", "test" };
        ASSERT(status == DocumentStatus::BANNED && words_res == res);   // Находим документ по проверке с минус-словом
    }
}

// Проверка правильности сортировки (по убыванию релевантности)
void TestCorrectSort() {
    SearchServer search_server("и в на"s);

    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });

    const string query = "белый пушистый"s;
    auto documents = search_server.FindTopDocuments(query);
    for (size_t i = 0; i + 1 < documents.size(); i++) {
        ASSERT(documents[i + 1].relevance < documents[i].relevance);
    }
}

// Проверка вычисления среднего рейтинга
void TestCalculateAverage() {
    SearchServer search_server("и в на"s);

    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });

    auto average = [](vector<int> nums) {
        int sum = accumulate(nums.begin(), nums.end(), 0);
        return sum / static_cast<int>(nums.size());
    };

    vector<Document> documents = search_server.FindTopDocuments("белый кот"s);
    vector<int> res = { average({ 8, -3 }), average({ 7, 2, 7 }) }; // рейтинги из условия
    if (documents.size() != res.size()) {
        ASSERT_HINT(documents.size() == res.size(), "FindTopDocuments(query) returned a vector of the wrong size");
    }
    else {
        int i = 0;
        for (auto& doc : documents) {
            ASSERT_HINT(doc.rating == res[i], "The average rating is calculated incorrectly"s);
            i++;
        }
    }
}

// Фильтрация результатов поиска с использованием предиката, задаваемого пользователем
void TestPredicateFiltration() {
    SearchServer search_server("и в на"s);

    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });

    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
        ASSERT(document.id % 2 == 0);
    }
}

// Проверка поиска документа с недефолтным статусом
void TestFindDocumentWithNonDefaultStatus() {
    SearchServer search_server("и в на"s);

    int docBanned_id = 3;
    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });

    const string query = "скворец"s;
    auto res = search_server.FindTopDocuments(query, DocumentStatus::BANNED);
    auto [words, status] = search_server.MatchDocument(query, 3);
    if (!res.empty()) {
        // Если нашли неправильно
        ASSERT_HINT(res.at(0).id == docBanned_id && status == DocumentStatus::BANNED, "Document with status BANNED found incorrently"s);
    }
    // Если не нашли вовсе
    ASSERT_HINT(!res.empty(), "Document with status BANNED was not found"s);
}

// Проверка правильности вычисления релевантности
void TestRelevanceComputing() {
    vector<string> query = SplitIntoWords("пушистый ухоженный кот"s);
    map <string, map<int, double>> wordsToFreq;
    map <int, double> docToRelevance;
    vector <string> v_doc0 = SplitIntoWords("белый модный ошейник"s);
    vector <string> v_doc1 = SplitIntoWords("ухоженный скворец евгений"s);
    int document_count = 2;

    // Заполняем словарь подобно классу
    for (auto& i : v_doc0)
        wordsToFreq[i].insert({ 1, 1.0 / v_doc0.size() });
    for (auto& i : v_doc1)
        wordsToFreq[i].insert({ 3, 1.0 / v_doc1.size() });

    SearchServer search_server("и в на"s);

    search_server.AddDocument(0, "белый и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "ухоженный скворец евгений"s, DocumentStatus::ACTUAL, { 9 });

    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::ACTUAL)) {
        const double rel = document.relevance;
        for (auto& word : query) {
            if (wordsToFreq.count(word) == 0)
                continue;
            const double idf = log(document_count / wordsToFreq.at(word).size());
            for (auto& [id, relevance] : wordsToFreq.at(word)) {
                docToRelevance[id] += relevance * idf;
                ASSERT(abs(docToRelevance[id] - rel) < EPSILON);
            }
        }
    }
}

void TestIncorrectStopWords() {
    try {
        SearchServer server("in the with \x12"s);
        ASSERT_HINT(false, "Exception with incorrect stop-words does not work");
    }
    catch (invalid_argument) {
        //cout << err.what() << endl;
    }
}

void TestIncorrectQuery() {
    try {
        SearchServer server("in the with"s);
        const int doc_id = 1;
        const string content = "Hey class lets find this document"s;
        const vector<int> ratings = { 2, 1, -2 };
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        vector<Document> res = server.FindTopDocuments("class find this document \x14");
        ASSERT_HINT(false, "Exception with incorrect query does not work");
    }
    catch (invalid_argument/* & err*/) {
        //cout << err.what() << endl;
    }
    try {
        SearchServer server("in the with"s);
        const int doc_id = 1;
        const string content = "Hey class lets find this document"s;
        const vector<int> ratings = { 2, 1, -2 };
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        vector<Document> res = server.FindTopDocuments("class -");
        ASSERT_HINT(false, "Exception with incorrect query does not work");
    }
    catch (invalid_argument/*& err*/) {
        //cout << err.what() << endl;
    }
    try {
        SearchServer search_server("и с в на"s);
        search_server.AddDocument(1, "Пушистый кот с приятной шерстью"s, DocumentStatus::ACTUAL, { 1, 1, 1 });
        search_server.AddDocument(2, "Пёс с милой мордашкой"s, DocumentStatus::ACTUAL, { 1, 1, 1 });
        auto res = search_server.FindTopDocuments("кот --пушистый"s);
        ASSERT_HINT(false, "Exception with incorrect query does not work");
    }
    catch (invalid_argument/*& err*/) {
        //cout << err.what() << endl;
    }

}

void TestIncorrectGetDocId() {
    try {
        SearchServer server("in the with"s);
        const int doc_id = 1;
        const string content = "Hey class lets find this document"s;
        const vector<int> ratings = { 2, 1, -2 };
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.GetDocumentId(-1);
        ASSERT_HINT(false, "Exception with incorrect doc id does not work");
    }
    catch (out_of_range) {

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
    RUN_TEST(TestFindDocumentWithNonDefaultStatus);
    RUN_TEST(TestRelevanceComputing);
    RUN_TEST(TestIncorrectStopWords);
    RUN_TEST(TestIncorrectQuery);
    RUN_TEST(TestIncorrectGetDocId);
}

int main() {
    setlocale(LC_ALL, "RU");
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}
