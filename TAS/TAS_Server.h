#pragma once

class TAS {
public:
	TAS() {};
	void strip(utility::string_t &row, size_t &index, bool ignoreDot = false);
	void ExtractTOC(json::value const &value);
	void fill_common_words();
	void addToMap(int pos, int start, int end, utility::string_t &word);
	void addToMapCommon(int pos, int start, int end, utility::string_t &word);
	void tokenize(utility::string_t &substr/*, size_t &i, size_t &j*/);
	void find_start_end(utility::string_t &ss, size_t &i, size_t &j);
	void indexify(utility::string_t &ss);
	void print_search_results(json::value const & value);
	pplx::task<void> HTTPGetTOC();
	pplx::task<void> HTTPGetAsync();
	void search_tokenize(vector<utility::string_t> &v, vector<utility::string_t> &vc, utility::string_t &enter, queue<size_t> &st);
	void search(utility::string_t &enter);
	void strip_input(utility::string_t &word);
	bool CheckNonAlphaNumber(utility::string_t &row, int index, bool isstart_end = false);
	void parsewikiClass(utility::string_t &ss, size_t &index);
	void print_output(std::map<utility::string_t, std::map<int, std::pair<int, int>>> &dict, vector<utility::string_t> &v, utility::string_t &enter);
private:
	utility::string_t output;
	//The map has string as the key and another map as value.
	//The second map, stores approximate position of the word and start and end of the sentence the word is found in
	//To make search faster for non-common words common words are stored in a separate map.
	std::map<utility::string_t, std::map<int, std::pair<int, int>>> dict;
	std::map<utility::string_t, std::map<int, std::pair<int, int>>> dict_common;
	std::set<utility::string_t> common_words;
	queue<size_t> st;
};
