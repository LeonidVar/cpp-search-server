#pragma once
#include <iterator>
#include <vector>


template <typename Iterator>
class IteratorRange {
public:
    IteratorRange(Iterator begin, Iterator end)
        : begin_(begin), end_(end), size_(std::distance(begin_, end_)) {
    }

    Iterator begin() {
        return begin_;
    }

    Iterator end() {
        return end_;
    }
    size_t size() const {
        return size_;
    }

private:
    Iterator begin_;
    Iterator end_;
    size_t size_;

};

template <typename Iterator>
class Paginator {
public:
    Paginator(Iterator begin, Iterator end, size_t page_size) {
        int dist = std::distance(begin, end);
        if (dist <= page_size) {
            page_.push_back({ begin, end});
        }
        else {
            for (int i = 0; i <= dist / page_size; i += page_size) {
                Iterator temp_i = begin;
                std::advance(begin, page_size);
                page_.push_back({ temp_i, begin });
            }
            if (begin != end) {
                page_.push_back({ begin, end });
            }
        }

    }

    auto begin() const {
        return page_.begin();
    }

    auto end() const {
        return page_.end();
    }

    auto size() const {
        return page_.size();
    }

private:
    std::vector<IteratorRange<Iterator>> page_;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}