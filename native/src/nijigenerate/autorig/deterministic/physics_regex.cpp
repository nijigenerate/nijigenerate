// Boolean projection of the authored physics expressions using Phobos's
// ordered Thompson/ShiftOr input semantics. Copyright Dmitry Olshansky, 2011-.
// BSL-1.0; see port/licenses/phobos-physics-regex.txt.
#include "detail/physics.hpp"
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/phobos_unicode_cursor.hpp>
#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <memory>
#include <new>
#include <regex>
#include <source_location>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nijigenerate::autorig::deterministic::physics {
namespace {
constexpr auto absent = (std::numeric_limits<std::size_t>::max)();

enum class SyntaxKind { Empty, Literal, Begin, End, Sequence, Choice, Optional, Negative };
struct Syntax {
    SyntaxKind kind;
    char32_t literal = 0;
    std::size_t left = absent, right = absent;
};
enum class Code { Literal, Begin, End, Split, Negative, Accept };
struct Instruction {
    Code code;
    char32_t literal = 0;
    std::size_t first = absent, second = absent;
    std::size_t assertionBegin = absent;
};
struct Program {
    compat::RetainedStringSlice pattern;
    std::vector<Instruction> code;
    std::vector<std::array<unsigned char, 256>> prefix;
    std::size_t entry = 0;
    bool oneShot = false;
};

// These are private, closed production patterns: all material-role expressions
// and the structure/control literal alternatives. No public general-regex API is
// exposed, and unsupported syntax is never treated as a successful/no match.
class PatternCompiler {
    compat::RetainedStringSlice patternOwner_;
    std::string_view pattern_;
    std::size_t position_ = 0;
    std::vector<Syntax> syntax_;

