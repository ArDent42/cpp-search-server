#include "request_queue.h"
#include "document.h"
#include "search_server.h"
#include <queue>
#include <vector>
#include <string>

using namespace std;

RequestQueue::RequestQueue(const SearchServer &search_server) :
		search_server_(search_server) {
}
vector<Document> RequestQueue::AddFindRequest(const string &raw_query, DocumentStatus status) {
	return AddFindRequest(
			raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
				return document_status == status;
			});
}
vector<Document> RequestQueue::AddFindRequest(const string &raw_query) {
	return AddFindRequest(raw_query, DocumentStatus::ACTUAL);
}
int RequestQueue::GetNoResultRequests() const {
	return empty_requests_;
}
