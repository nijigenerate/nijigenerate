module creator.core.selector.parser;

import std.string;
import std.conv;
import std.uni;
import std.utf;
import std.algorithm;
import std.array;
import std.typecons;
import creator.core.selector.tokenizer;

class Grammar {
public:
    enum Type {
        Token,
        And,
        Or,
        ExOr,
        Empty,
        Reference,
        Repeat,
        Invalid
    }

    struct Ref {
        string name;
        bool lazyEval;
    };

    int priority = 0;
    Type type;
    string name;
    bool forgetUnnamed = false;
    union {
        Grammar[] subGrammars;
        Token token;
        Ref reference;
    }
    int minimumCount = 0;

    this(Grammar[] subGrammars, string name = null) {
        this(0, subGrammars, name);
    }

    this(int priority, Grammar[] subGrammars, string name = null) {
        this.priority = priority;
        type = Type.And;
        this.subGrammars = subGrammars[];
        this.name = name;
    }

    this(int priority, Token token, string name = null) {
        this.priority = priority;
        type = Type.Token;
        this.token = token;
        this.name = name;
    }

    this(int priority, Type type, string name = null) {
        this.priority = priority;
        this.type = type;
        this.name = name;
    }

    this(int priority, string refName, bool lazyEval = false, string name = null) {
        this.priority = priority;
        this.type = Type.Reference;
        this.reference.name = refName;       
        this.reference.lazyEval = lazyEval;
        this.name = name;
    }

    this(int priority, Type type, Grammar[] subGrammars, string name = null) {
        this.priority = priority;
        this.type = type;
        this.subGrammars = subGrammars;
        this.name = name;
    }

    this() {
        this.type = Type.Invalid;
        this.name = "";
    }

    Grammar dup() {
        Grammar result = new Grammar();
        result.name = name;
        result.type = type;
        result.forgetUnnamed = forgetUnnamed;
        result.minimumCount = minimumCount;
        result.priority = priority;
        if (type == Type.And || type == Type.Or || type == Type.ExOr || type == Type.Repeat) {
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
            case Type.ExOr:
                if (subGrammars[$-1].type == Type.Empty)
                    return base ~ "{" ~ to!string(subGrammars[0..$-1].map!(t=>to!string(t)).array.join(" | ")) ~ "}?";
                return base ~ "{" ~ to!string(subGrammars.map!(t=>to!string(t)).array.join(" | ")) ~ "}";
            case Type.Repeat:
                    return base ~ "{" ~ to!string(subGrammars.map!(t=>to!string(t)).array.join(" ")) ~ "}%s".format(minimumCount > 0? "+":"*");
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
            body = (subContexts.map!(t=>t.toString()).array.join(" "));
        else
            body = (matchedToken.type != Token.Type.Invalid? matchedToken.literal : target.toString());
        return "%s%s%s%s".format(matched? "✅":"❎", target.name? "%s:◀".format(target.name):"", body, target.name? "▶":"");
    }
}

class AST {
    string name;
    AST[string] children;
    Token token;
    this(EvalContext context) {
        name = context.target? context.target.name: null;
        token = context.matchedToken;
        foreach (ctx; context.subContexts) {
            if (ctx.target && ctx.target.name)
                children[ctx.target.name] = new AST(ctx);
        }
    }

    AST opIndex(string name) {
        if (name in children)
            return children[name];
        return null;
    }

    int opApply(int delegate(string, AST) iter) {
        int result = 0;
        foreach (key; children.keys.sort!((a,b) => a<b)) {
            auto child = children[key];
            int r = iter(key, child);
            if (r) {
                result = r;
                break;
            }
        }
        return result;
    }

    override
    string toString() {
        return pretty(0);
    }

    string pretty(int index) {
        string prefix;
        foreach (i; 0..index) prefix ~= " ";
        string childrenDesc = children.length > 0 ? "\n%s".format(children.keys.sort!((a,b)=>a<b).map!(t=>children[t].pretty(index + 2)).array.join("\n")): "";
        return "%s◀%s: %s%s▶".format(prefix, name, token.literal, childrenDesc);
    }
}

class Parser {
protected:
    Token dummyToken = Token(Token.Type.Invalid);
    Grammar empty = new Grammar(-1, Grammar.Type.Empty);

    void registerGrammar(string name, Grammar grammar) {
        grammars[name] = grammar;
        grammar.name = name;
    }

    Grammar _t(string literal, string name = null) { 
        if (literal in tokenizer.reservedDict) 
            return new Grammar(0, *tokenizer.reservedDict[literal], name);
        else
            return new Grammar(0, dummyToken, name);
    }

    Grammar _seq(Grammar[] grammars, string name = null) {
        auto result = new Grammar(0, Grammar.Type.And, grammars, name);
        if (name) result.forgetUnnamed = true;
        return result;
    }

