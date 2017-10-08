/*
 +-------------------+
 |                   |  Christian Schlittchen <corwin@amber.kn-bremen.de>
 | Eressea PBEM host |  Enno Rehling <enno@eressea.de>
 | (c) 1998 - 2014   |  Katja Zedel <katze@felidae.kn-bremen.de>
 |                   |
 +-------------------+

 This program may not be used, modified or distributed
 without prior permission by the authors of Eressea.
 */

#include <platform.h>
#include <kernel/config.h>
#include "order.h"

#include "skill.h"
#include "keyword.h"

#include <util/base36.h>
#include <util/bsdstring.h>
#include <util/language.h>
#include <util/log.h>
#include <util/parser.h>

/* libc includes */
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

# define ORD_KEYWORD(ord) (keyword_t)((ord)->command & 0xFFFF)
# define OD_LOCALE(odata) ((odata) ? (odata)->lang : NULL)
# define OD_STRING(odata) ((odata) ? (odata)->_str : NULL)

typedef struct order_data {
    const char *_str;
    int _refcount;
    const struct locale *lang;
} order_data;

#include <selist.h>

static selist * orders;

order_data *load_data(int id) {
    if (id > 0) {
        order_data * od = (order_data *)selist_get(orders, id - 1);
        ++od->_refcount;
        return od;
    }
    return NULL;
}

int add_data(order_data *od) {
    if (od->_str) {
        ++od->_refcount;
        selist_push(&orders, od);
        return selist_length(orders);
    }
    return 0;
}

static void release_data(order_data * data)
{
    if (data) {
        if (--data->_refcount == 0) {
            free(data);
        }
    }
}

void free_data_cb(void *entry) {
    order_data *od = (order_data *)entry;
    if (od->_refcount > 1) {
        log_error("refcount=%d for order %s", od->_refcount, od->_str);
    }
    release_data(od);
}

void free_data(void) {
    selist_foreach(orders, free_data_cb);
    selist_free(orders);
    orders = NULL;
}

void replace_order(order ** dlist, order * orig, const order * src)
{
    assert(src);
    assert(orig);
    assert(dlist);
    while (*dlist != NULL) {
        order *dst = *dlist;
        if (dst->id == orig->id) {
            order *cpy = copy_order(src);
            *dlist = cpy;
            cpy->next = dst->next;
            dst->next = 0;
            free_order(dst);
        }
        dlist = &(*dlist)->next;
    }
}

keyword_t getkeyword(const order * ord)
{
    if (ord == NULL) {
        return NOKEYWORD;
    }
    return ORD_KEYWORD(ord);
}

/** returns a plain-text representation of the order.
 * This is the inverse function to the parse_order command. Note that
 * keywords are expanded to their full length.
 */
char* get_command(const order *ord, char *sbuffer, size_t size) {
    char *bufp = sbuffer;
    order_data *od;
    const char * text;
    keyword_t kwd = ORD_KEYWORD(ord);
    int bytes;

    if (ord->command & CMD_QUIET) {
        if (size > 0) {
            *bufp++ = '!';
            --size;
        }
        else {
            WARN_STATIC_BUFFER();
        }
    }
    if (ord->command & CMD_PERSIST) {
        if (size > 0) {
            *bufp++ = '@';
            --size;
        }
        else {
            WARN_STATIC_BUFFER();
        }
    }

    od = load_data(ord->id);
    text = OD_STRING(od);
    if (kwd != NOKEYWORD) {
        const struct locale *lang = OD_LOCALE(od);
        if (size > 0) {
            const char *str = (const char *)LOC(lang, keyword(kwd));
            assert(str);
            if (text) --size;
            bytes = (int)strlcpy(bufp, str, size);
            if (wrptr(&bufp, &size, bytes) != 0) {
                WARN_STATIC_BUFFER();
            }
            if (text) *bufp++ = ' ';
        }
        else {
            WARN_STATIC_BUFFER();
        }
    }
    if (text) {
        bytes = (int)strlcpy(bufp, (const char *)text, size);
        if (wrptr(&bufp, &size, bytes) != 0) {
            WARN_STATIC_BUFFER();
            if (bufp - sbuffer >= 6) {
                bufp -= 6;
                while (bufp > sbuffer && (*bufp & 0x80) != 0) {
                    ++size;
                    --bufp;
                }
                memcpy(bufp, "[...]", 6);   /* TODO: make sure this only happens in eval_command */
                bufp += 6;
            }
        }
    }
    release_data(od);
    if (size > 0) *bufp = 0;
    return sbuffer;
}

void free_order(order * ord)
{
    if (ord != NULL) {
        assert(ord->next == 0);
        free(ord);
    }
}

order *copy_order(const order * src)
{
    if (src != NULL) {
        order *ord = (order *)malloc(sizeof(order));
        ord->next = NULL;
        ord->command = src->command;
        ord->id = src->id;
        return ord;
    }
    return NULL;
}