    [[noreturn]] static void unsupported() {
        throw std::regex_error(std::regex_constants::error_escape);
    }
    bool consume(char value) {
        if (position_ == pattern_.size() || pattern_[position_] != value) return false;
        ++position_;
        return true;
    }
    std::size_t add(SyntaxKind kind, std::size_t left = absent,
            std::size_t right = absent, char32_t literal = 0) {
        syntax_.push_back({kind, literal, left, right});
        return syntax_.size() - 1;
    }
    std::size_t choice() {
        auto result = sequence();
        while (consume('|')) {
            const auto right = sequence();
            result = add(SyntaxKind::Choice, result, right);
        }
        return result;
    }
    std::size_t sequence() {
        auto result = add(SyntaxKind::Empty);
        while (position_ != pattern_.size() && pattern_[position_] != ')' && pattern_[position_] != '|') {
            auto part = atom();
            if (consume('?')) part = add(SyntaxKind::Optional, part);
            result = add(SyntaxKind::Sequence, result, part);
        }
        return result;
    }
    std::size_t atom() {
        const auto token = static_cast<unsigned char>(pattern_[position_++]);
        if (token == '^') return add(SyntaxKind::Begin);
        if (token == '$') return add(SyntaxKind::End);
        if (token == '(') {
            bool negative = false;
            if (consume('?')) {
                if (consume(':')) {
                    // Noncapturing group; the caller consumes only match.empty.
                } else if (consume('!')) {
                    negative = true;
                } else if (consume('<')) {
                    const auto end = pattern_.find('>', position_);
                    if (end == pattern_.npos || end == position_) unsupported();
                    position_ = end + 1;
                } else unsupported();
            }
            const auto child = choice();
            if (!consume(')')) unsupported();
            return negative ? add(SyntaxKind::Negative, child) : child;
        }
        if (token == '\\') {
            if (position_ == pattern_.size()) unsupported();
            const auto escaped = static_cast<unsigned char>(pattern_[position_++]);
            if (escaped >= 0x80 || (escaped >= 'a' && escaped <= 'z') ||
                    (escaped >= 'A' && escaped <= 'Z') || (escaped >= '0' && escaped <= '9')) unsupported();
            return add(SyntaxKind::Literal, absent, absent, escaped);
        }
        if (token >= 0x80 || token == '.' || token == '[' || token == ']' || token == '*' ||
                token == '+' || token == '{' || token == '}' || token == '?' || token == ')' || token == '|')
            unsupported();
        return add(SyntaxKind::Literal, absent, absent, token);
    }
    enum class First { Skip, Begin, Other };
    First first(std::size_t index) const {
        const auto node = syntax_[index];
        if (node.kind == SyntaxKind::Empty || node.kind == SyntaxKind::End) return First::Skip;
        if (node.kind == SyntaxKind::Begin) return First::Begin;
        if (node.kind == SyntaxKind::Sequence) {
            const auto left = first(node.left);
            return left == First::Skip ? first(node.right) : left;
        }
        return First::Other;
    }
    static std::size_t emit(std::vector<Instruction>& code, Instruction instruction) {
        code.push_back(instruction);
        return code.size() - 1;
    }
    std::size_t lower(std::size_t index, std::size_t continuation, std::vector<Instruction>& code) const {
        const auto node = syntax_[index];
        switch (node.kind) {
            case SyntaxKind::Empty: return continuation;
            case SyntaxKind::Literal: return emit(code, {Code::Literal, node.literal, continuation});
            case SyntaxKind::Begin: return emit(code, {Code::Begin, 0, continuation});
            case SyntaxKind::End: return emit(code, {Code::End, 0, continuation});
            case SyntaxKind::Sequence: {
                const auto tail = lower(node.right, continuation, code);
                return lower(node.left, tail, code);
            }
            case SyntaxKind::Choice: {
                const auto left = lower(node.left, continuation, code);
                const auto right = lower(node.right, continuation, code);
                return emit(code, {Code::Split, 0, left, right});
            }
            case SyntaxKind::Optional: {
                const auto child = lower(node.left, continuation, code);
                return emit(code, {Code::Split, 0, child, continuation});
            }
            case SyntaxKind::Negative: {
                const auto end = emit(code, {Code::Accept});
                const auto child = lower(node.left, end, code);
                return emit(code, {Code::Negative, 0, child, continuation, end});
            }
        }
        unsupported();
    }
    std::vector<std::string> words(std::size_t index) const {
        const auto node = syntax_[index];
        if (node.kind == SyntaxKind::Literal) return {std::string(1, static_cast<char>(node.literal))};
        if (node.kind == SyntaxKind::Sequence) {
            const auto left = words(node.left), right = words(node.right);
            std::vector<std::string> result;
            for (const auto& a : left) for (const auto& b : right) result.push_back(a + b);
            return result;
        }
        if (node.kind == SyntaxKind::Choice) {
            auto result = words(node.left);
            auto right = words(node.right);
            for (auto& value : right) result.push_back(std::move(value));
            return result;
        }
        if (node.kind == SyntaxKind::Optional) {
            auto result = words(node.left);
            result.emplace_back();
            return result;
        }
        // ShiftOr ignores zero-width assertions while deriving its coarse prefix.
        return {std::string{}};
    }
public:
    explicit PatternCompiler(compat::RetainedStringSlice pattern)
        : patternOwner_(std::move(pattern)), pattern_(patternOwner_.view()) {}
    Program compile() {
        const auto root = choice();
        if (position_ != pattern_.size()) unsupported();
        Program result;
        result.pattern = patternOwner_;
        const auto end = emit(result.code, {Code::Accept});
        result.entry = lower(root, end, result.code);
        result.oneShot = first(root) == First::Begin;
        if (!result.oneShot) {
            const auto paths = words(root);
            std::size_t width = 32;
            for (const auto& path : paths) width = std::min(width, path.size());
            result.prefix.resize(width);
            for (auto& mask : result.prefix) mask.fill(0);
            for (const auto& path : paths) for (std::size_t i = 0; i < width; ++i)
                result.prefix[i][static_cast<unsigned char>(path[i])] = 1;
        }
        return result;
    }
};

struct Input {
    compat::RetainedStringSlice owner;
    std::string_view text = owner.view();
    std::size_t next = 0, index = 0;
    char32_t front = 0;
    bool atEnd() const { return index == text.size() && next == text.size(); }
    bool advance() {
        index = next;
        if (next == text.size()) return false;
        using Cursor = compat::phobos::detail::UnicodeCursor<std::string_view, char>;
        front = Cursor(text.substr(next)).front();
        next += front < 0x80 ? 1 : front < 0x800 ? 2 : front < 0x10000 ? 3 : 4;
        return true;
    }
    void reload(std::size_t position) { next = position; advance(); }
    bool search(const Program& program) {
        if (program.prefix.empty()) return advance();
        const auto width = program.prefix.size();
        for (auto candidate = next; candidate < text.size(); ++candidate) {
            if (width > text.size() - candidate) break;
            bool possible = true;
            for (std::size_t i = 0; i < width; ++i) {
                if (!program.prefix[i][static_cast<unsigned char>(text[candidate + i])]) {
                    possible = false;
                    break;
                }
            }
            if (possible) { next = candidate; return advance(); }
        }
        next = index = text.size();
        return false;
    }
};

struct Frame {
    std::vector<std::size_t> current, next, pending;
    std::vector<unsigned char> seen;
    void prepare(std::size_t count) {
        current.reserve(count);
        next.reserve(count);
        pending.reserve(count);
        seen.resize(count);
    }
    void beginPosition() {
        next.clear();
        std::fill(seen.begin(), seen.end(), 0);
    }
};

class Matcher {
    const Program& program_;
    std::vector<Instruction> ownedCode_;
    std::span<const Instruction> code_;
    std::size_t base_ = 0;
    Frame frame_;

