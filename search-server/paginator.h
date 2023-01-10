#pragma once
#include <utility>
#include <vector>



template<typename It>
class IteratorRange {
private:
	std::pair<It, It> It_page;
	public:
	IteratorRange(const It &begin, const It &end) :
			It_page( { begin, end }) {
	}
	It begin() const {
		return It_page.first;
	}
	It end() const {
		return It_page.second;
	}
	std::size_t size() const {
		return distance(begin, end);
	}
};

template<typename Iterator>
class Paginator {
private:
	std::vector<IteratorRange<Iterator>> pages;
	public:
	Paginator(const Iterator &begin, const Iterator &end, size_t page_size) {
		Iterator begin_t = begin;
		Iterator end_t = next(begin, page_size);
		while (distance(begin_t, end) > 0) {
			pages.push_back( { begin_t, end_t });
			begin_t = end_t;
			if (distance(begin_t, end) < page_size) {
				end_t = next(begin_t, page_size - 1);
			} else {
				end_t = next(begin_t, page_size);
			}
		}
	}
	auto begin() const {
		return pages.begin();
	}
	auto end() const {
		return pages.end();
	}
	std::size_t size() const {
		return pages.size();
	}
};

template<typename Container>
auto Paginate(const Container &c, std::size_t page_size) {
	return Paginator(begin(c), end(c), page_size);
}

template<typename It>
ostream& operator<<(ostream &os, const IteratorRange<It> &page) {
	for (It it = page.begin(); it != page.end(); ++it) {
		os << *it;
	}
	return os;
}
