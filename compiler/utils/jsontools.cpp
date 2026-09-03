/* ----------------------------------------------------------------------------
 * This file is originated here: https://github.com/nvitya/cpputils
 * Copyright (c) 2026 Viktor Nagy, nvitya
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 * and associated documentation files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:

 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * --------------------------------------------------------------------------- */
/*
 *  file:     jsontools.cpp
 *  created:  2026-01-06
 *  authors:  nvitya, ChatGPT-5.2
 *  brief:    Simple, fast and well designed JSON Parser and Writer.
 *            This library is created from the Pascal JsonTools here: https://github.com/nvitya/JsonTools
 *            The original Pascal version is translated by ChatGPT-5.2, with more iteration and hinting.
*/

#include "stdint.h"

#include "jsontools.h"

#include <sstream>
#include <fstream>
#include <cctype>
#include <limits>
#include <utility>
#include <charconv>

using namespace std;

/* ============================================================
 Parser / formatter helpers (internal)
 ============================================================ */
namespace
{

inline void Throw(const string & msg)
{
	throw EJsonException(msg);
}

inline void Require(bool cond, const string & msg)
{
	if (!cond)
		Throw(msg);
}

inline void SkipWs(const char *& p)
{
	while (*p && isspace(static_cast<unsigned char>(*p)))
		++p;
}

inline bool IsHex(char c)
{
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

inline uint32_t HexVal(char c)
{
	if (c >= '0' && c <= '9')
	{
		return uint32_t(c - '0');
	}
	if (c >= 'a' && c <= 'f')
	{
		return uint32_t(10 + (c - 'a'));
	}
	return uint32_t(10 + (c - 'A'));
}

inline void AppendUtf8(string & out, uint32_t cp)
{
	if (cp <= 0x7Fu)
	{
		out.push_back(static_cast<char>(cp));
	}
	else if (cp <= 0x7FFu)
	{
		out.push_back(static_cast<char>(0xC0u | (cp >> 6)));
		out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
	}
	else if (cp <= 0xFFFFu)
	{
		out.push_back(static_cast<char>(0xE0u | (cp >> 12)));
		out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
		out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
	}
	else if (cp <= 0x10FFFFu)
	{
		out.push_back(static_cast<char>(0xF0u | (cp >> 18)));
		out.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
		out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
		out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
	}
	else
	{
		out.append("\xEF\xBF\xBD"); // U+FFFD
	}
}

inline string ParseJsonString(const char *& p)
{
	Require(*p == '"', "Expected '\"' to start string");
	++p;

	string out;
	while (*p)
	{
		char c = *p++;
		if (c == '"')
		{
			break;
		}

		if (static_cast<unsigned char>(c) < 0x20)
		{
			Throw("Invalid control character in string");
		}

		if (c != '\\')
		{
			out.push_back(c);
			continue;
		}

		char e = *p++;
		switch (e)
		{
		case '"':
			out.push_back('"');
			break;
		case '\\':
			out.push_back('\\');
			break;
		case '/':
			out.push_back('/');
			break;
		case 'b':
			out.push_back('\b');
			break;
		case 'f':
			out.push_back('\f');
			break;
		case 'n':
			out.push_back('\n');
			break;
		case 'r':
			out.push_back('\r');
			break;
		case 't':
			out.push_back('\t');
			break;
		case 'u':
		{
			Require(IsHex(p[0]) && IsHex(p[1]) && IsHex(p[2]) && IsHex(p[3]), "Invalid \\u escape");
			uint32_t u = (HexVal(p[0]) << 12) | (HexVal(p[1]) << 8) | (HexVal(p[2]) << 4) | HexVal(p[3]);
			p += 4;

			if (u >= 0xD800u && u <= 0xDBFFu)
			{
				// high surrogate must be followed by \u low surrogate
				Require(p[0] == '\\' && p[1] == 'u', "Invalid surrogate pair");
				p += 2;
				Require(IsHex(p[0]) && IsHex(p[1]) && IsHex(p[2]) && IsHex(p[3]),  "Invalid \\u escape (low surrogate)");
				uint32_t l = (HexVal(p[0]) << 12) | (HexVal(p[1]) << 8) | (HexVal(p[2]) << 4) | HexVal(p[3]);
				p += 4;
				Require(l >= 0xDC00u && l <= 0xDFFFu, "Invalid low surrogate");
				uint32_t cp = 0x10000u + (((u - 0xD800u) << 10) | (l - 0xDC00u));
				AppendUtf8(out, cp);
			}
			else if (u >= 0xDC00u && u <= 0xDFFFu)
			{
				Throw("Unexpected low surrogate");
			}
			else
			{
				AppendUtf8(out, u);
			}

			break;
		}

		default:
			Throw("Invalid escape sequence in string");
		}
	}

	return out;
}

inline string EncodeJsonString(const string & s)
{
	string out;
	out.push_back('"');

	static const char *  hex = "0123456789ABCDEF";

	for (unsigned char uc : s)
	{
		char c = static_cast<char>(uc);
		switch (c)
		{
		case '"':
			out += "\\\"";
			break;
		case '\\':
			out += "\\\\";
			break;
		case '\b':
			out += "\\b";
			break;
		case '\f':
			out += "\\f";
			break;
		case '\n':
			out += "\\n";
			break;
		case '\r':
			out += "\\r";
			break;
		case '\t':
			out += "\\t";
			break;
		default:
			if (uc < 0x20)
			{
				out += "\\u00";
				out.push_back(hex[(uc >> 4) & 0xF]);
				out.push_back(hex[uc & 0xF]);
			}
			else
			{
				out.push_back(c);
			}
			break;
		}
	}

	out.push_back('"');
	return out;
}

inline string ParseNumberLexeme(const char *& p)
{
	// JSON number: -?(0|[1-9]\d*)(\.\d+)?([eE][+-]?\d+)?
	const char *start = p;

	if (*p == '-')
	{
		++p;
	}

	if (*p == '0')
	{
		++p;
	}
	else
	{
		Require(*p >= '1' && *p <= '9', "Invalid number");
		while (*p >= '0' && *p <= '9')
		{
			++p;
		}
	}

	if (*p == '.')
	{
		++p;

		Require(*p >= '0' && *p <= '9', "Invalid number fractional part");
		while (*p >= '0' && *p <= '9')
		{
			++p;
		}
	}

	if (*p == 'e' || *p == 'E')
	{
		++p;
		if (*p == '+' || *p == '-')
		{
			++p;
		}

		Require(*p >= '0' && *p <= '9', "Invalid number exponent");
		while (*p >= '0' && *p <= '9')
		{
			++p;
		}
	}

	return string(start, p);
}

static string DoubleToLexeme(double v)
{
	char buf[64];
	auto res = std::to_chars(buf, buf + sizeof(buf), v, std::chars_format::general, 15);   // 15 is enough for double

	if (res.ec != std::errc())
	{
		throw EJsonException("Double conversion failed");
	}

	return string(buf, res.ptr);
}

/* ---------- Path parsing ---------- */

struct TPathStep
{
	enum TType
	{
		Key, Index
	} Type;
	string KeyName;
	int IndexVal = -1;
};

inline vector<TPathStep> ParsePath(const string & path)
{
	// Supports: a.b.c, a/b/c, arr[0].x, arr[10]/x
	vector<TPathStep> steps;
	string cur;

	auto FlushKey = [&]()
	{
		if (!cur.empty())
		{
			steps.push_back(TPathStep	{	TPathStep::Key, cur, -1});
			cur.clear();
		}
	};

	for (size_t i = 0; i < path.size();)
	{
		char c = path[i];
		if (c == '.' || c == '/')
		{
			FlushKey();
			++i;
			continue;
		}

		if (c == '[')
		{
			FlushKey();
			++i;
			size_t j = i;
			while (j < path.size() && isdigit(static_cast<unsigned char>(path[j])))
				++j;
			Require(j > i, "Invalid path index");
			Require(j < path.size() && path[j] == ']', "Unterminated path index");
			int idx = stoi(path.substr(i, j - i));
			steps.push_back(TPathStep	{ TPathStep::Index, "", idx });
			i = j + 1;
			continue;
		}
		cur.push_back(c);
		++i;
	}

	FlushKey();
	return steps;
}

} // anonymous namespace

/* ============================================================
 Public types
 ============================================================ */

EJsonException::EJsonException(const string & msg) :
		runtime_error(msg)
{
}

/* ============================================================
 TJsonNode
 ============================================================ */

TJsonNode::TJsonNode() = default;

TJsonNode::TJsonNode(TJsonNodeKind k)
{
	SetKind(k);
}

/* ---------- property replacements ---------- */

TJsonNode * TJsonNode::GetRoot()
{
	TJsonNode *n = this;
	while (n->FParent)
	{
		n = n->FParent;
	}
	return n;
}

const TJsonNode * TJsonNode::GetRoot() const
{
	const TJsonNode *  n = this;
	while (n->FParent)
	{
		n = n->FParent;
	}
	return n;
}

TJsonNode * TJsonNode::GetParent()
{
	return FParent;
}
const TJsonNode * TJsonNode::GetParent() const
{
	return FParent;
}

TJsonNodeKind TJsonNode::GetKind() const
{
	return FKind;
}

void TJsonNode::SetKind(TJsonNodeKind k)
{
	FKind = k;
	FChildren.clear();

	switch (k)
	{
	case nkBool:
		FValue = "false";
		break;
	case nkNumber:
		FValue = "0";
		break;
	case nkString:
		FValue.clear();
		break;
	default:
		FValue.clear();
		break;
	}
}

string TJsonNode::GetName() const
{
	return FName;
}

void TJsonNode::SetName(const string & name)
{
	FName = name;
}

string TJsonNode::GetValue() const
{
	if (FKind == nkObject || FKind == nkArray)
	{
		return GetAsJson(true);
	}
	if (FKind == nkNull)
	{
		return "null";
	}
	return FValue;
}

void TJsonNode::SetValue(const string & json)
{
	Parse(json);
}

int TJsonNode::GetCount() const
{
	if (FKind == nkObject || FKind == nkArray)
	{
		return static_cast<int>(FChildren.size());
	}
	return 0;
}

string TJsonNode::GetAsJson(bool compact, const string & indent) const
{
	return compact ? FormatCompact() : FormatPretty("", indent);
}

void TJsonNode::SetAsJson(const string & json)
{
	Parse(json);
}

TJsonNode & TJsonNode::GetAsArray()
{
	if (FKind != nkArray)
	{
		SetKind(nkArray);
	}
	return *this;
}

TJsonNode & TJsonNode::GetAsObject()
{
	if (FKind != nkObject)
	{
		SetKind(nkObject);
	}
	return *this;
}

TJsonNode & TJsonNode::GetAsNull()
{
	SetKind(nkNull);
	return *this;
}

bool TJsonNode::GetAsBoolean() const
{
	if (FKind != nkBool)
	{
		Throw("Not a boolean");
	}
	return FValue == "true";
}

void TJsonNode::SetAsBoolean(bool v)
{
	FKind = nkBool;
	FValue = v ? "true" : "false";
	FChildren.clear();
}

string TJsonNode::GetAsString() const
{
	if (FKind == nkString)
	{
		return FValue;
	}
	if (FKind == nkNull)
	{
		return "null";
	}
	if (FKind == nkBool || FKind == nkNumber)
	{
		return FValue;
	}
	// for objects/arrays return compact JSON
	return GetAsJson(true);
}

void TJsonNode::SetAsString(const string & v)
{
	FKind = nkString;
	FValue = v;
	FChildren.clear();
}

double TJsonNode::GetAsNumber() const
{
	if (FKind != nkNumber)
	{
		Throw("Not a number");
	}
	return stod(FValue);
}

void TJsonNode::SetAsNumber(double v)
{
	FKind = nkNumber;
	FValue = DoubleToLexeme(v);
	FChildren.clear();
}

/* ---------- IO ---------- */

void TJsonNode::LoadFromStream(istream & is)
{
	ostringstream ss;
	ss << is.rdbuf();
	Parse(ss.str());
}

void TJsonNode::SaveToStream(ostream & os, bool compact) const
{
	os << GetAsJson(compact);
}

void TJsonNode::LoadFromFile(const string & fileName)
{
	ifstream f(fileName, ios::binary);
	if (!f)
	{
		Throw("Failed to open file for reading: " + fileName);
	}
	LoadFromStream(f);
}

void TJsonNode::SaveToFile(const string & fileName, bool compact) const
{
	ofstream f(fileName, ios::binary);
	if (!f)
	{
		Throw("Failed to open file for writing: " + fileName);
	}
	SaveToStream(f, compact);
}

/* ---------- building ---------- */

TJsonNode & TJsonNode::AddInternal(TJsonNodeKind kind, const string & name,  const string & value)
{
	auto n = make_unique<TJsonNode>(kind);
	n->FName = name;
	n->FValue = value;
	n->FParent = this;
	FChildren.emplace_back(move(n));
	return *FChildren.back();
}

TJsonNode & TJsonNode::Add(const string & name)
{
	GetAsObject();
	return AddInternal(nkNull, name, "");
}

TJsonNode & TJsonNode::Add(const string & name, const char * astr)
{
	GetAsObject();
	return AddInternal(nkString, name, string(astr));
}

TJsonNode & TJsonNode::Add(const string & name, int v)
{
	GetAsObject();
	return AddInternal(nkNumber, name, to_string(v));
}

TJsonNode & TJsonNode::Add(const string & name, long v)
{
	GetAsObject();
	return AddInternal(nkNumber, name, to_string(v));
}

TJsonNode & TJsonNode::Add(const string & name, long long v)
{
	GetAsObject();
	return AddInternal(nkNumber, name, to_string(v));
}

TJsonNode & TJsonNode::Add(const string & name, bool b)
{
	GetAsObject();
	return AddInternal(nkBool, name, b ? "true" : "false");
}

TJsonNode & TJsonNode::Add(const string & name, double d)
{
	GetAsObject();
	return AddInternal(nkNumber, name, DoubleToLexeme(d));
}

TJsonNode & TJsonNode::Add(const string & name, const string & s)
{
	GetAsObject();
	return AddInternal(nkString, name, s);
}

TJsonNode & TJsonNode::Add()
{
	GetAsArray();
	return AddInternal(nkNull, "", "");
}

/* ---------- deletion ---------- */

void TJsonNode::RequireCollection() const
{
	if (FKind != nkObject && FKind != nkArray)
	{
		Throw("Not a collection");
	}
}

void TJsonNode::Delete(int index)
{
	RequireCollection();
	if (index < 0 || index >= static_cast<int>(FChildren.size()))
	{
		Throw("Index out of range");
	}
	FChildren.erase(FChildren.begin() + index);
}

void TJsonNode::Delete(const string & name)
{
	RequireCollection();
	if (FKind != nkObject)
	{
		Throw("Delete(name) requires object");
	}
	for (auto it = FChildren.begin(); it != FChildren.end(); ++it)
	{
		if ((*it)->FName == name)
		{
			FChildren.erase(it);
			return;
		}
	}
}

void TJsonNode::Clear()
{
	if (FKind == nkObject || FKind == nkArray)
	{
		FChildren.clear();
	}
}

/* ---------- child ---------- */

TJsonNode & TJsonNode::Child(int index)
{
	RequireCollection();
	if (index < 0 || index >= static_cast<int>(FChildren.size()))
	{
		Throw("Index out of range");
	}
	return *FChildren[static_cast<size_t>(index)];
}

const TJsonNode & TJsonNode::Child(int index) const
{
	RequireCollection();
	if (index < 0 || index >= static_cast<int>(FChildren.size()))
	{
		Throw("Index out of range");
	}
	return *FChildren[static_cast<size_t>(index)];
}

TJsonNode * TJsonNode::Child(const string & name)
{
	if (FKind != nkObject)
	{
		return nullptr;
	}

	for (auto &c : FChildren)
	{
		if (c->FName == name)
		{
			return c.get();
		}
	}
	return nullptr;
}

const TJsonNode * TJsonNode::Child(const string & name) const
{
	if (FKind != nkObject)
	{
		return nullptr;
	}

	for (auto &c : FChildren)
	{
		if (c->FName == name)
		{
			return c.get();
		}
	}
	return nullptr;
}

/* ---------- parsing core ---------- */

void TJsonNode::Parse(const string & json)
{
	const char *p = json.c_str();
	SkipWs(p);

	// Pascal behavior: root must be object or array
	if (!FParent)
	{
		Require(*p == '{' || *p == '[', "Root JSON must be object or array");
	}

	ParseValueIntoThis(p);
	SkipWs(p);
	Require(*p == '\0', "Trailing content after JSON");
}

bool TJsonNode::TryParse(const string & json) noexcept
{
	try
	{
		Parse(json);
		return true;
	}
	catch (...)
	{
		return false;
	}
}

#define JT_THROW(MSG) throw EJsonException(MSG)

#define JT_SKIPWS(P) do { \
  while (*(P) && isspace((unsigned char)*(P))) ++(P); \
} while (0)

