#pragma once

#if __cplusplus >= 201703L
# if !defined(__GNUC__) || (__GNUC__ >= 7)
#  define HAVE_STRING_VIEW
# endif
#endif

#include <memory>
#include <algorithm>
#include <vector>
#include <list>
#include <string>
#include <ostream>
#include <istream>
#include <sstream>
#include <chrono>
#include <limits>
#include <assert.h>

#ifdef HAVE_STRING_VIEW
# include <string_view>
#else
# warning "AutoPatterns: std::string_view not available so will fallback to std::string that is much slower. Update your compiler to get better performance."
#endif

#include "tokens.hpp"
#include "utils.hpp"

template <class CharT, size_t ConvergeThreshold = 2>
	class AutoPatterns : protected AutoPatternsUtils
{
enum {
	DESCRIPT_NESTING_MATCHES_TRH = 2,
	DESCRIPT_LIMIT_REDUNDANTS = 8,
	DESCRIPT_LIMIT_MISSES = 8,
	DESCRIPT_LIMIT_TIME = 5,
	BINSEARCH_THRESHOLD = 10
};

typedef std::basic_string<CharT> String;
#ifdef HAVE_STRING_VIEW
typedef std::basic_string_view<CharT> StringView;
#else
typedef std::basic_string<CharT> StringView;
#endif

typedef std::vector<String> StringVec;
typedef std::vector<StringView> StringViewVec;
typedef std::basic_ostream<CharT> OStream;
typedef std::basic_istream<CharT> IStream;

typedef AutoPatternsTokens<String, StringView, IStream, OStream> Tokens;
typedef typename Tokens::NodePtr TokenNodePtr;
typedef typename Tokens::Node TokenNode;
typedef typename Tokens::Nodes TokenNodes;
typedef typename Tokens::TokenString TokenString;
typedef typename Tokens::TokenStringWithNumbers TokenStringWithNumbers;
typedef typename Tokens::TokenStringClass TokenStringClass;

public:

/******************************* PUBLIC INTERFACE **************************************/

enum TokenStatus : unsigned char
{
	TS_MATCH = 0,
	TS_MISMATCH,
	TS_REDUNDANT,
	TS_MISSING
};

struct TokenDescription
{
	TokenStatus status;
	StringView token;
};

/// Returned by Trie::Descript - see its comment for details
typedef std::vector<TokenDescription> SampleDescription;

/// Main class to be instantiated and manipulated by user
struct Trie
{
	/// Creates empty trie
	Trie()
	{
	}

	/// Creates trie and loads from stream previously Save()'ed learned patterns into it
	Trie(IStream &is)
	{
		String identity;
		if (!std::getline(is, identity)) {
			throw std::runtime_error("empty trie");
		}
		if (identity != "AutoPatternsTrie:1") {
			throw std::runtime_error("bad trie format");
		}
		_root.Deserialize(is);
		TransformToMemoryRepresentation(_root.kidz);
	}

	/// Saves current trie into file, that can be loaded in future to avoid full dataset re-learnings
	void Save(OStream &os, bool compact)
	{
		os << "AutoPatternsTrie:1" << std::endl;
		TransformToStorageRepresentation(_root.kidz);
		_root.Serialize(os, compact);
		TransformToMemoryRepresentation(_root.kidz);
	}

	/// Learns given set of samples, making them (and similar) samples recognized in future by Match()
	template <class SamplesT>
		void Learn(const SamplesT &samples)
	{
		StringViewVec refined_samples(samples.size());
		std::copy(samples.begin(), samples.end(), refined_samples.begin());
		SortAndUniq(refined_samples);
		BuildPatternTreeRecurse(_root.kidz, refined_samples);
		ConvergeSimilarNodes(_root.kidz);
	}

	/// Simple and fast matcher - returns true if given sample matches to learned trie
	template <class SampleT>
		bool Match(const SampleT &sample)
	{
		return MatchByNodes(sample, _root.kidz);
	}

