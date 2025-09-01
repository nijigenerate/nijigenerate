module nijigenerate.core.selector.query;


import nijilive;
import std.string;
import std.conv;
import std.uni;
import std.utf;
import std.algorithm;
import std.array;
import nijigenerate.core.selector.tokenizer;
import nijigenerate.core.selector.parser;
import nijigenerate.core.selector.resource: Resource, ResourceInfo, ResourceType, Proxy;
import nijigenerate.core.selector.resource: to;
import std.stdio : writefln;
import nijigenerate;
import nijigenerate.ext;
//import std.stdio;

private {
    ExParameterGroup dummyRoot = null;
}

class ResourceCache {
    Resource[uint] cache;
    Resource create(T)(T obj, void delegate(Resource) callback = null) {
        if (obj.uuid in cache) { 
            return cache[obj.uuid]; 
        } else {
            cache[obj.uuid] = new Proxy!T(obj);
            if (callback !is null) callback(cache[obj.uuid]);
            return cache[obj.uuid];
        }
    }
    Resource create(T: Parameter)(T obj, void delegate(Resource) callback = null) { 
        auto result = new Proxy!Parameter(obj); 
        if (callback !is null) callback(result);
        return result;
    }
    Resource create(T: ParameterBinding)(T obj, void delegate(Resource) callback = null) { 
        auto result = new Proxy!ParameterBinding(obj); 
        if (callback !is null) callback(result);
        return result;
    }
    Resource create(T: Resource)(T obj, void delegate(Resource) callback = null) {
        if (obj.type == ResourceType.Parameter || obj.type == ResourceType.Binding)
            return obj;
        if (obj.uuid in cache) { 
            return cache[obj.uuid]; 
        } else {
            cache[obj.uuid] = obj;
            return obj;
        }
    }
}


class Processor(T) {
    T[] process(T[] targets) {
        T[] result = [];
        return result;
    }
}

alias ResourceProcessor = Processor!Resource;

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
            auto tname = target.name.toStringz.fromStringz;
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
    ResourceCache cache;
    override
    Resource[] process(Resource[] targets) {
        Resource[] result;
        foreach (t; targets) {
            if (!cast(Proxy!Node)t) continue;
            auto target = to!Node(t);
            foreach (child; target.children) {
                auto proxy = cache.create(child, (proxy) {
                    proxy.source = t;
                    proxy.index = result.length;
                });
                result ~= proxy;
            }
        }
        return result;
    }
    this(ResourceCache cache) {
        this.cache = cache;
    }
};

class ResourceWalker(S: Node, T: Node, bool direct: false) : ResourceProcessor {
    ResourceCache cache;
    override
    Resource[] process(Resource[] targets) {
        Resource[] result;
        bool[uint] uuidMap;

        void traverse(Node node, Resource source) {
            if (node.uuid !in uuidMap) {
                auto proxy = cache.create(node, (proxy) {
                    proxy.source = source;
                    proxy.index = result.length;
                    uuidMap[proxy.uuid] = true;
                });
                result ~= proxy;
                foreach (child; node.children) {
                    traverse(child, proxy);
                }
            }
        }

        foreach (t; targets) {
            if (!cast(Proxy!Node)t) continue;
            auto target = to!Node(t);
            foreach (child; target.children) {
                traverse(child, t);
            }
        }
        return result;
    }
    this(ResourceCache cache) {
        this.cache = cache;
    }
};

alias NodeChildWalker = ResourceWalker!(Node, Node, true);
alias NodeDescendantsWalker = ResourceWalker!(Node, Node, false);

class ResourceWalker(S: Parameter, T: ParameterBinding, bool direct: true) : ResourceProcessor {
    ResourceCache cache;
    override
    Resource[] process(Resource[] targets) {
        Resource[] result;
        foreach (t; targets) {
            Resource source = t.source;
            Node targetNode = null;
            while (source) {
                if (source.type == ResourceType.Node) {
                    targetNode = to!Node(source);
                    break;
                }
                source = source.source;
            }
            if (!cast(Proxy!Parameter)t) continue;
            auto target = to!Parameter(t);
            foreach (child; target.bindings) {
                if (!targetNode || child.getTarget().target == targetNode) {
                    auto proxy = cache.create(child, (proxy) {
                        proxy.source = t;
                        proxy.index = result.length;
                    });
                    result ~= proxy;
                }
            }
        }
        return result;
    }
    this(ResourceCache cache) {
        this.cache = cache;
    }
};

