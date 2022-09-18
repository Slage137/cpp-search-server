#include "request_queue.h"

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentStatus status) {
    return AddFindRequest(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
        });
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query) {
    return AddFindRequest(raw_query, DocumentStatus::ACTUAL);
}

// private
void RequestQueue::AddRequest(size_t res_num) {
    curTime++; // Новый запрос - новая секунда (минута?)
    // Нужно проверить на наличие устаревших запросов и удалить их из начала очереди
    while (!requests_.empty() && min_in_day_ <= curTime - requests_.front().time) {
        if (requests_.front().result_size == 0)          // Проверка на пустую поисковую выдачу
            --no_results_cnt_;
        requests_.pop_front();                           // Само удаление
    }
    QueryResult query_res(curTime, res_num);
    requests_.push_back(query_res);
    if (res_num == 0)   // Если поисковая выдача пустая, то увеличиваем счетчик пустой выдачи
        ++no_results_cnt_;
}