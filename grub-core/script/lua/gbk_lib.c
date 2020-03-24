/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2009,2018,2019,2020  Free Software Foundation, Inc.
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "grub_lib.h"
#include "gbk.h"

#define UTF_MAX     8
#define DBCS_MAX    2

static unsigned from_utf8(unsigned uni_code) {
    const unsigned short *page = from_uni[(uni_code >> 8) & 0xFF];
    return page == NULL ? DBCS_DEFAULT_CODE : page[uni_code & 0xFF];
}

static unsigned to_utf8(unsigned cp_code) {
    const unsigned short *page = to_uni[(cp_code >> 8) & 0xFF];
    return page == NULL ? UNI_INVALID_CODE : page[cp_code & 0xFF];
}

static size_t utf8_encode(char *s, unsigned ch) {
    if (ch < 0x80) {
        s[0] = (char)ch;
        return 1;
    }
    if (ch <= 0x7FF) {
        s[1] = (char) ((ch | 0x80) & 0xBF);
        s[0] = (char) ((ch >> 6) | 0xC0);
        return 2;
    }
    if (ch <= 0xFFFF) {
three:
        s[2] = (char) ((ch | 0x80) & 0xBF);
        s[1] = (char) (((ch >> 6) | 0x80) & 0xBF);
        s[0] = (char) ((ch >> 12) | 0xE0);
        return 3;
    }
    if (ch <= 0x1FFFFF) {
        s[3] = (char) ((ch | 0x80) & 0xBF);
        s[2] = (char) (((ch >> 6) | 0x80) & 0xBF);
        s[1] = (char) (((ch >> 12) | 0x80) & 0xBF);
        s[0] = (char) ((ch >> 18) | 0xF0);
        return 4;
    }
    if (ch <= 0x3FFFFFF) {
        s[4] = (char) ((ch | 0x80) & 0xBF);
        s[3] = (char) (((ch >> 6) | 0x80) & 0xBF);
        s[2] = (char) (((ch >> 12) | 0x80) & 0xBF);
        s[1] = (char) (((ch >> 18) | 0x80) & 0xBF);
        s[0] = (char) ((ch >> 24) | 0xF8);
        return 5;
    }
    if (ch <= 0x7FFFFFFF) {
        s[5] = (char) ((ch | 0x80) & 0xBF);
        s[4] = (char) (((ch >> 6) | 0x80) & 0xBF);
        s[3] = (char) (((ch >> 12) | 0x80) & 0xBF);
        s[2] = (char) (((ch >> 18) | 0x80) & 0xBF);
        s[1] = (char) (((ch >> 24) | 0x80) & 0xBF);
        s[0] = (char) ((ch >> 30) | 0xFC);
        return 6;
    }

    /* fallback */
    ch = 0xFFFD;
    goto three;
}

static size_t utf8_decode(const char *s, const char *e, unsigned *pch) {
    unsigned ch;

    if (s >= e) {
        *pch = 0;
        return 0;   
    }

    ch = (unsigned char)s[0];
    if (ch < 0xC0) goto fallback;
    if (ch < 0xE0) {
        if (s+1 >= e || (s[1] & 0xC0) != 0x80)
            goto fallback;
        *pch = ((ch   & 0x1F) << 6) | (s[1] & 0x3F);
        return 2;
    }
    if (ch < 0xF0) {
        if (s+2 >= e || (s[1] & 0xC0) != 0x80
                || (s[2] & 0xC0) != 0x80)
            goto fallback;
        *pch = ((ch   & 0x0F) << 12) | ((s[1] & 0x3F) <<  6) | (s[2] & 0x3F);
        return 3;
    }
    {
        int count = 0; /* to count number of continuation bytes */
        unsigned res = 0;
        while ((ch & 0x40) != 0) { /* still have continuation bytes? */
            int cc = (unsigned char)s[++count];
            if ((cc & 0xC0) != 0x80) /* not a continuation byte? */
                goto fallback; /* invalid byte sequence, fallback */
            res = (res << 6) | (cc & 0x3F); /* add lower 6 bits from cont. byte */
            ch <<= 1; /* to test next bit */
        }
        if (count > 5)
            goto fallback; /* invalid byte sequence */
        res |= ((ch & 0x7F) << (count * 5)); /* add first byte */
        *pch = res;
        return count+1;
    }

fallback:
    *pch = ch;
    return 1;
}

static void add_utf8char(luaL_Buffer *b, unsigned ch) {
    char buff[UTF_MAX];
    size_t n = utf8_encode(buff, ch);
    luaL_addlstring(b, buff, n);
}

static size_t dbcs_decode(const char *s, const char *e, unsigned *pch) {
    unsigned ch;
    if (s >= e) {
        *pch = 0;
        return 0;
    }

    ch = s[0] & 0xFF;
    if (to_uni_00[ch] != UNI_INVALID_CODE) {
        *pch = ch;
        return 1;
    }

    *pch = (ch << 8) | (s[1] & 0xFF);
    return 2;
}

