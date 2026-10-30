#include "json.hpp"
#include "detail/json_first_pass_reader.hpp"
#include "detail/json_numeric_locale.hpp"
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <cmath>
#include <stdexcept>
#include <string>
#include <string_view>

namespace nijigenerate::autorig::json {
namespace {
class NumericLocale {
public:
    NumericLocale() : handle_(detail::createArtifactNumericLocale()) {}
    ~NumericLocale() { detail::freeArtifactNumericLocale(handle_); }
    NumericLocale(const NumericLocale&) = delete;
    NumericLocale& operator=(const NumericLocale&) = delete;
    void* get() const noexcept { return handle_; }
private:
    void* handle_ = nullptr;
};

class Restorer {
public:
    Restorer(compat::RetainedStringSlice text, void* locale) : text_(std::move(text)), locale_(locale) {}
    void whitespace() {
        while (cursor_ < text_.size() && white(at(cursor_, 26))) ++cursor_;
    }
    std::size_t cursor() const noexcept { return cursor_; }
    void restore(core::settings::SettingsJsonValue& value) {
        whitespace();
        if (value.is_object()) {
            ++cursor_;
            whitespace();
            while (at(cursor_, 40) != '}') {
                const auto start = cursor_;
                skipString();
                auto key = detail::FirstPassReader(slice(start, cursor_, 41)).parseRoot().as_string();
                whitespace();
                ++cursor_;
                auto& object = value.as_object();
                // Default parseJSON never produces ordered objects; this is the
                // actual surviving AA entry, not an object-ref getter repair.
                auto* member = object.if_contains(key.view());
                if (!member) throw core::settings::SettingsJsonError(
                    compat::RetainedString("Key not found: " + std::string(key.view())),
                    compat::RetainedString::fromStaticLiteral("std/json.d"), 806);
                restore(*member);
                whitespace();
                if (at(cursor_, 45) == ',') { ++cursor_; whitespace(); }
                else break;
            }
            ++cursor_;
        } else if (value.is_array()) {
            ++cursor_;
            for (auto& child : value.as_array()) {
                restore(child);
                whitespace();
                if (at(cursor_, 53) == ',') ++cursor_;
            }
            whitespace();
            ++cursor_;
        } else if (value.is_string()) skipString();
        else {
            const auto start = cursor_;
            while (cursor_ < text_.size() && at(cursor_, 59) != ',' && at(cursor_, 59) != ']' &&
                at(cursor_, 60) != '}' && !white(at(cursor_, 60))) ++cursor_;
            if (value.is_double()) {
                const auto token = slice(start, cursor_, 63);
                value = detail::restoreArtifactDouble(token, locale_);
                if (!std::isfinite(value.as_double())) throw compat::ThrowableException(
                    detail::copyArtifactMessage("Nonfinite JSON artifact number"),
                    compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/json.d"), 76);
            }
        }
    }
private:
    compat::RetainedStringSlice text_;
    void* locale_;
    std::size_t cursor_ = 0;
    static bool white(char c) noexcept { return c == ' ' || c == '\r' || c == '\n' || c == '\t'; }
    char at(std::size_t i, std::size_t line) const {
        if (i >= text_.size()) throw compat::ngStaticError<compat::ArrayIndexError>(i, text_.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/json.d"), line);
        return text_.data()[i];
    }
    compat::RetainedStringSlice slice(std::size_t first, std::size_t last, std::size_t line) const {
        if (first > last || last > text_.size()) throw compat::ngStaticError<compat::ArraySliceError>(
            first, last, text_.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/json.d"), line);
        return text_.slice(first, last);
    }
    void skipString() {
        ++cursor_;
        while (cursor_ < text_.size()) {
            if (at(cursor_, 32) == '\\') cursor_ += 2;
            else if (at(cursor_++, 33) == '"') return;
        }
    }
};
} // namespace

core::settings::SettingsJsonValue ngParseAutoRigJson(compat::RetainedStringSlice text) {
    auto result = detail::FirstPassReader(text).parseRoot();
    NumericLocale locale;
    Restorer restorer(text, locale.get());
    restorer.restore(result);
    restorer.whitespace();
    if (restorer.cursor() != text.size()) throw compat::ThrowableException(
        detail::copyArtifactMessage("JSON artifact cursor mismatch"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/json.d"), 81);
    return result;
}

} // namespace nijigenerate::autorig::json
