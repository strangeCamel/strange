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
#include <assert.h>
#include <time.h>

#ifdef HAVE_STRING_VIEW
# include <string_view>
#else
# warning "AutoPatterns: std::string_view not available so will fallback to std::string that is much slower. Update your compiler get better performance."
#endif

#include "tokens.hpp"
#include "utils.hpp"

#define DESCRIPT_LIMIT_REDUNDANTS	8
#define DESCRIPT_LIMIT_MISSES		8
#define DESCRIPT_LIMIT_TIME		5

template <class CharT, size_t ConvergeThreshold = 2>
	class AutoPatterns : protected AutoPatternsUtils
{
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
typedef typename Tokens::TokenStringClass TokenStringClass;

public:

/******************************* PUBLIC INTERFACE **************************************/

enum TokenStatus
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

typedef std::vector<TokenDescription> SampleDescription;

struct Trie
{
	Trie()
	{
	}

	Trie(IStream &is)
	{
		std::string identity;
		if (!std::getline(is, identity)) {
			throw std::runtime_error("empty trie");
		}
		if (identity != "AutoPatternsTrie:1") {
			throw std::runtime_error("bad trie format");
		}
		_root.Deserialize(is);
		TransformToMemoryRepresentation(_root.kidz);
	}

	void Save(OStream &os, bool compact)
	{
		os << "AutoPatternsTrie:1" << std::endl;
		TransformToStorageRepresentation(_root.kidz);
		_root.Serialize(os, compact);
		TransformToMemoryRepresentation(_root.kidz);
	}

	template <class SamplesT>
		void Learn(const SamplesT &samples)
	{
		StringViewVec refined_samples(samples.size());
		std::copy(samples.begin(), samples.end(), std::back_inserter(refined_samples));
		SortAndUniq(refined_samples);
		BuildPatternTreeRecurse(_root.kidz, refined_samples);
		ConvergeSimilarNodes(_root.kidz);
	}

	template <class SampleT>
		bool Match(const SampleT &sample)
	{
		return MatchByNodes(sample, _root.kidz);
	}