    static std::vector<Instruction> duplicateCode(std::span<const Instruction> code) {
        try {
            return std::vector<Instruction>(code.begin(), code.end());
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
        }
    }

    bool evaluate(std::size_t pc, Input& input, bool withInput,
            bool& matched, std::size_t& matchEnd) {
        auto& frame = frame_;
        frame.pending.clear();
        frame.pending.push_back(pc);
        while (!frame.pending.empty()) {
            pc = frame.pending.back();
            frame.pending.pop_back();
            if (frame.seen[pc - base_]) continue;
            frame.seen[pc - base_] = 1;
            const auto instruction = code_[pc - base_];
            switch (instruction.code) {
                case Code::Literal:
                    if (withInput && !input.atEnd() && input.front == instruction.literal)
                        frame.next.push_back(instruction.first);
                    break;
                case Code::Begin:
                    if (input.index == 0) frame.pending.push_back(instruction.first);
                    break;
                case Code::End:
                    if (input.atEnd()) frame.pending.push_back(instruction.first);
                    break;
                case Code::Split:
                    // LIFO worklist: evaluate the higher-priority branch first.
                    frame.pending.push_back(instruction.second);
                    frame.pending.push_back(instruction.first);
                    break;
                case Code::Negative: {
                    auto probe = input;
                    const auto begin = instruction.assertionBegin;
                    const auto window = code_.subspan(begin - base_, pc - begin);
                    auto assertion = std::make_unique<Matcher>(program_, window, begin);
                    const auto accepted = assertion->anchored(instruction.first, probe);
                    input.reload(input.index);
                    if (!accepted) frame.pending.push_back(instruction.second);
                    break;
                }
                case Code::Accept:
                    matched = true;
                    matchEnd = input.index;
                    frame.pending.clear();
                    // Cut lower-priority current/epsilon work, but preserve
                    // higher-priority continuations already queued in next.
                    return true;
            }
        }
        return false;
    }
    void current(Input& input, bool withInput,
            bool& matched, std::size_t& matchEnd) {
        auto& frame = frame_;
        for (auto pc : frame.current)
            if (evaluate(pc, input, withInput, matched, matchEnd)) break;
        frame.current.clear();
    }
    bool anchored(std::size_t entry, Input& input) {
        auto& frame = frame_;
        frame.current.clear();
        frame.current.push_back(entry);
        bool matched = false;
        std::size_t matchEnd = input.index;
        for (;;) {
            frame.beginPosition();
            current(input, !input.atEnd(), matched, matchEnd);
            if (frame.next.empty() || input.atEnd()) break;
            frame.current.swap(frame.next);
            input.advance();
        }
        if (!matched) {
            frame.beginPosition();
            evaluate(entry, input, false, matched, matchEnd);
        }
        return matched;
    }
public:
    explicit Matcher(const Program& program)
        : program_(program), code_(program.code.data(), program.code.size()) {
        frame_.prepare(code_.size());
    }
    Matcher(const Program& program, std::span<const Instruction> code, std::size_t base)
        : program_(program), ownedCode_(duplicateCode(code)),
          code_(ownedCode_.data(), ownedCode_.size()), base_(base) {
        // Source fwdMatcher creates this object and duplicates its actual code
        // window only when the lookahead opcode executes, not before input.
        frame_.prepare(code_.size());
    }
    bool first(compat::RetainedStringSlice text) {
        Input input{std::move(text)};
        if (program_.oneShot) {
            input.advance();
            return anchored(program_.entry, input);
        }
        auto& frame = frame_;
        input.search(program_);
        bool matched = false;
        std::size_t matchEnd = input.index;
        while (!input.atEnd()) {
            frame.beginPosition();
            current(input, true, matched, matchEnd);
            if (!matched) evaluate(program_.entry, input, true, matched, matchEnd);
            else if (frame.next.empty()) break;
            frame.current.swap(frame.next);
            if (frame.current.empty()) {
                if (!input.search(program_)) break;
            } else if (!input.advance()) break;
        }
        frame.beginPosition();
        current(input, false, matched, matchEnd);
        if (!matched) evaluate(program_.entry, input, false, matched, matchEnd);
        if (matched) input.reload(matchEnd);
        return matched;
    }
};
} // namespace

bool detail::physicsRegexSearch(compat::RetainedStringSlice text, compat::RetainedStringSlice pattern,
        std::source_location source) {
    try {
        const auto program = PatternCompiler(std::move(pattern)).compile();
        return std::make_unique<Matcher>(program)->first(std::move(text));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(source);
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(source);
    }
}
} // namespace nijigenerate::autorig::deterministic::physics