void set_order(struct order **destp, struct order *src)
{
    if (*destp == src)
        return;
    free_order(*destp);
    *destp = src;
}

void free_orders(order ** olist)
{
    while (*olist) {
        order *ord = *olist;
        *olist = ord->next;
        ord->next = NULL;
        free_order(ord);
    }
}

static char *mkdata(order_data **pdata, size_t len, const struct locale *lang, const char *str)
{
    order_data *data;
    char *result;
    data = malloc(sizeof(order_data) + len + 1);
    result = (char *)(data + 1);
    data->lang = lang;
    data->_refcount = 0;
    data->_str = (len > 0) ? result : NULL;
    if (str) strcpy(result, str);
    if (pdata) *pdata = data;
    return result;
}

static order_data *create_data(keyword_t kwd, const char *sptr, const struct locale *lang)
{
    const char *s = sptr;
    order_data *data;

    if (kwd != NOKEYWORD)
        s = (*sptr) ? sptr : NULL;

    /* orders with no parameter, only one order_data per order required */
    if (kwd != NOKEYWORD && *sptr == 0) {
        mkdata(&data, 0, lang, NULL);
        data->_refcount = 1;
        return data;
    }
    mkdata(&data, s ? strlen(s) : 0, lang, s);
    data->_refcount = 1;
    return data;
}

static order *create_order_i(order *ord, keyword_t kwd, const char *sptr, bool persistent,
    bool noerror, const struct locale *lang)
{
    order_data *od;

    assert(ord);
    if (kwd == NOKEYWORD || keyword_disabled(kwd)) {
        log_error("trying to create an order for disabled keyword %s.", keyword(kwd));
        return NULL;
    }

    /* if this is just nonsense, then we skip it. */
    if (lomem) {
        switch (kwd) {
        case K_KOMMENTAR:
        case NOKEYWORD:
            return NULL;
        default:
            break;
        }
    }

    ord->command = (int)kwd;
    if (persistent) ord->command |= CMD_PERSIST;
    if (noerror) ord->command |= CMD_QUIET;
    ord->next = NULL;

    while (isspace(*(unsigned char *)sptr)) ++sptr;

    od = create_data(kwd, sptr, lang);
    ord->id = add_data(od);
    release_data(od);

    return ord;
}

order *create_order(keyword_t kwd, const struct locale * lang,
    const char *params, ...)
{
    order *ord;
    char zBuffer[DISPLAYSIZE];
    if (params) {
        char *bufp = zBuffer;
        int bytes;
        size_t size = sizeof(zBuffer) - 1;
        va_list marker;

        assert(lang);
        va_start(marker, params);
        while (*params) {
            if (*params == '%') {
                int i;
                const char *s;
                ++params;
                switch (*params) {
                case 's':
                    s = va_arg(marker, const char *);
                    assert(s);
                    bytes = (int)strlcpy(bufp, s, size);
                    if (wrptr(&bufp, &size, bytes) != 0)
                        WARN_STATIC_BUFFER();
                    break;
                case 'd':
                    i = va_arg(marker, int);
                    bytes = (int)strlcpy(bufp, itoa10(i), size);
                    if (wrptr(&bufp, &size, bytes) != 0)
                        WARN_STATIC_BUFFER();
                    break;
                case 'i':
                    i = va_arg(marker, int);
                    bytes = (int)strlcpy(bufp, itoa36(i), size);
                    if (wrptr(&bufp, &size, bytes) != 0)
                        WARN_STATIC_BUFFER();
                    break;
                default:
                    assert(!"unknown format-character in create_order");
                }
            }
            else if (size > 0) {
                *bufp++ = *params;
                --size;
            }
            ++params;
        }
        va_end(marker);
        *bufp = 0;
    }
    else {
        zBuffer[0] = 0;
    }
    ord = (order *)malloc(sizeof(order));
    return create_order_i(ord, kwd, zBuffer, false, false, lang);
}

order *parse_order(const char *s, const struct locale * lang)
{
    assert(lang);
    assert(s);
    if (*s != 0) {
        keyword_t kwd = NOKEYWORD;
        const char *sptr = s;
        bool persistent = false, noerror = false;
        const char * p;

        p = *sptr ? parse_token_depr(&sptr) : 0;
        if (p) {
            while (*p == '!' || *p == '@') {
                if (*p == '!') noerror = true;
                else if (*p == '@') persistent = true;
                ++p;
            }
            kwd = get_keyword(p, lang);
        }
        if (kwd == K_MAKE) {
            const char *sp = sptr;
            p = parse_token_depr(&sp);
            if (p && isparam(p, lang, P_TEMP)) {
                kwd = K_MAKETEMP;
                sptr = sp;
            }
        }
        if (kwd != NOKEYWORD) {
            order *ord = (order *)malloc(sizeof(order));
            return create_order_i(ord, kwd, sptr, persistent, noerror, lang);
        }
    }
    return NULL;
}

