#include <fstream>
#include <vector>
#include <string>
#include <string.h>
#include <iostream>

#include "autopatterns.hpp"

#ifndef VERINFO
# define VERINFO "???"
#endif

#define ANSI_RED        "\033[31m"
#define ANSI_RED_HI     "\033[1;31m"
#define ANSI_GREEN      "\033[32m"
#define ANSI_GREEN_HI	"\033[1;32m"
#define ANSI_YELLOW     "\033[33m"
#define ANSI_YELLOW_HI  "\033[1;33m"
#define ANSI_DEFAULT    "\033[m"

typedef AutoPatterns<char> AutoPatternsC;

static bool TrimLine(std::string &line)
{
	while (!line.empty() && (line.back() == '\r' || line.back() == '\n'
			|| line.back() == ' ' || line.back() == '\t')) {
		line.pop_back();
	}
	while (!line.empty() && (line.front() == ' ' || line.front() == '\t' || line.front() == '\r')) {
		line.erase(line.begin());
	}
	return !line.empty();
}


struct LoadLines : std::vector<std::string>
{
	template <class IStream>
		bool Load(IStream &is)
	{
		std::string line;
		while (std::getline(is, line)) if (TrimLine(line)) {
			emplace_back(line);
		}
		return true;
	}
};

class Commander
{
	AutoPatternsC::TriePtr _t;
	size_t _context = 0;
	int _exit_code = 0;
	bool _descript = false;
	bool _color = false;

	enum ExitCodeBit
	{
		ECB_ANOMALY = 0x01,
		ECB_READ_ERROR = 0x02,
		ECB_WRITE_ERROR = 0x04,
		ECB_CMDLINE_ERROR = 0x20,
		ECB_UNSPECIFIED_ERROR = 0x40,
	};

	void ToggleExitCode(ExitCodeBit bit)
	{
		_exit_code|= bit;
	}

	bool CheckOperandsCount(const std::string &cmd, int expected_count, int operands_count)
	{
		if (expected_count == operands_count) {
			return true;
		}

		ToggleExitCode(ECB_CMDLINE_ERROR);
		std::cerr << "Wrong operands count for " << cmd << ": " << std::dec << operands_count << std::endl;
		return false;
	}

	////////

	void PrintMatchingLine(const std::string &line)
	{
		if (_color) {
			std::cout << ANSI_GREEN << line << ANSI_DEFAULT << std::endl;
		} else {
			std::cout << ' ' << line << std::endl;
		}
	}

