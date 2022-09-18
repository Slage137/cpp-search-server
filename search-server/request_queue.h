#pragma once
#include <deque>
#include "search_server.h"

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server)
        : search_server(search_server)
    {
    }
    
    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
        auto result = search_server.FindTopDocuments(raw_query, document_predicate);
        AddRequest(result.size());
        return result;
    }

    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);
    std::vector<Document> AddFindRequest(const std::string& raw_query);

    int GetNoResultRequests() const {
        return no_results_cnt_;
    }
private:
    struct QueryResult {
        int time;
        size_t result_size;

        QueryResult(int curTime, size_t res_size) :
            time(curTime),
            result_size(res_size)
        {
        }
    };
    std::deque<QueryResult> requests_;
    const SearchServer& search_server;
    int no_results_cnt_ = 0;
    int curTime = 0;
    const static int min_in_day_ = 1440;
    const static int sec_in_min_ = 60;
    const static int sec_in_day_ = sec_in_min_ * min_in_day_;

    void AddRequest(size_t res_num);
};