	/// Verbose matcher - returns per-token sequence of description that indicates
	/// best match path in trie, and each desciption shows token value and its
	/// status - either token mismatched, or redundant, or something missing
	template <class SampleT>
		SampleDescription Descript(const SampleT &sample)
	{
		SampleStatus sample_status;
		StatusByNodesContext ctx;
		StatusByNodes(sample_status, sample, _root.kidz, ctx);
		// Sample_status now represents status of each token
		// (present or missing) of specified sample that describes
		// found closest match.
		// To make SampleDescription from it need to split sample
		// by tokens and compose resulting vector of elements each
		// representing token status and (if not missing) value.
		SampleDescription out;
		StringView tail = sample;
		for (const auto &token_status : sample_status) {
			out.emplace_back();
			out.back().status = token_status;
			if (token_status != TS_MISSING) {
				out.back().token = HeadingToken(tail);
				tail = tail.substr(out.back().token.size());
			}
		}
		if (!tail.empty()) {
			// StatusByNodes returned inconsistent amount of statuses
			std::cerr << std::endl << "UNDESCRIPTED_TAIL:" << tail << std::endl;
			abort();
		}
		return out;
	}

private:
	TokenNode _root;

	Trie(const Trie &) = delete;
};

typedef std::unique_ptr<Trie> TriePtr;

/*******************************************************************************/

private:

static TokenNode *ObtainSubnode(TokenNodes &kidz, const StringView &head, bool without_kidz)
{
	for (auto &kid : kidz) {
		if (kid->kidz.empty() == without_kidz && kid->token->Match(head))  {
			return kid.get();
		}
	}

	kidz.emplace_back(new TokenNode);
	kidz.back()->token.reset(new TokenString(head));
	return kidz.back().get();
}

static void BuildPatternTreeRecurse(TokenNodes &kidz, const StringViewVec &samples)
{
	StringViewVec subsamples;
	for (typename StringViewVec::const_iterator i = samples.begin(); i != samples.end();) {
		if (i->empty()) {
			std::cerr << std::endl << "EMPTY_SAMPLE_NOT_ALLOWED" << std::endl;
			abort();
		}

		StringView head = HeadingToken(*i);
		if (head.size() < i->size()) {
			do {
				subsamples.emplace_back(i->substr(head.size()));
				++i;
			} while (i != samples.end() && i->size() > head.size() && HeadingToken(*i) == head);
			SortAndUniq(subsamples);

		} else {
			++i;
		}

		TokenNode *subnode = ObtainSubnode(kidz, head, subsamples.empty());

		if (!subsamples.empty()) {
			BuildPatternTreeRecurse(subnode->kidz, subsamples);
			subsamples.clear();
		}
	}
}

struct TokenNodeSearchCmp
{
	bool operator() (const TokenNodePtr &l, const TokenNodePtr &r) const
	{
		const auto *lstr = l->token->GetString();
		const auto *rstr = r->token->GetString();
		assert(lstr && rstr);
		return *lstr < *rstr;
	}
	bool operator() (const TokenNodePtr &l, const StringView &r) const
	{
		const auto *lstr = l->token->GetString();
		assert(lstr);
		return *lstr < r;
	}
	bool operator() (const StringView &l, const TokenNodePtr &r) const
	{
		const auto *rstr = r->token->GetString();
		assert(rstr);
		return l < *rstr;
	}
};

template <bool sort_for_converging>
	static void SortNodes(TokenNodes &kidz)
{
	std::sort(kidz.begin(), kidz.end(),
		[](const TokenNodePtr &a, const TokenNodePtr &b) -> bool
	{ 
		const auto *astr = a->token->GetString();
		const auto *bstr = b->token->GetString();

		const auto acls = a->token->GetStringClass();
		const auto bcls = a->token->GetStringClass();

		if (sort_for_converging) {
			// those who have kidz and those who not - cannot be converged,
			// so separate them at very beginning
			if (a->kidz.empty() != b->kidz.empty()) {
				return a->kidz.empty() < b->kidz.empty();
			}
			// group nodes of same class together
			if (acls != bcls) {
				return acls < bcls;
			}
			if ( !!astr != !!bstr) {
				return !!astr < !!bstr;
			}
			if (astr && *astr != *bstr) {
				return *astr < *bstr;
			}
		} else {
			// put first class-covering nodes
			// then extact string nodes, sorted by values
			if ( !!astr != !!bstr) {
				return !!astr < !!bstr;
			}
			if (astr && *astr != *bstr) {
				return *astr < *bstr;
			}
			if (acls != bcls) {
				return acls < bcls;
			}
		}

		const auto alenmin = a->token->GetLengthMin();
		const auto blenmin = b->token->GetLengthMin();
//		if (alenmin != blenmin) {
			return (alenmin < blenmin);
//		}
//		const auto alenmax = a->token->GetLengthMax();
//		const auto blenmax = b->token->GetLengthMax();
//		return (alenmax < blenmax);
	});
}

static void EstimatedMinMaxLenExpand(size_t &min_len, size_t &max_len)
{
	if (min_len < max_len) {
		if (min_len > 1) {
			min_len/= 2;
		}
		max_len*= 2;
	}
}

static void ConvergeNodesWithSimilarTokens(TokenNodes &kidz)
{
	TokenStringWithNumbers itswn;

	for (auto i = kidz.begin(); i != kidz.end(); ) {
		auto sc = (*i)->token->GetStringClass();
		if (sc == SCF_SPACES) {
			// if token contains _only_ whitespaces then
			// converge it with other _only_ whitespaces tokens
			;
		} else if ( (sc & (SCF_WEEKDAY | SCF_MONTH)) != 0) {
			// if token contains calendar name then
			// converge it with others containing similar names
			;
		} else if ((sc & SCF_MASK_ALNUM) != SCF_NO_ALNUM
			&& (sc & SCF_MASK_ALNUM) != SCF_ALPHADEC)  {
			// if token contains digits - decimal or hexadecimal, then
			// converge it with others containing similar digits
			;
		} else {
			// other cases should not be converged by class
			sc = SCF_INVALID;
		}

		size_t min_len = (*i)->token->GetLengthMin();
		size_t max_len = (*i)->token->GetLengthMax();

		bool all_same_strings = true;

		const auto *istr = (*i)->token->GetString();
		if (istr) {
			itswn.Reinit(*istr);
		}

		auto j = i;
		for (++j; j != kidz.end() && (*i)->kidz.empty() == (*j)->kidz.empty(); ++j) {
			const auto *jstr = (*j)->token->GetString();

			if (sc != SCF_INVALID) {
				if ((*j)->token->GetStringClass() != sc) {
					break;
				}
				min_len = std::min(min_len, (*j)->token->GetLengthMin());
				max_len = std::max(max_len, (*j)->token->GetLengthMax());

				if (!istr || !jstr || *istr != *jstr) {
					all_same_strings = false;
				}

			} else if (!istr || !jstr || !itswn.Match(*jstr)) {
				break;

			} else {
				min_len = std::min(min_len, (*j)->token->GetLengthMin());
				max_len = std::max(max_len, (*j)->token->GetLengthMax());

				if (*istr != *jstr) {
					all_same_strings = false;
				}
			}
		}

		EstimatedMinMaxLenExpand(min_len, max_len);

		if (j - i > ConvergeThreshold || (j - i > 1 && (all_same_strings || sc == SCF_SPACES))) {
			TokenNodePtr new_kid(new TokenNode);
			if (all_same_strings) {
				new_kid->token.reset(new TokenString(*(*i)->token->GetString()));

			} else if (sc == SCF_INVALID) {
				new_kid->token.reset(new TokenStringWithNumbers(*(*i)->token->GetString(), max_len));

			} else {
				new_kid->token.reset(new TokenStringClass(sc, min_len, max_len));
			}
			new_kid->kidz.reserve(new_kid->kidz.size() + (j - i));
			for (auto k = i; k != j; ++k) {
				for (auto &old_kid : (*k)->kidz) {
					new_kid->kidz.emplace_back(old_kid.release());
				}
			}
			*i = std::move(new_kid);
			++i;
			i = kidz.erase(i, j);

		} else {
			i = j;
		}
	}
}

static void ConvergeNodesWithRandomTokensAndMatchingSubnodes(TokenNodes &nodes)
{
	std::vector<std::unique_ptr<std::basic_ostringstream<CharT>>> ss(nodes.size());
	for (size_t i = 0; i < nodes.size(); ++i) {
		const auto *si = nodes[i]->token->GetString();
		if (si && (ClassifyString(*si) & SCF_MASK_ALNUM) != SCF_NO_ALNUM && IsRandomAlphaNums(*si)) {
			ss[i].reset(new std::basic_ostringstream<CharT>);
			nodes[i]->Serialize(*ss[i], true);
		}
	}

	std::vector<size_t> group;
	String merged_tokens;
	for (size_t i = 0; i + 1 < nodes.size(); ++i) if (ss[i]) {
		group.clear();
		const auto &si = ss[i]->str();
		for (size_t j = i + 1; j < nodes.size(); ++j) if (ss[j]) {
			if (si == ss[j]->str()) {
				group.emplace_back(j);
			}
		}
		if (group.size() <= ConvergeThreshold) {
			continue;
		}
		group.emplace_back(i);
		merged_tokens.clear();

		size_t min_len = std::numeric_limits<std::size_t>::max();
		size_t max_len = 0;
		for (const auto &j : group) {
			const auto &token = nodes[j]->token;
			merged_tokens+= *token->GetString();
			min_len = std::min(min_len, token->GetLengthMin());
			max_len = std::max(max_len, token->GetLengthMax());
		}
		if (!IsRandomAlphaNums(merged_tokens)) {
			continue;
		}
		EstimatedMinMaxLenExpand(min_len, max_len);
		StringClass sc = ClassifyString(merged_tokens) & SCF_MASK_ALNUM;
		assert(sc != SCF_NO_ALNUM);
		nodes[i]->token.reset(new TokenStringClass(sc | SCF_RANDOM, min_len, max_len));
		group.pop_back();
		for (auto rit = group.rbegin(); rit != group.rend(); ++rit) {
			nodes.erase(nodes.begin() + *rit);
			ss.erase(ss.begin() + *rit);
		}
	}
}

static void ConvergeNodesWithMatchingTokens(TokenNodes &nodes)
{
	std::vector<std::unique_ptr<std::basic_ostringstream<CharT>>> ss(nodes.size());
	for (size_t i = 0; i < nodes.size(); ++i) {
		ss[i].reset(new std::basic_ostringstream<CharT>);
		nodes[i]->token->Serialize(*ss[i]);
	}

	for (size_t i = 0; i + 1 < nodes.size(); ++i) {
		for (size_t j = i + 1; j < nodes.size(); ) {
			if (ss[i]->str() == ss[j]->str() && nodes[i]->kidz.empty() == nodes[j]->kidz.empty()) {
				for (auto &k : nodes[j]->kidz) {
					nodes[i]->kidz.emplace_back(std::move(k));
				}
				nodes.erase(nodes.begin() + j);
				ss.erase(ss.begin() + j);
			} else {
				++j;
			}
		}
	}
}


static void ConvergeSimilarNodes(TokenNodes &kidz)
{
	for (;;) {
		const size_t initial_kidz_count = kidz.size();
		if (kidz.size() > 1) {
			SortNodes<true>(kidz);
			ConvergeNodesWithSimilarTokens(kidz);
		}

		for (auto &kid : kidz) {
			ConvergeSimilarNodes(kid->kidz);
		}

		if (kidz.size() > 1) {
			ConvergeNodesWithRandomTokensAndMatchingSubnodes(kidz);
			if (kidz.size() > 1) {
				ConvergeNodesWithMatchingTokens(kidz);
			}
		}

		if (kidz.size() == initial_kidz_count) {
			if (kidz.size() > 1) {
				SortNodes<false>(kidz);
			}
			break;
		}
	}
}

static void TransformToStorageRepresentation(TokenNodes &kidz)
{

	// Check for tokens chains coalescion: in case of having
	// nesting tokens chain without extra branching - merge
	// that tokens into single one to avoid excessive storage use.
	for (auto &kid : kidz) {
		TransformToStorageRepresentation(kid->kidz);
		if (kid->kidz.size() == 1 && kid->token->GetString() != nullptr
				&& kid->kidz.front()->token->GetString() != nullptr) {
			String merged_string = *kid->token->GetString();
			merged_string+= *kid->kidz.front()->token->GetString();
			kid->token.reset(new TokenString(merged_string));
			auto tmp_subkidz = std::move(kid->kidz.front()->kidz);
			kid->kidz = std::move(tmp_subkidz);
		}
	}

	SortNodes<false>(kidz);
}

static void TransformToMemoryRepresentation(TokenNodes &kidz)
{
	// Explode chain of coalesced tokens into nested sequence 
	// of actual tokens as needed for matching logic.
	for (auto kidz_it = kidz.begin(); kidz_it != kidz.end(); ++kidz_it) {
		auto &kid = *kidz_it;
		const auto *str = kid->token->GetString();
		if (str && str->size() > 1) {
			StringView sv(*str);
			const auto &head = HeadingToken(sv);
			if (head.size() < sv.size()) {
				std::unique_ptr<TokenString> head_token(new TokenString(head));
				std::unique_ptr<TokenString> tail_token(new TokenString(sv.substr(head.size())));
				TokenNodePtr new_subkid(new TokenNode);
				new_subkid->token = std::move(tail_token);
				new_subkid->kidz = std::move(kid->kidz);
				kid->kidz.clear();
				kid->kidz.emplace_back(std::move(new_subkid));
				kid->token = std::move(head_token);
			}
		}

		TransformToMemoryRepresentation(kid->kidz);
	}

	SortNodes<false>(kidz);
}

static bool MatchByNodes(const StringView &value, TokenNodes &kidz)
{
	if (value.size() == 0 && kidz.size() == 0) {
		return true;
	}

	const StringView &head = HeadingToken(value);
	const StringView &tail = value.substr(head.size());
	// kidz are sorted in a way that
	// in beginning there're string-class matchers kidz
	// followed by exact-string matchers sorted by values
	// 
	// First scan string-class matchers that go in beginning
	// checking if reached first exact-string kidz
	// of range big enough to use binary-search optimization
	// and if so - do it as separate phase for remaining
	// range that contains sorted by exact-string values kidz
	// 
	size_t i;
	for (i = 0; i != kidz.size(); ++i) {
		const auto &kid = kidz[i];
		if (i + BINSEARCH_THRESHOLD < kidz.size() && kid->token->GetString()) {
			break; // bail out to binary search phase
		}
		if (kid->token->Match(head) && MatchByNodes(tail, kid->kidz)) {
			return true;
		}
	}

	if (i != kidz.size()) {
		// Reached range of exact-string kidz [i .. kidz.size())
		// lookup by upper_bound first kid that has string bigger
		// that need to match, that means it cannot be a trailing
		// for a needed value.
		// Then iterate from this kid backward, as kidz that have
		// trailing exact-strings less tham needed value are all
		// candidates for matching sequence leaders.
		TokenNodeSearchCmp cmp;
		auto start = kidz.begin() + i;
		auto kid_it = std::upper_bound(start, kidz.end(), head, cmp);
			
		while (kid_it != start) {
			--kid_it;
			if (cmp(*kid_it, head) != 0) {
				break;
			}
			if (MatchByNodes(tail, (*kid_it)->kidz)) {
				return true;
			}
		}
	}

	return false;
}

typedef std::vector<TokenStatus> SampleStatus;

struct FoundNode
{
	FoundNode(TokenNodes &kidz_, size_t depth_)
		: kidz(kidz_), depth(depth_) {}

