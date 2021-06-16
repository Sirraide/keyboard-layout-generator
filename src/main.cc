#include <cctype>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fmt/format.h>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <regex>
#include <string>
#include <utility>

#define RED "\u001b[31m"
#define RESET "\u001b[0m"

#define repeat(N_) for (size_t iter__ = 0, N__ = N_; iter__ < N__; iter__++)

using namespace std;

int	   levels = 0;
string ifname, ofname;

void wrred(string str) {
	cerr << RED << str << RESET << "\n";
}

void fatal(string message) {
	wrred(string("Error: ") + message);
	exit(1);
}

void fatal(function<void(void)> lambda, string message) {
	lambda();
	fatal(message);
}

void PrintUsage() {
	cerr << "Usage: gen [<file> -<number> | -e] -o <ofile>\n"
			"\t<number> of levels must be a power of 2 that is >= 4.\n"
			"\t-e generates an empty keymap template\n\n"
			"\t' ', ',', ';', '\\t', '\\r', '\\n' must be escaped,\n"
			"\tuse 'space', 'comma', 'semicolon', 'tab', 'cr', 'lf' instead\n\n";
	exit(1);
}

bool option_e = false;

void HandleArguments(int argc, char** argv) {
	if (argc == 1) fatal("No input file specified.");

	int	 i		 = 1;
	auto advance = [&] {
		if (i >= argc) PrintUsage();
		i++;
	};

	for (; i < argc; i++) {
		switch (argv[i][0]) {
			case '-': {
				switch (argv[i][1]) {
					case 0:
						fatal(string("Unrecognised option ") + argv[i]);
					case '0' ... '9':
						levels = atoi(argv[i] + 1);
						if (levels < 4 || levels % 2) PrintUsage();
						break;
					case 'o':
						advance();
						ofname = argv[i];
						break;
					case 'e':
						option_e = true;
						break;
					default:
						PrintUsage();
				}
			} break;
			default:
				if (!ifname.empty()) PrintUsage();
				ifname = argv[i];
		}
	}

	if (ifname.empty() && !option_e) fatal("No input file specified.");
	if (!levels) levels = 4;
	if (ofname.empty()) ofname = "a.generated.keymap";
}

void SkipWhitespace(wstring::iterator& it, const wstring::iterator& end) {
	while (it != end && iswspace(*it)) it++;
}

wstring CollectChars(wstring::iterator& it, const wstring::iterator& end) {
	SkipWhitespace(it, end);
	wstring buf;
	while (it != end && !iswspace(*it) && *it != L',' && *it != L';') buf += *(it++);
	return buf;
}

const wregex key = wregex(L"(?:[ \t\r\n]*?A?[EDCBedcb]\\d+?(?:[ \t\r\n]*?([^;,])*?[ \t\r\n]*?,)*?[ \t\r\n]*?([^;,])*?[ \t\r\n]*?;)|(^\\s*?$)+");

bool err = false;

#define lineerror(lineno, errmsg)                                    \
	{                                                                \
		wrred("Error in line " + to_string(lineno) + ": " + errmsg); \
		err = true;                                                  \
		continue;                                                    \
	}

bool iswspace(const wstring& str) {
	for (auto c : str)
		if (!iswspace(c)) return false;
	return true;
}

template <typename T, typename A>
wostream& operator<<(wostream& os, const vector<T, A>& v) {
	size_t s = 0;
	for (size_t len = v.size(); s < len - 1; ++s)
		os << v[s] << ", ";
	os << v[s];
	return os;
}

struct Entry {
	wstring			row;
	long			column;
	vector<wstring> chars;

	Entry(wstring row, long column, vector<wstring>&& chars)
		: row(row), column(column), chars(chars) {}
	Entry(wstring row, long column, const vector<wstring>& chars)
		: row(row), column(column), chars(chars) {}
};

wstring ustring(const wstring& str) {
	wstring ret;
	for (auto c : str)
		ret += fmt::format(L"U{:04X}", c);
	return ret;
}

void EmitKeymap(wofstream& ofile, const vector<Entry>& entries) {
	ofile << L"default partial\nxkb_symbols \"basic\" {\n";
	for (auto& e : entries) {
		if (e.row == L"[[newline]]") {
			ofile << L"\n";
			continue;
		}
		ofile << L"\tkey <";
		if (e.column)
			ofile << L"A" << e.row << ((e.column < 10) ? L"0" : L"") << e.column;
		else /* handle TLDE, BKSL */
			ofile << e.row;
		ofile << L">  { [" << e.chars << L"] };\n";
	}
	ofile << L"};\n";
}

int main(int argc, char** argv) {
	setlocale(LC_ALL, "en_GB.UTF-8");

	HandleArguments(argc, argv);

	wofstream	  ofile{ofname};
	vector<Entry> entries;

	if (option_e) {
		vector<wstring> vec;
		repeat(levels)
			vec.push_back(L"NoSymbol");

		repeat(12)
			entries.push_back({L"E", static_cast<long>(iter__ + 1), vec});

		entries.push_back({L"[[newline]]", 0, {}});

		repeat(12)
			entries.push_back({L"D", static_cast<long>(iter__ + 1), vec});

		entries.push_back({L"[[newline]]", 0, {}});

		repeat(11)
			entries.push_back({L"C", static_cast<long>(iter__ + 1), vec});
		entries.push_back({L"TLDE", 0, vec});

		entries.push_back({L"[[newline]]", 0, {}});

		entries.push_back({L"BKSL", 0, vec});
		repeat(10)
			entries.push_back({L"B", static_cast<long>(iter__ + 1), vec});

		EmitKeymap(ofile, entries);
		return 0;
	}

	FILE* infile = fopen(ifname.c_str(), "r");

	wstring ibuf, obuf;
	wchar_t c = L' ';

	int lineno = 1;

	while (!feof(infile)) {
		ibuf.clear();
		if (!feof(infile))
			while (c != L';') {
				c = fgetwc(infile);
				if (c == L'\n') lineno++;
				if (!feof(infile)) ibuf += c;
				else
					break;
			}
		if (ibuf.empty() || iswspace(ibuf)) {
			if (!feof(infile)) {
				c = fgetwc(infile);
				if (c == L'\n') lineno++;
				continue;
			}
			goto _write;
		}

		if (c != L';') lineerror(lineno, "Incomplete declaration.");
		c = fgetwc(infile);
		if (c == L'\n') lineno++;

		if (!regex_match(ibuf, key)) lineerror(lineno, "Syntax error");

		bool empty = true;
		for (auto c : ibuf) {
			if (!isspace(c)) {
				empty = false;
				break;
			}
		}
		if (empty) continue;

		auto it	 = ibuf.begin();
		auto end = ibuf.end();
		SkipWhitespace(it, end);
		if (*it == L'A' || *it == L'a') it++;
		wchar_t cls = *(it++);
		long	which;
		wstring numbuffer;
		while (isdigit(*it)) numbuffer += *(it++);
		which = wcstol(numbuffer.c_str(), nullptr, 10);

		SkipWhitespace(it, end);

		vector<wstring> chars;

		for (;;) {
			chars.push_back(ustring(CollectChars(it, end)));
			if (*(it++) == L';') break;
		}

		for (auto& el : chars)
			if (el.empty()) el = L"NoSymbol";
		entries.push_back({wstring() + static_cast<wchar_t>(towupper(cls)), which, move(chars)});
	}
_write:
	if (err) return 0;
	EmitKeymap(ofile, entries);
}