void TJsonNode::ParseValueIntoThis(const char *& p)
{
	JT_SKIPWS(p);

	char c = *p;

	// ---- object ----
	if (c == '{')
	{
		SetKind(nkObject);
		++p;
		JT_SKIPWS(p);

		if (*p == '}')
		{
			++p;
			return;
		}

		for (;;)
		{
			JT_SKIPWS(p);
			if (*p != '"')
				JT_THROW("Expected object key string");

			string key = ParseJsonString(p);

			JT_SKIPWS(p);
			if (*p != ':')
				JT_THROW("Expected ':' after object key");
			++p;

			TJsonNode &child = AddInternal(nkNull, key, "");
			child.ParseValueIntoThis(p);

			JT_SKIPWS(p);
			c = *p;
			if (c == ',')
			{
				++p;
				continue;
			}
			if (c == '}')
			{
				++p;
				break;
			}
			JT_THROW("Expected ',' or '}' in object");
		}
		return;
	}

	// ---- array ----
	if (c == '[')
	{
		SetKind(nkArray);
		++p;
		JT_SKIPWS(p);

		if (*p == ']')
		{
			++p;
			return;
		}

		for (;;)
		{
			TJsonNode &child = AddInternal(nkNull, "", "");
			child.ParseValueIntoThis(p);

			JT_SKIPWS(p);
			c = *p;
			if (c == ',')
			{
				++p;
				continue;
			}
			if (c == ']')
			{
				++p;
				break;
			}
			JT_THROW("Expected ',' or ']' in array");
		}
		return;
	}

	// ---- string ----
	if (c == '"')
	{
		SetKind(nkString);
		FValue = ParseJsonString(p);
		return;
	}

	// ---- true ----
	if (c == 't')
	{
		if (!(p[1] == 'r' && p[2] == 'u' && p[3] == 'e'))
			JT_THROW("Invalid token");
		p += 4;
		SetAsBoolean(true);
		return;
	}

	// ---- false ----
	if (c == 'f')
	{
		if (!(p[1] == 'a' && p[2] == 'l' && p[3] == 's' && p[4] == 'e'))
			JT_THROW("Invalid token");
		p += 5;
		SetAsBoolean(false);
		return;
	}

	// ---- null ----
	if (c == 'n')
	{
		if (!(p[1] == 'u' && p[2] == 'l' && p[3] == 'l'))
			JT_THROW("Invalid token");
		p += 4;
		SetKind(nkNull);
		return;
	}

	// ---- number ----
	if (c == '-' || (c >= '0' && c <= '9'))
	{
		SetKind(nkNumber);
		FValue = ParseNumberLexeme(p);
		return;
	}

	JT_THROW("Unexpected character while parsing JSON");
}

