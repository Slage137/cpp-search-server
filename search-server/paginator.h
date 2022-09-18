#pragma once

#include <vector>
#include <iostream>
#include <iterator>

template <typename Iterator>
class IteratorRange {
    Iterator first;
    Iterator last;

public:
    // Конструктор
    explicit IteratorRange(Iterator begin, Iterator end)
        : first(begin),
        last(end)
    {
    }

    // Методы
    Iterator begin() const {
        return first;
    }
    Iterator end() const {
        return last;
    }
    size_t size() const {
        return std::distance(first, last);
    }
};

template <typename Iterator>
std::ostream& operator<<(std::ostream& out, const IteratorRange<Iterator>& range) {
    for (Iterator it = range.begin(); it != range.end(); ++it) {
        out << *it;
    }
    return out;
}

template <typename Iterator>
class Paginator {
    std::vector<IteratorRange<Iterator>> pages;
public:
    Paginator(Iterator begin, Iterator end, size_t page_size)
    {
        assert(end >= begin && page_size != 0);
        auto size_container = distance(begin, end);
        auto page_number = size_container / page_size; // Количество целых страниц
        // итератор, по которому будем делить страницы.
        Iterator tmp = begin;
        for (auto page_cnt = 0; page_cnt < page_number; page_cnt++) {
            IteratorRange<Iterator> page(tmp, tmp + page_size);
            pages.push_back(page);
            tmp += page_size;
        }
        // Добавляем последнюю неполную страницу
        if (tmp != end) {
            IteratorRange<Iterator> last_page(tmp, end);
            pages.push_back(last_page);
        }
    }
    auto begin() const {
        return pages.begin();
    }
    auto end() const {
        return pages.end();
    }
    size_t size() {
        return pages.size();
    }
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}
