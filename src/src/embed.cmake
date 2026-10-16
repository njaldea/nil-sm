file(READ "${INPUT}" content HEX)
string(LENGTH "${content}" length)
math(EXPR size "${length} / 2")
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${content}")

file(
    WRITE "${OUTPUT}"
    "#include \"assets.hpp\"\n"
    "\n"
    "namespace nil::sm::viz::assets\n"
    "{\n"
    "    std::string_view ${NAME}()\n"
    "    {\n"
    "        static const unsigned char data[] = {${bytes}};\n"
    "        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)\n"
    "        return {reinterpret_cast<const char*>(data), ${size}};\n"
    "    }\n"
    "}\n"
)