	TokenNodes &kidz;
	size_t depth;
};

struct FindNestedNodes : std::vector<FoundNode>
{
	void Lookup(TokenNodes &kidz, const StringView &token_value, size_t depth_limit)
	{
		_depth_limit = depth_limit;
		std::vector<FoundNode>::clear();
		for (const auto &kid : kidz) {
			LookupRecurse(kid->kidz, token_value, 1);
		}
	}
private:
	size_t _depth_limit;

	void LookupRecurse(TokenNodes &kidz, const StringView &token_value, size_t depth)
	{
		if (depth >= _depth_limit) {
			return;
		}

		for (const auto &kid : kidz) {
			if (kid->token->Match(token_value)) {
				std::vector<FoundNode>::emplace_back(kid->kidz, depth);
			}
			LookupRecurse(kid->kidz, token_value, depth + 1);
		}
	}
};

class StatusByNodesContext
{
	// this is essentially temporary objects cache
	// to reduce needed reallocations
	struct FrameData
	{
		SampleStatus ss;
		FindNestedNodes fnn;
	};

	struct FramesData : std::list<FrameData> {} _frames_data;
	typename FramesData::iterator _frames_data_top;
	const std::chrono::time_point<std::chrono::steady_clock> _start_timepoint{std::chrono::steady_clock::now()};
	bool _time_to_hurry = false;
	size_t _time_to_hurry_counter = 0;

public:
	bool TimeToHurry()
	{
		// now() is quite expensive, so do some measurements decimation..
		if (!_time_to_hurry && ++_time_to_hurry_counter > 128) {
			_time_to_hurry_counter = 0;
			const auto cur_timepoint = std::chrono::steady_clock::now();
			const auto passed_seconds = std::chrono::duration_cast<std::chrono::seconds>(cur_timepoint - _start_timepoint);
			_time_to_hurry = (passed_seconds.count() > DESCRIPT_LIMIT_TIME);
		}
		return _time_to_hurry;
	}

