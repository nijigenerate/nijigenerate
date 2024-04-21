module creator.core.selector.parser;

import std.string;
import std.conv;
import std.uni;
import std.utf;
import std.algorithm;
import std.array;
import creator.core.selector.tokenizer;

class Grammar {
public:
    enum Type {
        Token,
        And,
        Or,
        Empty,
        Reference,
        Invalid
    }

    struct Ref {
        string name;
        bool lazyEval;
    };

    int priority = 0;
    Type type;
    string name;
    union {
        Grammar[] subGrammars;
        Token token;
        Ref reference;
    }

    this(Grammar[] subGrammars) {
        this(0, subGrammars);
    }

    this(int priority, Grammar[] subGrammars) {
        this.priority = priority;
        type = Type.And;
        this.subGrammars = subGrammars[];
    }

    this(int priority, Token token) {
        this.priority = priority;
        type = Type.Token;
        this.token = token;
    }

    this(int priority, Type type) {
        this.priority = priority;
        this.type = type;
    }

    this(int priority, string refName, bool lazyEval = false) {
        this.priority = priority;
        this.type = Type.Reference;
        this.reference.name = refName;       
        this.reference.lazyEval = lazyEval;
    }

    this(int priority, Type type, Grammar[] subGrammars) {
        this.priority = priority;
        this.type = type;
        this.subGrammars = subGrammars;
    }

    this() {
        this.type = Type.Invalid;
    }

    Grammar dup() {
        Grammar result = new Grammar();
        result.type = type;
        result.priority = priority;
        if (type == Type.And || type == Type.Or) {
            result.subGrammars = subGrammars.map!(s=>s.dup).array;
        } else if (type == Type.Token) {
            result.token = token;
        } else if (type == Type.Reference) {
            result.reference.name = reference.name;
            result.reference.lazyEval = reference.lazyEval;
        }
        return result;
    }
    
    override
    string toString() {
        string base = name.length > 0? "%s: ".format(name) : "";
        switch (type) {
            case Type.Token:
                return base ~ "\"%s\"".format(token);
            case Type.And:
                return base ~ to!string(subGrammars.map!(t=>to!string(t)).array.join(" "));
            case Type.Or:
                if (subGrammars[$-1].type == Type.Empty)
                    return base ~ "{" ~ to!string(subGrammars[0..$-1].map!(t=>to!string(t)).array.join(" | ")) ~ "}?";
                return base ~ "{" ~ to!string(subGrammars.map!(t=>to!string(t)).array.join(" | ")) ~ "}";
            case Type.Empty:
                return base ~ "---";
            case Type.Reference:
                return base ~ "-->%s".format(reference.name);
            case Type.Invalid:
                return base ~ "<Invalid>";
            default:
                return base ~ "<Undefined>";
        }
    }
}


class EvalContext {
    Scanner scanner;
    Grammar target;
    bool matched = false;
    Token matchedToken = Token(Token.Type.Invalid);
    EvalContext[] subContexts;

    this(Scanner scanner, Grammar target) {
        this.scanner = scanner;
        this.target  = target;
    }

    override
    string toString() {
        string body;
        if (subContexts.length > 0)
            body = "◀" ~ (subContexts.map!(t=>t.toString()).array.join(", ")) ~ "▶";
        else
            body = (matchedToken.type != Token.Type.Invalid? matchedToken.literal : target.toString());
        return "%s%s".format(matched? "✅":"❎", body);
    }
}


class Parser {
private:
    Token dummyToken = Token(Token.Type.Invalid);
    Grammar empty = new Grammar(-1, Grammar.Type.Empty);

    void registerGrammar(string name, Grammar grammar) {
        grammars[name] = grammar;
        grammar.name = name;
    }

    Grammar _t(string literal) { 
        if (literal in tokenizer.reservedDict) 
            return new Grammar(0, *tokenizer.reservedDict[literal]);
        else
            return new Grammar(0, dummyToken);
    }

    Grammar _seq(Grammar[] grammars) {
        return new Grammar(0, Grammar.Type.And, grammars);
    }

    Grammar _or(Grammar[] grammars) {
        return new Grammar(0, Grammar.Type.Or, grammars);
    }

    Grammar _opt(Grammar grammar) {
        return new Grammar(0, Grammar.Type.Or, [grammar, empty]);
    }

    Grammar _opt(Grammar[] grammar) {
        return new Grammar(0, Grammar.Type.Or, [_seq(grammar), empty]);
    }

