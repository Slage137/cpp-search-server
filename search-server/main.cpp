#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace std;


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
            words.push_back(word);
            word = "";
        }
        else {
            word += c;
        }
    }
    words.push_back(word);

    return words;
}

struct Document {
    int id;
    double relevance;
};

/////////////////////////// Класс //////////////////////
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
        const double TF = 1.0 / words.size();
        for (const string& word : words) {
            wordsToDocsFreqs[word][document_id] += TF;
        }
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query);

        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                return lhs.relevance > rhs.relevance;
            });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }
    /////////////////////////// private //////////////////////
private:
    int document_count_ = 0;
    set<string> stop_words_;
    map<string, map<int, double>> wordsToDocsFreqs;

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

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWordsNoStop(text)) {
            // Если минус-слово
            if (auto it = word.find('-'); it != string::npos) {
                string minus_word = word.substr(++it);    // Временная переменная для хранения минус-слова
                query.minus_words.insert(minus_word);
            }
            else {  // Если плюс-слово
                query.plus_words.insert(word);   // Вставка плюс-слова
            }
        }
        return query;
    }

    void DeleteMinusWordsDocs(map <int, double>& document_relevance, const Query& query_words) const {
        for (auto& word : query_words.minus_words) {
            if (wordsToDocsFreqs.count(word) == 0)
                continue;
            //cout << "Minus-word: " << word << " found!" << endl;
            for (auto& id : wordsToDocsFreqs.at(word)) {
                //cout << "Docs id: " << id.first << " "s;
                document_relevance.erase(id.first);
            }
        }
    }

    vector<Document> FindAllDocuments(const Query& query) const {
        map<int, double> document_relevance;
        for (const string& word : query.plus_words) {
            if (wordsToDocsFreqs.count(word) == 0) {
                continue;
            }
            double IDF = log(document_count_ * 1.0 / wordsToDocsFreqs.at(word).size());
            for (const auto& [document_id, TF] : wordsToDocsFreqs.at(word)) {
                document_relevance[document_id] += TF * IDF;
            }
        }
        DeleteMinusWordsDocs(document_relevance, query);
        vector<Document> matched_documents;
        for (const auto& [document_id, relevance] : document_relevance) {
            matched_documents.push_back({ document_id, relevance });
        }
        return matched_documents;
    }
};
/////////////////////////// Конец класса //////////////////////

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
    for (auto& [document_id, relevance] : search_server.FindTopDocuments(query)) {
        cout << "{ document_id = "s << document_id << ", "
            << "relevance = "s << relevance << " }"s
            << endl;
    }
    return 0;
}