/* ---------- formatting ---------- */

string TJsonNode::FormatCompact() const
{
	ostringstream o;

	switch (FKind)
	{
	case nkNull:
		return "null";
	case nkBool:
		return FValue;
	case nkNumber:
		return FValue;
	case nkString:
		return EncodeJsonString(FValue);

	case nkArray:
		o << "[";
		for (size_t i = 0; i < FChildren.size(); ++i)
		{
			if (i)  o << ",";

			o << FChildren[i]->FormatCompact();
		}
		o << "]";
		return o.str();

	case nkObject:
		o << "{";
		for (size_t i = 0; i < FChildren.size(); ++i)
		{
			if (i)	o << ",";
			o << EncodeJsonString(FChildren[i]->FName) << ":" << FChildren[i]->FormatCompact();
		}
		o << "}";
		return o.str();
	}

	return
	{};
}

string TJsonNode::FormatPretty(const string & pad, const string & indent) const
{
	ostringstream o;
	string npad = pad + indent;

	switch (FKind)
	{
	case nkNull:
		return "null";
	case nkBool:
		return FValue;
	case nkNumber:
		return FValue;
	case nkString:
		return EncodeJsonString(FValue);

	case nkArray:
		if (FChildren.empty())
		{
			return "[]";
		}

		o << "[\n";
		for (size_t i = 0; i < FChildren.size(); ++i)
		{
			o << npad << FChildren[i]->FormatPretty(npad, indent);
			if (i + 1 < FChildren.size())
			{
				o << ",";
			}
			o << "\n";
		}
		o << pad << "]";
		return o.str();

	case nkObject:
		if (FChildren.empty())
		{
			return "{}";
		}
		o << "{\n";
		for (size_t i = 0; i < FChildren.size(); ++i)
		{
			o << npad << EncodeJsonString(FChildren[i]->FName) << ": "  << FChildren[i]->FormatPretty(npad, indent);
			if (i + 1 < FChildren.size())
			{
				o << ",";
			}
			o << "\n";
		}
		o << pad << "}";
		return o.str();
	}

	return {};
}

