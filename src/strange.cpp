#include <fstream>
#include <vector>
#include <string>
#include <string.h>
#include <iostream>

#include "autopatterns.hpp"

typedef AutoPatterns<char> AutoPatternsC;

static bool TrimLine(std::string &line)
{
	while (!line.empty() && (line.back() == '\r' || line.back() == '\n'
			|| line.back() == ' ' || line.back() == '\t')) {
		line.pop_back();
	}
	while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) {
		line.erase(line.begin());
	}
	return !line.empty();
}


struct LoadLines : std::vector<std::string>
{
	LoadLines(const char *file)
	{
		std::ifstream infile(file);
		std::string line;
		while (std::getline(infile, line)) if (TrimLine(line)) {
			emplace_back(line);
		}
	}
};

static void PrintMatchingLine(bool color, const std::string &line)
{
	if (color) {
		std::cout << "\033[32m" << line << "\033[m" << std::endl;

	} else {
		std::cout << ' ' << line << std::endl;
	}
}

static void PrintMismatchingLine(AutoPatternsC::TriePtr &t, bool descript, bool color, const std::string &line)
{
	if (!color) {
		std::cout << '!';
	}

	if (descript) {
		const auto &sd = t->Descript(line);
		char status_fin_char = -1;
		for (const auto &td : sd) {
			switch (td.status) {
				case AutoPatternsC::TS_MATCH:
					if (color && status_fin_char) {
						std::cout << "\033[1;32m";
					} else if (status_fin_char > 0) {
						std::cout << status_fin_char;
					}
					status_fin_char = 0;
					break;
				case AutoPatternsC::TS_MISMATCH:
					if (color && status_fin_char != ']') {
						std::cout << "\033[1;33m";
					} else if (status_fin_char != ']') {
						if (status_fin_char > 0) {
							std::cout << status_fin_char;
						}
						std::cout << '[';
					}
					status_fin_char = ']';
					break;
				case AutoPatternsC::TS_REDUNDANT:
					if (color && status_fin_char != '>') {
						std::cout << "\033[1;31m";
					} else if (status_fin_char != '>') {
						if (status_fin_char > 0) {
							std::cout << status_fin_char;
						}
						std::cout << '<';
					}
					status_fin_char = '>';
					break;
				case AutoPatternsC::TS_MISSING:
					if (color && status_fin_char != ')') {
						std::cout << "\033[1;31m";
					} else if (status_fin_char != ')') {
						if (status_fin_char > 0) {
							std::cout << status_fin_char;
						}
						std::cout << '(';
					}
					status_fin_char = ')';
					break;
			}
			if (td.status != AutoPatternsC::TS_MISSING) {
				std::cout << td.token;
			} else {
				std::cout << "\xE2\x80\xA2"; // '?';//
			}
		}
		if (color) {
			std::cout << "\033[m";

		} else if (status_fin_char > 0) {
			std::cout << status_fin_char;
		}


	} else if (color) {
		std::cout << "\033[1;33m" << line << "\033[m";

	} else {
		std::cout << line;
	}

	std::cout << std::endl;
}

template <class IStream>
	static void EvalStream(AutoPatternsC::TriePtr &t, IStream &is, size_t context, bool descript, bool color)
{
	std::string line;
	std::list<std::string> context_matching_backlog;
	size_t context_matching_countdown = 0;
	for (size_t index = 0; std::getline(is, line); ++index) if (TrimLine(line)) {
		if (!t->Match(line)) {
			if (context != 0 && context != std::string::npos) {
				context_matching_countdown = context;
				if (!context_matching_backlog.empty()) {
					std::cout << std::endl;
					for (const auto &matching_line : context_matching_backlog) {
						PrintMatchingLine(color, matching_line);
					}
				}
			}
			PrintMismatchingLine(t, descript, color, line);
			context_matching_backlog.clear();

		} else if (context == 0) {
			;

		} else if (context == std::string::npos) {
			PrintMatchingLine(color, line);

		} else if (context_matching_countdown) {
			PrintMatchingLine(color, line);
			--context_matching_countdown;

		} else {
			context_matching_backlog.emplace_back(line);
			if (context_matching_backlog.size() > context) {
				context_matching_backlog.pop_front();
			}
		}
	}
}

