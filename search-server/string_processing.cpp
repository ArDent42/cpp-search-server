#include "string_processing.h"
#include <string>
#include <iostream>
#include <vector>

using namespace std;

vector<string_view> SplitIntoWords(string_view str) {
	vector<string_view> result;
	str.remove_prefix(min(str.size(), str.find_first_not_of(' ')));
	const int64_t pos_end = str.npos;
	while (!str.empty()) {
		int64_t space = str.find(' ');
		result.push_back(str.substr(0, space));
		if (space == pos_end) {
			break;
		} else {
			str.remove_prefix(space + 1);
			if (str[0] == ' ') {
				str.remove_prefix(min(str.size(), str.find_first_not_of(' ')));
			}
		}
	}
	return result;
}

