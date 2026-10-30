#pragma once

namespace nijigenerate::compat {

// A sequence of genuine types, without tuple element storage or construction.
// This maps type-only AliasSeq uses; it does not represent value/symbol packs.
template<class... Types> struct TypeSequence {};

} // namespace nijigenerate::compat
