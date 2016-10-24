#include <cpprest/http_listener.h>
#include <cpprest/filestream.h>
#include <cpprest/json.h>
#include "cpprest/http_client.h"
#include "cpprest/producerconsumerstream.h"
#include <iostream>
#include <sstream>
#include <map>
#include <string>
#include <set>
#include <memory>
#include <array>
#include <locale>
#include <wctype.h>
#include <stack>
#include <queue>
#include <utility>
#include <fcntl.h>
#include <io.h>

using namespace ::pplx;
using namespace web;
using namespace web::http;
using namespace web::http::experimental::listener;
using namespace concurrency::streams;
using namespace web::http::client;
using namespace std;

#include "TAS_Server.h"

void TAS::fill_common_words()
{
	//Some of the more common words in the page
	std::vector<utility::string_t> arr = { L"this", L"was", L"a", L"are", L"the", L"The",
		L"which", L"where", L"when", L"how", L"why", L"at", L"give", L"to", L"for", L"by", L"As", L"as",
		L"with", L"its", L"it", L"was", L"is", L"in", L"of", L"given", L"give", L"that"};
	common_words.insert(arr.begin(), arr.end());
	return;
}

void TAS::addToMap(int pos, int start, int end, utility::string_t &word)
{
	auto iter = dict.find(word);
	if (iter != dict.end()) {
		iter->second.insert(pair<int, pair<int, int>>(pos, pair<int, int>(start, end)));
	}
	else {
		map<int, pair<int, int>> local;
		local.insert(pair<int, pair<int, int>>(pos, pair<int, int>(start, end)));
		dict.insert(pair <utility::string_t, map<int, std::pair<int, int>>>(word, local));
	}
}

void TAS::addToMapCommon(int pos, int start, int end, utility::string_t &word)
{
	auto iter = dict_common.find(word);
	if (iter != dict_common.end()) {
		iter->second.insert(pair<int, pair<int, int>>(pos, pair<int, int>(start, end)));
	}
	else {
		map<int, pair<int, int>> local;
		local.insert(pair<int, pair<int, int>>(pos, pair<int, int>(start, end)));
		dict_common.insert(pair <utility::string_t, map<int, std::pair<int, int>>>(word, local));
	}
}

bool TAS::CheckNonAlphaNumber(utility::string_t &row, int index, bool isstart_end)
{
	bool ret = false;
	switch (row[index])
	{
	case '\'':
	case '!':
	case '[':
	case ']':
	case '\"':
	case '|':
	case '-':
	case '–':
	case '*':
		ret = true;
		break;
	default:
		break;
	}
	return ret;
}

void TAS::strip(utility::string_t &row, size_t &i, bool ignoreDot)
{
	size_t row_index = 0;
	while (row_index < row.size())
	{
		auto c = row[row_index];
		if ((row[row_index] == '\\' && row[row_index + 1] == 'n')) {
			row.erase(row_index, 2);
			continue;
		}
		if (row[row_index] == '[') {
			int row_index_temp = row_index;
			row.erase(row_index, 1);
			if (row[row_index] == '[') {
				row.erase(row_index, 1);
			}
			stack<wchar_t> st;
			//push all the items inbetween [[]] to stack, if found '|', pop everything and delete from the row.
			while (row[row_index_temp] != '|' && row[row_index_temp] != ']') {
				st.push(row_index_temp);
				++row_index_temp;
			}
			if (row[row_index_temp] == '|') {
				while (!st.empty()) {
					row.erase(st.top(), 1);
					st.pop();
				}
			}
		}
		if (CheckNonAlphaNumber(row, row_index) == true) {
			row.erase(row_index, 1);
			continue;
		}
		if (row[row_index] == '.' && ignoreDot == false) {
			break;
		}
		++row_index;
	}
	while (row[0] == ' ') {
		row = row.substr(1);
		++i;
	}
}

