module creator.core.selector.tokenizer;

import std.string;
import std.conv;
import std.uni;
import std.utf;
import std.algorithm;
import std.array;

struct Token {
public:
    enum Type {
        Identifier,     // [a-zA-Z]([a-zA-Z0-9_])*
        Digits,         // [0-9]\d(\.[0-9])*
        Equals,         // ==
        NotEquals,      // !=
        Greater,        // >
        GreaterEq,      // >=
        Less,           // <
        LessEq,         // <=
        Assign,         // =
        LLParen,        // [
        RLParen,        // ]
        LMParen,        // {
        RMParen,        // }
        LSParen,        // (
        RSParen,        // )
        Comma,          // ,
        Dot,            // .
        Quote,          // "
        Space,          // [\s\t\n]*
        Colon,          // :
        Plus,           // +
        Minus,          // -
        Multiply,       // *
        Division,       // /
        Sharp,          // #
        Hash,           // |
        Dollar,         // $
        Question,       // ?
        String,         // \"[^\"]*\"
        Invalid,
    };
    Type type;
    string literal;
    this(Type type, string literal) {
        this.type = type;
        this.literal = literal;
    }
    this(Type type) {
        this.type = type;
        this.literal = "";
    }

    bool match(string text) {
        if (text.length >= literal.length) {
            if (text[0..literal.length] == literal) {
                return true;
            }
        }
        return false;
    }

    string toString() {
        switch (type) {
            case Type.Identifier:
                if (literal == "") return "<ID>";
                return literal;
            case Type.Digits:
                if (literal == "") return "<##>";
                return literal;
            case Type.String:
                if (literal == "") return "<TEXT>";
                return literal;
            default:
                return literal;
        }
    }

    bool opEquals(ref Token rhs) {
        if (type == Type.Identifier || type == Type.Digits || type == Type.String) {
            if (literal.length == 0)
                return type == rhs.type;
            else
                return type == rhs.type && literal == rhs.literal;
        } else {
            return type == rhs.type;
        }
    }

    bool equals(Type rhs) {
        return type == rhs;
    }

    bool equals(string rhs) {
        return literal == rhs;
    }

}

class Tokenizer {
public:
    Token[] reservedWord;
    Token*[string] reservedDict;

    this() {
        reservedWord ~= Token(Token.Type.Equals,    "==");
        reservedWord ~= Token(Token.Type.NotEquals, "!=");
        reservedWord ~= Token(Token.Type.GreaterEq, ">=");
        reservedWord ~= Token(Token.Type.LessEq,    "<=");
        reservedWord ~= Token(Token.Type.Greater,   ">");
        reservedWord ~= Token(Token.Type.Less,      "<");
        reservedWord ~= Token(Token.Type.Assign,    "=");
        reservedWord ~= Token(Token.Type.LLParen,   "[");
        reservedWord ~= Token(Token.Type.RLParen,   "]");
        reservedWord ~= Token(Token.Type.LMParen,   "{");
        reservedWord ~= Token(Token.Type.RMParen,   "}");
        reservedWord ~= Token(Token.Type.LSParen,   "(");
        reservedWord ~= Token(Token.Type.RSParen,   ")");
        reservedWord ~= Token(Token.Type.Comma,     ",");
        reservedWord ~= Token(Token.Type.Dot,       ".");
        reservedWord ~= Token(Token.Type.Colon,     ":");
        reservedWord ~= Token(Token.Type.Plus,      "+");
        reservedWord ~= Token(Token.Type.Minus,     "-");
        reservedWord ~= Token(Token.Type.Multiply,  "*");
        reservedWord ~= Token(Token.Type.Division,  "/");
        reservedWord ~= Token(Token.Type.Sharp,     "#");
        reservedWord ~= Token(Token.Type.Hash,      "|");
        reservedWord ~= Token(Token.Type.Dollar,    "$");
        reservedWord ~= Token(Token.Type.Question,  "?");

        foreach (i; 0..reservedWord.length) {
            reservedDict[reservedWord[i].literal] = &reservedWord[i];
        }
    }

