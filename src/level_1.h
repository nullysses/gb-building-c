#ifndef LEVEL_1_H
#define LEVEL_1_H

typedef enum level_result_t {
    LEVEL_RESULT_COMPLETE = 0u,
    LEVEL_RESULT_RESTART
} level_result_t;

level_result_t level_1_run(void);

#endif
