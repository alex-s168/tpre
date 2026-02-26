#define NODE_DONE ((tpre_nodeid_t) - 2)
#define NODE_ERR ((tpre_nodeid_t) - 1)

#define SPECIAL_ANY (0)
#define SPECIAL_SPACE (1)
#define SPECIAL_END (2)
#define SPECIAL_START (3)
#define SPECIAL_DIGIT (4)
#define SPECIAL_WORDC (5)
#define SPECIAL_BT_PUSH (6)
#define NO(c)         \
  ((tpre_pattern_t) { \
    .is_special = 0, .val = (uint8_t) c, .invert = 0 })
#define SP(c)         \
  ((tpre_pattern_t) { \
    .is_special = 1, .val = (uint8_t) c, .invert = 0 })
