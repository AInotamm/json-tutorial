#include "leptjson.h"
#include <assert.h>  /* assert() */
#include <stdlib.h>  /* NULL, strtod() */
#include <ctype.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#define EXPECT(c, ch)       do { assert(*c->json == (ch)); c->json++; } while(0)
#define ISDIGIT(ch)         ((ch) >= '0' && (ch) <= '9')
#define ISDIGIT1TO9(ch)     ((ch) >= '1' && (ch) <= '9')

typedef struct {
    const char* json;
}lept_context;

static void lept_parse_whitespace(lept_context* c) {
    const char *p = c->json;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    c->json = p;
}

// static int lept_parse_true(lept_context* c, lept_value* v) {
//     EXPECT(c, 't');
//     if (c->json[0] != 'r' || c->json[1] != 'u' || c->json[2] != 'e')
//         return LEPT_PARSE_INVALID_VALUE;
//     c->json += 3;
//     v->type = LEPT_TRUE;
//     return LEPT_PARSE_OK;
// }

// static int lept_parse_false(lept_context* c, lept_value* v) {
//     EXPECT(c, 'f');
//     if (c->json[0] != 'a' || c->json[1] != 'l' || c->json[2] != 's' || c->json[3] != 'e')
//         return LEPT_PARSE_INVALID_VALUE;
//     c->json += 4;
//     v->type = LEPT_FALSE;
//     return LEPT_PARSE_OK;
// }

// static int lept_parse_null(lept_context* c, lept_value* v) {
//     EXPECT(c, 'n');
//     if (c->json[0] != 'u' || c->json[1] != 'l' || c->json[2] != 'l')
//         return LEPT_PARSE_INVALID_VALUE;
//     c->json += 3;
//     v->type = LEPT_NULL;
//     return LEPT_PARSE_OK;
// }

static int lept_parse_literal(lept_context* c, lept_value* v, const char* literal, lept_type type) {
    EXPECT(c, *literal++);
    while (*literal != '\0') 
        if (*c->json == '\0' || *c->json++ != *literal++)
            return LEPT_PARSE_INVALID_VALUE;

    v->type = type;
    return LEPT_PARSE_OK;
}

static int lept_parse_number(lept_context* c, lept_value* v) {
    // char* end;
    const char* number = c->json;

    /* \TODO validate number */
    // v->n = strtod(c->json, &end);
    // if (c->json == end)
        // return LEPT_PARSE_INVALID_VALUE;
    // c->json = end;
    
    // validate number
    if ('-' == *number) number++;
    if (!isdigit(*number))
        return LEPT_PARSE_INVALID_VALUE;
    if ('0' == *number) number++; 
    else 
        while (*number != '\0') {
            if (!isdigit(*number))
                break;
            
            number++;
        }

    if ('.' == *number) {
        if ('\0' == *(number + 1)) 
            return LEPT_PARSE_INVALID_VALUE;
        while (*++number != '\0' && isdigit(*number));
    }

    if ('e' == tolower(*number)) {
        switch(*(number + 1)) {
            case '\0':
                return LEPT_PARSE_INVALID_VALUE;
            case '+':
            case '-':
                number++;
                if ('\0' == *(number + 1))
                    return LEPT_PARSE_INVALID_VALUE;
            default:
                while (*++number != '\0' && isdigit(*number));
        }
    }

    errno = 0;
    v->n = strtod(c->json, NULL);
    if (ERANGE == errno && (HUGE_VAL == v->n || -HUGE_VAL == v->n))
        return LEPT_PARSE_NUMBER_TOO_BIG;

    v->type = LEPT_NUMBER;
    c->json = number;
    return LEPT_PARSE_OK;
}

static int lept_parse_value(lept_context* c, lept_value* v) {
    switch (*c->json) {
        // case 't':  return lept_parse_true(c, v);
        // case 'f':  return lept_parse_false(c, v);
        // case 'n':  return lept_parse_null(c, v);
        case 't':  return lept_parse_literal(c, v, "true", LEPT_TRUE);
        case 'f':  return lept_parse_literal(c, v, "false", LEPT_FALSE);
        case 'n':  return lept_parse_literal(c, v, "null", LEPT_NULL);
        case '\0': return LEPT_PARSE_EXPECT_VALUE;
        default:   return lept_parse_number(c, v);
    }
}

int lept_parse(lept_value* v, const char* json) {
    lept_context c;
    // int ret;
    assert(v != NULL);
    c.json = json;
    v->type = LEPT_NULL;
    // lept_parse_whitespace(&c);
    // if ((ret = lept_parse_value(&c, v)) == LEPT_PARSE_OK) {
    //     lept_parse_whitespace(&c);
    //     if (*c.json != '\0') {
    //         v->type = LEPT_NULL;
    //         ret = LEPT_PARSE_ROOT_NOT_SINGULAR;
    //     }
    // }
    // return ret;
    // parse ws
    lept_parse_whitespace(&c);
    // continue; not return lept_parse_value(&c, v);
    switch (lept_parse_value(&c, v)) {
        case LEPT_PARSE_EXPECT_VALUE:
            return LEPT_PARSE_EXPECT_VALUE;
        case LEPT_PARSE_INVALID_VALUE:
            return LEPT_PARSE_INVALID_VALUE;
        case LEPT_PARSE_NUMBER_TOO_BIG:
            return LEPT_PARSE_NUMBER_TOO_BIG;
        case LEPT_PARSE_OK:
            lept_parse_whitespace(&c);
            if (LEPT_PARSE_EXPECT_VALUE == lept_parse_value(&c, v))
                return LEPT_PARSE_OK;
    }

    v->type = LEPT_NULL;

    return LEPT_PARSE_ROOT_NOT_SINGULAR;
}

lept_type lept_get_type(const lept_value* v) {
    assert(v != NULL);
    return v->type;
}

double lept_get_number(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_NUMBER);
    return v->n;
}
