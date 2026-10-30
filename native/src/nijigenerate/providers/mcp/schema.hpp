#pragma once
#include "common.hpp"
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <limits>

namespace nijigenerate::providers::mcp::schema {
enum class SchemaType { string_,number,integer,boolean,array,object,enum_ };
class SchemaValidationError:public compat::ThrowableException {
    String path;
public:
    explicit SchemaValidationError(String message,String path={},
        std::source_location source=std::source_location::current());
    std::string_view ngThrowableTypeName() const noexcept override {return "mcp.schema.SchemaValidationError";}
};
class SchemaBuilder:public std::enable_shared_from_this<SchemaBuilder> {
public:
    using Ref=std::shared_ptr<SchemaBuilder>;
private:
    SchemaType type=SchemaType::string_;
    String description;
    bool required=true;
    StringMap<Ref> properties;
    compat::Slice<String> requiredProps;
    bool additionalProperties=false;
    Ref elementType;
    std::size_t minItems=0,maxItems=0;
    bool hasMinItems=false,hasMaxItems=false,uniqueItems=false;
    double minimumValue=std::numeric_limits<double>::quiet_NaN();
    double maximumValue=std::numeric_limits<double>::quiet_NaN();
    bool hasMin=false,hasMax=false,exclusiveMin=false,exclusiveMax=false;
    double multipleOfValue=std::numeric_limits<double>::quiet_NaN();
    bool hasMultipleOf=false;
    std::size_t minLength=0,maxLength=0;
    bool hasMinLength=false,hasMaxLength=false;
    String patternValue;
    bool hasPattern=false;
    compat::Slice<String> enumValues;
    SchemaBuilder()=default;
    static Ref make(SchemaType type);
    static void require(bool condition,const char* message);
    void validateType(Json value,String path) const;
    void validateConstraints(Json value,String path) const;
    void validateStringConstraints(String value,String path) const;
    void validateNumericConstraints(Json value,String path) const;
    void validateArrayConstraints(Array value,String path) const;
    void validateObjectConstraints(Json value,String path) const;
    void validateEnumConstraints(String value,String path) const;
    static const char* schemaTypeToString(SchemaType type);
public:
    void validate(Json value,String path={}) const;
    static Ref string_();static Ref number();static Ref integer();static Ref boolean();
    static Ref array(Ref element);static Ref object();
    static Ref enum_(compat::Slice<String> values);
    static Ref enum_(std::initializer_list<String> values) { return enum_(compat::Slice<String>(values)); }
    Ref setDescription(String value);
    Ref optional();Ref addProperty(String name,Ref property);Ref allowAdditional(bool value);
    Ref length(std::size_t minimum,std::size_t maximum);Ref unique();
    template<class T> requires std::is_arithmetic_v<T> Ref range(T minimum,T maximum) {
        require(type==SchemaType::number || type==SchemaType::integer,"Range constraints only apply to numeric schema");
        minimumValue=static_cast<double>(minimum);maximumValue=static_cast<double>(maximum);
        hasMin=hasMax=true;
        if constexpr(std::is_integral_v<T>) type=SchemaType::integer;
        return shared_from_this();
    }
    template<class T> requires std::is_arithmetic_v<T> Ref exclusiveRange(T minimum,T maximum) {
        range(minimum,maximum);exclusiveMin=exclusiveMax=true;return shared_from_this();
    }
    Ref setMultipleOf(double value);Ref stringLength(std::size_t minimum,std::size_t maximum);
    Ref setPattern(String expression);
    Json toJSON() const;
};
// The independent Phobos regex contract is a separate provider. A generic
// std::regex engine is not an equivalent substitute for its Unicode grammar.
bool ngPhobosSchemaPatternMatch(String input,String pattern);
}