int main(int argc, char **argv)
{
	AutoPatternsC::TriePtr t;
	int out = 0;
	size_t context = 0;
	bool descript = false, color = false;
	for (int i = 1; i < argc; ++i) {
		const char *argvi = argv[i];
		while (argvi[0] == '-' && argvi[1] == '-') {
			++argvi;
		}

		if (strcmp(argvi, "-descript") == 0) {
			descript = true;

		} else if (strcmp(argvi, "-color") == 0) {
			color = true;

		} else if (strstr(argvi, "-context=") == argvi) {
			const char *val = strchr(argvi, '=') + 1;
			context = strcasecmp(val, "ALL") ? atoi(val) : (size_t)-1;

		} else if (strcmp(argvi, "-eval") == 0) {
			if (!t) {
				std::cerr << "ERROR: No trie for eval" << std::endl;
				out = -1;
				continue;
			}

			EvalStream(t, std::cin, context, descript, color);

		} else if (strstr(argvi, "-eval=") == argvi) {
			if (!t) {
				std::cerr << "ERROR: No trie for eval" << std::endl;
				out = -1;
				continue;
			}
			std::ifstream infile(strchr(argvi, '=') + 1);
			EvalStream(t, infile, context, descript, color);

		} else if (strstr(argvi, "-learn=") == argvi) {
			LoadLines lines(strchr(argvi, '=') + 1);
			if (!t) {
				t.reset(new AutoPatternsC::Trie);
			}
			t->Learn(lines);

		} else if (strstr(argvi, "-load=") == argvi) {
			const char *fpath = strchr(argvi, '=') + 1;
			std::ifstream is(fpath);
			if (t) {
				std::cerr << "WARNING: Load dismisses previous trie" << std::endl;
			}
			try {
				t.reset(new AutoPatternsC::Trie(is));
			} catch (std::exception &e) {
				std::cerr << e.what() << " while loading " << fpath << std::endl;
			}

		} else if (strstr(argvi, "-save=") == argvi
				|| strstr(argvi, "-save-compact=") == argvi) {
			if (!t) {
				std::cerr << "ERROR: No trie for save" << std::endl;
				continue;
			}

			std::ofstream os(strchr(argvi, '=') + 1);
			t->Save(os, strstr(argvi, "-save-compact=") == argvi);

		} else if (*argvi) {
			if (strstr(argvi, "-help") != argvi) {
				std::cerr << "Bad argument: " << argvi << std::endl;
			}
			std::cerr << "Strange Tool v0.1 BETA" << std::endl;
			std::cerr << "Usage: " << argv[0]
				<< " [-load=TRIE_FILE] [-learn=SAMPLES_FILE] [-descript] [-color] [-context=#] [-eval=SAMPLES_FILE] [-eval] [-save=TRIE_FILE] [-save-compact=TRIE_FILE]"
					<< std::endl;
			std::cerr << "Operations are executed in exactly same order as specified by command line." << std::endl;
			std::cerr << "Operations description:" << std::endl;
			std::cerr << "  -load= loads ready to use patterns from specified trie file. Loading discards any already existing in memory patterns (from previous load or learn operations)." << std::endl;
			std::cerr << "  -learn= learns samples from specified text file. If there're some already existing patterns in memory - learning will incrementally extend them, without discarding." << std::endl;
			std::cerr << "  -descript enable detailed description of anomal lines found by -eval operation (slow and experimental)." << std::endl;
			std::cerr << "  -color denote by colors anomalies found by -eval operation." << std::endl;
			std::cerr << "  -context= make -eval operation to print # numberlines before and after only mismatched line. If # is ALL then everything will be printed." << std::endl;
			std::cerr << "  -eval= evaluates samples from specified text file and prints to stdout results" << std::endl;
			std::cerr << "  -eval evaluates samples written to standard input, behaves same as --eval=..." << std::endl;
			std::cerr << "  -save= saves existing in memory patterns into specified trie file with indentation for better readablity." << std::endl;
			std::cerr << "  -save-compact= saves existing in memory patterns into specified trie file in compact form to save space." << std::endl;
			out = -2;
			break;
		}
	}

//for (;;);
	return out;
}
