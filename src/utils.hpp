#pragma once
#include <math.h>
#include <unordered_set>

//#define RND_DETECTION

struct AutoPatternsUtils
{
#ifdef RND_DETECTION
static constexpr size_t LengthRandomThreshold = 16;
static constexpr double SigmaRandomThreshold = 0.1;
#endif
#if __cplusplus >= 201703L
static constexpr char Monthes[] = 
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

static constexpr char WeekDays[] =
	"sun\0sunday\0"
	"mon\0monday\0"
	"tue\0tuesday\0"
	"wed\0wednesday\0"
	"thu\0thursday\0"
	"fri\0friday\0"
	"sat\0saturday\0\0";
#endif
////////////

typedef unsigned int StringClass;


enum StringClassFeats
{	// base feats, order matters - wider class should be before narrower!
	SCF_UNCLASSIFIED = 0,

	SCF_ALPHADEC,
	SCF_DIGITS_HEXADECIMAL,
	SCF_DIGITS_DECIMAL,

	SCF_MASK_BASE = 0x00000fff,

	// modifiers, order doesn't matter
	SCF_NON_ALPHADEC = 0x00001000,
	SCF_RANDOM = 0x00002000,
	SCF_WEEKDAY = 0x00004000,
	SCF_MONTH = 0x00004000,
	SCF_MASK_MODIFIERS = 0xfffff000,

	SCF_INVALID = 0xffffffff
};

////////////
template <class VectorT>
	static void SortAndUniq(VectorT &v)
{
	std::sort(v.begin(), v.end());
	auto new_end = std::unique(v.begin(), v.end());
	v.resize(new_end - v.begin());
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

template <class StringT>
	static StringClass ClassifyString(const StringT &s)
{
	size_t freqs[0x100]{};
	bool dec = true, hex = true, aldec = true;
	bool has_dec = false, has_hex = false, has_aldec = false;
	StringClass mods = 0;

	for (size_t i = 0; i != s.size(); ++i) {
		const auto c = s[i];

		if (!IsAlphaDec(c)) {
			mods|= SCF_NON_ALPHADEC;
			continue;
		}

		has_aldec = true;

		if ((unsigned int)c < sizeof(freqs) / sizeof(freqs[0])) {
			freqs[(unsigned int)c]++;
		}

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

	size_t total_chars = 0, diff_chars = 0;
	for (const auto &freq : freqs) if (freq) {
		total_chars+= freq;
		++diff_chars;
	}
#ifdef RND_DETECTION
	if (diff_chars >= LengthRandomThreshold) {
		double avg_freq = ((double)total_chars) / ((double)diff_chars);

		double sigma = 0;
		for (const auto &freq : freqs) if (freq) {
			const double delta = freq - avg_freq;
			sigma+= delta * delta;
		}

		sigma/= avg_freq * avg_freq * (double)diff_chars;
		if (sigma < SigmaRandomThreshold) {
			mods|= SCF_RANDOM;
		}
	}
#endif
	if (dec && has_dec) {
		return SCF_DIGITS_DECIMAL | mods;
	}

	if (hex && has_hex) {
		return SCF_DIGITS_HEXADECIMAL | mods;
	}

	if (aldec && has_aldec) {
#if __cplusplus >= 201703L
		if (MatchesAnyOfWords(s, &WeekDays[0])) {
			mods|= SCF_WEEKDAY;
		}
		if (MatchesAnyOfWords(s, &Monthes[0])) {
			mods|= SCF_MONTH;
		}
#endif
		return SCF_ALPHADEC | mods;
	}

	return SCF_UNCLASSIFIED | mods;
}

template <class StringT>
	static bool StringFitsClass(const StringT &s, StringClass sc)
{
	StringClass sc_s = ClassifyString(s);
	if ( (sc_s & SCF_MASK_BASE) < (sc & SCF_MASK_BASE)) {
		return false;
	}
	if ( (sc_s & SCF_NON_ALPHADEC) != 0 && (sc & SCF_NON_ALPHADEC) == 0) {
		return false;
	}
//	if ( (sc_s & (SCF_WEEKDAY|SCF_MONTH)) != (sc & (SCF_WEEKDAY|SCF_MONTH))) {
	if ( (sc_s & SCF_WEEKDAY) != 0 && (sc & SCF_WEEKDAY) == 0) {
		return false;
	}
	if ( (sc_s & SCF_MONTH) != 0 && (sc & SCF_MONTH) == 0) {
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

	if (out.size() > 1) {
		// extra check for having two subparts of alphas and nums
		// to split sequences like '462ms' into '462' 'ms'
		bool prev = IsHex(sample[0]);
		size_t changes = 0, changed_at = StringT::npos;
		for (size_t i = 1; i != out.size(); ++i) {
			bool cur = IsHex(sample[i]);
			if (prev != cur) {
				changed_at = i;
				++changes;
				prev = cur;
			}
		}
		if (changes == 1) {
			out = out.substr(0, changed_at);
		}
	}

	return out;
}

};