/* ---------- path ---------- */

bool TJsonNode::Exists(const string & path) const
{
	const TJsonNode *out = nullptr;
	return Find(path, out);
}

TJsonNode * TJsonNode::Find(const string & path)
{
	TJsonNode *out = nullptr;
	if (!Find(path, out))
		return nullptr;
	return out;
}

const TJsonNode * TJsonNode::Find(const string & path) const
{
	const TJsonNode *  out = nullptr;
	if (!Find(path, out))
	{
		return nullptr;
	}
	return out;
}

bool TJsonNode::Find(const string & path, TJsonNode *& out)
{
	out = nullptr;
	TJsonNode *  cur = this;

	vector<TPathStep> steps = ParsePath(path);

	for (const auto &st : steps)
	{
		if (!cur)
		{
			return false;
		}

		if (st.Type == TPathStep::Key)
		{
			if (cur->FKind != nkObject)
			{
				return false;
			}
			cur = cur->Child(st.KeyName);
			if (!cur)
			{
				return false;
			}
		}
		else
		{
			if (cur->FKind != nkArray)
			{
				return false;
			}
			if ((st.IndexVal < 0) || (st.IndexVal >= static_cast<int>(cur->FChildren.size())))
			{
				return false;
			}
			cur = cur->FChildren[static_cast<size_t>(st.IndexVal)].get();
		}
	}

	out = cur;
	return (out != nullptr);
}

