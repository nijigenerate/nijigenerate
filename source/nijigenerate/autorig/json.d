module nijigenerate.autorig.json;

import std.json : JSONValue, JSONType, parseJSON;
import std.exception : enforce;
import std.math : isFinite;

version (Windows) {
    import core.stdc.locale : LC_NUMERIC;
    private extern(C) {
        void* _create_locale(int category, const char* name);
        void _free_locale(void* locale);
        double _strtod_l(const char* text, char** end, void* locale);
    }
}

/** Preserve binary doubles when loading owned JSON artifacts, without changing global locale. */
JSONValue ngParseAutoRigJson(string text) {
    auto result = parseJSON(text);
    version (Windows) {
        auto locale = _create_locale(LC_NUMERIC,"C".ptr);
        enforce(locale !is null,"Could not create JSON numeric locale");
        scope(exit) _free_locale(locale);
    }
    size_t cursor;
    void whitespace() {
        while (cursor<text.length && (text[cursor] == ' ' || text[cursor] == '\r' ||
            text[cursor] == '\n' || text[cursor] == '\t')) ++cursor;
    }
    void skipString() {
        ++cursor;
        while (cursor<text.length) {
            if (text[cursor] == '\\') cursor += 2;
            else if (text[cursor++] == '"') return;
        }
    }
    void restore(ref JSONValue value) {
        whitespace();
        if (value.type == JSONType.object) {
            ++cursor; whitespace();
            while (text[cursor] != '}') {
                auto start = cursor; skipString();
                auto key = parseJSON(text[start .. cursor]).str;
                whitespace(); ++cursor;
                restore(value[key]); whitespace();
                if (text[cursor] == ',') { ++cursor; whitespace(); }
                else break;
            }
            ++cursor;
        } else if (value.type == JSONType.array) {
            ++cursor;
            foreach (ref child; value.array) {
                restore(child); whitespace();
                if (text[cursor] == ',') ++cursor;
            }
            whitespace(); ++cursor;
        } else if (value.type == JSONType.string) skipString();
        else {
            auto start = cursor;
            while (cursor<text.length && text[cursor] != ',' && text[cursor] != ']' &&
                text[cursor] != '}' && text[cursor] != ' ' && text[cursor] != '\r' &&
                text[cursor] != '\n' && text[cursor] != '\t') ++cursor;
            if (value.type == JSONType.float_) {
                auto token = text[start .. cursor];
                version (Windows) {
                    import std.string : toStringz;
                    char[128] buffer;
                    const(char)* input;
                    if (token.length<buffer.length) {
                        buffer[0 .. token.length] = token[]; buffer[token.length] = 0; input = buffer.ptr;
                    } else input = token.toStringz;
                    value.floating = _strtod_l(input,null,locale);
                } else {
                    import std.conv : parse;
                    value.floating = cast(double)parse!real(token);
                }
                enforce(isFinite(value.floating),"Nonfinite JSON artifact number");
            }
        }
    }
    restore(result); whitespace();
    enforce(cursor == text.length,"JSON artifact cursor mismatch");
    return result;
}
