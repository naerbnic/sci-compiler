#ifndef UTIL_MACROS_FOR_EACH_HPP
#define UTIL_MACROS_FOR_EACH_HPP

#define FOR_EACH_1(macro, arg1) macro(arg1)
#define FOR_EACH_2(macro, arg1, ...) macro(arg1) FOR_EACH_1(macro, __VA_ARGS__)
#define FOR_EACH_3(macro, arg1, ...) macro(arg1) FOR_EACH_2(macro, __VA_ARGS__)
#define FOR_EACH_4(macro, arg1, ...) macro(arg1) FOR_EACH_3(macro, __VA_ARGS__)
#define FOR_EACH_5(macro, arg1, ...) macro(arg1) FOR_EACH_4(macro, __VA_ARGS__)
#define FOR_EACH_6(macro, arg1, ...) macro(arg1) FOR_EACH_5(macro, __VA_ARGS__)
#define FOR_EACH_7(macro, arg1, ...) macro(arg1) FOR_EACH_6(macro, __VA_ARGS__)
#define FOR_EACH_8(macro, arg1, ...) macro(arg1) FOR_EACH_7(macro, __VA_ARGS__)
#define FOR_EACH_9(macro, arg1, ...) macro(arg1) FOR_EACH_8(macro, __VA_ARGS__)
#define FOR_EACH_10(macro, arg1, ...) macro(arg1) FOR_EACH_9(macro, __VA_ARGS__)

#define FOR_EACH_GET_MACRO(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, N, ...) N
#define FOR_EACH(macro, ...) \
  FOR_EACH_##FOR_EACH_GET_MACRO(__VA_ARGS__, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1)(macro, __VA_ARGS__)

#endif