bool TJsonNode::Find(const string & path, const TJsonNode *& out) const
{
	out = nullptr;
	const TJsonNode *cur = this;

	vector<TPathStep> steps = ParsePath(path);

	for (const auto &st : steps)
	{
		if (!cur)
		{
			return false;
		}

		if (st.Type == TPathStep::Key)
		{
			if (cur->FKind != nkObject)
			{
				return false;
			}

			cur = cur->Child(st.KeyName);
			if (!cur)
			{
				return false;
			}
		}
		else
		{
			if (cur->FKind != nkArray)
			{
				return false;
			}
			if ((st.IndexVal < 0) || (st.IndexVal >= static_cast<int>(cur->FChildren.size())))
			{
				return false;
			}
			cur = cur->FChildren[static_cast<size_t>(st.IndexVal)].get();
		}
	}

	out = cur;
	return (out != nullptr);
}

TJsonNode & TJsonNode::Force(const string & path)
{
	TJsonNode *  cur = this;
	vector<TPathStep> steps = ParsePath(path);

	for (const auto &st : steps)
	{
		if (st.Type == TPathStep::Key)
		{
			// ensure object
			if (cur->FKind != nkObject)
			{
				cur->SetKind(nkObject);
			}

			TJsonNode *next = cur->Child(st.KeyName);
			if (!next)
			{
				// create as null for now; next step may convert to object/array
				next = &cur->AddInternal(nkNull, st.KeyName, "");
			}
			cur = next;
		}
		else
		{
			// ensure array
			if (cur->FKind != nkArray)
			{
				cur->SetKind(nkArray);
			}

			int idx = st.IndexVal;
			Require(idx >= 0, "Negative array index in path");

			while (static_cast<int>(cur->FChildren.size()) <= idx)
			{
				cur->AddInternal(nkNull, "", "");
			}
			cur = cur->FChildren[static_cast<size_t>(idx)].get();
		}
	}

	return *cur;
}