	class Frame
	{
		StatusByNodesContext &_ctx;
		typename FramesData::iterator _pos;
	public:
		Frame(StatusByNodesContext &ctx)
			: _ctx(ctx)
		{
			if (_ctx._frames_data.empty()
			 	|| _ctx._frames_data_top == _ctx._frames_data.end()) {

				_ctx._frames_data.emplace_back();
				_ctx._frames_data_top = _ctx._frames_data.end();
				_ctx._frames_data_top--;
			}

			_pos = _ctx._frames_data_top++;
		}

		~Frame()
		{
			_ctx._frames_data_top--;
			assert(_pos == _ctx._frames_data_top);
		}

		SampleStatus &SS()
		{
			return _pos->ss;
		}

		FindNestedNodes &FNN()
		{
			return _pos->fnn;
		}
	};
};

template <size_t NESTING_MATCHES = 1>
	static size_t StatusByNodes(SampleStatus &out,
		const StringView &value, TokenNodes &kidz,
		StatusByNodesContext &ctx)
{
	const size_t initial_size = out.size();

	if (kidz.empty()) {
		StringView tmp_value = value;
		while (!tmp_value.empty()) {
			const StringView &head = HeadingToken(tmp_value);
			tmp_value = tmp_value.substr(head.size());
			out.emplace_back(TS_REDUNDANT);
		}
		return out.size() - initial_size;
	}

	typename StatusByNodesContext::Frame frame(ctx);
	SampleStatus &ss = frame.SS();
	FindNestedNodes &fnn = frame.FNN();

	size_t best_mismatches = (size_t)-1;
	const StringView &head = HeadingToken(value);
	const StringView &tail = value.substr(head.size());
	// check score for head match/mismatch/missing cases

	bool current_level_matched = false;

	for (const auto &kid : kidz) {
		const bool matched = kid->token->Match(head);
		if (matched || best_mismatches > 1) {
			ss.clear();
			size_t mismatches = (matched ? 0 : 1);
			if (matched) {
				current_level_matched = true;
				mismatches+= StatusByNodes< (NESTING_MATCHES < DESCRIPT_NESTING_MATCHES_TRH)
								? NESTING_MATCHES + 1 : DESCRIPT_NESTING_MATCHES_TRH>
									(ss, tail, kid->kidz, ctx);
			} else {
				mismatches+= StatusByNodes<0>(ss, tail, kid->kidz, ctx);
			}

			if (best_mismatches > mismatches) {
				best_mismatches = mismatches;
				out.resize(initial_size);
				out.emplace_back(matched ? TS_MATCH : TS_MISMATCH);
				out.insert(out.end(), ss.begin(), ss.end());
				if (best_mismatches == 0) {
					return 0;
				}
			}
		}
	}

	// avoid time-expensive checks for redundant/missing entries if...
	if (
		// upper token mismatched
		NESTING_MATCHES == 0

		// lot of tokens (including current one) matched sequenctially
		|| (NESTING_MATCHES >= DESCRIPT_NESTING_MATCHES_TRH && current_level_matched)

		// definately can't find better score than 1
		|| best_mismatches == 1

		// its time to hurry up even by cost of losing exactness
		|| ctx.TimeToHurry()) {

		return best_mismatches;
	}

	if (DESCRIPT_LIMIT_MISSES != 0) {
		// special case check: may be sample has missing some token(s)
		fnn.Lookup(kidz, head, std::min(best_mismatches, (size_t)DESCRIPT_LIMIT_MISSES));

		for (const auto &fn : fnn) if (fn.depth < best_mismatches) {
			ss.clear();
			const size_t mismatches = fn.depth
				+ StatusByNodes<1>(ss, tail, fn.kidz, ctx);
			if (best_mismatches > mismatches) {
				best_mismatches = mismatches;
				out.resize(initial_size);
				out.insert(out.end(), fn.depth, TS_MISSING);
				out.emplace_back(TS_MATCH);
				out.insert(out.end(), ss.begin(), ss.end());
			}
		}
	}

	if (DESCRIPT_LIMIT_REDUNDANTS != 0 && !head.empty()) {
		// special case check: may be sample has extra token(s)
		StringView tmp_value = tail;
		for (size_t skip_count = 1; skip_count < best_mismatches
				&& skip_count < DESCRIPT_LIMIT_REDUNDANTS
					&& !tmp_value.empty(); ++skip_count) {
			const StringView &tmp_head = HeadingToken(tmp_value);
			const StringView &tmp_tail = tmp_value.substr(tmp_head.size());
			for (const auto &kid : kidz) {
				if (kid->token->Match(tmp_head)) {
					ss.clear();
					const size_t mismatches = skip_count
						+ StatusByNodes<1>(ss, tmp_tail, kid->kidz, ctx);
					if (best_mismatches > mismatches) {
						best_mismatches = mismatches;
						out.resize(initial_size);
						out.insert(out.end(), skip_count, TS_REDUNDANT);
						out.emplace_back(TS_MATCH);
						out.insert(out.end(), ss.begin(), ss.end());
					}
				}

			}
			tmp_value = tmp_tail;
		}
	}

	return best_mismatches;
}


};

