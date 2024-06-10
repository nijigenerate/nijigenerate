module nijigenerate.core.selector.parser;

import std.string;
import std.conv;
import std.uni;
import std.utf;
import std.algorithm: remove, map, sort, canFind;
import std.array;
import std.typecons;
import nijigenerate.core.selector.tokenizer;
import std.stdio;

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

    this() {
        this.name = null;
    }

    this(string name) {
        this.name = name;
    }

    this(string name, Token token) {
        this.name = name;
        this.token = token;
    }

    this(string name, AST[string] children) {
        this.name = name;
        this.children = children;
    }

    this(string name, AST[] children) {
        this.name = name;
        foreach (c; children) {
            this.children[c.name] = c;
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

    AST dup() {
        auto result = new AST(name);
        result.children = children.dup;
        result.token = token;
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
        return new Grammar(0, Grammar.Type.Or, [grammar, new Grammar(-1, Grammar.Type.Empty)], name);
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
                if (grammar.forgetUnnamed && context.subContexts.length == 1 && context.subContexts[0].target.name == "") {
                    context.subContexts[0].target.name = context.target.name;
                    context = context.subContexts[0];
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

    Grammar grammarFromId(int id) {
        if (id in reverseIDs) return reverseIDs[id];
        else return null;
    }

public:
    Grammar rootGrammar;
    Grammar[string] grammars;

    const int EmptyState = -1;
    const int GenerateEmpty = -2;
    int[Grammar] grammarIDs;
    Grammar[int] reverseIDs;
    int[][int][int] grammarMap;
    enum StackOp { Push, Pop }
    struct StackOpFrame {
        StackOp op;
        ulong numValue;
    }
    StackOpFrame[int] stackOps;

    struct ASTStack {
        AST[] values;
        Parser parser;

        void pushItem(AST ast) {
            values ~= ast;
        }

        AST popFrame(int targetId) {
            if (targetId !in parser.stackOps) return null;
            auto g = parser.grammarFromId(targetId);
            AST targetAST = new AST(g? g.name: null);
            ulong numValue = parser.stackOps[targetId].numValue;
            if (values.length < numValue) {
                writefln("|**| Something wrong happened. tried to took %d, stack has only %d", numValue, values.length);
            } else {
                AST[] subASTs = values[$-numValue..$];
                values = values[0..$-numValue];
                if (subASTs.length > 1) {
                    writefln("Insert multiple children", values);
                    foreach (i, ast; subASTs) {
                        if (ast.name)
                            targetAST.children[ast.name] = ast;
//                        else
//                            targetAST.children["%d".format(i)] = ast;
                    }
                } else if (subASTs.length == 1) {
                    if (g.name) {
                        if (subASTs[0].name) {
                            writefln("Nest child: %s", subASTs[0]);
                            targetAST.children[subASTs[0].name] = subASTs[0];
                        } else {
                            writefln("Replaced with child: %s", subASTs[0]);
                            targetAST = subASTs[0];
                            targetAST.name = g.name;
                        }
                    } else {
                        writefln("Anonymous replacement: %s", subASTs[0]);
                        targetAST = subASTs[0];
                    }
                } else {
                    writefln("########### %s", subASTs.length);                    
                }
                writefln("|**| Took %d items", numValue);
            }
            return targetAST;
        }
        this(Parser parser) {
            this.parser = parser;
        }
    }

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

    AST parse2(string text) {
        struct LRStackElement {
            int id;
            string literal;
            this(int i) { id = i; literal = null; }
            this(int i, string l) { id = i; literal = l; }
            LRStackElement[] candidates;
        }

        Token[] tokens;
        LRStackElement[] lrStack;
        size_t nextPosition;
        tokenizer.tokenize(text, 0, tokens, nextPosition);
        ASTStack astStack = ASTStack(this);

        Scanner scanner = new Scanner(tokens);
        lrStack ~= LRStackElement(-1);

        AST result;

        // DEBUG------------------------------------------------------
        string toId(int k1) {
            string state;
            if (k1 == EmptyState) { state = "<>"; }
            else if (k1 == GenerateEmpty) { state = "<>"; }
            else if (k1 in reverseIDs) {
                state = "%s".format(reverseIDs[k1]);
            } else {
                Token.Type type = cast(Token.Type)k1;
                state = "%s".format(type);
            }
            return state;
        }

        void dumpStack() {
            writefln("Stack:");
            foreach (s; lrStack) {
                writefln("  %s", toId(s.id));
            }
        }
        // /DEBUG------------------------------------------------------

        Token token;
        bool tryEmpty = false;
        bool found;

        enum MatchMode { ConsumeOne, Reduce, GenerateEmpty }
        void buildASTFor(MatchMode mode)(LRStackElement prevState, LRStackElement state, int nextState) {
            if (mode == MatchMode.GenerateEmpty) {
                astStack.pushItem(new AST());
            }
            auto nextg = grammarFromId(nextState);
            if (auto g = grammarFromId(state.id)) {
                writefln("*******%s: %s --> %s", mode, grammarFromId(state.id), nextg);
            } else if (state.id >= 0) {
                astStack.pushItem(new AST(null, Token(cast(Token.Type)state.id, state.literal)));
                writefln("*******%s: %s --> %s", mode, cast(Token.Type)state.id, nextg);
            }
            AST ast = astStack.popFrame(nextState);
            if (ast) {
                astStack.pushItem(ast);
            }
            writefln("|||| %s", astStack.values);
        }

        void iterate2() {
            if (lrStack.length > 1) {
                auto prevState = lrStack[$-2];
                auto state = lrStack[$-1];
                if (prevState.id in grammarMap && state.id in grammarMap[prevState.id]) {
                    found = true;
                    int[] next = grammarMap[prevState.id][state.id];

                    buildASTFor!(MatchMode.ConsumeOne)(prevState, state, next[0]);

                    lrStack = lrStack.remove(lrStack.length - 1);
                    lrStack[$-1].id = next[0];
                    lrStack[$-1].literal = token.literal;
//                    writefln(" >>>>> move to %s", grammarFromId(lrStack[$-1].id));
                } else {
//                        writefln("Rule not found for %d, %d", prevState.id, state.id);
                }
            }
            if (lrStack.length > 0) {
                auto prevState = lrStack[$-1];
                if (prevState.id in grammarMap && EmptyState in grammarMap[prevState.id]) {
                    found = true;
                    int[] next = grammarMap[prevState.id][EmptyState];

                    buildASTFor!(MatchMode.Reduce)(prevState, prevState, next[0]);

                    lrStack[$-1].id = next[0];
                    lrStack[$-1].literal = null;
//                    writefln(" >>>>> move to %s", grammarFromId(lrStack[$-1].id));
                } else {
//                    writefln("Rule not found for %d, %d", prevState.id, EmptyState);
                }
            }
        }

        void iterate0() {
            while (lrStack.length > 0) {
                auto prevState = lrStack[$-1];
                if (prevState.id in grammarMap && GenerateEmpty in grammarMap[prevState.id]) {
                    found = true;
                    int[] next = grammarMap[prevState.id][GenerateEmpty];

                        buildASTFor!(MatchMode.GenerateEmpty)(prevState, prevState, next[0]);

                    lrStack[$-1].id = next[0];
                    lrStack[$-1].literal = null;
//                    writefln(" >>>>> move to %s", grammarFromId(lrStack[$-1].id));
                    break;
                } else {
                    break;
                }
            }
        }
        while(lrStack.length > 0 && !scanner.isEnd()) {
//            dumpStack();
            found = false;
            if (!tryEmpty) {
                token = scanner.scan();
            } else {
                tryEmpty = false;
            }
            lrStack ~= LRStackElement(token.type, token.literal);
//            writefln(">>>>>>Read token: %s", token);

            iterate2();
            if (found) continue;
            lrStack = lrStack.remove(lrStack.length - 1);
            tryEmpty = true;
            iterate2();
            if (found) continue;

            iterate0();
        }
        found = true;
        while (found) {
            found = false;
            iterate2();
            if (found) continue;
            iterate0();
        }
//        writeln("Last");
//        dumpStack();
        writefln("Final\n%s", astStack.values);
        return result;
    }

    void build() {
        int numIDs = 0;
        void traverse(Grammar g) {
            grammarIDs[g] = cast(int)(numIDs + Token.Type.Invalid + 1);
            numIDs ++;
            switch (g.type) {
                case Grammar.Type.And:
                case Grammar.Type.Repeat:
                case Grammar.Type.Or:
                case Grammar.Type.ExOr:
                    foreach (child; g.subGrammars) {
                        traverse(child);
                    }
                    break;
                default:
            }
        }
        foreach (g; [rootGrammar] ~ grammars.values.array) {
            traverse(g);
        }

        void addMap(int state, int last, int nextState) {
            grammarMap.require(state);
            grammarMap[state].require(last);
            grammarMap[state][last] ~= nextState;
        }

        int traverse2(Grammar g, int prevState) {
            int state = prevState;
            int targetId = grammarIDs[g];
            int nextState;
            switch (g.type) {
                case Grammar.Type.And:
                    foreach (i, sub; g.subGrammars) {
                        nextState = traverse2(sub, state);
                        grammarMap.require(state);
                        state = nextState;
                    }
                    if (g.name)
                        stackOps[targetId] = StackOpFrame(StackOp.Pop, g.subGrammars.length);
                    addMap(state, EmptyState, targetId);
                    break;
                case Grammar.Type.Repeat:
                    if (g.minimumCount == 0)
                        addMap(state, GenerateEmpty, targetId);
                    foreach (i, sub; g.subGrammars) {
                        nextState = traverse2(sub, state);
                        grammarMap.require(state);
                        state = nextState;
                    }                    
                    if (g.name)
                        stackOps[targetId] = StackOpFrame(StackOp.Pop, g.subGrammars.length);
                    
                    Grammar repeated = g.dup;
                    traverse(repeated);
                    int repeatedTargetId = grammarIDs[repeated];
                    addMap(state, GenerateEmpty, targetId);
                    foreach (i, sub; repeated.subGrammars) {
                        nextState = traverse2(sub, state);
                        grammarMap.require(state);
                        state = nextState;
                    }                    
                    addMap(state, EmptyState, repeatedTargetId);
                    if (g.name)
                        stackOps[repeatedTargetId] = StackOpFrame(StackOp.Pop, repeated.subGrammars.length);
                    break;

                case Grammar.Type.Or:
                case Grammar.Type.ExOr:
                    foreach (i, sub; g.subGrammars) {
                        int subId = traverse2(sub, state);
                        if (g.name)
                            stackOps[targetId] = StackOpFrame(StackOp.Pop, 1);
                        addMap(subId, EmptyState, targetId);
                    }
                    break;
                case Grammar.Type.Token:
                    addMap(state, g.token.type, targetId);
                    if (g.name)
                        stackOps[targetId] = StackOpFrame(StackOp.Pop, 1);
                    break;
                case Grammar.Type.Empty:
                    addMap(state, GenerateEmpty, targetId);
                    break;
                case Grammar.Type.Reference:
                    if (g.reference.name in grammars) {
                        Grammar sub = grammars[g.reference.name].dup;
                        traverse(sub);
                        int subId = traverse2(sub, state);
                        if (g.name)
                            stackOps[targetId] = StackOpFrame(StackOp.Pop, 1);
                        addMap(subId, EmptyState, targetId);
                    } else {
                        // TBD: Should handle internal error.
                    }
                    break;
                default:
            }
            return targetId;
        }
        traverse2(rootGrammar, EmptyState);
        foreach (k1, v1; grammarIDs) { reverseIDs[v1] = k1; }
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

        registerGrammar("selectors",      _repeat1([_xor([_t("#"), _t(".")], "kind"), _xor([_id, _str, _d], "name")]) );

        registerGrammar("typeIdQuery",    _seq([_xor([_id, _t("*")], "typeId"), _opt(_ref("selectors")), _opt(_ref("pseudoClass")), _opt(_ref("attr"))]) );
        registerGrammar("attrQuery",      _seq([_ref("selectors"),                                     _opt(_ref("pseudoClass")), _opt(_ref("attr"))]) );

        registerGrammar("oneQuery",       _repeat1([_xor([
                                                        _seq([_t(">", "kind"), _xor([_ref("typeIdQuery"), _ref("attrQuery")])], ""), 
                                                        _xor([_ref("typeIdQuery"), _ref("attrQuery")])]) ]) );
        registerGrammar("query",          _repeat1([_ref("oneQuery"), _opt(_t(","))]) );

        foreach (grammar; grammars.byValue) {
            grammar.forgetUnnamed = true;
        }
        rootGrammar = grammars[ROOT].dup;
    }
};