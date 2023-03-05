#pragma once
#include <algorithm>
#include <stdexcept>
#include <vector>
#include <string>
#include <utility>
#include <set>
#include <map>
#include <cmath>
#include <execution>
#include <string_view>
#include <future>
#include "document.h"
#include "string_processing.h"
#include "concurrent_map.h"
#include "log_duration.h"

using namespace std;
class SearchServer {
public:
	template<typename StringContainer>
	explicit SearchServer(const StringContainer &stop_words);

	explicit SearchServer(const std::string &stop_words_text);

	explicit SearchServer(std::string_view stop_words_text);

	void AddDocument(int document_id, std::string_view document,
			DocumentStatus status, const std::vector<int> &ratings);

	auto begin() const {
		return document_ids_.begin();
	}
	;
	auto end() const {
		return document_ids_.end();
	}
	;

	const std::map<std::string_view, double>& GetWordFrequencies(
			int document_id) const;

	void RemoveDocument(int document_id);
	void RemoveDocument(const std::execution::parallel_policy &policy,
			int document_id);
	void RemoveDocument(const std::execution::sequenced_policy &policy,
			int document_id);

	template<typename DocumentPredicate>
	std::vector<Document> FindTopDocuments(std::string_view raw_query,
			DocumentPredicate document_predicate) const;

	std::vector<Document> FindTopDocuments(std::string_view raw_query,
			DocumentStatus status) const;

	std::vector<Document> FindTopDocuments(std::string_view raw_query) const;

	template<typename DocumentPredicate>
	std::vector<Document> FindTopDocuments(
			const std::execution::parallel_policy &policy,
			std::string_view raw_query,
			DocumentPredicate document_predicate) const;

	std::vector<Document> FindTopDocuments(
			const std::execution::parallel_policy &policy,
			std::string_view raw_query, DocumentStatus status) const;

	std::vector<Document> FindTopDocuments(
			const std::execution::parallel_policy &policy,
			std::string_view raw_query) const;

	template<typename DocumentPredicate>
	std::vector<Document> FindTopDocuments(
			const std::execution::sequenced_policy &policy,
			std::string_view raw_query,
			DocumentPredicate document_predicate) const;

	std::vector<Document> FindTopDocuments(
			const std::execution::sequenced_policy &policy,
			std::string_view raw_query, DocumentStatus status) const;

	std::vector<Document> FindTopDocuments(
			const std::execution::sequenced_policy &policy,
			std::string_view raw_query) const;

	int GetDocumentCount() const;

	int GetDocumentId(int index) const;

	std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(
			std::string_view raw_query, int document_id) const;

	std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(
			const std::execution::parallel_policy &policy,
			std::string_view raw_query, int document_id) const;

	std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(
			const std::execution::sequenced_policy &policy,
			std::string_view raw_query, int document_id) const;
private:
	struct DocumentData {
		int rating;
		DocumentStatus status;
	};

	struct QueryWord {
		std::string_view data;
		bool is_minus;
		bool is_stop;
	};

	struct Query {
		std::vector<std::string_view> plus_words;
		std::vector<std::string_view> minus_words;
	};

	const std::set<std::string, std::less<>> stop_words_;
	std::map<std::string, std::map<int, double>, std::less<>> word_to_document_freqs_;
	std::map<int, DocumentData> documents_;
	std::set<int> document_ids_;
	std::map<int, std::map<std::string_view, double>, std::less<>> word_frequencies_;
	std::map<std::string_view, double> empty_map_;

	bool IsStopWord(std::string_view word) const;
	static bool IsValidWord(std::string_view word);
	std::vector<std::string_view> SplitIntoWordsNoStop(
			std::string_view text) const;
	static int ComputeAverageRating(const std::vector<int> &ratings);
	QueryWord ParseQueryWord(std::string_view text) const;
	Query ParseQuery(std::string_view text, bool NeedSort = false) const;
	double ComputeWordInverseDocumentFreq(std::string_view word) const;

	template<typename DocumentPredicate>
	std::vector<Document> FindAllDocuments(
			const std::execution::sequenced_policy &policy, const Query &query,
			DocumentPredicate document_predicate) const;
	template<typename DocumentPredicate>
	std::vector<Document> FindAllDocuments(
			const std::execution::parallel_policy &policy, const Query &query,
			DocumentPredicate document_predicate) const;
	template<typename DocumentPredicate>
	std::vector<Document> FindAllDocuments(const Query &query,
			DocumentPredicate document_predicate) const;
};

template<typename StringContainer>
SearchServer::SearchServer(const StringContainer &stop_words) :
		stop_words_(MakeUniqueNonEmptyStrings(stop_words)) // Extract non-empty stop words
{

	if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
		throw invalid_argument("Some of stop words are invalid"s);
	}
}

template<typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query &query,
		DocumentPredicate document_predicate) const {
	std::map<int, double> document_to_relevance;
	for (std::string_view word : query.plus_words) {
		if (word_to_document_freqs_.count(word) == 0) {
			continue;
		}
		const double inverse_document_freq = ComputeWordInverseDocumentFreq(
				word);
		for (const auto [document_id, term_freq] : word_to_document_freqs_.at(
				std::string(word))) {
			const auto &document_data = documents_.at(document_id);
			if (document_predicate(document_id, document_data.status,
					document_data.rating)) {
				document_to_relevance[document_id] += term_freq
						* inverse_document_freq;
			}
		}
	}
	for (std::string_view word : query.minus_words) {
		if (word_to_document_freqs_.count(word) == 0) {
			continue;
		}
		for (const auto [document_id, _] : word_to_document_freqs_.at(
				std::string(word))) {
			document_to_relevance.erase(document_id);
		}
	}
	std::vector<Document> matched_documents;
	for (const auto [document_id, relevance] : document_to_relevance) {
		matched_documents.push_back(
				{ document_id, relevance, documents_.at(document_id).rating });
	}
	return matched_documents;
}

