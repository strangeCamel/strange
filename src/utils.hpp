#pragma once
#include <math.h>
#include <unordered_set>

struct AutoPatternsUtils
{
static constexpr size_t RandomCountThreshold = 4;
static constexpr double RandomDeltaCaseThreshold = 0.2;
static constexpr double RandomEntropyThreshold = 0.85; //1.0 is 'ideally random'

static inline const char *Monthes()
{
	return
		"jan\0january\0"
		"feb\0february\0"
		"mar\0march\0"
		"apr\0april\0"
		"may\0"
		"jun\0june\0"
		"jul\0july\0"
		"aug\0august\0"
		"sep\0september\0"
		"oct\0october\0"
		"nov\0november\0"
		"dec\0december\0\0";
}

static inline const char *WeekDays()
{
	return
		"sun\0sunday\0"
		"mon\0monday\0"
		"tue\0tuesday\0"
		"wed\0wednesday\0"
		"thu\0thursday\0"
		"fri\0friday\0"
		"sat\0saturday\0\0";
}

////////////

typedef unsigned int StringClass;


enum StringClassFeats
{	// alphanum feats, order matters - wider class should be before narrower!
	SCF_NO_ALNUM = 0,
	SCF_ALPHADEC,
	SCF_DIGITS_HEXADECIMAL,
	SCF_DIGITS_DECIMAL,

	SCF_MASK_ALNUM     = 0x0000000f,

	SCF_SPACES         = 0x00000010, // string contains whitespace characters
	SCF_PUNCTUATION    = 0x00000020, // string contains punctuation characters

	SCF_UNSPECIFIED    = 0x00001000, // string contains characters beside alnum spaces and punctuation

	SCF_RANDOM         = 0x00002000, // string looks as randomly generated sequence
	SCF_WEEKDAY        = 0x00004000, // string represents some week day name
	SCF_MONTH          = 0x00004000, // string represents some month name

	SCF_MASK_OTHER     = 0xfffffff0,

