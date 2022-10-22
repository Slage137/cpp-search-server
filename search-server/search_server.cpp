#include "search_server.h"
#include <numeric> // for accumulate

void SearchServer::AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings) {
        if (document_id < 0) {
            throw std::invalid_argument("Incorrect document id. Id < 0");
        }
        if (documents_.count(document_id) > 0)
            throw std::invalid_argument("Document with this id already exists");

        const auto words = SplitIntoWordsNoStop(document);

        const double inv_word_count = 1.0 / static_cast<int>(words.size());
        for (const std::string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
            document_to_word_freqs_[document_id][word] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
        document_ids_.push_back(document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, DocumentStatus status) const {
    return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
        });
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(const std::string& raw_query, int document_id) const {
    const auto query = ParseQuery(raw_query);

    std::vector<std::string> matched_words;
    for (const std::string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    for (const std::string& word : query.minus_words) {
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

const std::map<std::string, double>& SearchServer::GetWordFrequencies(int document_id) const {
    static const std::map<std::string, double> map;
    if (documents_.count(document_id) > 0)
        return document_to_word_freqs_.at(document_id);
    return map;
}

void SearchServer::RemoveDocument(int document_id) {
    if (documents_.count(document_id) == 0) return;
    documents_.erase(document_id);
    // Находим нужный элемент в векторе документов
    auto elem = std::find(document_ids_.begin(), document_ids_.end(), document_id);
    document_ids_.erase(elem);  // удаляем документ
    for (auto& [word, _] : document_to_word_freqs_.at(document_id)) {
        word_to_document_freqs_.at(word).erase(document_id);
    }
    document_to_word_freqs_.erase(document_id);
}

std::vector<std::string> SearchServer::SplitIntoWordsNoStop(const std::string& text) const {
    std::vector<std::string> words;
    for (const std::string& word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument("Word "s + word + " is invalid"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = std::accumulate(ratings.begin(), ratings.end(), 0);
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(const std::string& text) const {
    if (text.empty()) {
        throw std::invalid_argument("Query word is empty"s);
    }
    std::string word = text;
    bool is_minus = false;
    if (word[0] == '-') {
        is_minus = true;
        word = word.substr(1);
    }
    if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
        throw std::invalid_argument("Query word "s + text + " is invalid");
    }

    return { word, is_minus, IsStopWord(word) };
}

SearchServer::Query SearchServer::ParseQuery(const std::string& text) const {
    Query result;
    for (const std::string& word : SplitIntoWords(text)) {
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

void RemoveDuplicates(SearchServer& search_server) {
    using namespace std;
    set<int> documentsToRemove;
    set<vector<string>> matchedWords; // набор совпадающих слов
    for (int document_id : search_server) {
        vector<string> wordsInDocument;
        for (auto& [words, _] : search_server.GetWordFrequencies(document_id)) {
            wordsInDocument.push_back(words);
        }
        if (matchedWords.count(wordsInDocument) > 0) {
            documentsToRemove.insert(document_id);
            continue;
        }
        matchedWords.insert(wordsInDocument);
    }
    // собрали номера документов на удаление и удаляем
    for (int removedDocument : documentsToRemove) {
        cout << "Found duplicate document id " << removedDocument << endl;
        search_server.RemoveDocument(removedDocument);
    }
}

void AddDocument(SearchServer& search_server, int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings) {
    search_server.AddDocument(document_id, document, status, ratings);
}
