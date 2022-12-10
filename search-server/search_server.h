#pragma once

#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <execution>
#include <deque>        // garbage with document text
#include <future>       // for ForEach
#include "log_duration.h"
#include "document.h"
#include "string_processing.h"
#include "concurrent_map.h"

template <typename ExecutionPolicy, typename ForwardRange, typename Function>   // prototype
void ForEach(const ExecutionPolicy& policy, ForwardRange& range, Function function);

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;

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
                throw std::invalid_argument("Incorrect stop-words");
        }
    }

    explicit SearchServer(const std::string& stop_words_text)
        : SearchServer(SplitIntoWordsView(stop_words_text))  // Invoke delegating constructor
                                                             // from string container
    {
    }

    explicit SearchServer(const std::string_view stop_words_text)
            : SearchServer(SplitIntoWordsView(stop_words_text))  // Invoke delegating constructor
    // from string container
    {
    }

    SearchServer() = default;

    void AddDocument(int document_id, const std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentPredicate document_predicate) const {
        return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
    }

    // Вызывает версию с предикатом
    template <typename ExecPolicy>
    std::vector<Document> FindTopDocuments(ExecPolicy&& policy, const std::string_view raw_query, DocumentStatus status = DocumentStatus::ACTUAL) const {
        return FindTopDocuments(policy, raw_query, [status] (   // predicate
                [[maybe_unused]] int document_id, DocumentStatus document_status, [[maybe_unused]] int rating) {
            return document_status == status;
        });
    }

    // new version with Execution policy (Final task sprint9)
    template <typename ExecPolicy, typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(ExecPolicy&& policy, const std::string_view raw_query, DocumentPredicate document_predicate) const {
        auto query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(policy, query, document_predicate);

        sort(policy, matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < EPSILON) {
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

    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(const std::string_view raw_query) const;

    int GetDocumentCount() const { return static_cast<int>(documents_.size()); }

    std::set<int>::const_iterator begin() const { return document_ids_.begin(); }

    std::set<int>::const_iterator end() const { return document_ids_.end(); }

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string_view raw_query, int document_id) const;

    template <typename ExecPolicy>
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(
            ExecPolicy&& policy, const std::string_view raw_query, int document_id) const {

        if (documents_.count(document_id) == 0) {
            throw std::invalid_argument(" ");
        }
        const Query query = ParseQuery(policy, raw_query);

        auto checker = [this, document_id](const auto word) {
            return word_to_document_freqs_.count(word) > 0 &&
                   word_to_document_freqs_.at(word).count(document_id);
        };

        if (std::any_of(policy, query.minus_words.begin(), query.minus_words.end(), checker)) {
            return {std::vector<std::string_view>{}, documents_.at(document_id).status};
        }

        std::vector<std::string_view> matched_words(query.plus_words.size());
        auto it = std::copy_if(policy,
                               query.plus_words.begin(),
                               query.plus_words.end(),
                               matched_words.begin(),
                               checker);

        if constexpr (!std::is_same_v<std::execution::sequenced_policy, ExecPolicy>) {
            std::sort(policy, matched_words.begin(), it);
            it = std::unique(policy, matched_words.begin(), it);
            matched_words.erase(it, matched_words.end());
        }
        return {matched_words, documents_.at(document_id).status};
    }

    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);

    template <typename ExecPolicy>
    void RemoveDocument(ExecPolicy&& policy, int document_id) {
        if (documents_.count(document_id) == 0) return;
        documents_.erase(document_id);

        const std::map<std::string_view, double>& wordsToFreqs = GetWordFrequencies(document_id);
        std::vector<std::string_view> words(wordsToFreqs.size());

        for (auto& [word, _] : document_to_word_freqs_.at(document_id)) {
            words.push_back(word);
        }
        
        std::for_each(policy, words.begin(), words.end(), [&](std::string_view word) {
            if (word_to_document_freqs_.count(word) > 0) {
                word_to_document_freqs_.at(word).erase(document_id);
            }
        });
        document_to_word_freqs_.erase(document_id);
        document_ids_.erase(document_id);
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    std::deque<std::string> words_;
    std::set<std::string, std::less<>> stop_words_;
    std::map<std::string_view, std::map<int, double>, std::less<>> word_to_document_freqs_;
    std::map<int, std::map<std::string_view, double>> document_to_word_freqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> document_ids_;

    bool IsStopWord(const std::string_view word) const {
        return stop_words_.count(std::string(word)) > 0;
    }

    bool IsValidWord(const std::string_view word) const {
        return IsValidWord(std::execution::seq, word);
    }

    template <typename ExecPolicy>
    bool IsValidWord(ExecPolicy&& policy, const std::string_view word) const {
        return std::none_of(policy, word.begin(), word.end(), [](char symbol) {
            return symbol >= '\0' && symbol < ' '; });
    }

    std::vector<std::string_view> SplitIntoWordsNoStop(const std::string_view text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    [[nodiscard]] QueryWord ParseQueryWord(const std::string_view text) const {
        return ParseQueryWord(std::execution::seq, text);
    }

    template <typename ExecutionPolicy>
    [[nodiscard]] QueryWord ParseQueryWord(ExecutionPolicy&& policy, const std::string_view text) const {
        using namespace std::literals;
        if (text.empty()) {
            throw std::invalid_argument("Query word is empty"s);
        }
        std::string_view word = text;
        bool is_minus = false;
        if (word[0] == '-') {
            is_minus = true;
            word = word.substr(1);
        }
        if (word.empty() || word[0] == '-' || !IsValidWord(policy, word)) {
            throw std::invalid_argument("Query word "s + text.data() + " is invalid");
        }
        return { word, is_minus, IsStopWord(word) };
    }

    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;

        enum class TypeWord {
            ePlus,
            eMinus
        };

        void RemoveDuplicates() {
            RemoveDuplicates(TypeWord::ePlus);
            RemoveDuplicates(TypeWord::eMinus);
        }

        void RemoveDuplicates(TypeWord type) {
            if (type == TypeWord::ePlus) {
                std::sort(plus_words.begin(), plus_words.end());
                auto it = std::unique(plus_words.begin(), plus_words.end());
                plus_words.resize(std::distance(plus_words.begin(), it));
            }
            else {
                std::sort(minus_words.begin(), minus_words.end());
                auto it = std::unique(minus_words.begin(), minus_words.end());
                minus_words.resize(std::distance(minus_words.begin(), it));
            }
        }
    };

    Query ParseQuery(const std::string_view text) const {
        return ParseQuery(std::execution::seq, text);
    }

    template <typename ExecPolicy>
    Query ParseQuery(ExecPolicy policy, const std::string_view text) const {
        const auto query_words = SplitIntoWordsView(text);
        Query result;
        result.plus_words.reserve(query_words.size());

        for (const auto word : query_words) {
            const auto query_word = ParseQueryWord(word);
            if (query_word.is_stop)
                continue;
            query_word.is_minus ?
            result.minus_words.emplace_back(query_word.data) :
            result.plus_words.emplace_back(query_word.data);
        }
        if constexpr (std::is_same_v<std::execution::sequenced_policy, ExecPolicy>) {
            result.RemoveDuplicates();
        }
        return result;
    }

    // Existence required
    double ComputeWordInverseDocumentFreq(const std::string_view word) const {
        return log(GetDocumentCount() * 1.0 / static_cast<int>(word_to_document_freqs_.at(word).size()));
    }

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
        return FindAllDocuments(std::execution::seq, query, document_predicate);
    }

    template <typename ExecPolicy, typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(ExecPolicy&& policy, const Query& query, DocumentPredicate document_predicate) const {
        const int BUCKET_COUNT = 8;
        ConcurrentMap<int, double> document_to_relevance(BUCKET_COUNT);

        const auto plusWordsIDF = [this, &document_predicate, &document_to_relevance] (const std::string_view word) {
            if (word_to_document_freqs_.count(word) == 0) {
                return;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                }
            }
        };

        ForEach(policy, query.plus_words, plusWordsIDF);

        auto docToRel = document_to_relevance.BuildOrdinaryMap();
        const auto eraseMinusWords = [&] (const std::string_view word) {
            if (word_to_document_freqs_.count(word) == 0) {
                return;
            }
            for (const auto& [document_id, _] : word_to_document_freqs_.at(word)) {
                docToRel.erase(document_id);
            }
        };

        ForEach(policy, query.minus_words, eraseMinusWords);

        std::vector<Document> matched_documents;
        for (const auto& [document_id, relevance] : docToRel) {
            matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
        }
        return matched_documents;
    }
};

