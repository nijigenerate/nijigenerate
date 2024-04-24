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
import creator.core.selector.resource;
import creator;
import std.stdio;


class Processor(T) {
    T[] process(T[] targets) {
        T[] result = [];
        return result;
    }
}

alias ResourceProcessor = Processor!Resource;
alias NodeProcessor = Processor!Resource;

class MarkerProcessor : ResourceProcessor {
    override
    Resource[] process(Resource[] targets) {
        foreach (target; targets) {
            target.explicit = true;
        }
        return targets;
    }
}

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
                target.index = result.length;
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
                target.index = result.length;
                result ~= target;
            }
        }
        return result;
    }

    this(T value) {
        this.value = value;
    }
}
alias TypeIdFilter = ResourceAttrFilter!("typeId", string, "==");
alias UUIDFilter   = ResourceAttrFilter!("uuid", uint, "==");
alias NameFilter   = ResourceAttrFilter!("name", string, "==");


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
                auto proxy = new Proxy!Node(child);
                proxy.source = t;
                proxy.index = result.length;
                result ~= proxy;
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

        void traverse(Node node, Resource source) {
            if (node.uuid !in uuidMap) {
                auto proxy = new Proxy!Node(node);
                proxy.source = source;
                proxy.index = result.length;
                uuidMap[node.uuid] = true;
                result ~= proxy;
                foreach (child; node.children) {
                    traverse(child, proxy);
                }
            }
        }

        foreach (t; targets) {
            if (!cast(Proxy!Node)t) continue;
            auto target = (cast(Proxy!Node)t).obj();
            foreach (child; target.children) {
                traverse(child, t);
            }
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
            Resource source = t.source;
            Node targetNode = null;
            while (source) {
                if (source.type == ResourceType.Node) {
                    targetNode = (cast(Proxy!Node)source).obj();
                    break;
                }
                source = source.source;
            }
            if (!cast(Proxy!Parameter)t) continue;
            auto target = (cast(Proxy!Parameter)t).obj();
            foreach (child; target.bindings) {
                if (!targetNode || child.getTarget().node == targetNode) {
                    auto proxy = new Proxy!ParameterBinding(child);
                    proxy.source = t;
                    proxy.index = result.length;
                    result ~= proxy;
                }
            }
        }
        return result;
    }
};

alias ParameterChildWalker = ResourceWalker!(Parameter, ParameterBinding, true);


class ResourceWalker(S: Node, T: ParameterBinding, bool direct: true) : ResourceProcessor {
    override
    Resource[] process(Resource[] targets) {
        if (incActivePuppet is null) return [];
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
                if (binding.getTarget().node == target) {
                    auto proxy = new Proxy!ParameterBinding(binding);
                    proxy.index = result.length;
                    proxy.source = t;
                    result ~= proxy;
                }
            }
        }
        return result;
    }
};


class ResourceWalker(S: Node, T: Parameter, bool direct: true) : ResourceProcessor {
    override
    Resource[] process(Resource[] targets) {
        if (incActivePuppet is null) return [];
        Resource[] result;
        Parameter[] parameters = incActivePuppet().parameters;
        foreach (t; targets) {
            if (!cast(Proxy!Node)t) continue;
            auto target = (cast(Proxy!Node)t).obj();

            foreach (param; parameters) {
                foreach (binding; param.bindings) {
                    if (binding.getTarget().node == target) {
                        auto proxy = new Proxy!Parameter(param);
                        proxy.source = t;
                        result ~= proxy;
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
        if (incActivePuppet is null) return [];
        auto root = new Proxy!Node(incActivePuppet.root);
        targets = [root];
        auto result = super.process(targets);
        foreach (r; incActivePuppet.parameters.map!(t => new Proxy!Parameter(t))) {
            r.index = result.length;
            result ~= r;
            r.source = root;
        }
        return result;
    }
}

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
        bool lastHasSelectors = false;
        int i = 0;
        foreach (idKey, filterAST; rootAST) {
            AST query = null;
            if (filterAST["typeIdQuery"]) { query = filterAST["typeIdQuery"]; }
            else if (filterAST["attrQuery"]) { query = filterAST["attrQuery"]; }
            if (query is null) continue;

            auto typeId = query["typeId"];
            auto selectors = query["selectors"];

            string typeIdStr = typeId? typeId.token.literal: "";
            bool isPrevNode = inHasNodeType(lastTypeIdStr) || lastTypeIdStr == "" || lastTypeIdStr == "*";
            bool isNode = inHasNodeType(typeIdStr) || typeIdStr == "" || typeIdStr == "*";

            if (isPrevNode && isNode) {
                if (i > 0) {
                    if (filterAST["kind"] && filterAST["kind"].token.equals(">")) {
                        processors ~= new NodeChildWalker;
                    } else {
                        processors ~= new NodeDescendantsWalker;
                    }
                }
            } else if (isPrevNode) {
                if (i > 0 && typeIdStr == "Parameter") {
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
            if (selectors) {
                foreach (selectKey, selector; selectors) {
                    if (selector["kind"] && selector["kind"].token.equals(".")) {
                        processors ~= new NameFilter(selector["name"].token.literal);
                    } else if (selector["kind"] && selector["kind"].token.equals("#")) {
                        try {
                            string value = selector["name"].token.literal.dup;
                            uint uuid = parse!uint(value);
                            processors ~= new UUIDFilter(uuid);
                        } catch (std.conv.ConvException e) {        
                            writefln("parse error %s", selector["name"].token.literal);
                        }
                    } else {
                        // Should not reached here.
                    }
                }
            }
            processors ~= new MarkerProcessor;
            lastTypeIdStr = typeIdStr;
            lastHasSelectors = selectors !is null;
            i ++;
        }
        if (i > 0) {
            processors = cast(NodeProcessor[])[new PuppetWalker()] ~ processors;
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