	template <class SampleT>
		SampleDescription Descript(const SampleT &sample)
	{
		SampleStatus sample_status;
		StatusByNodesContext ctx;
		StatusByNodes(sample_status, sample, _root.kidz, ctx);
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
			std::cerr<<std::endl<<"TAIL_REMAINED:" << tail << std::endl;
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

static void BuildPatternTreeRecurse(TokenNodes &kidz, const StringViewVec &samples)
{
	StringViewVec subsamples;
	for (typename StringViewVec::const_iterator i = samples.begin(); i != samples.end();) {
		StringView head;
		if (!i->empty()) {
			head = HeadingToken(*i);
			do {
				subsamples.emplace_back(i->substr(head.size()));
				++i;
			} while (i != samples.end() && HeadingToken(*i) == head);
			SortAndUniq(subsamples);

		} else {
			++i;
		}

		TokenNode *subnode = nullptr;
		for (auto &kid : kidz) {
			if (kid->kidz.empty() == subsamples.empty() && kid->token->Match(head))  {
				subnode = kid.get();
				break;
			}
		}

		if (!subnode) {
			kidz.emplace_back(new TokenNode);
			subnode = kidz.back().get();
			subnode->token.reset(new TokenString(head));
		}

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
		if (alenmin != blenmin) {
			return (alenmin < blenmin);
		}
		const auto alenmax = a->token->GetLengthMax();
		const auto blenmax = b->token->GetLengthMax();
		return (alenmax < blenmax);
	});
}

static void ConvergeSimilarNodes(TokenNodes &kidz)
{
	SortNodes<true>(kidz);

	for (auto i = kidz.begin(); i != kidz.end(); ) {
		auto sc = (*i)->token->GetStringClass();
		if ((sc & SCF_MASK_BASE) == SCF_UNCLASSIFIED ||
			(sc & SCF_MASK_BASE) == SCF_ALPHADEC) {

			sc = SCF_INVALID;
		}

		size_t min_len = (*i)->token->GetLengthMin();
		size_t max_len = (*i)->token->GetLengthMax();

		bool all_same_strings = true;

		auto j = i;
		for (++j; j != kidz.end(); ++j) {
			const auto *istr = (*i)->token->GetString();
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

			} else if (!istr || !jstr || *istr != *jstr) {
				break;
			}
		}

		if (j - i > ConvergeThreshold || (j - i > 1 && all_same_strings)) {
			TokenNodePtr new_kid(new TokenNode);
			if (sc != SCF_INVALID && !all_same_strings) {
				new_kid->token.reset(new TokenStringClass(sc, min_len, max_len));
			} else {
				new_kid->token.reset(new TokenString(*(*i)->token->GetString()));
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

	for (auto &kid : kidz) {
		ConvergeSimilarNodes(kid->kidz);
	}

	SortNodes<false>(kidz);
}

static void TransformToStorageRepresentation(TokenNodes &kidz)
{
	for (auto &kid : kidz) {
		TransformToStorageRepresentation(kid->kidz);
		// check for tokens chains coalescion
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
	std::vector<StringView> parts;
	for (auto kidz_it = kidz.begin(); kidz_it != kidz.end(); ++kidz_it) {
		auto &kid = *kidz_it;
		const auto *str = kid->token->GetString();
		if (str && str->size() > 1) {
			StringView sv(*str);
			// check for tokens chain explode
			for (size_t i = 0; i < str->size();) {
				const auto &tail = sv.substr(i);
				const auto &head = HeadingToken(tail);
				parts.emplace_back(head);
				i+= head.size();
			}
			if (parts.size() > 1) {
				TokenNodePtr prev_node;
				for (auto it = parts.rbegin(); it != parts.rend(); ++it) {
					TokenNodePtr node(new TokenNode);
					node->token.reset(new TokenString(*it));
					if (prev_node) {
						node->kidz.push_back(std::move(prev_node));
					} else {
						node->kidz = std::move(kid->kidz);
					}
					prev_node = std::move(node);
				}
				kid = std::move(prev_node);
			}
			parts.clear();
		}

		TransformToMemoryRepresentation(kid->kidz);
	}
	SortNodes<false>(kidz);
}

#define BINSEARCH_THRESHOLD 10

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
	time_t _timeout;

public:
	StatusByNodesContext()
		: _timeout(time(NULL) + DESCRIPT_LIMIT_TIME)
	{
	}

	bool TimeToHurry() const
	{
		return (time(NULL) > _timeout);
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

	for (const auto &kid : kidz) {
		const bool matched = kid->token->Match(head);
		if (matched || best_mismatches > 1) {
			ss.clear();
			const size_t mismatches = (matched ? 0 : 1)
				+ StatusByNodes(ss, tail, kid->kidz, ctx);
			if (best_mismatches > mismatches) {
				best_mismatches = mismatches;
				out.resize(initial_size + 1);
				out.back() = matched ? TS_MATCH : TS_MISMATCH;
				out.insert(out.end(), ss.begin(), ss.end());
				if (best_mismatches == 0) {
					return 0;
				}
			}
		}
	}

	if (best_mismatches == 1 || ctx.TimeToHurry()) {
		return best_mismatches;
	}

#if DESCRIPT_LIMIT_MISSES != 0
	// special case check: may be sample has missing some token(s)
	fnn.Lookup(kidz, head, std::min(best_mismatches, (size_t)DESCRIPT_LIMIT_MISSES));

	for (const auto &fn : fnn) {
		ss.clear();
		const size_t mismatches = fn.depth
			+ StatusByNodes(ss, tail, fn.kidz, ctx);
		if (best_mismatches > mismatches) {
			best_mismatches = mismatches;
			out.resize(initial_size + fn.depth + 1);
			std::fill(out.begin() + initial_size, out.end(), TS_MISSING);
			out.back() = TS_MATCH;
			out.insert(out.end(), ss.begin(), ss.end());
		}
	}
#endif

#if DESCRIPT_LIMIT_REDUNDANTS != 0
	if (!head.empty()) { // special case check: may be sample has extra token(s)
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
						+ StatusByNodes(ss, tmp_tail, kid->kidz, ctx);
					if (best_mismatches > mismatches) {
						best_mismatches = mismatches;
						out.resize(initial_size + skip_count + 1);
						std::fill(out.begin() + initial_size, out.end(), TS_REDUNDANT);
						out.back() = TS_MATCH;
						out.insert(out.end(), ss.begin(), ss.end());
					}
				}

			}
			tmp_value = tmp_tail;
		}
	}
#endif

	return best_mismatches;
}


};