void TAS::tokenize(utility::string_t &substr/*, size_t &i, size_t &j*/)
{
	utility::string_t word;
	size_t current = 0;
	std::locale loc;
	while (current <= substr.size()) {
		//auto l = current;
		auto next = substr.find(' ', current);
		if (next == utility::string_t::npos) {
			next = substr.find('.', current);
			if (next == utility::string_t::npos) {
				next = substr.size();
			}
		}
		word = substr.substr(current, next - current);
		while (!word.empty() && word[0] == '.') {
			word = word.substr(1);
		}
		if (word.empty()) {
			current = next + 1;
			continue;
		}
		auto word_copy = word;
		for (size_t word_i = 0; word_i < word.size(); ++word_i) {
			word_copy[word_i] = towlower(word[word_i]);
		}
		while (!word_copy.empty() && !iswalnum(word_copy[0])) {
			word_copy = word_copy.substr(1);
			word = word.substr(1);
		}
		while (!word_copy.empty() && !iswalnum(word_copy[word_copy.size() - 1])) {
			word_copy = word_copy.substr(0, word_copy.size() - 1);
			word = word.substr(0, word.size() - 1);
		}
		if (word.empty()) {
			current = next + 1;
			continue;
		}
		if (common_words.find(word) == common_words.end()) {
			addToMap(output.size() + (current == 0 ? 1 : current),
				output.size(), output.size() + substr.size() - 1, word);
		}
		else {
			addToMapCommon(output.size() + (current == 0 ? 1 : current),
				output.size(), output.size() + substr.size() - 1, word);
		}

		current = next + 1;
	}
}

void TAS::find_start_end(utility::string_t &ss, size_t &index_start, size_t &index_present)
{
	//find the start and end of the line
	index_present = index_start;
	while (index_present < ss.size())
	{
		auto c = ss[index_present];
		if ((ss[index_present] == '\\' && ss[index_present + 1] == 'n')) {
			ss.erase(index_present, 2);
			continue;
		}
		if (CheckNonAlphaNumber(ss, index_present) == true) {
			ss.erase(index_present, 1);
			continue;
		}
		if (ss[index_present] == '.' && ss[index_present + 1] != '.') {
			break;
		}
		++index_present;
	}

	utility::string_t substr = ss.substr(index_start, index_present - index_start + 1);
	while (substr[0] == ' ') {
		substr = substr.substr(1);
		++index_start;
	}
	//start and end of the line are i and j
	//for each word in the range i and j.
	tokenize(substr);
	output += (output.empty()) ? substr : L" " + substr;
}

void TAS::parsewikiClass(utility::string_t &ss, size_t &index)
{
	size_t infobox = ss.find(L"-\\n", index);
	while (infobox != utility::string_t::npos) {
		index = infobox + 1;
		infobox = ss.find(L"-\\n", infobox + 1);
		if (infobox == utility::string_t::npos) {
			infobox = ss.find(L"\\n\\n}", index);
			if (infobox == utility::string_t::npos) {
				std::wcout << "Something's not right" << std::endl;
				return;
			}
		}
		utility::string_t row = ss.substr(index, infobox - index + 1);
		strip(row, index);
		tokenize(row);
		output += (output.empty()) ? row : L" " + row;
		index = infobox + 1;
		infobox = ss.find(L"-\\n", index);
		if (infobox == utility::string_t::npos) {
			infobox = ss.find(L"\\n|}", index);
			if (infobox == utility::string_t::npos) {
				std::wcout << "Something's not right" << std::endl;
				return;
			}
			else {
				utility::string_t t = L"\\n|}";
				row = ss.substr(index, infobox - index);
				strip(row, index);
				tokenize(row);
				output += (output.empty()) ? row : L" " + row;
				index = infobox + t.size();
				break;
			}
		}
	}
}