alias ParameterChildWalker = ResourceWalker!(Parameter, ParameterBinding, true);


class ResourceWalker(S: Node, T: ParameterBinding, bool direct: true) : ResourceProcessor {
    ResourceCache cache;
    bool armedOnly = false;
    override
    Resource[] process(Resource[] targets) {
        if (incActivePuppet is null) return [];
        Resource[] result;
        Parameter[] parameters = incActivePuppet().parameters;
        ParameterBinding[] bindings;
        if (!armedOnly) {
            foreach (param; parameters) {
                foreach (binding; param.bindings) {
                    bindings ~= binding;
                }
            }
        } else {
            if (incArmedParameter())
                bindings = incArmedParameter().bindings;
        }
        foreach (t; targets) {
            if (!cast(Proxy!Node)t) continue;
            auto target = to!Node(t);

            foreach (binding; bindings) {
                if (binding.getTarget().target == target) {
                    auto proxy = cache.create(binding, (proxy) {
                        proxy.index = result.length;
                        proxy.source = t;
                    });
                    result ~= proxy;
                }
            }
        }
        return result;
    }
    this(ResourceCache cache, bool armedOnly) {
        this.cache = cache;
        this.armedOnly = armedOnly;
    }
};


class ResourceWalker(S: Node, T: Parameter, bool direct: true) : ResourceProcessor {
    ResourceCache cache;
    override
    Resource[] process(Resource[] targets) {
        if (incActivePuppet is null) return [];
        Resource[] result;
        Parameter[] parameters = incActivePuppet().parameters;
        foreach (t; targets) {
            if (!cast(Proxy!Node)t) continue;
            auto target = to!Node(t);

            foreach (param; parameters) {
                foreach (binding; param.bindings) {
                    if (binding.getTarget().target == target) {
                        auto proxy = cache.create(param, (proxy) {
                            proxy.source = t;
                            proxy.index = result.length;
                        });
                        result ~= proxy;
                        break;
                    }
                }
            }
        }
        return result;
    }
    this(ResourceCache cache) {
        this.cache = cache;
    }
};


class PuppetWalker : NodeDescendantsWalker {
    override
    Resource[] process(Resource[] targets) {
        if (incActivePuppet() is null) return [];
        if (dummyRoot is null) {
            dummyRoot = new ExParameterGroup();
            dummyRoot.name = "Parameters";
        }
        auto paramRoot = cache.create(dummyRoot);
        paramRoot.index = 0;
        auto root = cache.create(incActivePuppet.root);
        targets = [root];
        root.index = 1;
        auto exPuppet = cast(ExPuppet)incActivePuppet();
        Resource[] result;
        bool[Parameter] paramAdded;
        if (exPuppet !is null) {
            foreach (g; exPuppet.groups) {
                auto gres = cache.create(g);
                gres.index = result.length;
                result ~= gres;
                gres.source = paramRoot;
                gres.explicit = true;
                foreach (param; g.children) {
                    auto r = cache.create(param);
                    r.index = result.length;
                    result ~= r;
                    r.source = gres;
                    paramAdded[param] = true;
                }
            }
            foreach (param; incActivePuppet().parameters) {
                if (param !in paramAdded) {
                    auto r = cache.create(param);
                    r.index = result.length;
                    result ~= r;
                    r.source = paramRoot;
                }
            }
        } else {
            foreach (r; incActivePuppet().parameters.map!(t => cache.create(t))) {
                r.index = result.length;
                result ~= r;
                r.source = paramRoot;
            }
        }
        foreach(n; super.process(targets)) {
            result ~= n;
        }
        return result;
    }
    this(ResourceCache cache) {
        super(cache);
    }
}

class Selector {
    ResourceProcessor[][] processors;
    Tokenizer tokenizer;
    SelectorParser parser;

    this() {
        tokenizer = new Tokenizer();
        parser = new SelectorParser(tokenizer);
    }