    void tokenize(string text, size_t position, out Token[] tokens, out size_t nextPosition, bool skipSpace = true) {
        tokens.length = 0;
        size_t i = position;
        while (i < text.length) {
//            import std.stdio;
//            writefln("tok: %s", text[i..text.length]);
            // Check reserved words.
            bool found = false;
            foreach (token; reservedWord) {
                if (token.match(text[i..$])) {
                    tokens ~= token;
                    i += token.literal.length;
                    found = true;
                    break;
                }
            }
            if (found) continue;

            dchar head = text.decode(i);

            // Check Identifier
            if (isAlpha(head)) {
                string literal;
                literal ~= to!string(head);
                if (i < text.length) {
                    dchar next = text.decode(i);
                    while (true) {
                        if (!isAlphaNum(next) && next != '-' && next != '_') {
                            i -= to!string(next).stride(0);
                            break;
                        }
                        literal ~= to!string(next);
                        if (i >= text.length) break;
                        next = text.decode(i);
                    }
                }
                auto token = Token(Token.Type.Identifier, literal);
                tokens ~= token;
                continue;
            }

            // Check Digits
            if (isNumber(head)) {
                string literal;
                literal ~= to!string(head);
                if (i < text.length) {
                    dchar next = text.decode(i);
                    while (true) {
                        if (!isNumber(next)) {
                            break;
                        }
                        literal ~= to!string(next);
                        if (i >= text.length) break;
                        next = text.decode(i);
                    }
                    if (next == '.') {
                        literal ~= ".";
                        next = text.decode(i);
                        while (true) {
                            if (!isNumber(next)) {
                                i -= to!string(next).stride(0);
                                break;
                            }
                            literal ~= to!string(next);
                            if (i >= text.length) break;
                            next = text.decode(i);
                        }
                    } else {
                        i -= to!string(next).stride(0);
                    }
                }
                auto token = Token(Token.Type.Digits, literal);
                tokens ~= token;
                continue;
            }

            // Check Space
            if (isSpace(head)) {
                string literal;
                literal ~= to!string(head);
                if (i < text.length) {
                    dchar next = text.decode(i);
                    while (true) {
                        if (!isSpace(next)) {
                            i -= to!string(next).stride(0);
                            break;
                        }
                        literal ~= to!string(next);
                        if (i >= text.length) break;
                        next = text.decode(i);
                    }
                }
                if (!skipSpace) {
                    auto token = Token(Token.Type.Identifier, literal);
                    tokens ~= token;
                }
                continue;
            }

            // Check String
            if (head == '"') {
                string literal;
                if (i >= text.length) {
                    // Error
                } else {
                    dchar next = text.decode(i);
                    while (next != '"') {
                        literal ~= to!string(next);
                        if (i >= text.length) break;
                        next = text.decode(i);
                    }
                    i -= to!string(next).stride(0);
                    if (next != '"') {
                        // Error
                    }
                    auto token = Token(Token.Type.String, literal);
                    tokens ~= token;
                    i ++;
                }
                continue;
            }

            // Error

        }
        nextPosition = i;
    }
}

class Scanner {
public:
    Token[] tokens;
    int index = 0;

    this(Token[] tokens) {
        this.tokens = tokens;
        index = 0;
    }

    Token scan() {
        import std.stdio;
        if (index < tokens.length) {
//            writefln("Scan: %s", tokens[index].literal);
            return tokens[index++];
        } else {
            return Token(Token.Type.Invalid);
        }
    }

    void rewind(int num) {
        if (index - num >= 0) {
            index -= num;
        } else {
            index = 0;
        }
    }

    Scanner dup() {
        auto scanner = new Scanner(tokens);
        scanner.index = index;
        return scanner;
    }

    bool isEnd() {
        return index >= tokens.length;
    }
}