#ifndef KDL_INTERNAL_GRAMMAR_H_
#define KDL_INTERNAL_GRAMMAR_H_

#include <stdbool.h>
#include <stdint.h>

#include <kdl/tokenizer.h>

bool _kdl_is_whitespace(kdl_character_set charset, uint32_t c);
bool _kdl_is_newline(uint32_t c);
bool _kdl_is_id(kdl_character_set charset, uint32_t c);
bool _kdl_is_id_start(kdl_character_set charset, uint32_t c);
bool _kdl_is_end_of_word(kdl_character_set charset, uint32_t c);
bool _kdl_is_illegal_char(kdl_character_set charset, uint32_t c);
bool _kdl_is_equals_sign(kdl_character_set charset, uint32_t c);

#endif // KDL_INTERNAL_GRAMMAR_H_