	SCF_INVALID        = 0xffffffff
};

////////////
template <class VectorT>
	static void SortAndUniq(VectorT &v)
{
	std::sort(v.begin(), v.end());
	auto new_end = std::unique(v.begin(), v.end());
	v.resize(new_end - v.begin());
}


// some heuristics that returns true if incoming string looks as randomly generated sequence
// like session ID etc
template <class StringT>
	static bool IsRandomAlphaNums(const StringT &s)
{
	size_t freqs[(1 + 'z' - 'a') + (1 + 'Z' - 'A') + (1 + '9' - '0') ]{};
	size_t cnt_relevant = 0, cnt_locase = 0, cnt_upcase = 0;
	bool has_not_hexadecimals = false, has_decimals = false;
	for (const auto &c : s) {
		if (c >= 'a' && c <= 'z') {
			if (c > 'f') {
				has_not_hexadecimals = true;
			}
			++cnt_locase;
			++cnt_relevant;
			++freqs[(size_t)(c - 'a')];

		} else if (c >= 'A' && c <= 'Z') {
			if (c > 'F') {
				has_not_hexadecimals = true;
			}
			++cnt_upcase;
			++cnt_relevant;
			++freqs[(size_t)(c - 'A') + (1 + 'z' - 'a')];

		} else if (c >= '0' && c <= '9') {
			has_decimals = true;
			++cnt_relevant;
			++freqs[(size_t)(c - '0') + (1 + 'z' - 'a') + (1 + 'Z' - 'A')];
		}
	}

	if (cnt_relevant < RandomCountThreshold) {
		return false;
	}

	// guessed span of unique values
	size_t span = 0;
	if (cnt_locase != 0) {
		span+= has_not_hexadecimals ? 26 : 6;
	}
	if (cnt_upcase != 0) {
		span+= has_not_hexadecimals ? 26 : 6;
	}

	if (cnt_locase != 0 && cnt_upcase != 0) {
		// in true random locase and upcase are approx same amount
		size_t cnt_bothcase = cnt_locase + cnt_upcase;
		size_t delta_case = (cnt_locase > cnt_upcase)
			? cnt_locase - cnt_upcase : cnt_upcase - cnt_locase;
		double norm_delta_case = ((double)delta_case / (double)cnt_bothcase);
		// some correction to make check more relaxed on short sequences...
		norm_delta_case/= (1.0 + (((double)(span/4) / ((double)(span/4) + (double)cnt_bothcase))));
//std::cerr << "norm_delta_case: " << norm_delta_case << std::endl;
		if (norm_delta_case > RandomDeltaCaseThreshold) {
			return false;
		}
	}

	if (has_decimals) {
		span+= 10;
	}
	if (span == 0) {
		return false;
	}

	// sort-of Shannon entropy estimation
	double entropy = 0;
	for (const auto &freq : freqs) if (freq) {
		const double v = (double)freq / (double)cnt_relevant;
		entropy+= -v * log2(v);
	}
	size_t span_redundancy = cnt_relevant / span;
	if (span_redundancy * span < cnt_relevant) {
		++span_redundancy;
	}

	entropy/= log2(cnt_relevant / (double)span_redundancy);
//std::cerr << std::endl << "Entropy:" << entropy << std::endl;

	return entropy > RandomEntropyThreshold;
}

template <class CharT>
	static bool IsPunctuation(CharT c)
{
	return (c == '=' || c == '+' || c == '-' || c == '*' || c == '/' || c == '%'
		|| c == ',' || c == '.' || c == '!' || c == '?'
		|| c == '$' || c == '&' || c == '#' || c == '^' || c == '|'
		|| c == '(' || c == ')' || c == '{' || c == '}' || c == '[' || c == ']'
		|| c == ':' || c == ';');
}

template <class CharT>
	static bool IsSpace(CharT c)
{
	return (c == ' ' || c == '\t');
}

template <class CharT>
	static bool IsAlpha(CharT c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || ((unsigned int)c) >= 0x80;
}

template <class CharT>
	static bool IsDec(CharT c)
{
	return (c >= '0' && c <= '9');
}

template <class CharT>
	static bool IsHex(CharT c)
{
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

template <class CharT>
	static bool IsAlphaDec(CharT c)
{
	return IsAlpha(c) || IsDec(c);
}

template <class CharT>
	static bool IsEOL(CharT c)
{
	if (c == '\n') {
		return true;
	}
#ifdef __WIN32
	if (c == '\r') {
		return true;
	}
#endif
	return false;
}

template <class IntT, class StringT>
	static IntT ParseDecAsInt(const StringT &s, size_t &pos)
{
	IntT out = 0;
	bool neg = false;
	if (((IntT)-1) < 0 && pos < s.size() && s[pos] == '-') {
		neg = true;
		++pos;
	}
	for (; pos < s.size(); ++pos) {
		const auto &c = s[pos];
		if (c >= '0' && c <= '9') {
			out*= 10;
			out+= c - '0';
		} else {
			break;
		}
	}

	return neg ? -out : out;
}

template <class StringT>
	static bool SkipNonAlphaNum(const StringT &s, size_t &pos)
{
	while (pos < s.size() && !IsAlphaDec(s[pos])) {
		++pos;
	}
	return pos < s.size();
}


template <class String>
	static bool MatchesAnyOfWords(const String &s, const char *words)
{
	for (size_t i = 0, j = 0;;) {
		if (!words[i]) {
			const size_t l = i - j;
			if (l == 0) {
				return false;
			}
			if (s.size() == l) {
				for (size_t k = 0;; ++k) {
					if (k == l) {
						return true;
					}
					auto sc = s[k];
					if (sc >= 'A' && sc <= 'Z') {
						sc+= 'a' - 'A';
					}
					if (sc != words[j + k]) {
						break;
					}
				}
			}
			j = ++i;
		} else {
			++i;
		}
	}
}

// Does not check for randomness cuz its rather slow,
// use IsRandomAlphaNums to detect SCF_RANDOM when really needed
template <class StringT>
	static StringClass ClassifyString(const StringT &s)
{
	if (MatchesAnyOfWords(s, WeekDays())) {
		return SCF_ALPHADEC | SCF_WEEKDAY;
	}
	if (MatchesAnyOfWords(s, Monthes())) {
		return SCF_ALPHADEC | SCF_MONTH;
	}

	bool dec = true, hex = true, aldec = true;
	bool has_dec = false, has_hex = false, has_aldec = false;
	StringClass mods = 0;

	for (size_t i = 0; i != s.size(); ++i) {
		const auto c = s[i];

		if (IsSpace(c)) {
			mods|= SCF_SPACES;
			continue;
		}
		if (IsPunctuation(c)) {
			mods|= SCF_PUNCTUATION;
			continue;
		}
		if (!IsAlphaDec(c)) {
			mods|= SCF_UNSPECIFIED;
			continue;
		}

		has_aldec = true;

		if (c < '0' || c > '9') {
			dec = false;
			if ( (c < 'a' || c > 'f') && (c < 'A' || c > 'F')) {
				if (c != 'x' || ( (i != 1 || s[0] != '0') && i != 0) || i + 1 == s.size())  {
					hex = false;
				}
			} else {
				has_hex = true;
			}
			if ( (c < 'a' || c > 'z') && (c < 'A' || c > 'Z')) {
				aldec = false;
			}
		} else {
			has_dec = has_hex = true;
		}
	}

	if (dec && has_dec) {
		return SCF_DIGITS_DECIMAL | mods;
	}

	if (hex && has_hex) {
		return SCF_DIGITS_HEXADECIMAL | mods;
	}

	if (aldec && has_aldec) {
		return SCF_ALPHADEC | mods;
	}

	return SCF_NO_ALNUM | mods;
}

// checks if given string fits into given string class allowed features
// in other words, that string does not include anything that is not included in string class
template <class StringT>
	static bool StringFitsClass(const StringT &s, StringClass sc)
{
	StringClass sc_s = ClassifyString(s);

	// if sc specifies some calendar name - sc_s should fall into
	// some of specified calendar category
	if ((sc & (SCF_WEEKDAY | SCF_MONTH)) != 0) {
		return (sc_s & (sc & (SCF_WEEKDAY | SCF_MONTH))) != 0;
	}

	if ( (sc_s & SCF_MASK_ALNUM) < (sc & SCF_MASK_ALNUM)) {
		return false;
	}
	if ( (sc_s & SCF_SPACES) != 0 && (sc & SCF_SPACES) == 0) {
		return false;
	}
	if ( (sc_s & SCF_PUNCTUATION) != 0 && (sc & SCF_PUNCTUATION) == 0) {
		return false;
	}
	if ( (sc_s & SCF_UNSPECIFIED) != 0 && (sc & SCF_UNSPECIFIED) == 0) {
		return false;
	}
	// ClassifyString doesnt check for SCF_RANDOM, do it manually if need
	if ( (sc & SCF_RANDOM) != 0 && !IsRandomAlphaNums(s)) {
		return false;
	}
	return true;
}

template <class StringT>
	static StringT HeadingToken(const StringT &sample)
{
	StringT out;
	const bool aldec = IsAlphaDec(sample[0]);
	for (size_t i = 1; ; ++i) {
		if (i == sample.size() || IsAlphaDec(sample[i]) != aldec) {
			out = sample.substr(0, i);
			break;
		}
	}

	return out;
}
};
