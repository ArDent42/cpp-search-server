#pragma once
#include "search_server.h"
#include "document.h"
#include <queue>
#include <vector>
#include <string>

class RequestQueue {
public:
	explicit RequestQueue(const SearchServer &search_server);
	template<typename DocumentPredicate>
	std::vector<Document> AddFindRequest(const std::string &raw_query, DocumentPredicate document_predicate);
	std::vector<Document> AddFindRequest(const std::string &raw_query, DocumentStatus status);
	std::vector<Document> AddFindRequest(const std::string &raw_query);
	int GetNoResultRequests() const;

private:
	struct QueryResult {
		std::string raw_query;
		std::vector<Document> result;
	};
	std::deque<QueryResult> requests_;
	const static int min_in_day_ = 1440;
	int empty_requests_ = 0;
	const SearchServer &search_server_;
};

template<typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string &raw_query, DocumentPredicate document_predicate) {
		std::vector<Document> results;
		results = search_server_.FindTopDocuments(raw_query, document_predicate);
		requests_.push_back( { raw_query, results });
		if (results.empty()) {
			++empty_requests_;
		}
		if (requests_.size() > min_in_day_) {
			if (requests_.front().result.empty()) {
				--empty_requests_;
				requests_.pop_front();
			} else {
				requests_.pop_front();
			}
		}
		return results;
	}
