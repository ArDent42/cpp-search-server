#include <map>
#include <set>
#include <string>
#include "search_server.h"
#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer &search_server) {
	std::set<std::set<std::string>> dupl;
	std::set<int> id_to_delete;
	for (int id : search_server) {
		std::map<std::string_view, double> found_words = search_server.GetWordFrequencies(id);
		std::set<std::string> words;
		for (const auto &word : found_words) {
			words.insert(std::string(word.first));
		}
		if (dupl.count(words)) {
			id_to_delete.insert(id);
			cout << "Found duplicate document id " << id << endl;
		} else {
			dupl.insert(words);
		}
	}
	for (int id : id_to_delete) {
		search_server.RemoveDocument(id);
	}
}