void TAS::indexify(utility::string_t &ss)
{
	fill_common_words();
	size_t index = 0;
	//avoid infobox and its contents
	auto infobox = ss.find(L"Infobox");
	if (infobox == utility::string_t::npos) {
		//Something is not right...infobox must be present
		std::wcout << "infobox is not present" << std::endl;
		return;
	}
	else {
		infobox = ss.find(L"}}");
		ss = ss.substr(infobox);
	}
	int size = ss.size();
	while (index < ss.size())
	{
		//if (ss[index] == '{' || ss[index] == '[' || (ss[index] == '\\' && ss[index + 1] == 'n') || ss[index] == '}' || ss[index] == '>'
			//|| ss[index] == ' ' || ss[index] == '|' || ss[index] == '!' || ss[index] == ']') {
		if (CheckNonAlphaNumber(ss, index) == true || (ss[index] == '\\' && ss[index + 1] == 'n') ||
			ss[index] == '}' || ss[index] == '>' || ss[index] == '{') {
			if (ss[index] == '\\') {
				ss.erase(index, 2);
			}
			else {
				ss.erase(index, 1);
			}
			continue;
		}
		while (iswspace(ss[index])) {
			ss.erase(index, 1);
		}
		//Check if any jpeg files and ignore the section.
		utility::string_t jpg = L"File";
		if (ss.substr(index, jpg.length()).compare(jpg) == 0) {
			infobox = ss.find(L"]]", index);
			if (infobox != utility::string_t::npos) {
				//This section has a jpeg/jpg file, avoid the entire section.
				index = infobox + 1;
				continue;
			}
		}
		//wikimedia class
		utility::string_t class_t = L"class";
		if (ss.substr(index, class_t.length()).compare(class_t) == 0) {
			parsewikiClass(ss, index);
		}
		if (ss[index] == '<') {
			++index;
			utility::string_t ref = L"ref";
			if (ss.substr(index, ref.length()).compare(ref) == 0) {
				index += ref.length();
				infobox = ss.find(ref, index);
				index = infobox + ref.length();
				continue;
			}
		}
		if (ss[index] == '=') {
			//Header??
			auto equal = index;
			if (ss[equal + 1] != '=') {
				//erase
			}
			while (ss[equal++] == '=');
			utility::string_t nnn = L"\\n";
			infobox = ss.find(nnn, equal);
			auto head = ss.substr(equal - 1, infobox - equal - 1);
			equal -= index + 1;
			strip(head, index);
			tokenize(head);
			output += (output.empty()) ? head : L" " + head;
			index = infobox;
			//for references, ignore all the reflist and the http address...
			if (head.compare(L"References") == 0) {
				infobox = ss.find(L"{{reflist}}");
				if (infobox != utility::string_t::npos) {
					index = infobox + utility::string_t(L"{{reflist}}").length();
					continue;
				}
			}
			if (head.compare(L"External links") == 0) {
				infobox = ss.find(L"[", index);
				if (infobox != utility::string_t::npos) {
					infobox = ss.find(L" ", infobox);
					utility::string_t ext = ss.substr(infobox, ss.find(L"]", infobox) - infobox);
					strip(ext, index);
					tokenize(ext);
					output += (output.empty()) ? ext : L" " + ext;
					index = infobox + ext.length();
					continue;
				}
			}
			utility::string_t nn_start = L"\\n*";
			utility::string_t mm_start = L"\\n\\n*";
			if (ss.substr(index, nn_start.length()).compare(nn_start) == 0 ||
				ss.substr(index, mm_start.length()).compare(mm_start) == 0) {
				//multiple nominations parse
				infobox = ss.find(L"\\n*", index);
				while (infobox != utility::string_t::npos) {
					utility::string_t nn_start_ind = L"\\n*";
					index = infobox + nn_start_ind.length();
					infobox = ss.find(L"\\n*", index);
					if (infobox == utility::string_t::npos) {
						infobox = 0;
						infobox = ss.find(L"\\n\\n}", index);
						if (infobox == utility::string_t::npos) {
							std::wcout << "Something's not right" << std::endl;
							return;
						}
					}
					utility::string_t row = ss.substr(index, infobox - index);
					auto jj = 0;
					strip(row, index);
					jj = index + row.length();
					tokenize(row);
					output += (output.empty()) ? row : L" " + row;

					auto infobox_temp = ss.find(L"\\n*", infobox + nn_start.length());
					//check for next section...if nextsection is less than infobox, this is the last entry in the list
					auto next_section = ss.find(L"==", infobox + nn_start.length());
					if (next_section < infobox_temp) {
						//This is the last entry in the list
						//infobox = ss.find(L"\\n\\n}", i);
						//utility::string_t t = L"\\n\\n";
						index = infobox + nn_start.length();
						row = ss.substr(index, next_section - index);
						strip(row, index);
						jj = index + row.length();
						tokenize(row);
						output += (output.empty()) ? row : L" " + row;
						index = next_section;
						break;
					}
					//not required
					else if (infobox_temp == utility::string_t::npos) {
						index = infobox + nn_start.length();
						infobox = ss.find(L"\\n\\n}", index);
						if (infobox == utility::string_t::npos) {
							std::wcout << "Something's not right" << std::endl;
							return;
						}
						else {
							utility::string_t t = L"\\n\\n";
							row = ss.substr(index, infobox - index);
							strip(row, index);
							jj = index + row.length();
							tokenize(row);
							output += (output.empty()) ? row : L" " + row;
							index = infobox_temp + t.size();
							break;
						}
					}
				}
			}
			continue;
		}
		//find the start and end of the line
		size_t end = index;
		find_start_end(ss, index, end);
		//start and end of the line are i and j
		//for each word in the range i and j.
		index = end + 1;
	}
}

