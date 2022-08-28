#ifndef KDL_INTERNAL_GRAMMAR_H_
#define KDL_INTERNAL_GRAMMAR_H_

bool _kdl_is_whitespace(uint32_t c);
bool _kdl_is_newline(uint32_t c);
bool _kdl_is_id(uint32_t c);
bool _kdl_is_id_start(uint32_t c);
bool _kdl_is_end_of_word(uint32_t c);

#endif // KDL_INTERNAL_GRAMMAR_H_
