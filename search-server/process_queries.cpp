#include "process_queries.h"
#include <numeric>
#include <execution>

std::vector<std::vector<Document>> ProcessQueries(const SearchServer& search_server, const std::vector<std::string>& queries)
{
    std::vector<std::vector<Document>> result;
    result.resize(queries.size());
    std::transform(std::execution::par, queries.begin(), queries.end(), result.begin(),
                   [&search_server](const std::string_view query) {
        return search_server.FindTopDocuments(query);
        });
    return result;
}

std::vector<Document> ProcessQueriesJoined(const SearchServer& search_server, const std::vector<std::string>& queries)
{
    std::vector<Document> documents_joined;
    for (auto &documents : ProcessQueries(search_server, queries)) {
        for (auto &doc : documents)
            documents_joined.push_back(doc);
    }
    return documents_joined;
}