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

	template<typename ExecutionPolicy>
	void RemoveDocument(const ExecutionPolicy &policy,
			int document_id);

	template<typename DocumentPredicate>
	std::vector<Document> FindTopDocuments(std::string_view raw_query,
			DocumentPredicate document_predicate) const;

	std::vector<Document> FindTopDocuments(std::string_view raw_query,
			DocumentStatus status) const;

	std::vector<Document> FindTopDocuments(std::string_view raw_query) const;

	template<typename DocumentPredicate, typename ExecutionPolicy>
	std::vector<Document> FindTopDocuments(const ExecutionPolicy &policy,
			std::string_view raw_query,
			DocumentPredicate document_predicate) const;
	template<typename ExecutionPolicy>
	std::vector<Document> FindTopDocuments(
			const ExecutionPolicy &policy,
			std::string_view raw_query, DocumentStatus status) const;
	template<typename ExecutionPolicy>
	std::vector<Document> FindTopDocuments(
			const ExecutionPolicy &policy,
			std::string_view raw_query) const;

	int GetDocumentCount() const;

	int GetDocumentId(int index) const;

	std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(
			std::string_view raw_query, int document_id) const;

	template<typename ExecutionPolicy>
	std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(
			const ExecutionPolicy &policy,
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

	template<typename DocumentPredicate, typename ExecutionPolicy>
	std::vector<Document> FindAllDocuments(
			const ExecutionPolicy &policy, const Query &query,
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

template<typename DocumentPredicate, typename ExecutionPolicy>
std::vector<Document> SearchServer::FindAllDocuments(
		const ExecutionPolicy &policy, const Query &query,
		DocumentPredicate document_predicate) const {
	if constexpr (is_same_v<decay_t<ExecutionPolicy>,
			execution::sequenced_policy>) {
		return FindAllDocuments(query, document_predicate);
	}
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

template<typename DocumentPredicate, typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(
		const ExecutionPolicy &policy,
		std::string_view raw_query,
		DocumentPredicate document_predicate) const {
	if constexpr (is_same_v<decay_t<ExecutionPolicy>,
			execution::sequenced_policy>) {
		return FindTopDocuments(raw_query, document_predicate);
	}
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

template<typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy &policy,
		std::string_view raw_query, DocumentStatus status) const {
	if constexpr (is_same_v<decay_t<ExecutionPolicy>,
			execution::sequenced_policy>) {
		return FindTopDocuments(raw_query,
				[status](int document_id, DocumentStatus document_status,
						int rating) {
					return document_status == status;
				});
	}
	return FindTopDocuments(policy, raw_query,
			[status](int document_id, DocumentStatus document_status,
					int rating) {
				return document_status == status;
			});
}

template<typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy &policy,
		std::string_view raw_query) const {
	if constexpr (is_same_v<decay_t<ExecutionPolicy>,
			execution::sequenced_policy>) {
		return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
	}
	return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}

template<typename ExecutionPolicy>
std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(
		const ExecutionPolicy &policy,
		std::string_view raw_query, int document_id) const {
	if constexpr (is_same_v<decay_t<ExecutionPolicy>,
			execution::sequenced_policy>) {
		return MatchDocument(raw_query, document_id);
	}
	if (raw_query.empty()) {
		throw std::invalid_argument("");
	}
	if (document_ids_.count(document_id) == 0) {
		throw std::out_of_range("");
	}
	const auto query = ParseQuery(raw_query);
	if (std::any_of(query.minus_words.begin(), query.minus_words.end(),
			[&](string_view word) {
				return word_to_document_freqs_.at(std::string(word)).count(document_id);
			})) {
		return {std::vector<std::string_view> {},
			documents_.at(document_id).status};
	}
	std::vector<std::string> matched_words(query.plus_words.size());
	auto it = std::copy_if(query.plus_words.begin(),
			query.plus_words.end(), matched_words.begin(),
			[&](string_view word) {
				return word_to_document_freqs_.at(std::string(word)).count(document_id);
			}
	);
	matched_words.erase(it, matched_words.end());
	std::sort(matched_words.begin(), matched_words.end());
	auto last = std::unique(matched_words.begin(), matched_words.end());
	matched_words.erase(last, matched_words.end());
	std::vector<std::string_view> matched_words_res(matched_words.size());
	std::transform(matched_words.begin(), matched_words.end(), matched_words_res.begin(), [&](const string &str) {
		return std::string_view((*word_to_document_freqs_.find(str)).first);
	});
	return {matched_words_res, documents_.at(document_id).status};
}

template<typename ExecutionPolicy>
void SearchServer::RemoveDocument(const ExecutionPolicy &policy,
		int document_id) {
	if constexpr (is_same_v<decay_t<ExecutionPolicy>,
			execution::sequenced_policy>) {
		RemoveDocument(document_id);
		return;
	}
	const std::map<std::string_view, double> &words_to_del = GetWordFrequencies(
			document_id);
	std::vector<const std::string_view*> p_words_to_del(words_to_del.size());
	std::transform(words_to_del.begin(), words_to_del.end(),
			p_words_to_del.begin(),
			[](const auto &word) {
				return &word.first;
			});
	for_each(p_words_to_del.begin(), p_words_to_del.end(),
			[&](const std::string_view *word) {
				word_to_document_freqs_.at(std::string(*word)).erase(document_id);
			});
	documents_.erase(document_id);
	document_ids_.erase(document_id);
	word_frequencies_.erase(document_id);
}
