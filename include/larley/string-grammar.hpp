#pragma once

#include <array>
#include <regex>
#include <string_view>
#include <variant>
#include <vector>

#include "semantics.hpp"

namespace larley
{

namespace StringGrammar
{

using Str = std::string;

using LiteralTerminalSymbol = Str;

using ChoiceTerminalSymbol = std::vector<LiteralTerminalSymbol>;
using RangeTerminalSymbol = std::pair<LiteralTerminalSymbol, LiteralTerminalSymbol>;
using RegexTerminalSymbol = std::regex;

using TerminalSymbol = std::variant<LiteralTerminalSymbol, ChoiceTerminalSymbol, RangeTerminalSymbol, RegexTerminalSymbol>;

int match(std::span<const char> data, size_t index, const TerminalSymbol& symbol)
{
    return std::visit(
        overloaded{
            [&](const LiteralTerminalSymbol& symbol) -> int
            {
                if (index + symbol.size() > data.size())
                {
                    return -1;
                }

                const std::string_view subsrc(data.begin() + index, data.begin() + index + symbol.size());
                const auto matches = subsrc == symbol;

                return matches ? static_cast<int>(subsrc.size()) : -1;
            },
            [&](const ChoiceTerminalSymbol& symbol) -> int
            {
                for (const auto& partial : symbol)
                {
                    if (index + partial.size() >= data.size())
                    {
                        continue;
                    }

                    const std::string_view subsrc(data.begin() + index, data.begin() + index + partial.size());
                    if (const auto matches = subsrc == partial)
                    {
                        return static_cast<int>(subsrc.size());
                    }
                }

                return -1;
            },
            [&](const RangeTerminalSymbol& symbol) -> int
            {
                if (index >= data.size())
                {
                    return -1;
                }

                if (const auto matches = data[index] >= symbol.first[0] && data[index] <= symbol.second[0])
                {
                    return 1;
                }

                return -1;
            },
            [&](const RegexTerminalSymbol& symbol) -> int
            {
                if (index >= data.size())
                {
                    return -1;
                }

                if (std::cmatch match; std::regex_search(data.data() + index, data.data() + data.size(), match, symbol, std::regex_constants::match_continuous))
                {
                    return match[0].length();
                }

                return -1;
            }},
        symbol);
}
} // namespace StringGrammar

template <typename ParserTypes>
struct StringGrammarBuilder
{
    using NT = ParserTypes::NonTerminal;
    using LT = ParserTypes::Terminal;

    NT startSymbol;
    std::vector<Rule<ParserTypes>> rules;
    Semantics<ParserTypes> semantics;

    using Str = StringGrammar::Str;

    StringGrammarBuilder(NT startSymbol) : startSymbol { startSymbol }
    {

    }

    struct StringSymbolBuilder
    {
      protected:
        StringSymbolBuilder(StringGrammar::TerminalSymbol symbol) : symbol{std::move(symbol)}
        {

        }

      public: 
        StringGrammar::TerminalSymbol symbol;
    };

    struct Range : public StringSymbolBuilder
    {
        explicit Range(const Str& start, const Str& end) : StringSymbolBuilder(StringGrammar::RangeTerminalSymbol{start, end})
        {
        }
    };

    struct Choice : public StringSymbolBuilder
    {
        template <typename... Args>
        explicit Choice(Args... args) : StringSymbolBuilder(StringGrammar::ChoiceTerminalSymbol{args...})
        {
        }
    };

    struct Regex : public StringSymbolBuilder
    {
        template <typename... Args>
        explicit Regex(Args... args) : StringSymbolBuilder(StringGrammar::RegexTerminalSymbol{args...})
        {
        }
    };

    struct RuleBuilder
    {
        StringGrammarBuilder& grammarBuilder;

        Rule<ParserTypes> rule;
        Semantics<ParserTypes>::SemanticAction action;

        RuleBuilder& operator&(const StringSymbolBuilder& symbolBuilder)
        {
            rule.symbols.emplace_back(symbolBuilder.symbol);
            return *this;
        }

        RuleBuilder& operator&(const NT& token)
        {
            rule.symbols.emplace_back(token);
            return *this;
        }

        RuleBuilder& operator&(const LT& token)
        {
            rule.symbols.emplace_back(token);
            return *this;
        }

        template <typename T>
        RuleBuilder& operator>>(T&& t)
        {
            return *this & std::forward<T>(t);
        }

        void operator|(Semantics<ParserTypes>::SemanticAction semanticAction)
        {
            action = semanticAction;
        }

        ~RuleBuilder()
        {
            rule.id = grammarBuilder.rules.size();
            grammarBuilder.semantics.setAction(rule.id, action);
            grammarBuilder.rules.emplace_back(std::move(rule));
        }
    };

    auto operator()(NT token)
    {
        return RuleBuilder{*this, token};
    }

    template <typename... Args>
    static auto make(Args... args)
    {
        return std::array<Rule<ParserTypes>, sizeof...(args)>{args.rule...};
    }

    using Terminal = char;

    Grammar<ParserTypes> getGrammar() const
    {
        return {startSymbol, rules};
    }

    const Semantics<ParserTypes>& getSemantics() const
    {
        return semantics;
    }

    ParserInputs<ParserTypes> makeInputs(const Str& str) const
    {
        return 
        {
            {str.begin(), str.size()},
            Grammar<ParserTypes>{startSymbol, rules},
            &StringGrammar::match,
            semantics
        };
    }
};

} // namespace larley