module creator.core.selector;

public import creator.core.selector.tokenizer;
public import creator.core.selector.parser;
public import creator.core.selector.resource;
public import creator.core.selector.query;


unittest {
    import std.stdio;
    import std.algorithm;
    import std.array;
    import std.datetime.stopwatch;

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
    tokenizer.tokenize(test, pos, tokens, pos);
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

    writeln();
    writeln(">>> Parsing test");
    writeln("---------------------------------------------------------");

    test = "Root Node.class[name=\"日本語の文字列\"][uuid=11111111] Part[property0=12.33] > #\"Eye\":nth-child(10, 0) *";
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
    writefln("%s", selector.processors);

}