	void PrintMismatchingLine(const std::string &line)
	{
		if (!_color) {
			std::cout << '!';
		}

		if (_descript) {
			const auto &sd = _t->Descript(line);
			char status_fin_char = -1;
			for (const auto &td : sd) {
				switch (td.status) {
					case AutoPatternsC::TS_MATCH:
						if (_color && status_fin_char) {
							std::cout << ANSI_GREEN_HI;
						} else if (status_fin_char > 0) {
							std::cout << status_fin_char;
						}
						status_fin_char = 0;
						break;
					case AutoPatternsC::TS_MISMATCH:
						if (_color && status_fin_char != ']') {
							std::cout << ANSI_YELLOW_HI;
						} else if (status_fin_char != ']') {
							if (status_fin_char > 0) {
								std::cout << status_fin_char;
							}
							std::cout << '[';
						}
						status_fin_char = ']';
						break;
					case AutoPatternsC::TS_REDUNDANT:
						if (_color && status_fin_char != '>') {
							std::cout << ANSI_RED_HI;
						} else if (status_fin_char != '>') {
							if (status_fin_char > 0) {
								std::cout << status_fin_char;
							}
							std::cout << '<';
						}
						status_fin_char = '>';
						break;
					case AutoPatternsC::TS_MISSING:
						if (_color && status_fin_char != ')') {
							std::cout << ANSI_RED_HI;
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
			if (_color) {
				std::cout << ANSI_DEFAULT;
			} else if (status_fin_char > 0) {
				std::cout << status_fin_char;
			}


		} else if (_color) {
			std::cout << ANSI_YELLOW_HI << line << ANSI_DEFAULT;

		} else {
			std::cout << line;
		}

		std::cout << std::endl;
	}

	template <class IStream>
		void EvalStream(IStream &is)
	{
		std::string line;
		std::list<std::string> context_matching_backlog;
		size_t context_matching_countdown = 0;
		for (size_t index = 0; std::getline(is, line); ++index) if (TrimLine(line)) {
			if (!_t->Match(line)) {
				ToggleExitCode(ECB_ANOMALY);
				if (_context != 0 && _context != std::string::npos) {
					context_matching_countdown = _context;
					if (!context_matching_backlog.empty()) {
						for (const auto &matching_line : context_matching_backlog) {
							PrintMatchingLine(matching_line);
						}
					}
				}
				PrintMismatchingLine(line);
				context_matching_backlog.clear();

			} else if (_context == 0) {
				;

			} else if (_context == std::string::npos) {
				PrintMatchingLine(line);

			} else if (context_matching_countdown) {
				PrintMatchingLine(line);
				--context_matching_countdown;
				if (context_matching_countdown == 0) {
					std::cout << std::endl;
				}

			} else {
				context_matching_backlog.emplace_back(line);
				if (context_matching_backlog.size() > _context) {
					context_matching_backlog.pop_front();
				}
			}
		}
	}


	void ExecuteInner(const std::string &cmd, char **operands, int operands_count)
	{
		if (cmd == "descript") {
			_descript = true;
			CheckOperandsCount(cmd, 0, operands_count);

		} else if (cmd == "color") {
			_color = true;
			CheckOperandsCount(cmd, 0, operands_count);

		} else if (cmd == "context") {
			if (operands_count == 0) {
				_context = 3;

			} else if (CheckOperandsCount(cmd, 1, operands_count)) {
				_context = strcasecmp(*operands, "ALL") ? atoi(*operands) : (size_t)-1;
			}

		} else if (cmd == "eval") {
			if (!_t) {
				ToggleExitCode(ECB_CMDLINE_ERROR);
				std::cerr << "No trie for " << cmd << std::endl;

			} else if (operands_count == 0) {
				EvalStream(std::cin);

			} else for (int i = 0; i < operands_count; ++i) {
				std::ifstream is(operands[i]);
				if (!is.is_open()) {
					ToggleExitCode(ECB_READ_ERROR);
					std::cerr << "Can't open: " << operands[i] << std::endl;
				} else {
					EvalStream(is);
				}
			}

		} else if (cmd == "learn") {
			if (!_t) {
				_t.reset(new AutoPatternsC::Trie);
			}
			LoadLines lines;
			if (operands_count == 0) {
				lines.Load(std::cin);

			} else for (int i = 0; i < operands_count; ++i) {
				std::ifstream is(operands[i]);
				if (!is.is_open()) {
					ToggleExitCode(ECB_READ_ERROR);
					std::cerr << "Can't open: " << operands[i] << std::endl;
				} else {
					lines.Load(is);
				}
			}
			_t->Learn(lines);

		} else if (cmd == "load") {
			if (_t) {
				std::cerr << "WARNING: Load dismisses previous trie" << std::endl;
			}
			if (operands_count == 0) {
				_t.reset(new AutoPatternsC::Trie(std::cin));

			} else {
				CheckOperandsCount(cmd, 1, operands_count);
				std::ifstream is(operands[0]);
				if (!is.is_open()) {
					ToggleExitCode(ECB_READ_ERROR);
					std::cerr << "Can't open: " << operands[0] << std::endl;
				} else {
					_t.reset(new AutoPatternsC::Trie(is));
				}
			}

		} else if (cmd == "save" || cmd == "save-compact") {
			if (!_t) {
				ToggleExitCode(ECB_CMDLINE_ERROR);
				std::cerr << "No trie for " << cmd << std::endl;

			} else if (operands_count == 0) {
				_t->Save(std::cout, cmd == "save-compact");

			} else for (int i = 0; i < operands_count; ++i) {
				std::ofstream os(operands[i]);
				if (!os.is_open()) {
					ToggleExitCode(ECB_WRITE_ERROR);
					std::cerr << "Can't create:" << operands[i] << std::endl;
				} else {
					_t->Save(os, cmd == "save-compact");
				}
			}

		} else {
			if (cmd != "help") {
				std::cerr << "Bad argument: " << cmd << std::endl;
				ToggleExitCode(ECB_CMDLINE_ERROR);
			}
			PrintUsage();
		}
	}

	void PrintUsage()
	{
		std::cerr << "Strange Tool by strangeCamel, BETA " << VERINFO << std::endl;
		std::cerr << "Usage: strange"
			<< " [-load TRIE_FILE] [-learn SAMPLES_FILE1 [SAMPLES_FILE2..]] [-descript] [-color] [-context [#]] [-eval SAMPLES_FILE1 [SAMPLES_FILE2..]] [-save TRIE_FILE] [-save-compact TRIE_FILE]"
				<< std::endl;
		std::cerr << "Operations are executed in exactly same order as specified by command line." << std::endl;
		std::cerr << "Operations description:" << std::endl;
		std::cerr << "  -load loads ready to use patterns from specified trie file. Loading discards any already existing in memory patterns (from previous load or learn operations)." << std::endl;
		std::cerr << "  -learn learns samples from specified text file(s) or stdin if no files specified. If there're some already existing patterns in memory - learning will incrementally extend them, without discarding." << std::endl;
		std::cerr << "  -descript enables per-token description of anomal lines found by -eval operation (can be slow)." << std::endl;
		std::cerr << "  -color enables using of ASCII colors in output of -eval operation." << std::endl;
		std::cerr << "  -context makes -eval operation to print # number of lines before and after each mismatched line. If # is ALL then everything will be printed. If # is omitted - then its defaulted to 3 lines." << std::endl;
		std::cerr << "  -eval evaluates samples from specified text file(s) and prints results to stdout." << std::endl;
		std::cerr << "  -save saves existing in memory patterns into specified trie file with indentation for better readablity." << std::endl;
		std::cerr << "  -save-compact saves existing in memory patterns into specified trie file in compact form to save space." << std::endl;
		std::cerr << "Exit code composed of following bits:" << std::endl;
		std::cerr << std::dec;
		std::cerr << "  " << ECB_ANOMALY << " if -eval used and found anomalies" << std::endl;
		std::cerr << "  " << ECB_READ_ERROR << " if failed to read some files" << std::endl;
		std::cerr << "  " << ECB_WRITE_ERROR << " if failed to write some files" << std::endl;
		std::cerr << "  " << ECB_CMDLINE_ERROR << " in case of bad command line arguments" << std::endl;
		std::cerr << "  " << ECB_UNSPECIFIED_ERROR << " in case of any other failure" << std::endl;
	}

public:
	void Execute(const std::string &cmd, char **operands, int operands_count)
	{
		try {
			ExecuteInner(cmd, operands, operands_count);

		} catch (std::exception &e) {
			ToggleExitCode(ECB_UNSPECIFIED_ERROR);
			std::cerr << "Error in '" << cmd << "': " << e.what() << std::endl;
		}
	}

	int ExitCode() const
	{
		return _exit_code;
	}
};

int main(int argc, char **argv)
{
	Commander c;
	int last_arg_cmd = 0;
	std::string cmd;
	if (argc <= 1) {
		c.Execute("help", nullptr, 0);

	} else for (int i = 1; i <= argc; ++i) {
		const char *argvi = argv[i];
		if (i >= argc || argv[i][0] == '-') {
			if (last_arg_cmd != 0) {
				char *arg_cmd = argv[last_arg_cmd];
				while (*arg_cmd == '-') {
					++arg_cmd;
				}
				char *eq = strchr(arg_cmd, '=');
				if (eq) {
					cmd.assign(arg_cmd, eq - arg_cmd);
					argv[last_arg_cmd] = eq + 1;
					c.Execute(cmd, &argv[last_arg_cmd], i - last_arg_cmd);
				} else {
					cmd.assign(arg_cmd);
					c.Execute(cmd, &argv[last_arg_cmd + 1], i - (last_arg_cmd + 1));
				}

			}
			last_arg_cmd = i;
		}
	}

	return c.ExitCode();
}