    Grammar _id() {
        return new Grammar(0, Token(Token.Type.Identifier));
    }

    Grammar _d() {
        return new Grammar(0, Token(Token.Type.Digits));
    }

    Grammar _str() {
        return new Grammar(0, Token(Token.Type.String));
    }

    Grammar _ref(string refName, bool lazyEval = false) {
        return new Grammar(0, refName, lazyEval);
    }

    EvalContext eval(EvalContext context) {
        import std.stdio;
        Grammar grammar = context.target;

//        writefln("GRM: %s", context.target);

        switch (grammar.type) {
            case Grammar.Type.And:
                context.matched = true;
                for (int i = 0; i < grammar.subGrammars.length; i ++) {
                    auto subContext = new EvalContext(context.scanner, grammar.subGrammars[i]);
                    auto result = eval(subContext);
                    context.subContexts ~= result;
                    context.scanner = result.scanner;
                    if (!result.matched) {
                        context.matched = false;
                        break;
                    }
                }
                break;

            case Grammar.Type.Or:
                EvalContext longestMatch = null;
                foreach (sub; grammar.subGrammars) {
                    auto subContext = new EvalContext(context.scanner.dup, sub);
                    auto result = eval(subContext);
                    if (result.matched) {
                        if (longestMatch is null) {
                            longestMatch = result;
                        } else {
                            if (longestMatch.scanner.index < result.scanner.index) {
                                longestMatch = result;
                            }
                        }
                    }
                }
                if (longestMatch !is null) {
                    context.subContexts = [longestMatch];
                    context.scanner = longestMatch.scanner;
                    context.matched = true;
                } else {
                    context.subContexts.length = 0;
                    context.matched = false;
                }
                break;

            case Grammar.Type.Token:
                Token next = context.scanner.scan();
                if (grammar.token == next) {
                    context.matchedToken = next;
                    context.matched = true;
                }
                break;

            case Grammar.Type.Empty:
                context.matched = true;
                break;

            case Grammar.Type.Reference:
                if (grammar.reference.name in grammars) {
                    Grammar referenced = grammars[grammar.reference.name].dup;
                    auto subContext = new EvalContext(context.scanner, referenced);
                    auto result = eval(subContext);
                    context = result;
                } else {
                    // TBD: Should handle internal error.
                }
                break;

            default:
                break;
        }
//        writefln("Ctx: %s   ==>   %s", grammar, context);
        return context;
    }

public:
    Grammar rootGrammar;
    Grammar[string] grammars;

    const static string ROOT = "query";

    Tokenizer tokenizer;
    this(Tokenizer tokenizer) {
        this.tokenizer = tokenizer;
        registerGrammar("value",          _or([_id, _d, _str]) );
        registerGrammar("attr",           _seq([_t("["), _id, _t("="), _ref("value"), _t("]"), _opt(_ref("attr"))]) );
        registerGrammar("args",           _seq([_id, _opt([_t(","), _ref("args") ])]) );
        registerGrammar("pseudoClass",    _seq([_t(":"), _id, _opt([_t("("), _ref("args"), _t(")")])]) );

        registerGrammar("typeIdQuery",    _seq([_id,                       _opt(_ref("pseudoClass")), _opt(_ref("attr")), _opt(_ref("subQuery"))]) );
        registerGrammar("nodeNameQuery",  _seq([_t("#"), _or([_id, _str]), _opt(_ref("pseudoClass")), _opt(_ref("attr")), _opt(_ref("subQuery"))]) );
        registerGrammar("nodeClassQuery", _seq([_t("."), _or([_id, _str]), _opt(_ref("pseudoClass")), _opt(_ref("attr")), _opt(_ref("subQuery"))]) );

        registerGrammar("subQuery",       _seq([_opt(_t(">")), _ref("query", true)]) );
        registerGrammar("query",          _or([_ref("typeIdQuery"), _ref("nodeNameQuery"), _ref("nodeClassQuery")]) );
    }

    EvalContext parse(string text) {
        Token[] tokens;
        size_t nextPosition;
        tokenizer.tokenize(text, 0, tokens, nextPosition);

        Scanner scanner = new Scanner(tokens);
        rootGrammar = grammars[ROOT].dup;
        EvalContext context = new EvalContext(scanner, rootGrammar);

        auto result = eval(context);

        return result;
    }
    
}