    void build(string text) {
        AST rootAST = parser.parse(text);
        auto cache = new ResourceCache;
        foreach (qId, queryAST; rootAST) {
            string lastTypeIdStr = "";
            bool lastHasSelectors = false;
            int i = 0;
            ResourceProcessor[] andProcessors;
            queryAST = queryAST["oneQuery"];
            if (queryAST is null) continue;
            foreach (idKey, filterAST; queryAST) {
                AST query = null;
                if (filterAST["typeIdQuery"]) { query = filterAST["typeIdQuery"]; }
                else if (filterAST["attrQuery"]) { query = filterAST["attrQuery"]; }
                if (query is null) continue;

                auto typeId = query["typeId"];
                auto selectors = query["selectors"];
                auto pseudoClass = query["pseudoClass"];
                auto attrs = query["attr"];

                string typeIdStr = typeId? typeId.token.literal: "";
                bool isPrevNode = inHasNodeType(lastTypeIdStr) || lastTypeIdStr == "" || lastTypeIdStr == "*";
                bool isNode = inHasNodeType(typeIdStr) || typeIdStr == "" || typeIdStr == "*";

                if (isPrevNode && isNode) {
                    if (i > 0) {
                        if (filterAST["kind"] && filterAST["kind"].token.equals(">")) {
                            andProcessors ~= new NodeChildWalker(cache);
                        } else {
                            andProcessors ~= new NodeDescendantsWalker(cache);
                        }
                    }
                } else if (isPrevNode) {
                    if (i > 0 && typeIdStr == "Parameter") {
                        andProcessors ~= new ResourceWalker!(Node, Parameter, true)(cache);
                    } else if (typeIdStr == "Binding") {
                        bool armedOnly = false;
                        if (pseudoClass !is null) {
                            if (pseudoClass["name"].token.equals("active"))
                                armedOnly = true;
                        }
                        andProcessors ~= new ResourceWalker!(Node, ParameterBinding, true)(cache, armedOnly);
                    }

                } else if (typeIdStr == "Binding") {
                    andProcessors ~= new ParameterChildWalker(cache);
                }

                if (typeIdStr != "*") {
                    if (typeId)
                        andProcessors ~= new TypeIdFilter(typeId.token.literal);
                }
                if (selectors) {
                    foreach (selectKey, selector; selectors) {
                        if (selector["kind"] && selector["kind"].token.equals(".")) {
                            andProcessors ~= new NameFilter(selector["name"].token.literal);
                        } else if (selector["kind"] && selector["kind"].token.equals("#")) {
                            try {
                                string value = selector["name"].token.literal.dup;
                                uint uuid = parse!uint(value);
                                andProcessors ~= new UUIDFilter(uuid);
                            } catch (std.conv.ConvException e) {        
//                                writefln("parse error %s", selector["name"].token.literal);
                            }
                        } else {
                            // Should not reached here.
                        }
                    }
                }
                // Handle attribute filters like [name="Foo"], [uuid=123], [typeId=Node]
                if (attrs) {
                    foreach (attrKey, attrItem; attrs) {
                        auto an = attrItem["name"];
                        auto av = attrItem["value"];
                        if (an is null || av is null) continue;
                        string attrName = an.token.literal;
                        // value token literal can be id/digits/string; use literal directly for name/typeId
                        string valStr = av.token.literal.dup;
                        if (attrName == "name") {
                            andProcessors ~= new NameFilter(valStr);
                        } else if (attrName == "uuid") {
                            try {
                                uint u = parse!uint(valStr);
                                andProcessors ~= new UUIDFilter(u);
                            } catch (std.conv.ConvException e) {}
                        } else if (attrName == "typeId") {
                            andProcessors ~= new TypeIdFilter(valStr);
                        }
                    }
                }
                andProcessors ~= new MarkerProcessor;
                lastTypeIdStr = typeIdStr;
                lastHasSelectors = selectors !is null;
                i ++;
            }
            if (i > 0) {
                andProcessors = cast(ResourceProcessor[])[new PuppetWalker(cache)] ~ andProcessors;
            }
            if (andProcessors.length > 0)
                processors ~= andProcessors;
        }
    }

    Resource[] run() {
        Resource[] result;
        foreach (andProcessors; processors) {
            Resource[] nodes = [];
            foreach (processor; andProcessors) {
                nodes = processor.process(nodes);
            }
            result ~= nodes;
        }
        return result;
    }

}
