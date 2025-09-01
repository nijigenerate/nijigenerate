module nijigenerate.core.selector;

public import nijigenerate.core.selector.tokenizer;
public import nijigenerate.core.selector.parser;
public import nijigenerate.core.selector.resource;
public import nijigenerate.core.selector.query;
public import nijigenerate.core.selector.treestore;


unittest {
    import std.stdio;
    import std.algorithm;
    import std.array;
    import std.datetime.stopwatch;
    import std.format;

    Tokenizer tokenizer = new Tokenizer();
    SelectorParser parser = new SelectorParser(tokenizer);

    string test = "Node[name=\"日本語の文字列\"] Part[property0=12.33] > #\"Eye\"";

    writeln();
    writeln(">>> Original text");
    writeln("---------------------------------------------------------");
    writefln(" %s", test);

    writeln();
    writeln(">>> Rules for selector Grammar");
    writeln("---------------------------------------------------------");

    foreach (e; parser.grammars.byKeyValue()) {
        writefln("%s", e.value);
    }

    writeln();
    writeln(">>> Tokenization test");
    writeln("---------------------------------------------------------");

    Token[] tokens;
    size_t pos;
    tokenizer.tokenize!true(test, pos, tokens, pos);
    writefln("Tokenized:\n %s", map!(t=>t.literal)(tokens).array);
    int i = 0;
    assert(tokens[i++].literal == "Node");
    assert(tokens[i++].literal == "[");
    assert(tokens[i++].literal == "name");
    assert(tokens[i++].literal == "=");
    assert(tokens[i++].literal == "日本語の文字列");
    assert(tokens[i++].literal == "]");
    assert(tokens[i++].literal == "Part");
    assert(tokens[i++].literal == "[");
    assert(tokens[i++].literal == "property0");
    assert(tokens[i++].literal == "=");
    assert(tokens[i++].literal == "12.33");
    assert(tokens[i++].literal == "]");
    assert(tokens[i++].literal == ">");
    assert(tokens[i++].literal == "#");
    assert(tokens[i++].literal == "Eye");
    assert(tokens.length == i);

    auto sw1 = StopWatch();
    test = "Root Node#1111111[name=\"日本語の文字列\"][uuid=11111111] Part[property0=12.33] > #\"Eye\":nth-child(10, 0) *";
    sw1.start();
    for (int x = 0; x < 1000; x ++) {
        tokenizer.tokenize!false(test, pos, tokens, pos);
    }
    sw1.stop();
    writefln("pure match: %f msecs / 1000 tries = usecs in average", sw1.peek.total!"usecs"/1000.0);

    sw1.start();
    for (int x = 0; x < 1000; x ++) {
        tokenizer.tokenize!true(test, pos, tokens, pos);
    }
    sw1.stop();
    writefln("lookup: %f msecs / 1000 tries = usecs in average", sw1.peek.total!"usecs"/1000.0);


    writeln();
    writeln(">>> Parsing test");
    writeln("---------------------------------------------------------");

    test = "Root Node#1111111[name=\"日本語の文字列\"][uuid=11111111] Part[property0=12.33] > #\"Eye\":nth-child(10, 0) *";
    writefln("Text:\n %s\n", test);

    tokens.length = 0;
    tokenizer.tokenize(test, 0, tokens, pos);
    writefln("Tokenized:\n %s", map!(t=>t.literal)(tokens).array);

    AST ast = parser.parse(test);
    writeln("\nParsed Tree:\n");
    writefln("%s", ast);
//    assert(context.matched);

    auto sw = StopWatch();
    sw.start();
    for (int x = 0; x < 1000; x ++) {
        auto result = parser.parse(test);
    }
    sw.stop();
    writefln("%f msecs / 1000 tries = usecs in average", sw.peek.total!"usecs"/1000.0);

    writeln();
    writeln(">>> Selector test");
    writeln("---------------------------------------------------------");

    test = "Part.\"目::R::MG\" Part.\"瞳\"";
    auto selector = new Selector();
    selector.build(test);

    writeln();
    writeln("Parser Test 2");
    writeln("---------------------------------------------------------");
    test = "Root Node#1111111[name=\"日本語の文字列\"][uuid=11111111] Part[property0=12.33] > #\"Eye\":nth-child(10, 0) *";
    parser.build();
    /*
    writefln("Parser grammar states: %s", parser.grammarIDs);
    Grammar[int] revMap;
    foreach (k1, v1; parser.grammarIDs) { revMap[v1] = k1; }
    foreach (k1, v1; parser.grammarMap) {
        string state;
        if (k1 == -1) { state = "<>"; }
        else if (k1 in revMap) {
            state = "%s".format(revMap[k1]);
        } else {
            Token.Type type = cast(Token.Type)k1;
            state = "%s".format(type);
        }
        foreach(k2, v2; v1) {
            string last;
            if (k2 == -1) { last = "<>"; }
            else if (k2 in revMap) {
                last = "%s".format(revMap[k2]);
            } else {
                Token.Type type = cast(Token.Type)k2;
                last = "%s".format(type);
            }
            foreach (v3; v2) {
                string next;
                if (v3 == -1) { state = "<>"; }
                else if (v3 in revMap) {
                    next = "%s".format(revMap[v3]);
                } else {
                    Token.Type type = cast(Token.Type)v3;
                    next = "%s".format(type);
                }
                writefln("◀%s, %s▶\n   =====> %s", state, last, next);
            }
        }
    }
    */
    writeln();
    writefln("Parse test 2");
    writeln();

    ast = parser.parse2(test);
    writeln("\nParsed Tree:\n");
    writefln("%s", ast);
    /*
    sw = StopWatch();
    sw.start();
    for (int x = 0; x < 1000; x ++) {
        parser.parse2(test);
//        auto result = parser.parse(test);
    }
    sw.stop();
    writefln("%f msecs / 1000 tries = usecs in average", sw.peek.total!"usecs"/1000.0);
    */

}
