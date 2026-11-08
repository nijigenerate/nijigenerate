file(READ "${INPUT}" bytes HEX)
string(REGEX REPLACE "(..)(..)(..)(..)" "0x\\4\\3\\2\\1u," words "${bytes}")
file(WRITE "${OUTPUT}" "alignas(4) inline constexpr uint32_t ${SYMBOL}[] = {${words}};\n")
