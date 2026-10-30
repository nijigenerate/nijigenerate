#include "typed_format_decimal.hpp"

namespace nijigenerate::core::translation {

template std::string ngFormatDecimal<float>(float, const DecimalFormatSpec&);
template std::string ngFormatDecimal<double>(double, const DecimalFormatSpec&);

} // namespace nijigenerate::core::translation