//template<typename DocumentPredicate>
//std::vector<Document> SearchServer::FindAllDocuments(
//		const std::execution::parallel_policy &policy, const Query &query,
//		DocumentPredicate document_predicate) const {
//	ConcurrentMap<int, double> document_to_relevance(10);
//	ForEach(policy, query.plus_words,
//			[&](std::string_view word) {
//				if (word_to_document_freqs_.count(word) != 0) {
//					const double inverse_document_freq =
//							ComputeWordInverseDocumentFreq(word);
//					for (const auto [document_id, term_freq] : word_to_document_freqs_.at(
//							std::string(word))) {
//						const auto &document_data = documents_.at(document_id);
//						if (document_predicate(document_id,
//								document_data.status, document_data.rating)) {
//							document_to_relevance[document_id].ref_to_value +=
//									term_freq * inverse_document_freq;
//						}
//					}
//				}
//			}
//	);
//	ForEach(policy, query.minus_words,
//			[&](std::string_view word) {
//				if (word_to_document_freqs_.count(word) != 0) {
//					for (const auto [document_id, _] : word_to_document_freqs_.at(
//							std::string(word))) {
//						document_to_relevance.erase(document_id);
//					}
//				}
//			});
//
//	std::vector<Document> matched_documents;
//	for (const auto [document_id, relevance] : document_to_relevance.BuildOrdinaryMap()) {
//		matched_documents.push_back(
//				{ document_id, relevance, documents_.at(document_id).rating });
//	}
//	return matched_documents;
//}

template<typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(
		const std::execution::parallel_policy &policy, const Query &query,
		DocumentPredicate document_predicate) const {
	ConcurrentMap<int, double> document_to_relevance(10);
	for_each(policy, query.plus_words.begin(), query.plus_words.end(),
			[&](std::string_view word) {
				if (word_to_document_freqs_.count(word) != 0) {
					const double inverse_document_freq =
							ComputeWordInverseDocumentFreq(word);
					for (const auto [document_id, term_freq] : word_to_document_freqs_.at(
							std::string(word))) {
						const auto &document_data = documents_.at(document_id);
						if (document_predicate(document_id,
								document_data.status, document_data.rating)) {
							document_to_relevance[document_id].ref_to_value +=
									term_freq * inverse_document_freq;
						}
					}
				}
			}
	);
	for_each(policy, query.minus_words.begin(), query.minus_words.end(),
			[&](std::string_view word) {
				if (word_to_document_freqs_.count(word) != 0) {
					for (const auto [document_id, _] : word_to_document_freqs_.at(
							std::string(word))) {
						document_to_relevance.erase(document_id);
					}
				}
			});

	std::vector<Document> matched_documents;
	for (const auto [document_id, relevance] : document_to_relevance.BuildOrdinaryMap()) {
		matched_documents.push_back(
				{ document_id, relevance, documents_.at(document_id).rating });
	}
	return matched_documents;
}

template<typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(
		const std::execution::sequenced_policy &policy, const Query &query,
		DocumentPredicate document_predicate) const {
	return FindAllDocuments(query, document_predicate);
}

template<typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query,
		DocumentPredicate document_predicate) const {
	const auto query = ParseQuery(raw_query, true);
	auto matched_documents = FindAllDocuments(query, document_predicate);
	sort(matched_documents.begin(), matched_documents.end(),
			[](const Document &lhs, const Document &rhs) {
				if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
					return lhs.rating > rhs.rating;
				} else {
					return lhs.relevance > rhs.relevance;
				}
			});
	if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
		matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
	}

	return matched_documents;
}

template<typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(
		const std::execution::sequenced_policy &policy,
		std::string_view raw_query,
		DocumentPredicate document_predicate) const {
	return FindTopDocuments(raw_query, document_predicate);
}

template<typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(
		const std::execution::parallel_policy &policy,
		std::string_view raw_query,
		DocumentPredicate document_predicate) const {
	const auto query = ParseQuery(raw_query, true);
	auto matched_documents = FindAllDocuments(policy, query,
			document_predicate);
	sort(policy, matched_documents.begin(), matched_documents.end(),
			[](const Document &lhs, const Document &rhs) {
				if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
					return lhs.rating > rhs.rating;
				} else {
					return lhs.relevance > rhs.relevance;
				}
			});
	if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
		matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
	}
	return matched_documents;
}

template<typename ExecutionPolicy, typename ForwardRange, typename Function>
void ForEach(const ExecutionPolicy &policy, ForwardRange &range,
		Function function) {
	if constexpr (is_same_v<decay_t<ExecutionPolicy>,
			execution::sequenced_policy>
			|| is_same_v<
					typename iterator_traits<typename ForwardRange::iterator>::iterator_category,
					random_access_iterator_tag>) {
		for_each(policy, range.begin(), range.end(), function);
	} else {
		int parts = 2;
		int elements_per_part = range.size() / parts;
		vector<future<void>> futures;
		auto begin = range.begin();
		auto end = next(begin, elements_per_part);
		for (int i = 0; i < parts; ++i) {
			futures.push_back(async([begin, end, &function] {
				for_each(begin, end, function);
			}));
			begin = end;
			if (i == parts - 2) {
				end = range.end();
			} else {
				end = next(begin, elements_per_part);
			}
		}
		for (future<void> &f : futures) {
			f.get();
		}
	}
}

template<typename ForwardRange, typename Function>
void ForEach(ForwardRange &range, Function function) {
	ForEach(execution::seq, range, function);
}

