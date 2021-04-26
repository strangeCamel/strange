#pragma once
#include "utils.hpp"
#include <set>
#include <algorithm>

template <class String, class StringView, class IStream, class OStream>
	struct AutoPatternsTokens : AutoPatternsUtils
{

struct Token
{
	virtual ~Token() {}
	virtual bool Match(const StringView &value) const = 0;
	virtual void Serialize(OStream &os) const = 0;

	virtual StringClass GetStringClass() const { return SCF_INVALID; }
	virtual size_t GetLengthMin() const { return 0; }
	virtual size_t GetLengthMax() const { return (size_t)-1; }

	virtual const String *GetString() const { return nullptr; } // TODO: do something better
};

struct Node;

typedef std::unique_ptr<Node> NodePtr;
typedef std::vector<NodePtr> Nodes;

struct Deserializer
{
	size_t depth = 0;
	char lead = 0;
	String data;

	Deserializer(IStream &is) : _is(is) { }

	void Fetch()
	{
		data.clear();
		depth = 0;
		while (_is.get(lead)) {
			if (lead == ' ') {
				++depth;

			} else if (lead >= '0' && lead<= '9') {
				depth*= 10;
				depth+= lead - '0';

			} else if (IsEOL(lead)) {
				// skip empty lines
				depth = 0;

			} else {
				char c;
				while (_is.get(c) && !IsEOL(c)) {
					data+= c;
				}
				return;
			}
		}
		lead = 0;
	}

private:
	IStream &_is;
};

struct Node
{
	std::unique_ptr<Token> token;
	Nodes kidz;

	void Serialize(OStream &os, bool compact) const
	{
		for (const auto &kid : kidz) {
			kid->SerializeInner(os, compact, 0);
		}
	}

	void Deserialize(IStream &is)
	{
		Deserializer des(is);
		des.Fetch();
		DeserializeInner(des, 0);
	}

private:
	void DeserializeInner(Deserializer &des, size_t depth)
	{
		while (des.lead) {
			if (des.depth > depth) {
				assert(des.depth == depth + 1);
				assert(!kidz.empty());
				kidz.back()->DeserializeInner(des, depth + 1);
				continue;
			}
			if (des.depth < depth) {
				break;
			}
			kidz.emplace_back(new Node);
			switch (des.lead) {
				case '$': {
					kidz.back()->token.reset(new TokenString(des));
				} break;
				case '?': {
					kidz.back()->token.reset(new TokenStringClass(des));
				} break;
				case '!': {
					kidz.back()->token.reset(new TokenStringWithNumbers(des));
				} break;
				default: {
					kidz.pop_back();
					std::cerr << __FUNCTION__ << ": bad lead=" << des.lead << std::endl;
				}
			}
			des.Fetch();
		}		
	}

	void SerializeInner(OStream &os, bool compact, size_t depth) const
	{
		if (compact) {
			os << std::dec << depth;

		} else for (size_t i = 0; i != depth; ++i) {
			os << ' ';
		}
		token->Serialize(os);
		for (const auto &kid : kidz) {
			kid->SerializeInner(os, compact, depth + 1);
		}
	}
};

struct TokenString : Token
{
	TokenString(const StringView &value)
	{
#ifdef STRINGS_INTERNING
		InternValue(value);
#else
		_value = value;
#endif
	}

	TokenString(Deserializer &des)
	{
#ifdef STRINGS_INTERNING
		InternValue(des.data);
#else
		_value = std::move(des.data);
		des.data.clear();
#endif
	}

	virtual bool Match(const StringView &value) const
	{
		return (value.size() == ValuePtr()->size() && value == *ValuePtr());
	}

	virtual void Serialize(OStream &os) const
	{
		os << '$' << *ValuePtr() << std::endl;
	}

	virtual StringClass GetStringClass() const
	{
		if (_sc == SCF_INVALID) {
			_sc = ClassifyString(*ValuePtr());
		}
		return _sc;
	}

	virtual size_t GetLengthMin() const
	{
		return ValuePtr()->size();
	}

	virtual size_t GetLengthMax() const
	{
		return ValuePtr()->size();
	}

	virtual const String *GetString() const
	{
		return ValuePtr();
	}

private:
#ifdef STRINGS_INTERNING
	const String *_value = nullptr;
	inline const String *ValuePtr() const { return _value; }