void RemoveDuplicates(SearchServer& search_server);

void AddDocument(SearchServer& search_server, int document_id, const std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

template <typename ExecutionPolicy, typename ForwardRange, typename Function>
void ForEach(const ExecutionPolicy& policy, ForwardRange& range, Function function) {
    using namespace std;
    if constexpr (
            is_same_v<ExecutionPolicy, execution::sequenced_policy>
                                       || is_same_v<typename iterator_traits<typename ForwardRange::iterator>::iterator_category,
                    random_access_iterator_tag>
            ) {
        for_each(policy, range.begin(), range.end(), function);

    } else {
        static constexpr int PART_COUNT = 4;
        const auto part_length = size(range) / PART_COUNT;
        auto part_begin = range.begin();
        auto part_end = next(part_begin, part_length);

        vector<future<void>> futures;
        for (int i = 0;
             i < PART_COUNT;
             ++i, part_begin = part_end, part_end = (i == PART_COUNT - 1
                                 ? range.end()
                                 : next(part_begin, part_length))
             ) {
            futures.push_back(async([function, part_begin, part_end] {
                for_each(part_begin, part_end, function);
            }));
        }
    }
}

template <typename ForwardRange, typename Function>
void ForEach(ForwardRange& range, Function function) {
    ForEach(std::execution::seq, range, function);
}