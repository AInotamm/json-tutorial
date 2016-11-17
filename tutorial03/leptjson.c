#include "leptjson.h"
#include <assert.h>  /* assert() */
#include <errno.h>   /* errno, ERANGE */
#include <math.h>    /* HUGE_VAL */
#include <ctype.h>   /* isdigit() */
#include <stdlib.h>  /* NULL, malloc(), realloc(), free(), strtod() */
#include <string.h>  /* memcpy() */

#ifndef LEPT_PARSE_STACK_INIT_SIZE
#define LEPT_PARSE_STACK_INIT_SIZE 256
#endif

#define EXPECT(c, ch)       do { assert(*c->json == (ch)); c->json++; } while(0)
#define ISDIGIT(ch)         ((ch) >= '0' && (ch) <= '9')
#define ISDIGIT1TO9(ch)     ((ch) >= '1' && (ch) <= '9')
#define PUTC(c, ch)         do { *(char*)lept_context_push(c, sizeof(char)) = (ch); } while(0)

typedef struct {
    const char* json;
    char* stack;
    size_t size, top;
}lept_context;

static void* lept_context_push(lept_context* c, size_t size) {
    void* ret;
    assert(size > 0);
    if (c->top + size >= c->size) {
        if (c->size == 0)
            c->size = LEPT_PARSE_STACK_INIT_SIZE;
        while (c->top + size >= c->size)
            c->size += c->size >> 1;  /* c->size * 1.5 */
        c->stack = (char*)realloc(c->stack, c->size);
    }
    ret = c->stack + c->top;
    c->top += size;
    return ret;
}

static void* lept_context_pop(lept_context* c, size_t size) {
    assert(c->top >= size);
    return c->stack + (c->top -= size);
}

static void lept_parse_whitespace(lept_context* c) {
    const char *p = c->json;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    c->json = p;
}

// replace own version
static int lept_parse_literal(lept_context* c, lept_value* v, const char* literal, lept_type type) {
    EXPECT(c, *literal++);
    while (*literal != '\0') 
        if (*c->json == '\0' || *c->json++ != *literal++)
            return LEPT_PARSE_INVALID_VALUE;

    v->type = type;
    return LEPT_PARSE_OK;
}

// replace own version
static int lept_parse_number(lept_context* c, lept_value* v) {
    const char* number = c->json;
    
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
    v->u.n = strtod(c->json, NULL);
    if (ERANGE == errno && (HUGE_VAL == v->u.n || -HUGE_VAL == v->u.n))
        return LEPT_PARSE_NUMBER_TOO_BIG;

    v->type = LEPT_NUMBER;
    c->json = number;
    return LEPT_PARSE_OK;
}

// string = quotation-mark *char quotation-mark
// char = unescaped /
//    escape (
//        %x22 /          ; "    quotation mark  U+0022
//        %x5C /          ; \    reverse solidus U+005C
//        %x2F /          ; /    solidus         U+002F
//        %x62 /          ; b    backspace       U+0008
//        %x66 /          ; f    form feed       U+000C
//        %x6E /          ; n    line feed       U+000A
//        %x72 /          ; r    carriage return U+000D
//        %x74 /          ; t    tab             U+0009
//        %x75 4HEXDIG )  ; uXXXX                U+XXXX
// escape = %x5C          ; \
// quotation-mark = %x22  ; "
// unescaped = %x20-21 / %x23-5B / %x5D-10FFFF
static int lept_parse_string(lept_context* c, lept_value* v) {
    size_t head = c->top, len;
    const char* p;

    EXPECT(c, '\"'); // 一定是以转义引号开头

    p = c->json;

    for (;;) {
        char ch = *p++;
        switch (ch) {
            case '\\':
                ch = *p++;
                switch (ch) {
                    case 'b': PUTC(c, '\b'); break;
                    case 'f': PUTC(c, '\f'); break;
                    case 'n': PUTC(c, '\n'); break;
                    case 'r': PUTC(c, '\r'); break;
                    case 't': PUTC(c, '\t'); break;
                    case '/':
                    case '\\':
                    case '\"':
                        PUTC(c, ch); break;
                    default:
                        c->top = head;
                        return LEPT_PARSE_INVALID_STRING_ESCAPE;
                }
                break;
            case '\"':
                len = c->top - head;
                lept_set_string(v, (const char*)lept_context_pop(c, len), len);
                c->json = p;
                return LEPT_PARSE_OK;
            case '\0':
                c->top = head;
                return LEPT_PARSE_MISS_QUOTATION_MARK;
            default:
                // 非法转义范围
                if ((unsigned char)ch < 0x20) {
                    c->top = head;
                    return LEPT_PARSE_INVALID_STRING_CHAR;
                }
                PUTC(c, ch); // 把字符压入堆栈缓冲区
        }
    }
}

static int lept_parse_value(lept_context* c, lept_value* v) {
    switch (*c->json) {
        case 't':  return lept_parse_literal(c, v, "true", LEPT_TRUE);
        case 'f':  return lept_parse_literal(c, v, "false", LEPT_FALSE);
        case 'n':  return lept_parse_literal(c, v, "null", LEPT_NULL);
        default:   return lept_parse_number(c, v);
        case '"':  return lept_parse_string(c, v);
        case '\0': return LEPT_PARSE_EXPECT_VALUE;
    }
}

int lept_parse(lept_value* v, const char* json) {
    lept_context c;
    int ret;
    assert(v != NULL);
    c.json = json;
    c.stack = NULL;
    c.size = c.top = 0;
    lept_init(v);
    lept_parse_whitespace(&c);
    if ((ret = lept_parse_value(&c, v)) == LEPT_PARSE_OK) {
        lept_parse_whitespace(&c);
        if (*c.json != '\0') {
            v->type = LEPT_NULL;
            ret = LEPT_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    assert(c.top == 0);
    free(c.stack);
    return ret;
}

void lept_free(lept_value* v) {
    assert(v != NULL);
    if (v->type == LEPT_STRING)
        free(v->u.s.s);
    v->type = LEPT_NULL;
}

lept_type lept_get_type(const lept_value* v) {
    assert(v != NULL);
    return v->type;
}

int lept_get_boolean(const lept_value* v) {
    /* \TODO */
    assert(v != NULL && (v->type == LEPT_TRUE || v->type == LEPT_FALSE));
    //return v->u.n;
    return v->type == LEPT_TRUE;
}

void lept_set_boolean(lept_value* v, int b) {
    /* \TODO */
    //assert(v != NULL); 
    lept_free(v);
    //v->u.n = (b != 0);
    //v->type = (v->u.n) ? LEPT_TRUE : LEPT_FALSE;
    v->type = b ? LEPT_TRUE : LEPT_FALSE;
}

double lept_get_number(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_NUMBER);
    return v->u.n;
}

void lept_set_number(lept_value* v, double n) {
    /* \TODO */
    //assert(v != NULL);
    lept_free(v);
    v->u.n = n;
    v->type = LEPT_NUMBER;
}

const char* lept_get_string(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_STRING);
    return v->u.s.s;
}

size_t lept_get_string_length(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_STRING);
    return v->u.s.len;
}

void lept_set_string(lept_value* v, const char* s, size_t len) {
    assert(v != NULL && (s != NULL || len == 0));
    lept_free(v);
    v->u.s.s = (char*)malloc(len + 1);
    memcpy(v->u.s.s, s, len);
    v->u.s.s[len] = '\0';
    v->u.s.len = len;
    v->type = LEPT_STRING;
}
