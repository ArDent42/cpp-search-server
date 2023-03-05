#include <vector>
#include <string>
#include <algorithm>
#include <execution>
#include <list>
#include "search_server.h"
#include "process_queries.h"

std::vector<std::vector<Document>> ProcessQueries(
		const SearchServer &search_server,
		const std::vector<std::string> &queries) {
	std::vector<std::vector<Document>> results(queries.size());
	std::transform(std::execution::par, queries.begin(), queries.end(),
			results.begin(), [search_server](const std::string &query) {
				return search_server.FindTopDocuments(query);
			});
	return results;
}

std::list<Document> ProcessQueriesJoined(const SearchServer &search_server,
		const std::vector<std::string> &queries) {
	std::list<Document> results;
	for (const auto& docs : ProcessQueries(search_server, queries)) {
		for (const Document &doc : docs) {
			results.push_back(doc);
		}
	}
	return results;
}
