module nijigenerate.ext.param;
import nijilive;
import nijilive.fmt;
import inmath;
import nijigenerate;
import nijigenerate.ext;

import std.algorithm.searching;
import std.algorithm.mutation: remove;

class ExParameterGroup : Parameter {
protected:
    override
    void serializeSelf(ref InochiSerializer serializer) {
        serializer.putKey("groupUUID");
        serializer.putValue(uuid);
        serializer.putKey("name");
        serializer.putValue(name);
        serializer.putKey("color");
        serializer.serializeValue(color.vector);
    }

public:
    vec3 color = vec3(0.15, 0.15, 0.15);
    Parameter[] children;

    this() { super(); }
    this(string name) { super(name, false); }
    this(string name, Parameter[] children) { 
        super(name, false); 
        this.children = children;    
    }

    override
    FghjException deserializeFromFghj(Fghj data) {
        data["groupUUID"].deserializeValue(this.uuid_);
        if (!data["name"].isEmpty) data["name"].deserializeValue(this.name_);
        if (!data["color"].isEmpty) data["color"].deserializeValue(this.color.vector);
        if (!data["children"].isEmpty)
            foreach (childData; data["children"].byElement) {
                auto child = inParameterCreate(childData);
                children ~= child;
            }
        return null;
    }

    override
    void reconstruct(Puppet _puppet) {
        auto puppet = cast(ExPuppet)_puppet;
        if (puppet !is null) {
            foreach (child; children) {
                if (auto exparam = cast(ExParameter)child) {
                    exparam.parent = this;
                    exparam.parentUUID = uuid_;
                }
                if (puppet.findParameter(name) is null)
                    puppet.parameters ~= child;
            }
            auto test = puppet.findParameter(uuid);
            if (test !is null) {
                puppet.removeParameter(this);
                puppet.addGroup(this);
            }
        }
        super.reconstruct(_puppet);
    }

}

class ExParameter : Parameter {
    ExParameterGroup parent;
    uint parentUUID = InInvalidUUID;
public:
    this() { 
        super(); 
        parent = null; 
    }
    this(string name) { 
        super(name, false); 
        parent = null;
    }
    this(string name, bool isVec2) { 
        super(name, isVec2); 
        parent = null;
    }
    this(string name, ExParameterGroup parent) { 
        super(name, false); 
        this.parent = parent;
    }
    this(string name, bool isVec2, ExParameterGroup parent) { 
        super(name, isVec2); 
        this.parent = parent;
    }
    override
    FghjException deserializeFromFghj(Fghj data) {
        if (!data["parentUUID"].isEmpty)
            data["parentUUID"].deserializeValue(this.parentUUID);
        try{
        return super.deserializeFromFghj(data);
        }catch(Exception ex) {
            throw ex;
        }
    }

    override
    void serializeSelf(ref InochiSerializer serializer) {
        if (parent !is null) {
            serializer.putKey("parentUUID");
            serializer.putValue(parent.uuid);
        }
        super.serializeSelf(serializer);
    }

    ExParameterGroup getParent() { return parent; }

    void setParent(ExParameterGroup newParent) {
        if (parent !is null && parent != newParent) {
            auto index = parent.children.countUntil(this);
            if (index >= 0) {
                parent.children = parent.children.remove(index);
            }
        }
        auto oldParent = parent;
        parent = newParent;
        if (parent !is null) {
            parentUUID = parent.uuid;
            if (oldParent != parent)
                parent.children ~= this;
        } else {
            parentUUID = InInvalidUUID;
        }
    }

    override
    void finalize(Puppet _puppet) {
        auto puppet = cast(ExPuppet)_puppet;
        if (puppet !is null && parent is null && parentUUID != InInvalidUUID) {
            setParent(puppet.findGroup(parentUUID));
        }
        super.finalize(_puppet);
    }

    /**
        Clone this parameter
    */
    override
    Parameter dup() {
        Parameter newParam = new ExParameter(name ~ " (Copy)", isVec2);

        newParam.min = min;
        newParam.max = max;
        newParam.axisPoints = axisPoints.dup;

        foreach(binding; bindings) {
            ParameterBinding newBinding;
            newBinding = newParam.createBinding(
                binding.getTarget.target,
                binding.getTarget.name,
                false
            );
            newBinding.interpolateMode = binding.interpolateMode;
            foreach(x; 0..axisPointCount(0)) {
                foreach(y; 0..axisPointCount(1)) {
                    binding.copyKeypointToBinding(vec2u(x, y), newBinding, vec2u(x, y));
                }
            }
            newParam.addBinding(newBinding);
        }

        return newParam;
    }
}

void incRegisterExParameter() {
    inParameterSetFactory((Fghj data) {
        if (!data["groupUUID"].isEmpty) {
            ExParameterGroup group = new ExParameterGroup;
            data.deserializeValue(group);
            return group;
        }

        Parameter param = new ExParameter;
        data.deserializeValue(param);
        return param;
    });
}

