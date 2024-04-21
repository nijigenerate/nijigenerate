module creator.core.selector.query;


import inochi2d;
import std.string;
import std.conv;
import std.uni;
import std.utf;
import std.algorithm;
import std.array;
import creator.core.selector.tokenizer;
import creator.core.selector.parser;

class Query(T) {
    void query(Token[] tokens, T[] targets, out T[] result, out Token[] newTokens) {
        newTokens = tokens;
        result = [];
    }
}

alias NodeQuery = Query!Node;

string AttrEquation(string str) {
    return "target." ~ str ~ " == value";
}

class NodeAttrQuery(alias f, T) : NodeQuery {
    override
    void query(Token[] tokens, Node[] targets, out Node[] result, out Token[] newTokens) {
        result.length = 0;
        if (tokens.length == 0) { 
            newTokens = tokens[1..$-1];
            return;
        }
        T value = parse!T(tokens[0].literal);
        foreach(target; targets) {
            if (mixin(AttrEquation(f))) {
                result ~= target;
            }
        }
    }
}

alias TypeIdQuery = NodeAttrQuery!("typeId", string);
alias UUIDQuery   = NodeAttrQuery!("uuid", uint);
alias NameQuery   = NodeAttrQuery!("name", string);

class QueryBuilder {

}