    Grammar _or(Grammar[] grammars, string name = null) {
        auto result = new Grammar(0, Grammar.Type.Or, grammars, name);
        if (name) result.forgetUnnamed = true;
        return result;
    }

    Grammar _xor(Grammar[] grammars, string name = null) {
        return new Grammar(0, Grammar.Type.ExOr, grammars, name);
    }

    Grammar _opt(Grammar grammar, string name = null) {
        return new Grammar(0, Grammar.Type.Or, [grammar, empty], name);
    }

    Grammar _opt(Grammar[] grammar, string name = null) {
        auto result = new Grammar(0, Grammar.Type.Or, [_seq(grammar), empty], name);
        if (name) result.forgetUnnamed = true;
        return result;
    }

    Grammar _id(string name = null) {
        return new Grammar(0, Token(Token.Type.Identifier), name);
    }

    Grammar _d(string name = null) {
        return new Grammar(0, Token(Token.Type.Digits), name);
    }

    Grammar _str(string name = null) {
        return new Grammar(0, Token(Token.Type.String), name);
    }

    Grammar _ref(string refName, bool lazyEval = false, string name = null) {
        return new Grammar(0, refName, lazyEval, name);
    }

    Grammar _repeat0(Grammar[] grammars, string name = null) {
        auto result = new Grammar(0, Grammar.Type.Repeat, grammars, name);
        if (name) result.forgetUnnamed = true;
        return result;
    }

    Grammar _repeat1(Grammar[] grammars, string name = null) {
        auto result = _repeat0(grammars, name);
        result.minimumCount = 1;
        return result;
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
                    if (!grammar.forgetUnnamed || result.target.name)
                        context.subContexts ~= result;
                    context.scanner = result.scanner;
                    if (!result.matched) {
                        context.matched = false;
                        break;
                    }
                }
                break;

            case Grammar.Type.Repeat:
                int i = 0;
                while (true) {
                    Grammar thisTry = grammar.dup;
                    thisTry.type = Grammar.Type.And;
                    thisTry.name = "%d".format(i);
                    auto subContext = new EvalContext(context.scanner.dup, thisTry);
                    auto result = eval(subContext);
                    if (result.matched) {
                        context.matched = true;
                        context.subContexts ~= result;
                        context.scanner = result.scanner;
                    } else {
                        break;
                    }
                    i ++;
                }
                if (i == 0) {
                    if (grammar.minimumCount == 0) {
                        context.target = empty;
                        context.matched = true;
                    } else {
                        context.matched = false;
                    }
                }
                break;

            case Grammar.Type.Or:
            case Grammar.Type.ExOr:
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
                        if (grammar.type == Grammar.Type.ExOr) break;
                    }
                }
                if (longestMatch !is null) {
                    if (grammar.name)
                        longestMatch.target.name = grammar.name;
                    context = longestMatch;
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
    Tokenizer tokenizer;

    this(Tokenizer tokenizer) {
        this.tokenizer = tokenizer;
    }

    AST parse(string text) {
        Token[] tokens;
        size_t nextPosition;
        tokenizer.tokenize(text, 0, tokens, nextPosition);

        Scanner scanner = new Scanner(tokens);
        EvalContext context = new EvalContext(scanner, rootGrammar);

        auto result = eval(context);

        return new AST(result);
    }
    
}

class SelectorParser : Parser {
public:
    const static string ROOT = "query";

    this(Tokenizer tokenizer) {
        super(tokenizer);
        registerGrammar("value",          _xor([_id, _d, _str]) );
        registerGrammar("attr",           _repeat1([_t("["), _id("name"), _xor([_t("=")], "matcher"), _ref("value"), _t("]")]) );
        registerGrammar("args",           _repeat1([_ref("value", false, "arg"), _opt(_t(","))]) );
        registerGrammar("pseudoClass",    _seq([_t(":"), _id("name"), _opt(_seq([_t("("), _ref("args"), _t(")")], "args"))]) );

        registerGrammar("selectors",      _repeat1([_xor([_t("#"), _t(".")], "kind"), _xor([_id, _str], "name")]) );

        registerGrammar("typeIdQuery",    _seq([_xor([_id, _t("*")], "typeId"), _opt(_ref("selectors")), _opt(_ref("pseudoClass")), _opt(_ref("attr"))]) );
        registerGrammar("attrQuery",      _seq([_ref("selectors"),                                     _opt(_ref("pseudoClass")), _opt(_ref("attr"))]) );

        registerGrammar("query",          _repeat1([_xor([_ref("typeIdQuery"), _ref("attrQuery")]), _opt(_t(">", "kind"))]) );

        foreach (grammar; grammars.byValue) {
            grammar.forgetUnnamed = true;
        }
        rootGrammar = grammars[ROOT].dup;
    }
};