void TAS::print_search_results(json::value const & value)
{
	if (!value.is_null()) {
		const json::value v = value;
		try
		{
			if (v.is_object() || v.is_array())
			{
				if (v.is_object()) {
					for (auto iter = v.as_object().begin(); iter != v.as_object().end(); ++iter)
					{
						if (iter->first == L"*")
						{
							auto ss = iter->second.serialize();
							indexify(ss);
							return;
						}
						if (iter->second.is_array() || iter->second.is_object())
						{
							print_search_results(iter->second);
						}
					}
				}
				else
				{
					//is array
					auto vv = v.as_array();
					for (auto iter = vv.begin(); iter != vv.end(); ++iter)
					{
						if (iter->is_array() || iter->is_object())
						{
							print_search_results(*iter);
						}
					}
				}
			}
		}
		catch (std::exception const & e)
		{
			std::wcout << "Exception occurred " << e.what() << std::endl;
		}
	}
}

void TAS::ExtractTOC(json::value const &value)
{
	if (!value.is_null()) {
		const json::value v = value;
		try
		{
			if (v.is_object() || v.is_array())
			{
				if (v.is_object()) {
					for (auto iter = v.as_object().begin(); iter != v.as_object().end(); ++iter)
					{
						if (iter->first == L"sections")
						{
							ExtractTOC(iter->second);
							return;
						}
						else if (iter->second.is_array() || iter->second.is_object())
						{
							ExtractTOC(iter->second);
						}
					}
				}
				else
				{
					//is array
					auto vv = v.as_array();
					for (auto iter = vv.begin(); iter != vv.end(); ++iter)
					{
						auto toc_num = iter->at(U("number"));
						auto head = iter->at(U("line"));
						auto combine = toc_num.serialize() + L" " + head.serialize();
						size_t i;
						strip(combine, i, true);
						tokenize(combine);
						output += (output.empty()) ? combine : L" " + combine;
					}
				}
			}
		}
		catch (std::exception const & e)
		{
			std::wcout << "Exception occurred " << e.what() << std::endl;
		}
	}

}

pplx::task<void> TAS::HTTPGetTOC()
{
	//https://en.wikipedia.org/w/api.php?action=parse&format=json&prop=sections&page=Filmfare_Award_for_Best_Actor
	http_client client(U("https://en.wikipedia.org/"));
	uri_builder builder(U("/w/api.php"));
	builder.append_query(U("action"), U("parse"));
	builder.append_query(U("page"), U("Filmfare_Award_for_Best_Actor"));
	builder.append_query(U("prop"), U("sections"));
	builder.append_query(U("format"), U("json"));
	return client.request(methods::GET, builder.to_string())
		// Make an HTTP GET request and asynchronously process the response
		// The following code executes when the response is available
		.then([](http_response response) -> pplx::task<json::value>
	{
		std::wostringstream stream;
		stream.str(std::wstring());
		stream << L"Content type: " << response.headers().content_type() << std::endl;
		stream << L"Content length: " << response.headers().content_length() << L"bytes" << std::endl;

		// If the status is OK extract the body of the response into a JSON value
		if (response.status_code() == status_codes::OK)
		{
			return response.extract_json();
		}
		else
		{
			// return an empty JSON value
			return pplx::task_from_result(json::value());
		}
		// Continue when the JSON value is available
	}).then([=](pplx::task<json::value> previousTask)
	{
		// Get the JSON value from the task and call the DisplayJSONValue method
		try
		{
			json::value const & value = previousTask.get();
			ExtractTOC(value);
		}
		catch (http_exception const & e)
		{
			std::wcout << e.what() << std::endl;
		}
	});
}

