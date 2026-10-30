#pragma once

namespace nijigenerate::compat {

// Platform headers and fault dispatch belong to the compiled implementation.
[[noreturn]] void ngFailIntegerDivideByZero();

} // namespace nijigenerate::compat
