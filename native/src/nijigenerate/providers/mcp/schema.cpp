// Port of MCP 1.0.1 schema. Copyright 2025 Garret Noble; port/licenses/mcp-stdio.txt.
#include "schema.hpp"
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/core/translation.hpp>
#include <array>
#include <cmath>

namespace nijigenerate::providers::mcp::schema {
namespace {
template<class... T> String fmt(std::string_view format,T&&...values) {
    const std::array arguments{core::translation::ngFormatArgument(std::forward<T>(values))...};
    return text(core::translation::ngFormatTyped(format,arguments));
}
bool close(double a,double b) {
    if(a==b)return true;
    if(std::isinf(a)||std::isinf(b))return false;
    const double difference=std::abs(a-b);
    return difference<=1e-9*std::abs(a)||difference<=1e-9*std::abs(b)||difference<=0.0;
}
}
SchemaValidationError::SchemaValidationError(String message,String value,std::source_location source)
    :ThrowableException(compat::RetainedString(std::string(message.view())+" at "+
        (value.empty()?"root":std::string(value.view()))),{},source),path(value) {}
void SchemaBuilder::require(bool condition,const char* message) {
#ifndef NDEBUG
    if(!condition)throw compat::AssertError{message,__FILE__,__LINE__};
#else
    (void)condition;(void)message;
#endif
}
void SchemaBuilder::validate(Json value,String path) const { validateType(value,path);validateConstraints(value,path); }
void SchemaBuilder::validateType(Json value,String path) const {
    const char* error=nullptr;
    switch(type) {
    case SchemaType::string_:case SchemaType::enum_:if(!value.is_string())error="Expected string value";break;
    case SchemaType::number:if(!value.is_double()&&!value.is_int64())error="Expected number value";break;
    case SchemaType::integer:if(!value.is_int64())error="Expected integer value";break;
    case SchemaType::boolean:if(!value.is_bool())error="Expected boolean value";break;
    case SchemaType::array:if(!value.is_array())error="Expected array value";break;
    case SchemaType::object:if(!value.is_object())error="Expected object value";break;
    default:compat::ngAssertUnreachable("Invalid schema type",__FILE__,__LINE__);
    }
    if(error)throw SchemaValidationError(text(error),path);
}
void SchemaBuilder::validateConstraints(Json value,String path) const {
    switch(type) {
    case SchemaType::string_:validateStringConstraints(value.as_string(),path);break;
    case SchemaType::number:case SchemaType::integer:validateNumericConstraints(value,path);break;
    case SchemaType::boolean:break;
    case SchemaType::array:validateArrayConstraints(value.as_array(),path);break;
    case SchemaType::object:validateObjectConstraints(value,path);break;
    case SchemaType::enum_:validateEnumConstraints(value.as_string(),path);break;
    default:compat::ngAssertUnreachable("Invalid schema type",__FILE__,__LINE__);
    }
}
void SchemaBuilder::validateStringConstraints(String value,String path) const {
    if(hasMinLength&&value.size()<minLength)
        throw SchemaValidationError(fmt("String length %d is less than minimum %d",value.size(),minLength),path);
    if(hasMaxLength&&value.size()>maxLength)
        throw SchemaValidationError(fmt("String length %d is greater than maximum %d",value.size(),maxLength),path);
    if(hasPattern&&!ngPhobosSchemaPatternMatch(value,patternValue))
        throw SchemaValidationError(fmt("String does not match pattern '%s'",patternValue.view()),path);
}
void SchemaBuilder::validateNumericConstraints(Json value,String path) const {
    const double number=value.is_int64()?static_cast<double>(value.as_int64()):value.as_double();
    if(hasMin) {
        if(exclusiveMin&&number<=minimumValue)
            throw SchemaValidationError(fmt("Value %g must be greater than %g",number,minimumValue),path);
        if(!exclusiveMin&&number<minimumValue)
            throw SchemaValidationError(fmt("Value %g must be greater than or equal to %g",number,minimumValue),path);
    }
    if(hasMax) {
        if(exclusiveMax&&number>=maximumValue)
            throw SchemaValidationError(fmt("Value %g must be less than %g",number,maximumValue),path);
        if(!exclusiveMax&&number>maximumValue)
            throw SchemaValidationError(fmt("Value %g must be less than or equal to %g",number,maximumValue),path);
    }
    if(hasMultipleOf) {
        const double remainder=std::fmod(number,multipleOfValue);
        if(!close(remainder,0.0)&&!close(remainder,multipleOfValue))
            throw SchemaValidationError(fmt("Value %g must be a multiple of %g",number,multipleOfValue),path);
    }
}
void SchemaBuilder::validateArrayConstraints(Array values,String path) const {
    if(hasMinItems&&values.size()<minItems)
        throw SchemaValidationError(fmt("Array length %d is less than minimum %d",values.size(),minItems),path);
    if(hasMaxItems&&values.size()>maxItems)
        throw SchemaValidationError(fmt("Array length %d is greater than maximum %d",values.size(),maxItems),path);
    if(uniqueItems)for(std::size_t i=0;i<values.size();++i)for(std::size_t j=i+1;j<values.size();++j)
        if(values[i]==values[j])throw SchemaValidationError(text("Array must have unique items"),path);
    for(std::size_t i=0;i<values.size();++i)elementType->validate(values[i],fmt("%s[%d]",path.view(),i));
}
void SchemaBuilder::validateObjectConstraints(Json value,String path) const {
    const auto object=value.as_object();
    for(const auto& property:requiredProps)if(!object.if_contains(property.view()))
        throw SchemaValidationError(fmt("Missing required property '%s'",property.view()),path);
    for(const auto& member:object) {
        const String propertyPath=path.empty()?String(member.name):fmt("%s.%s",path.view(),member.key());
        auto found=properties.find(member.key());
        if(found!=properties.end())found->second->validate(member.value(),propertyPath);
        else if(!additionalProperties)
            throw SchemaValidationError(fmt("Additional property '%s' is not allowed",member.key()),path);
    }
}
void SchemaBuilder::validateEnumConstraints(String value,String path) const {
    for(const auto& candidate:enumValues)if(candidate==value)return;
    // Phobos array %s formatting includes quoted strings. Use the real typed formatter.
    throw SchemaValidationError(fmt("Value '%s' must be one of %s",value.view(),enumValues),path);
}
SchemaBuilder::Ref SchemaBuilder::make(SchemaType type) { auto value=Ref(new SchemaBuilder);value->type=type;return value; }
SchemaBuilder::Ref SchemaBuilder::string_(){return make(SchemaType::string_);}
SchemaBuilder::Ref SchemaBuilder::number(){return make(SchemaType::number);}
SchemaBuilder::Ref SchemaBuilder::integer(){return make(SchemaType::integer);}
SchemaBuilder::Ref SchemaBuilder::boolean(){return make(SchemaType::boolean);}
SchemaBuilder::Ref SchemaBuilder::array(Ref element){auto value=make(SchemaType::array);value->elementType=element;return value;}
SchemaBuilder::Ref SchemaBuilder::object(){return make(SchemaType::object);}
SchemaBuilder::Ref SchemaBuilder::enum_(compat::Slice<String> values){auto value=make(SchemaType::enum_);value->enumValues=values.duplicate();return value;}
SchemaBuilder::Ref SchemaBuilder::setDescription(String value){description=value;return shared_from_this();}
SchemaBuilder::Ref SchemaBuilder::optional(){required=false;return shared_from_this();}
SchemaBuilder::Ref SchemaBuilder::addProperty(String name,Ref property) {
    require(type==SchemaType::object,"Can only add properties to object schema");
    properties[name]=property;if(property->required)requiredProps.append(name);return shared_from_this();
}
SchemaBuilder::Ref SchemaBuilder::allowAdditional(bool value) {
    require(type==SchemaType::object,"Additional properties only apply to object schema");
    additionalProperties=value;return shared_from_this();
}
SchemaBuilder::Ref SchemaBuilder::length(std::size_t minimum,std::size_t maximum) {
    require(type==SchemaType::array,"Length constraints only apply to array schema");
    minItems=minimum;maxItems=maximum;hasMinItems=hasMaxItems=true;return shared_from_this();
}
SchemaBuilder::Ref SchemaBuilder::unique(){require(type==SchemaType::array,"Unique constraint only applies to array schema");uniqueItems=true;return shared_from_this();}
SchemaBuilder::Ref SchemaBuilder::setMultipleOf(double value) {
    require(type==SchemaType::number||type==SchemaType::integer,"Multiple of constraint only applies to numeric schema");
    multipleOfValue=value;hasMultipleOf=true;return shared_from_this();
}
SchemaBuilder::Ref SchemaBuilder::stringLength(std::size_t minimum,std::size_t maximum) {
    require(type==SchemaType::string_,"String length only applies to string schema");
    minLength=minimum;maxLength=maximum;hasMinLength=hasMaxLength=true;return shared_from_this();
}
SchemaBuilder::Ref SchemaBuilder::setPattern(String value) {
    require(type==SchemaType::string_,"Pattern only applies to string schema");patternValue=value;hasPattern=true;return shared_from_this();
}
const char* SchemaBuilder::schemaTypeToString(SchemaType type) {
    switch(type) {
    case SchemaType::string_:case SchemaType::enum_:return "string";
    case SchemaType::number:return "number";case SchemaType::integer:return "integer";
    case SchemaType::boolean:return "boolean";case SchemaType::array:return "array";case SchemaType::object:return "object";
    default:compat::ngAssertUnreachable("Invalid schema type",__FILE__,__LINE__);
    }
}
Json SchemaBuilder::toJSON() const {
    Object result;result["type"]=schemaTypeToString(type);
    if(!description.empty())result["description"]=jsonString(description);
    switch(type) {
    case SchemaType::string_:
        if(hasMinLength)result["minLength"]=minLength;if(hasMaxLength)result["maxLength"]=maxLength;
        if(hasPattern)result["pattern"]=jsonString(patternValue);break;
    case SchemaType::number:case SchemaType::integer:
        if(hasMin)result[exclusiveMin?"exclusiveMinimum":"minimum"]=minimumValue;
        if(hasMax)result[exclusiveMax?"exclusiveMaximum":"maximum"]=maximumValue;
        if(hasMultipleOf)result["multipleOf"]=multipleOfValue;break;
    case SchemaType::boolean:break;
    case SchemaType::array:
        result["items"]=elementType->toJSON();if(hasMinItems)result["minItems"]=minItems;
        if(hasMaxItems)result["maxItems"]=maxItems;if(uniqueItems)result["uniqueItems"]=true;break;
    case SchemaType::object:
        if(!properties.empty()) {
            Object props;for(const auto& [name,property]:properties)props[core::settings::SettingsString(name)]=property->toJSON();
            result["properties"]=props;
            if(!requiredProps.empty()){Array values;for(const auto& name:requiredProps)values.append(jsonString(name));result["required"]=values;}
        }
        if(!additionalProperties)result["additionalProperties"]=false;break;
    case SchemaType::enum_: {Array values;for(const auto& value:enumValues)values.append(jsonString(value));result["enum"]=values;break;}
    default:compat::ngAssertUnreachable("Invalid schema type",__FILE__,__LINE__);
    }
    return result;
}
}
