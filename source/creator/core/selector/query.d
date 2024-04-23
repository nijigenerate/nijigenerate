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
import creator.core.resource;
import creator;


class Processor(T) {
    T[] process(T[] targets) {
        T[] result = [];
        return result;
    }
}

alias ResourceProcessor = Processor!Resource;
alias NodeProcessor = Processor!Resource;

string AttrComparison(string name, string op) {
    return "target." ~ name ~ op ~ " value";
}

class ResourceAttrFilter(string name, T, alias op) : ResourceProcessor {
    T value;
    override
    Resource[] process(Resource[] targets) {
        Resource[] result;
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

class ResourceAttrFilter(string name: "name", T, alias op) : ResourceProcessor {
    T value;
    override
    Resource[] process(Resource[] targets) {
        Resource[] result;
        foreach(target; targets) {
            if (mixin("(target.name.toStringz).fromStringz "~op~" value")) {
                result ~= target;
            }
        }
        return result;
    }

    this(T value) {
        this.value = value;
    }
}

class ResrouceWalker(S, T, bool direct = true) : ResourceProcessor {
}

class ResourceWalker(S: Node, T: Node, bool direct: true) : ResourceProcessor {
    override
    Resource[] process(Resource[] targets) {
        Resource[] result;
        foreach (t; targets) {
            if (!cast(Proxy!Node)t) continue;
            auto target = (cast(Proxy!Node)t).obj();
            foreach (child; target.children) {
                result ~= new Proxy!Node(child);
            }
        }
        return result;
    }
};

class ResourceWalker(S: Node, T: Node, bool direct: false) : ResourceProcessor {
    override
    Resource[] process(Resource[] targets) {
        Resource[] result;
        bool[uint] uuidMap;

        void traverse(Node node) {
            if (node.uuid !in uuidMap) {
                result ~= new Proxy!Node(node);
                uuidMap[node.uuid] = true;
                foreach (child; node.children) {
                    traverse(child);
                }
            }
        }

        foreach (t; targets) {
            if (!cast(Proxy!Node)t) continue;
            auto target = (cast(Proxy!Node)t).obj();
            traverse(target);
        }
        return result;
    }
};

alias NodeChildWalker = ResourceWalker!(Node, Node, true);
alias NodeDescendantsWalker = ResourceWalker!(Node, Node, false);

class ResourceWalker(S: Parameter, T: ParameterBinding, bool direct: true) : ResourceProcessor {
    override
    Resource[] process(Resource[] targets) {
        Resource[] result;
        foreach (t; targets) {
            if (!cast(Proxy!Parameter)t) continue;
            auto target = (cast(Proxy!Parameter)t).obj();
            foreach (child; target.bindings) {
                result ~= new Proxy!ParameterBinding(child);
            }
        }
        return result;
    }
};

alias ParameterChildWalker = ResourceWalker!(Parameter, ParameterBinding, true);


class ResourceWalker(S: Node, T: ParameterBinding, bool direct: true) : ResourceProcessor {
    override
    Resource[] process(Resource[] targets) {
        Resource[] result;
        Parameter[] parameters = incActivePuppet().parameters;
        ParameterBinding[] bindings;
        foreach (param; parameters) {
            foreach (binding; param.bindings) {
                bindings ~= binding;
            }
        }
        foreach (t; targets) {
            if (!cast(Proxy!Node)t) continue;
            auto target = (cast(Proxy!Node)t).obj();

            foreach (binding; bindings) {
                if (binding.getTarget().node == target)
                   result ~= new Proxy!ParameterBinding(binding);
            }
        }
        return result;
    }
};


class ResourceWalker(S: Node, T: Parameter, bool direct: true) : ResourceProcessor {
    override
    Resource[] process(Resource[] targets) {
        Resource[] result;
        Parameter[] parameters = incActivePuppet().parameters;
        foreach (t; targets) {
            if (!cast(Proxy!Node)t) continue;
            auto target = (cast(Proxy!Node)t).obj();

            foreach (param; parameters) {
                foreach (binding; param.bindings) {
                    if (binding.getTarget().node == target) {
                        result ~= new Proxy!Parameter(param);
                        break;
                    }
                }
            }
        }
        return result;
    }
};


class PuppetWalker : NodeDescendantsWalker {
    override
    Resource[] process(Resource[] targets) {
        if (incActivePuppet is null)
            return [];
        targets = [new Proxy!Node(incActivePuppet.root)];
        auto result = super.process(targets);
        foreach (r; incActivePuppet.parameters.map!(t => new Proxy!Parameter(t))) {
            result ~= r;
        }
        return result;
    }
}

alias TypeIdFilter = ResourceAttrFilter!("typeId", string, "==");
alias UUIDFilter   = ResourceAttrFilter!("uuid", uint, "==");
alias NameFilter   = ResourceAttrFilter!("name", string, "==");

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
        string lastTypeIdStr = "";
        foreach (idKey, filterAST; rootAST) {
            AST query = null;
            if (filterAST["typeIdQuery"]) { query = filterAST["typeIdQuery"]; }
            else if (filterAST["attrQuery"]) { query = filterAST["attrQuery"]; }
            if (query is null) continue;

            auto typeId = query["typeId"];
            string typeIdStr = typeId? typeId.token.literal: "";

            bool isPrevNode = inHasNodeType(lastTypeIdStr) || lastTypeIdStr == "" || lastTypeIdStr == "*";
            bool isNode = inHasNodeType(typeIdStr) || typeIdStr == "" || typeIdStr == "*";

            if (isPrevNode && isNode) {
                if (filterAST["kind"] && filterAST["kind"].token.equals(">")) {
                    processors ~= new NodeChildWalker;
                } else {
                    processors ~= new NodeDescendantsWalker;
                }
            } else if (isPrevNode) {
                if (lastTypeIdStr != "" && typeIdStr == "Parameter") {
                    processors ~= new ResourceWalker!(Node, Parameter, true);
                } else if (typeIdStr == "Binding") {
                    processors ~= new ResourceWalker!(Node, ParameterBinding, true);
                }

            } else if (typeIdStr == "Binding") {
                processors ~= new ParameterChildWalker;
            }

            if (typeIdStr != "*") {
                if (typeId)
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
            if (processors.length > 0) {
                processors = cast(NodeProcessor[])[new PuppetWalker()] ~ processors;
            }
            lastTypeIdStr = typeIdStr;
        }
    }

    Resource[] run() {
        Resource[] nodes = [];
        foreach (processor; processors) {
            nodes = processor.process(nodes);
        }
        return nodes;
    }

}