pplx::task<void> TAS::HTTPGetAsync()
{
	// Create an HTTP request.
	// Encode the URI query since it could contain special characters like spaces.
	http_client client(U("https://en.wikipedia.org/"));
	uri_builder builder(U("/w/api.php"));
	builder.append_query(U("action"), U("query"));
	builder.append_query(U("titles"), U("Filmfare_Award_for_Best_Actor"));
	builder.append_query(U("prop"), U("revisions"));
	builder.append_query(U("rvprop"), U("content"));
	builder.append_query(U("format"), U("json"));
	return client.request(methods::GET, builder.to_string())
		// Make an HTTP GET request and asynchronously process the response
		// The following code executes when the response is available
		.then([](http_response response) -> pplx::task<json::value>
	{
		std::wostringstream stream;
		stream.str(std::wstring());
		stream << L"Content type: " << response.headers().content_type() << std::endl;
		stream << L"Content length: " << response.headers().content_length() << L"bytes" << std::endl;

		// If the status is OK extract the body of the response into a JSON value
		if (response.status_code() == status_codes::OK)
		{
			return response.extract_json();
		}
		else
		{
			// return an empty JSON value
			return pplx::task_from_result(json::value());
		}
		// Continue when the JSON value is available
	}).then([this](pplx::task<json::value> previousTask)
	{
		try
		{
			json::value const & value = previousTask.get();
			print_search_results(value);
		}
		catch (http_exception const & e)
		{
			std::wcout << e.what() << std::endl;
		}
	});
}

void TAS::search_tokenize(vector<utility::string_t> &v, vector<utility::string_t> &vc, utility::string_t &enter, queue<size_t> &st)
{
	size_t k = 0;
	utility::string_t substr = enter;
	while (k <= substr.size()) {
		auto l = k;
		auto f = substr.find(' ', k);
		if (f == utility::string_t::npos) {
			f = substr.find('.', k);
			if (f == utility::string_t::npos) {
				f = substr.size();
			}
			else {
				st.push(f);
			}
		}
		auto word = substr.substr(k, f - k);
		while (!word.empty() && word[0] == '.') {
			word = word.substr(1);
		}
		if (word[word.size() - 1] == '.') {
			word = word.substr(0, word.size() - 1);
			st.push(f-1);
		}
		auto word_copy = word;
		for (size_t word_i = 0; word_i < word.size(); ++word_i) {
			word_copy[word_i] = towlower(word[word_i]);
		}
		while (!word_copy.empty() && !iswalnum(word_copy[0])) {
			word_copy = word_copy.substr(1);
			word = word.substr(1);
		}
		while (!word_copy.empty() && !iswalnum(word_copy[word_copy.size() - 1])) {
			word_copy = word_copy.substr(0, word_copy.size() - 1);
			word = word.substr(0, word.size() - 1);
		}
		if (word.empty()) {
			k = f + 1;
			continue;
		}
		if (common_words.find(word) == common_words.end())
		{
			v.push_back(word);
		}
		else
		{
			vc.push_back(word);
		}
		k = f + 1;
	}
	return;
}

void TAS::strip_input(utility::string_t &word)
{
	if (!word.empty() && towlower(word[0]) < 'a' || towlower(word[0]) > 'z')
	{
		word = word.substr(1);
	}
	if (!word.empty() && towlower(word[word.size()-1]) < 'a' || towlower(word[word.size()-1]) > 'z')
	{
		word = word.substr(1);
	}
}