/**
 * Returns true if the order qualifies as "repeated". An order is repeated if it will overwrite the
 * old default order. K_BUY is in this category, but not K_MOVE.
 *
 * \param ord An order.
 * \return true if the order is long
 * \sa is_exclusive(), is_repeated(), is_persistent()
 */
bool is_repeated(keyword_t kwd)
{
    switch (kwd) {
    case K_CAST:
    case K_BUY:
    case K_SELL:
    case K_ROUTE:
    case K_DRIVE:
    case K_WORK:
    case K_BESIEGE:
    case K_ENTERTAIN:
    case K_TAX:
    case K_RESEARCH:
    case K_SPY:
    case K_STEAL:
    case K_SABOTAGE:
    case K_STUDY:
    case K_TEACH:
    case K_GROW:
    case K_PLANT:
    case K_PIRACY:
    case K_MAKE:
    case K_LOOT:
    case K_DESTROY:
        return true;

    default:
        break;
    }
    return false;
}

/**
 * Returns true if the order qualifies as "exclusive". An order is exclusive if it makes all other
 * long orders illegal. K_MOVE is in this category, but not K_BUY.
 *
 * \param ord An order.
 * \return true if the order is long
 * \sa is_exclusive(), is_repeated(), is_persistent()
 */
bool is_exclusive(const order * ord)
{
    keyword_t kwd = ORD_KEYWORD(ord);

    switch (kwd) {
    case K_MOVE:
    case K_ROUTE:
    case K_DRIVE:
    case K_WORK:
    case K_BESIEGE:
    case K_ENTERTAIN:
    case K_TAX:
    case K_RESEARCH:
    case K_SPY:
    case K_STEAL:
    case K_SABOTAGE:
    case K_STUDY:
    case K_TEACH:
    case K_GROW:
    case K_PLANT:
    case K_PIRACY:
    case K_MAKE:
    case K_LOOT:
    case K_DESTROY:
        return true;

    default:
        break;
    }
    return false;
}

/**
 * Returns true if the order qualifies as "long". An order is long if it excludes most other long
 * orders.
 *
 * \param ord An order.
 * \return true if the order is long
 * \sa is_exclusive(), is_repeated(), is_persistent()
 */
bool is_long(keyword_t kwd)
{
    switch (kwd) {
    case K_CAST:
    case K_BUY:
    case K_SELL:
    case K_MOVE:
    case K_ROUTE:
    case K_DRIVE:
    case K_WORK:
    case K_BESIEGE:
    case K_ENTERTAIN:
    case K_TAX:
    case K_RESEARCH:
    case K_SPY:
    case K_STEAL:
    case K_SABOTAGE:
    case K_STUDY:
    case K_TEACH:
    case K_GROW:
    case K_PLANT:
    case K_PIRACY:
    case K_MAKE:
    case K_LOOT:
    case K_DESTROY:
        return true;

    default:
        break;
    }
    return false;
}

/**
 * Returns true if the order qualifies as "persistent". An order is persistent if it will be
 * included in the template orders. @-orders, comments and most long orders are in this category,
 * but not K_MOVE.
 *
 * \param ord An order.
 * \return true if the order is persistent
 * \sa is_exclusive(), is_repeated(), is_persistent()
 */
bool is_persistent(const order * ord)
{
    keyword_t kwd = ORD_KEYWORD(ord);
    switch (kwd) {
    case K_MOVE:
    case NOKEYWORD:
        /* lang, aber niemals persistent! */
        return false;
    case K_KOMMENTAR:
        return true;
    default:
        return (ord->command & CMD_PERSIST) || is_repeated(kwd);
    }
}

bool is_silent(const order * ord)
{
    return (ord->command & CMD_QUIET) != 0;
}

char *write_order(const order * ord, char *buffer, size_t size)
{
    if (ord == 0) {
        buffer[0] = 0;
    }
    else {
        keyword_t kwd = ORD_KEYWORD(ord);
        if (kwd == NOKEYWORD) {
            order_data *od = load_data(ord->id);
            const char *text = OD_STRING(od);
            if (text) strlcpy(buffer, (const char *)text, size);
            else buffer[0] = 0;
            release_data(od);
        }
        else {
            get_command(ord, buffer, size);
        }
    }
    return buffer;
}

void push_order(order ** ordp, order * ord)
{
    while (*ordp)
        ordp = &(*ordp)->next;
    *ordp = ord;
}

static order_data *parser_od;

keyword_t init_order(const struct order *ord)
{
    if (!ord) {
        release_data(parser_od);
        parser_od = NULL;
        return NOKEYWORD;
    }
    else {
        if (parser_od) {
            // TODO: warning
            release_data(parser_od);
        }
        parser_od = load_data(ord->id);
        init_tokens_str(OD_STRING(parser_od));
        return ORD_KEYWORD(ord);
    }
}

void close_orders(void) {
    if (parser_od) {
        init_order(NULL);
    }
    free_data();
}