static void add_dbcschar(luaL_Buffer *b, unsigned ch) {
    if (ch < 0x7F)
        luaL_addchar(b, ch);
    else {
        luaL_addchar(b, (ch >> 8) & 0xFF);
        luaL_addchar(b, ch & 0xFF);
    }
}

static size_t dbcs_length(const char *s, const char *e) {
    size_t dbcslen = 0;
    while (s < e) {
        if ((unsigned char)(*s++) > 0x7F)
            ++s;
        ++dbcslen;
    }
    return dbcslen;
}


/* dbcs module interface */

static const char *check_dbcs(lua_State *L, int idx, const char **pe) {
    size_t len;
    const char *s = luaL_checklstring(L, idx, &len);
    if (pe != NULL) *pe = s + len;
    return s;
}

static int posrelat(int pos, size_t len) {
    if (pos >= 0) return (size_t)pos;
    else if (0u - (size_t)pos > len) return 0;
    else return len - ((size_t)-pos) + 1;
}

static int string_from_utf8(lua_State *L) {
    const char *e, *s = check_dbcs(L, 1, &e);
    luaL_Buffer b;
    luaL_buffinit(L, &b);
    while (s < e) {
        unsigned ch;
        s += utf8_decode(s, e, &ch);
        add_dbcschar(&b, from_utf8(ch));
    }
    luaL_pushresult(&b);
    return 1;
}

static int string_to_utf8(lua_State *L) {
    const char *e, *s = check_dbcs(L, 1, &e);
    luaL_Buffer b;
    luaL_buffinit(L, &b);
    while (s < e) {
        unsigned ch;
        s += dbcs_decode(s, e, &ch);
        add_utf8char(&b, to_utf8(ch));
    }
    luaL_pushresult(&b);
    return 1;
}

static int lua_gbk_len (lua_State *state) {
    const char *e, *s = check_dbcs(state, 1, &e);
    lua_pushinteger(state, dbcs_length(s, e));
    return 1;
}

static int lua_gbk_byte(lua_State *state) {
    const char *e, *s = check_dbcs(state, 1, &e);
    size_t len = dbcs_length(s, e);
    int posi = posrelat((int)luaL_optinteger(state, 2, 1), len);
    int pose = posrelat((int)luaL_optinteger(state, 3, posi), len);
    const char *start = s;
    int i, n;
    if (posi < 1) posi = 1;
    if (pose > (int)len) pose = len;
    n = (int)(pose - posi + 1);
    if (posi + n <= pose) /* (size_t -> int) overflow? */
        return luaL_error(state, "string slice too long");
    luaL_checkstack(state, n, "string slice too long");
    for (i = 0; i < posi; ++i) {
        unsigned ch;
        start += dbcs_decode(start, e, &ch);
    }
    for (i = 0; i < n; ++i) {
        unsigned ch;
        start += dbcs_decode(start, e, &ch);
        lua_pushinteger(state, ch);
    }
    return n;
}

static int lua_gbk_char(lua_State *state) {
    luaL_Buffer b;
    int i, top = lua_gettop(state);
    luaL_buffinit(state, &b);
    for (i = 1; i <= top; ++i)
        add_dbcschar(&b, (unsigned)luaL_checkinteger(state, i));
    luaL_pushresult(&b);
    return 1;
}

static int lua_gbk_fromutf8(lua_State *state) {
    int i, top;
    switch (lua_type(state, 1)) {
    case LUA_TSTRING:
        return string_from_utf8(state);
    case LUA_TNUMBER:
        top = lua_gettop(state);
        for (i = 1; i <= top; ++i) {
            unsigned code = (unsigned)luaL_checkinteger(state, i);
            lua_pushinteger(state, (lua_Integer)from_utf8(code));
            lua_replace(state, i);
        }
        return top;
    }
    return luaL_error(state, "string/number expected, got %s",
            luaL_typename(state, 1));
}

static int lua_gbk_toutf8(lua_State *state) {
    int i, top;
    switch (lua_type(state, 1)) {
    case LUA_TSTRING:
        return string_to_utf8(state);
    case LUA_TNUMBER:
        top = lua_gettop(state);
        for (i = 1; i <= top; ++i) {
            unsigned code = (unsigned)luaL_checkinteger(state, i);
            lua_pushinteger(state, to_utf8(code));
            lua_replace(state, i);
        }
        return top;
    }
    return luaL_error(state, "string/number expected, got %s",
            luaL_typename(state, 1));
}

luaL_Reg gbklib[] = {
    {"len", lua_gbk_len},
    {"byte", lua_gbk_byte},
    {"char", lua_gbk_char},
    {"fromutf8", lua_gbk_fromutf8},
    {"toutf8", lua_gbk_toutf8},
    {0, 0}
};
