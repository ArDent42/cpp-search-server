#include "search_server.h"
#include "string_processing.h"
#include "concurrent_map.h"
#include <algorithm>
#include <stdexcept>
#include <vector>
#include <string>
#include <utility>
#include <set>
#include <map>
#include <execution>
#include <cassert>

SearchServer::SearchServer(const std::string &stop_words_text) :
		SearchServer(std::string_view(stop_words_text)) // Invoke delegating constructor from string container
{
}

SearchServer::SearchServer(std::string_view stop_words_text) :
		SearchServer(SplitIntoWords(stop_words_text)) // Invoke delegating constructor from string container
{
}

void SearchServer::AddDocument(int document_id, std::string_view document,
		DocumentStatus status, const std::vector<int> &ratings) {
	if ((document_id < 0) || (documents_.count(document_id) > 0)) {
		throw invalid_argument("Invalid document_id"s);
	}
	const auto words = SplitIntoWordsNoStop(document);

	const double inv_word_count = 1.0 / words.size();
	for (std::string_view word : words) {
		word_to_document_freqs_[std::string(
				word)][document_id] += inv_word_count;
	}
	for (std::string_view word : words) {
		word_frequencies_[document_id][word] =
				word_to_document_freqs_[std::string(
						word)][document_id];
	}
	documents_.emplace(document_id,
			DocumentData { ComputeAverageRating(ratings), status });
	document_ids_.insert(document_id);
}

const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(
		int document_id) const {
	if (word_frequencies_.count(document_id)) {
		return word_frequencies_.at(document_id);
	}
	return empty_map_;
}

void SearchServer::RemoveDocument(int document_id) {
	auto words_to_del = GetWordFrequencies(document_id);
	for (const auto &word : words_to_del) {
		word_to_document_freqs_.at(std::string(word.first)).erase(document_id);
	}
	documents_.erase(document_id);
	document_ids_.erase(document_id);
	word_frequencies_.erase(document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(
		std::string_view raw_query, DocumentStatus status) const {
	return FindTopDocuments(std::execution::seq, raw_query,
			[status](int document_id, DocumentStatus document_status,
					int rating) {
				return document_status == status;
			});
}

std::vector<Document> SearchServer::FindTopDocuments(
		std::string_view raw_query) const {
	return FindTopDocuments(std::execution::seq, raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
	return documents_.size();
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(
		std::string_view raw_query, int document_id) const {
	if (raw_query.empty()) {
		throw std::invalid_argument("");
	}
	if (document_ids_.count(document_id) == 0) {
		throw std::out_of_range("");
	}
	auto query = ParseQuery(raw_query, true);
	std::vector<std::string_view> matched_words;
	for (std::string_view word : query.minus_words) {
		if (word_to_document_freqs_.count(word) == 0) {
			continue;
		}
		if (word_to_document_freqs_.at(std::string(word)).count(document_id)) {
			return {matched_words, documents_.at(document_id).status};
		}
	}
	for (std::string_view word : query.plus_words) {
		if (word_to_document_freqs_.count(word) == 0) {
			continue;
		}
		if (word_to_document_freqs_.at(std::string(word)).count(document_id)) {
			matched_words.push_back((*word_to_document_freqs_.find(word)).first);
		}
	}
	return {matched_words, documents_.at(document_id).status};
}



bool SearchServer::IsStopWord(std::string_view word) const {
	return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(std::string_view word) {
	return none_of(word.begin(), word.end(), [](char c) {
		return c >= '\0' && c < ' ';
	});
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(
		std::string_view text) const {
	std::vector<std::string_view> words;
	for (std::string_view word : SplitIntoWords(text)) {
		if (!IsValidWord(word)) {
			throw invalid_argument("Word "s + std::string(word) + " is invalid"s);
		}
		if (!IsStopWord(word)) {
			words.push_back(word);
		}
	}
	return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int> &ratings) {
	if (ratings.empty()) {
		return 0;
	}
	int rating_sum = std::accumulate(ratings.begin(), ratings.end(), 0);
	return rating_sum / static_cast<int>(ratings.size());
}
SearchServer::QueryWord SearchServer::ParseQueryWord(
		std::string_view text) const {
	if (text.empty()) {
		throw invalid_argument("Query word is empty"s);
	}
	std::string_view word = text;
	bool is_minus = false;
	if (word[0] == '-') {
		is_minus = true;
		word = word.substr(1);
	}
	if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
		throw invalid_argument("Query word "s + std::string(text) + " is invalid");
	}
	return {word, is_minus, IsStopWord(word)};
}

SearchServer::Query SearchServer::ParseQuery(std::string_view text, bool NeedSort) const {
	Query query;
	for (std::string_view word : SplitIntoWords(text)) {
		const auto query_word = ParseQueryWord(word);
		if (!query_word.is_stop) {
			if (query_word.is_minus) {
				query.minus_words.push_back(query_word.data);
			} else {
				query.plus_words.push_back(query_word.data);
			}
		}
	}
	if (NeedSort) {
		std::sort(query.minus_words.begin(), query.minus_words.end());
		std::sort(query.plus_words.begin(), query.plus_words.end());
		auto last_minus = std::unique(query.minus_words.begin(),
				query.minus_words.end());
		auto last_plus = std::unique(query.plus_words.begin(),
				query.plus_words.end());
		query.minus_words.erase(last_minus, query.minus_words.end());
		query.plus_words.erase(last_plus, query.plus_words.end());
		return query;
	}
	return query;
}

double SearchServer::ComputeWordInverseDocumentFreq(
		std::string_view word) const {
	return log(
			GetDocumentCount() * 1.0 / word_to_document_freqs_.at(std::string(word)).size());
}