void TAS::print_output(std::map<utility::string_t, std::map<int, std::pair<int, int>>> &dict, vector<utility::string_t> &v, utility::string_t &enter)
{
	auto itr = dict.find(v[0]);
	if (itr == dict.end())
	{
		wcout << "Phrase not found" << std::endl;
		return;
	}
	else
	{
		for (auto iter = itr->second.begin(); iter != itr->second.end(); ++iter)
		{
			//find v[i] in entered string at if it is not at 0 or after .,
			size_t find = enter.find(v[0]);
			size_t pos = iter->first;
			size_t start = iter->second.first;
			size_t end = iter->second.second;
			find = pos - find - 5; //grace of 5 places
			if (find > pos) {
				find = 0;
			}
			while (find <= (pos + 5)) {
				if (find == output.size()) {
					break;
				}
				if (output[find] != enter[0]) {
				}
				else if (output.substr(find, enter.length()).compare(enter) == 0)
				{
					if (iswspace(output[start])) {
						++start;
						end = end - start + 2;
					}
					else {
						end = end - start + 1;
					}
					//auto t = output.substr(start, end - start + 1);
					auto t = output.substr(start, end);
					if (!st.empty()) {
						auto enter1 = enter.substr(st.front() + 1);
						strip_input(enter1);
						auto prev_f = 0;
						auto f = enter1.find(' ');
						auto str = enter1.substr(prev_f, f);
						prev_f = f;
						strip_input(str);
						while (common_words.find(str) != common_words.end()) {
							f = enter1.find(' ', f + 1);
							str = enter1.substr(prev_f, f - prev_f);
							strip_input(str);
							prev_f = f;
						}
						auto itr1 = dict.find(str);
						if (itr1 == dict.end()) {
							return;
						}
						else {
							for (auto iter1 = itr1->second.begin(); iter1 != itr1->second.end(); ++iter1)
							{
								if (start == iter1->second.second + 1 || end + 1 == iter1->second.first)
								{
									t = t + output.substr(iter1->second.first, iter1->second.second - iter1->second.first + 1);
									wcout << t << std::endl;
								}
							}
						}
						st.pop();
					}
					else {
						wcout << t << std::endl;
						break;
					}
				}
				find++;
			}
		}
		//done with the display
		return;
	}
}

void TAS::search(utility::string_t &enter)
{
	vector<utility::string_t> v;
	vector<utility::string_t> vc, l;
	while (!enter.empty() && (iswspace(enter[0]) || !iswalnum(enter[0]))) {
		enter = enter.substr(1);
	}
	while (!enter.empty() && (iswspace(enter[enter.size()-1]) || !iswalnum(enter[enter.size()-1]))) {
		enter = enter.substr(0, enter.size() - 1);
	}
	if (enter.empty()) {
		wcout << "Phrase not valid" << endl << endl;
		return;
	}
	search_tokenize(v, vc, enter, st);
	if (v.empty()) {
		print_output(dict_common, vc, enter);
	}
	else if (!v.empty())
	{
		print_output(dict, v, enter);
	}
	else {
		wcout << "Phrase not found" << endl << endl;
	}
	while (!st.empty()) {
		st.pop();
	}
}

#ifdef _WIN32
int wmain()
#else
int main()
#endif
{
	try
	{
		_setmode(_fileno(stdout), _O_U16TEXT);
		TAS tas;
		tas.HTTPGetAsync().wait();
		tas.HTTPGetTOC().wait();
		while (true)
		{
			utility::string_t enter;
			std::wcout << "Enter phrase to search....enter quit to exit" << std::endl;
			std::getline(std::wcin, enter);

			if (enter.compare(L"quit") == 0) {
				break;
			}
			if (enter.empty())
			{
				continue;
			}
			std::wcout << "The respective phrases are ----START" << std::endl << std::endl << std::endl;
			tas.search(enter);
			std::wcout << "The respective phrases are ----END" << std::endl << std::endl << std::endl;
		}
	}
	catch (exception const & e)
	{
		wcout << e.what() << endl;
	}
}