module utils;
import std.traits;

/**
    Returns whether the specified value is a power of two
*/
pragma(inline, true)
bool isPowerOfTwo(T)(T x) if (isIntegral!T) {
    return (x & (x-1)) == 0;
}

/**
    Rounds value up to a multiple
*/
pragma(inline, true)
T roundUpToMultiple(T)(T numToRound, T multOf) if (isIntegral!T) {
    assert(isPowerOfTwo(multOf), "Expected power-of-two multiplier");
    return (numToRound + (multOf - 1u)) & ~(multOf - 1u);
}

/**
    Rounds a value down to a multiple
*/
pragma(inline, true)
T roundDownToMultiple(T)(T numToRound, T multOf) if (isIntegral!T) {
    assert(isPowerOfTwo(multOf), "Expected power-of-two multiplier");
    return numToRound & ~(multOf - 1u);
}
