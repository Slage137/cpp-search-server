#include "search_server.h"
#include <numeric> // for accumulate
#include <iterator>

void SearchServer::AddDocument(int document_id, const std::string_view document, DocumentStatus status, const std::vector<int>& ratings) {
        if (document_id < 0) {
            throw std::invalid_argument("Incorrect document id. Id < 0");
        }
        if (documents_.count(document_id) > 0)
            throw std::invalid_argument("Document with this id already exists");

        words_.emplace_back(document); // deque

        const auto words = SplitIntoWordsNoStop(words_.back());
        const double inv_word_count = 1.0 / static_cast<int>(words.size());
        for (const std::string_view word : words) {
            //auto [elem, _] = words_.insert(std::string(word));    // set
            word_to_document_freqs_[word][document_id] += inv_word_count;
            document_to_word_freqs_[document_id][word] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
        document_ids_.insert(document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(raw_query, [status]
    ([[maybe_unused]] int document_id, DocumentStatus document_status, [[maybe_unused]] int rating) {
        return document_status == status;
        });
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::string_view raw_query, int document_id) const {
    const Query query = ParseQuery(raw_query);
    for (const std::string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            return { std::vector<std::string_view>{}, documents_.at(document_id).status };
        }
    }

    std::vector<std::string_view> matched_words;
    matched_words.reserve(query.plus_words.size());
    for (const std::string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }

    return {matched_words, documents_.at(document_id).status};
}

const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    static const std::map<std::string_view, double> map;
    if (documents_.count(document_id) > 0)
        return document_to_word_freqs_.at(document_id);
    return map;
}

void SearchServer::RemoveDocument(int document_id) {
    //RemoveDocument(std::execution::seq, document_id);
    if (documents_.count(document_id) == 0) return;
    documents_.erase(document_id);
    // Находим нужный элемент в векторе документов
    document_ids_.erase(document_id);  // удаляем документ
    for (auto& [word, _] : document_to_word_freqs_.at(document_id)) {
        word_to_document_freqs_.at(word).erase(document_id);
    }
    document_to_word_freqs_.erase(document_id);
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(const std::string_view text) const {
    std::vector<std::string_view> words;
    for (const std::string_view word : SplitIntoWordsView(text)) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument("Word "s + word.data() + " is invalid"s);
        }
        if (!IsStopWord(word)) {
            words.emplace_back(word);
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

void RemoveDuplicates(SearchServer& search_server) {
    using namespace std;
    set<int> documentsToRemove;
    set<vector<string_view>> matchedWords; // набор совпадающих слов
    for (int document_id : search_server) {
        vector<string_view> wordsInDocument;
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

void AddDocument(SearchServer& search_server, int document_id, const std::string_view document, DocumentStatus status, const std::vector<int>& ratings) {
    search_server.AddDocument(document_id, document, status, ratings);
}