	void InternValue(const StringView &value)
	{
		static std::set<String, std::less<> > s_interned_strings;
		auto it = s_interned_strings.find(value);
		if (it != s_interned_strings.end()) {
			_value = &(*it);
		} else {
			auto ir = s_interned_strings.emplace(value);
			_value = &(*ir.first);
		}
	}

#else
	String _value;
	inline const String *ValuePtr() const { return &_value; }
#endif
	mutable StringClass _sc = SCF_INVALID;
};

struct TokenStringClass : Token
{
	TokenStringClass(Deserializer &des)
	{
		size_t pos = 0;
		if (!SkipNonAlphaNum(des.data, pos)) {
			throw std::runtime_error("TokenStringClass: no class in serialized data");
		}
		_sc = ParseDecAsInt<StringClass>(des.data, pos);
		if (!SkipNonAlphaNum(des.data, pos)) {
			throw std::runtime_error("TokenStringClass: no min_len in serialized data");
		}
		_min_len = ParseDecAsInt<size_t>(des.data, pos);
		if (!SkipNonAlphaNum(des.data, pos)) {
			throw std::runtime_error("TokenStringClass: no max_len in serialized data");
		}
		_max_len = ParseDecAsInt<size_t>(des.data, pos);
	}

	TokenStringClass(StringClass sc, size_t min_len, size_t max_len)
		:
		_sc(sc),
		_min_len(min_len),
		_max_len(max_len)
	{
	}

	virtual bool Match(const StringView &value) const
	{
		return value.size() >= _min_len && value.size() <= _max_len && StringFitsClass(value, _sc);
	}

	virtual void Serialize(OStream &os) const
	{
		os << '?' << _sc << ':' << _min_len << ':' << _max_len << std::endl;
	}


	virtual StringClass GetStringClass() const
	{
		return _sc;
	}

	virtual size_t GetLengthMin() const
	{
		return _min_len;
	}

	virtual size_t GetLengthMax() const
	{
		return _max_len;
	}

private:
	StringClass _sc = SCF_INVALID;
	size_t _min_len, _max_len;
};


struct TokenStringWithNumbers : Token
{
	TokenStringWithNumbers() = default;

	TokenStringWithNumbers(Deserializer &des)
	{
		size_t pos = 0;
		if (!SkipNonAlphaNum(des.data, pos)) {
			throw std::runtime_error("TokenStringClass: no max_len in serialized data");
		}
		_max_len = ParseDecAsInt<size_t>(des.data, pos);
		if (pos == des.data.size() || des.data[pos] != ':') {
			throw std::runtime_error("TokenStringClass: no sequence in serialized data");
		}
		_sequence = des.data.substr(pos + 1);
	}

	TokenStringWithNumbers(const StringView &s, size_t max_len = (size_t)-1)
	{
		Reinit(s, max_len);
	}

	void Reinit(const StringView &s, size_t max_len = (size_t)-1)
	{
		_max_len = max_len;
		_sequence.clear();
		_sequence.reserve(s.size());
		for (const auto &c : s) {
			if (c == '#') {
				_sequence+= '_';

			} else if (IsHex(c)) {
				if (_sequence.empty() || _sequence.back() != '#') {
					_sequence+= '#';
				}

			} else {
				_sequence+= c;
			}
		}
	}

	virtual bool Match(const StringView &value) const
	{
		if (value.size() < _sequence.size() || value.size() > _max_len) {
			return false;
		}

		auto seq_it = _sequence.begin();
		bool prev_matched_num = false;
		for (auto c : value) {
			if (seq_it == _sequence.end()) {
				return false;
			}
			prev_matched_num = false;
			if (*seq_it == '#') {
				if (IsHex(c)) {
					prev_matched_num = true;
					continue;
				}
				++seq_it;
				if (seq_it == _sequence.end()) {
					return false;
				}
			}
			if (*seq_it != c && (c != '#' || *seq_it != '_')) {
				return false;
			}
			++seq_it;
		}

		return (seq_it == _sequence.end()
			|| (prev_matched_num && *seq_it == '#' && (seq_it + 1) == _sequence.end()));
	}

	virtual void Serialize(OStream &os) const
	{
		os << '!' << _max_len << ':';
		for (const auto &c : _sequence) {
			os << c;
		}
		os << std::endl;
	}

	virtual size_t GetLengthMin() const
	{
		return _sequence.size();
	}

	virtual size_t GetLengthMax() const
	{
		return _max_len;
	}

private:
	size_t _max_len = (size_t)-1;
	String _sequence;
};

};

