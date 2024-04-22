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
import creator;

class Processor(T) {
    T[] process(T[] targets) {
        T[] result = [];
        return result;
    }
}

alias NodeProcessor = Processor!Node;

string AttrComparison(string name, string op) {
    return "target." ~ name ~ op ~ " value";
}

class NodeAttrFilter(alias name, T, alias op) : NodeProcessor {
    T value;
    override
    Node[] process(Node[] targets) {
        Node[] result;
        foreach(target; targets) {
            if (mixin(AttrComparison(name, op))) {
                result ~= target;
            }
        }
        return result;
    }

    this(T value) {
        this.value = value;
    }
}

class NodeChildWalker : NodeProcessor {
    override
    Node[] process(Node[] targets) {
        Node[] result;
        foreach (target; targets) {
            foreach (child; target.children) {
                result ~= child;
            }
        }
        return result;
    }
};

class NodeDescendantsWalker : NodeProcessor {
    override
    Node[] process(Node[] targets) {
        Node[] result;
        bool[uint] uuidMap;

        void traverse(Node node) {
            if (node.uuid !in uuidMap) {
                result ~= node;
                uuidMap[node.uuid] = true;
                foreach (child; node.children) {
                    traverse(child);
                }
            }
        }

        foreach (target; targets) {
            traverse(target);
        }
        return result;
    }
};

class PuppetWalker : NodeDescendantsWalker {
    override
    Node[] process(Node[] targets) {
        targets = [incActivePuppet.root];
        return super.process(targets);
    }
}

alias TypeIdFilter = NodeAttrFilter!("typeId", string, "==");
alias UUIDFilter   = NodeAttrFilter!("uuid", uint, "==");
alias NameFilter   = NodeAttrFilter!("name", string, "==");

class Selector {
    NodeProcessor[] processors;
    Tokenizer tokenizer;
    SelectorParser parser;

    this() {
        tokenizer = new Tokenizer();
        parser = new SelectorParser(tokenizer);
    }

    void build(string text) {
        AST rootAST = parser.parse(text);
        processors.length = 0;
        processors ~= new PuppetWalker();
        foreach (idKey, filterAST; rootAST) {
            if (filterAST["kind"] && filterAST["kind"].token.equals(">")) {
                processors ~= new NodeChildWalker;
            } else {
                processors ~= new NodeDescendantsWalker;
            }
            AST query = null;
            if (filterAST["typeIdQuery"]) { query = filterAST["typeIdQuery"]; }
            else if (filterAST["attrQuery"]) { query = filterAST["attrQuery"]; }
            if (query is null) continue;

            auto typeId = query["typeId"];
            if (typeId) {
                processors ~= new TypeIdFilter(typeId.token.literal);
            }
            auto selectors = query["selectors"];
            if (selectors) {
                foreach (selectKey, selector; selectors) {
                    if (selector["kind"] && selector["kind"].token.equals(".")) {
                        processors ~= new NameFilter(selector["name"].token.literal);
                    } else if (selector["kind"] && selector["kind"].token.equals("#")) {
                        processors ~= new UUIDFilter(parse!int(selector["name"].token.literal));
                    } else {
                        // Should not reached here.
                    }
                }
            }            
            
        }
    }

    Node[] run() {
        Node[] nodes = null;
        foreach (processor; processors) {
            nodes = processor.process(nodes);
        }
        return nodes;
    }

}
