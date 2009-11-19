/* Copyright 2009
 * Kaz Kylheku <kkylheku@gmail.com>
 * Vancouver, Canada
 * All rights reserved.
 *
 * BSD License:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in
 *      the documentation and/or other materials provided with the
 *      distribution.
 *   3. The name of the author may not be used to endorse or promote
 *      products derived from this software without specific prior
 *      written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>
#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <dirent.h>
#include <setjmp.h>
#include <wchar.h>
#include "lib.h"
#include "gc.h"
#include "unwind.h"
#include "stream.h"
#include "utf8.h"

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

obj_t *interned_syms;

obj_t *null, *t, *cons_t, *str_t, *chr_t, *num_t, *sym_t, *fun_t, *vec_t;
obj_t *stream_t, *hash_t, *lcons_t, *lstr_t, *cobj_t;
obj_t *var, *regex, *set, *cset, *wild, *oneplus;
obj_t *zeroplus, *optional, *compound, *or, *quasi;
obj_t *skip, *trailer, *block, *next, *freeform, *fail, *accept;
obj_t *all, *some, *none, *maybe, *cases, *collect, *until, *coll;
obj_t *define, *output, *single, *frst, *lst, *empty, *repeat, *rep;
obj_t *flattn, *forget, *local, *mrge, *bind, *cat, *args;
obj_t *try, *catch, *finally, *nothrow, *throw, *defex;
obj_t *error, *type_error, *internal_err, *numeric_err, *range_err;
obj_t *query_error, *file_error, *process_error;

obj_t *zero, *one, *two, *negone, *maxint, *minint;
obj_t *null_string;
obj_t *nil_string;
obj_t *null_list;

obj_t *identity_f;
obj_t *equal_f;

obj_t *prog_string;

void *(*oom_realloc)(void *, size_t);


obj_t *identity(obj_t *obj)
{
  return obj;
}

static obj_t *identity_tramp(obj_t *env, obj_t *obj)
{
  (void) env;
  return identity(obj);
}

static obj_t *equal_tramp(obj_t *env, obj_t *, obj_t *);

static obj_t *code2type(int code)
{
  switch ((type_t) code) {
  case CONS: return cons_t;
  case STR: return str_t;
  case LIT: return str_t;
  case CHR: return chr_t;
  case NUM: return num_t;
  case SYM: return sym_t;
  case FUN: return fun_t;
  case VEC: return vec_t;
  case LCONS: return lcons_t;
  case LSTR: return lstr_t;
  case COBJ: return cobj_t;
  }
  return nil;
}

obj_t *typeof(obj_t *obj)
{
  switch (tag(obj)) {
  case TAG_NUM:
    return num_t;
  case TAG_CHR:
    return chr_t;
  case TAG_PTR:
    if (obj == nil) {
      return null;
    } else if (obj->t.type == COBJ) {
      return obj->co.cls;
    } else {
      obj_t *type = code2type(obj->t.type);
      if (!type)
        internal_error("corrupt type field");
      return type;
    }
  default:
    internal_error("invalid type tag");
  }
}

obj_t *type_check(obj_t *obj, int type)
{
  if (!is_ptr(obj) || obj->t.type != type)
    type_mismatch(lit("~s is not of type ~s"), obj, code2type(type), nao);
  return t;
}

obj_t *type_check2(obj_t *obj, int t1, int t2)
{
  if (!is_ptr(obj) || (obj->t.type != t1 && obj->t.type != t2))
    type_mismatch(lit("~s is not of type ~s or ~s"), obj,
                  code2type(t1), code2type(t2), nao);
  return t;
}

obj_t *type_check3(obj_t *obj, int t1, int t2, int t3)
{
  if (!is_ptr(obj) || (obj->t.type != t1 && obj->t.type != t2
                       && obj->t.type != t3))
    type_mismatch(lit("~s is not of type ~s, ~s nor ~s"), obj,
                  code2type(t1), code2type(t2), code2type(t3), nao);
  return t;
}

obj_t *car(obj_t *cons)
{
  if (cons == nil)
    return nil;
  else switch (type(cons)) {
  case CONS:
    return cons->c.car;
  case LCONS:
    if (cons->lc.func == nil) {
      return cons->lc.car;
    } else {
      if (!funcall1(cons->lc.func, cons))
        return nil;
      return cons->lc.car;
    }
  default:
    type_mismatch(lit("~s is not a cons"), cons, nao);
  }
}

obj_t *cdr(obj_t *cons)
{
  if (cons == nil)
    return nil;
  else switch (type(cons)) {
  case CONS:
    return cons->c.cdr;
  case LCONS:
    if (cons->lc.func == nil) {
      return cons->lc.cdr;
    } else {
      if (!funcall1(cons->lc.func, cons))
        return nil;
      return cons->lc.cdr;
    }
  default:
    type_mismatch(lit("~s is not a cons"), cons, nao);
  }
}

obj_t **car_l(obj_t *cons)
{
  switch (type(cons)) {
  case CONS:
    return &cons->c.car;
  case LCONS:
    funcall1(cons->lc.func, cons);
    return &cons->lc.car;
  default:
    type_mismatch(lit("~s is not a cons"), cons, nao);
  }
}

obj_t **cdr_l(obj_t *cons)
{
  switch (type(cons)) {
  case CONS:
    return &cons->c.cdr;
  case LCONS:
    funcall1(cons->lc.func, cons);
    return &cons->lc.cdr;
  default:
    type_mismatch(lit("~s is not a cons"), cons, nao);
  }
}

obj_t *first(obj_t *cons)
{
  return car(cons);
}

obj_t *rest(obj_t *cons)
{
  return cdr(cons);
}

obj_t *second(obj_t *cons)
{
  return car(cdr(cons));
}

obj_t *third(obj_t *cons)
{
  return car(cdr(cdr(cons)));
}

obj_t *fourth(obj_t *cons)
{
  return car(cdr(cdr(cdr(cons))));
}

obj_t *fifth(obj_t *cons)
{
  return car(cdr(cdr(cdr(cdr(cons)))));
}

obj_t *sixth(obj_t *cons)
{
  return car(cdr(cdr(cdr(cdr(cdr(cons))))));
}

obj_t **tail(obj_t *cons)
{
  while (cdr(cons))
    cons = cdr(cons);
  return cdr_l(cons);
}

obj_t *pop(obj_t **plist)
{
  obj_t *ret = car(*plist);
  *plist = cdr(*plist);
  return ret;
}

obj_t *push(obj_t *val, obj_t **plist)
{
  return *plist = cons(val, *plist);
}

obj_t *copy_list(obj_t *list)
{
  list_collect_decl (out, tail);

  while (consp(list)) {
    list_collect(tail, car(list));
    list = cdr(list);
  }

  list_collect_terminate(tail, list);

  return out;
}

obj_t *nreverse(obj_t *in)
{
  obj_t *rev = nil;

  while (in) {
    obj_t *temp = cdr(in);
    *cdr_l(in) = rev;
    rev = in;
    in = temp;
  }

  return rev;
}

obj_t *reverse(obj_t *in)
{
  obj_t *rev = nil;

  while (in) {
    rev = cons(car(in), rev);
    in = cdr(in);
  }

  return rev;
}

obj_t *append2(obj_t *list1, obj_t *list2)
{
  list_collect_decl (out, tail);

  while (list1) {
    list_collect(tail, car(list1));
    list1 = cdr(list1);
  }

  list_collect_terminate(tail, list2);
  return out;
}

obj_t *nappend2(obj_t *list1, obj_t *list2)
{
  obj_t *temp, *iter;

  if (list1 == nil)
    return list2;

  for (iter = list1; (temp = cdr(iter)) != nil; iter = temp)
    ; /* empty */

  *cdr_l(iter) = list2;
  return list1;
}

obj_t *flatten_helper(obj_t *env, obj_t *item)
{
  return flatten(item);
}

obj_t *memq(obj_t *obj, obj_t *list)
{
  while (list && car(list) != obj)
    list = cdr(list);
  return list;
}

obj_t *tree_find(obj_t *obj, obj_t *tree)
{
  if (equal(obj, tree))
    return t;
  else if (consp(tree))
    return some_satisfy(tree, bind2(func_n2(tree_find), obj), nil);
  return nil;
}

obj_t *some_satisfy(obj_t *list, obj_t *pred, obj_t *key)
{
  if (!key)
    key = identity_f;

  for (; list; list = cdr(list)) {
    if (funcall1(pred, funcall1(key, car(list))))
      return t;
  }

  return nil;
}

obj_t *all_satisfy(obj_t *list, obj_t *pred, obj_t *key)
{
  if (!key)
    key = identity_f;

  for (; list; list = cdr(list)) {
    if (!funcall1(pred, funcall1(key, car(list))))
      return nil;
  }

  return t;
}

obj_t *none_satisfy(obj_t *list, obj_t *pred, obj_t *key)
{
  if (!key)
    key = identity_f;

  for (; list; list = cdr(list)) {
    if (funcall1(pred, funcall1(key, car(list))))
      return nil;
  }

  return t;
}

obj_t *flatten(obj_t *list)
{
  if (atom(list))
    return cons(list, nil);

  return mappend(func_f1(nil, flatten_helper), list);
}

long c_num(obj_t *num);

obj_t *equal(obj_t *left, obj_t *right)
{
  /* Bitwise equality is equality, period. */
  if (left == right)
    return t;

  /* Objects are not bitwise equal. If either
     is nil, then they are not equal,
     since nil uses bitwise equality. */
  if (left == nil || right == nil)
    return nil;

  switch (type(left)) {
  case CHR:
  case NUM:
    return nil;
  case CONS:
  case LCONS:
    if ((type(right) == CONS || type(right) == LCONS) &&
        equal(car(left), car(right)) &&
        equal(cdr(left), cdr(right)))
    {
      return t;
    }
    return nil;
  case LIT:
    switch (type(right)) {
    case LIT:
      return wcscmp(litptr(left), litptr(right)) == 0 ? t : nil;
    case STR:
      return wcscmp(litptr(left), right->st.str) == 0 ? t : nil;
    case LSTR:
      lazy_str_force(right);
      return equal(left, right->ls.prefix);
    default:
      break;
    }
    return nil;
  case STR:
    switch (type(right)) {
    case LIT:
      return wcscmp(left->st.str, litptr(right)) == 0 ? t : nil;
    case STR:
      return wcscmp(left->st.str, right->st.str) == 0 ? t : nil;
    case LSTR:
      lazy_str_force(right);
      return equal(left, right->ls.prefix);
    default:
      break;
    }
    return nil;
  case SYM:
    return right == left ? t : nil;
  case FUN:
    if (type(right) == FUN &&
        left->f.functype == right->f.functype &&
        equal(left->f.env, right->f.env))
    {
      switch (left->f.functype) {
      case FINTERP: return (equal(left->f.f.interp_fun, right->f.f.interp_fun));
      case F0: return (left->f.f.f0 == right->f.f.f0) ? t : nil;
      case F1: return (left->f.f.f1 == right->f.f.f1) ? t : nil;
      case F2: return (left->f.f.f2 == right->f.f.f2) ? t : nil;
      case F3: return (left->f.f.f3 == right->f.f.f3) ? t : nil;
      case F4: return (left->f.f.f4 == right->f.f.f4) ? t : nil;
      case N0: return (left->f.f.n0 == right->f.f.n0) ? t : nil;
      case N1: return (left->f.f.n1 == right->f.f.n1) ? t : nil;
      case N2: return (left->f.f.n2 == right->f.f.n2) ? t : nil;
      case N3: return (left->f.f.n3 == right->f.f.n3) ? t : nil;
      case N4: return (left->f.f.n4 == right->f.f.n4) ? t : nil;
      }
      return nil;
    }
    return nil;
  case VEC:
    if (type(right) == VEC) {
      long i, fill;
      if (!equal(left->v.vec[vec_fill], right->v.vec[vec_fill]))
        return nil;
      fill = c_num(left->v.vec[vec_fill]);
      for (i = 0; i < fill; i++) {
        if (!equal(left->v.vec[i], right->v.vec[i]))
          return nil;
      }
      return t;
    }
    return nil;
  case LSTR:
    switch (type(right)) {
    case LIT:
    case STR:
    case LSTR:
      lazy_str_force(left);
      return equal(left->ls.prefix, right);
    default:
      break;
    }
    return nil;
  case COBJ:
    if (type(right) == COBJ)
      return left->co.ops->equal(left, right);
    return nil;
  }

  internal_error("unhandled case in equal function");
}

static obj_t *equal_tramp(obj_t *env, obj_t *left, obj_t *right)
{
  (void) env;
  return equal(left, right);
}

unsigned char *chk_malloc(size_t size)
{
  char *ptr = malloc(size);
  if (size && ptr == 0)
    ptr = oom_realloc(0, size);
  return ptr;
}

unsigned char *chk_realloc(void *old, size_t size)
{
  char *newptr = realloc(old, size);
  if (size != 0 && newptr == 0)
    newptr = oom_realloc(old, size);
  return newptr;
}

wchar_t *chk_strdup(const wchar_t *str)
{
  size_t nchar = wcslen(str) + 1;
  wchar_t *copy = (wchar_t *) chk_malloc(nchar * sizeof *copy);
  wmemcpy(copy, str, nchar);
  return copy;
}


obj_t *cons(obj_t *car, obj_t *cdr)
{
  obj_t *obj = make_obj();
  obj->c.type = CONS;
  obj->c.car = car;
  obj->c.cdr = cdr;
  return obj;
}

obj_t *list(obj_t *first, ...)
{
  va_list vl;
  obj_t *list = nil;
  obj_t *array[32], **ptr = array;

  if (first != nao) {
    obj_t *next = first;

    va_start (vl, first);

    do {
      *ptr++ = next;
      if (ptr == array + 32)
        internal_error("runaway arguments in list function");
      next = va_arg(vl, obj_t *);
    } while (next != nao);

    while (ptr > array)
      list = cons(*--ptr, list);
  }

  return list;
}

obj_t *consp(obj_t *obj)
{
  if (!obj) {
    return nil;
  } else {
    type_t ty = type(obj);
    return (ty == CONS || ty == LCONS) ? t : nil;
  }
}

obj_t *nullp(obj_t *obj)
{
  return obj == 0 ? t : nil;
}

obj_t *atom(obj_t *obj)
{
  return if3(consp(obj), nil, t);
}

obj_t *listp(obj_t *obj)
{
  return if2(obj == nil || consp(obj), t);
}

obj_t *proper_listp(obj_t *obj)
{
  while (consp(obj))
    obj = cdr(obj);

  return (obj == nil) ? t : nil;
}

obj_t *length(obj_t *list)
{
  long len = 0;
  while (consp(list)) {
    len++;
    list = cdr(list);
  }
  return num(len);
}

obj_t *num(long val)
{
  numeric_assert (val >= NUM_MIN && val <= NUM_MAX);
  return (obj_t *) ((val << TAG_SHIFT) | TAG_NUM);
}

long c_num(obj_t *num)
{
  if (!is_num(num))
    type_mismatch(lit("~s is not a number"), num, nao);
  return ((long) num) >> TAG_SHIFT;
}

obj_t *nump(obj_t *num)
{
  return (is_num(num)) ? t : nil;
}

obj_t *plus(obj_t *anum, obj_t *bnum)
{
  long a = c_num(anum);
  long b = c_num(bnum);

  numeric_assert (a <= 0 || b <= 0 || NUM_MAX - b >= a);
  numeric_assert (a >= 0 || b >= 0 || NUM_MIN - b >= a);

  return num(a + b);
}

obj_t *minus(obj_t *anum, obj_t *bnum)
{
  long a = c_num(anum);
  long b = c_num(bnum);

  numeric_assert (b != NUM_MIN || NUM_MIN == -NUM_MAX);
  numeric_assert (a <= 0 || -b <= 0 || NUM_MAX + b >= a);
  numeric_assert (a >= 0 || -b >= 0 || NUM_MIN + b >= a);

  return num(a - b);
}

obj_t *neg(obj_t *anum)
{
  long n = c_num(anum);
  return num(-n);
}

obj_t *zerop(obj_t *num)
{
  return c_num(num) == 0 ? t : nil;
}

obj_t *gt(obj_t *anum, obj_t *bnum)
{
  return c_num(anum) > c_num(bnum) ? t : nil;
}

obj_t *lt(obj_t *anum, obj_t *bnum)
{
  return c_num(anum) < c_num(bnum) ? t : nil;
}

obj_t *ge(obj_t *anum, obj_t *bnum)
{
  return c_num(anum) >= c_num(bnum) ? t : nil;
}

obj_t *le(obj_t *anum, obj_t *bnum)
{
  return c_num(anum) <= c_num(bnum) ? t : nil;
}

obj_t *numeq(obj_t *anum, obj_t *bnum)
{
  return c_num(anum) == c_num(bnum) ? t : nil;
}

obj_t *max2(obj_t *anum, obj_t *bnum)
{
  return c_num(anum) > c_num(bnum) ? anum : bnum;
}

obj_t *min2(obj_t *anum, obj_t *bnum)
{
  return c_num(anum) < c_num(bnum) ? anum : bnum;
}

obj_t *string_own(wchar_t *str)
{
  obj_t *obj = make_obj();
  obj->st.type = STR;
  obj->st.str = str;
  obj->st.len = nil;
  return obj;
}

obj_t *string(const wchar_t *str)
{
  obj_t *obj = make_obj();
  obj->st.type = STR;
  obj->st.str = (wchar_t *) chk_strdup(str);
  obj->st.len = nil;
  return obj;
}

obj_t *string_utf8(const char *str)
{
  obj_t *obj = make_obj();
  obj->st.type = STR;
  obj->st.str = utf8_dup_from(str);
  obj->st.len = nil;
  return obj;
}

obj_t *mkstring(obj_t *len, obj_t *ch)
{
  size_t nchar = c_num(len) + 1;
  wchar_t *str = (wchar_t *) chk_malloc(nchar * sizeof *str);
  obj_t *s = string_own(str);
  wmemset(str, c_chr(ch), nchar);
  s->st.len = len;
  return s;
}

obj_t *mkustring(obj_t *len)
{
  wchar_t *str = (wchar_t *) chk_malloc((c_num(len) + 1) * sizeof *str);
  obj_t *s = string_own(str);
  s->st.len = len;
  return s;
}

obj_t *init_str(obj_t *str, const wchar_t *data)
{
  wmemcpy(str->st.str, data, c_num(str->st.len) + 1);
  return str;
}

obj_t *copy_str(obj_t *str)
{
  return string(c_str(str));
}

obj_t *stringp(obj_t *str)
{
  switch (tag(str)) {
  case TAG_LIT:
    return t;
  case TAG_PTR:
    if (str == nil)
      return nil;
    switch (type(str)) {
    case STR: case LSTR:
      return t;
    default:
      break;
    }
  }
  return nil;
}

obj_t *lazy_stringp(obj_t *str)
{
  return (is_ptr(str) && (type(str) == LSTR)) ? t : nil;
}

obj_t *length_str(obj_t *str)
{
  if (tag(str) == TAG_LIT) {
    return num(wcslen(c_str(str)));
  } else {
    type_check2 (str, STR, LSTR);

    if (str->ls.type == LSTR) {
      lazy_str_force(str);
      return length_str(str->ls.prefix);
    }

    if (!str->st.len)
      str->st.len = num(wcslen(str->st.str));
    return str->st.len;
  }
}

const wchar_t *c_str(obj_t *obj)
{
  if (tag(obj) == TAG_LIT)
    return litptr(obj);

  type_check3(obj, STR, SYM, LSTR);

  switch (obj->t.type) {
  case STR:
    return obj->st.str;
  case SYM:
    return c_str(symbol_name(obj));
  case LSTR:
    lazy_str_force(obj);
    return c_str(obj->ls.prefix);
  default:
    abort();
  }
}

obj_t *search_str(obj_t *haystack, obj_t *needle, obj_t *start_num,
                  obj_t *from_end)
{
  if (length_str_lt(haystack, start_num)) {
    return nil;
  } else {
    obj_t *h_is_lazy = lazy_stringp(haystack);
    long start = c_num(start_num);
    long good = -1, pos = -1;
    const wchar_t *n = c_str(needle), *h;

    if (!h_is_lazy) {
      do {
        const wchar_t *f;
        h = c_str(haystack);

        f = wcsstr(h + start, n);

        if (f)
          pos = f - h;
        else
          pos = -1;
      } while (pos != -1 && (good = pos) != -1 && from_end && h[++start]);
    } else {
      size_t ln = c_num(length_str(needle));

      do {
        lazy_str_force_upto(haystack, plus(num(start + 1), length_str(needle)));
        h = c_str(haystack->ls.prefix);

        if (!wcsncmp(h + start, n, ln))
          good = start;
      } while (h[++start] && (from_end || good == -1));
    }
    return (good == -1) ? nil : num(good);
  }
}

obj_t *search_str_tree(obj_t *haystack, obj_t *tree, obj_t *start_num,
                       obj_t *from_end)
{
  if (stringp(tree)) {
    obj_t *result = search_str(haystack, tree, start_num, from_end);
    if (result)
      return cons(result, length_str(tree));
  } else if (consp(tree)) {
    while (tree) {
      obj_t *result = search_str_tree(haystack, car(tree), start_num, from_end);
      if (result)
        return result;
      tree = cdr(tree);
    }
  }

  return nil;
}

obj_t *sub_str(obj_t *str_in, obj_t *from, obj_t *to)
{
  if (from == nil || lt(from, zero))
    from = zero;
  if (to == nil)
    to = length_str(str_in);
  else if (lt(to, zero))
    to = zero;
  if (length_str_lt(str_in, from))
    from = length_str(str_in);
  if (length_str_lt(str_in, to))
    to = length_str(str_in);

  if (ge(from, to)) {
    return null_string;
  } else {
    size_t nchar = c_num(to) - c_num(from) + 1;
    wchar_t *sub = (wchar_t *) chk_malloc(nchar * sizeof (wchar_t));
    const wchar_t *str = c_str(lazy_stringp(str_in)
                               ? str_in->ls.prefix : str_in);
    wcsncpy(sub, str + c_num(from), nchar);
    sub[nchar-1] = 0;
    return string_own(sub);
  }
}

obj_t *cat_str(obj_t *list, obj_t *sep)
{
  long total = 0;
  obj_t *iter;
  wchar_t *str, *ptr;
  long len_sep = sep ? c_num(length_str(sep)) : 0;

  for (iter = list; iter != nil; iter = cdr(iter)) {
    obj_t *item = car(iter);
    if (!item)
      continue;
    if (stringp(item)) {
      total += c_num(length_str(item));
      if (len_sep && cdr(iter))
        total += len_sep;
      continue;
    }
    if (chrp(item)) {
      total += 1;
      if (len_sep && cdr(iter))
        total += len_sep;
      continue;
    }
    return nil;
  }

  str = (wchar_t *) chk_malloc((total + 1) * sizeof *str);

  for (ptr = str, iter = list; iter != nil; iter = cdr(iter)) {
    obj_t *item = car(iter);
    long len;
    if (!item)
      continue;
    if (stringp(item)) {
      len = c_num(length_str(item));
      wmemcpy(ptr, c_str(item), len);
      ptr += len;
    } else {
      *ptr++ = c_chr(item);
    }

    if (len_sep && cdr(iter)) {
      wmemcpy(ptr, c_str(sep), len_sep);
      ptr += len_sep;
    }
  }
  *ptr = 0;

  return string_own(str);
}

obj_t *split_str(obj_t *str, obj_t *sep)
{
  const wchar_t *cstr = c_str(str);
  const wchar_t *csep = c_str(sep);
  list_collect_decl (out, iter);

  for (;;) {
    size_t span = wcscspn(cstr, csep);
    obj_t *piece = mkustring(num(span));
    init_str(piece, cstr);
    list_collect (iter, piece);
    cstr += span;
    if (!*cstr)
      break;
    cstr++;
  }

  return out;
}

obj_t *trim_str(obj_t *str)
{
  const wchar_t *start = c_str(str);
  const wchar_t *end = start + c_num(length_str(str));

  while (start[0] && iswspace(start[0]))
    start++;

  while (end > start && iswspace(end[-1]))
    end--;

  if (end == start) {
    return null_string;
  } else {
    size_t len = end - start;
    wchar_t *new = (wchar_t *) chk_malloc((len + 1) * sizeof *new);
    wmemcpy(new, start, len);
    new[len] = 0;
    return string_own(new);
  }
}

obj_t *string_lt(obj_t *astr, obj_t *bstr)
{
  int cmp = wcscmp(c_str(astr), c_str(bstr));
  return cmp == -1 ? t : nil;
}

obj_t *chr(wchar_t ch)
{
  return (obj_t *) ((ch << TAG_SHIFT) | TAG_CHR);
}

obj_t *chrp(obj_t *chr)
{
  return (is_chr(chr)) ? t : nil;
}

wchar_t c_chr(obj_t *chr)
{
  if (!is_chr(chr))
    type_mismatch(lit("~s is not a character"), chr, nao);
  return ((wchar_t) chr) >> TAG_SHIFT;
}

obj_t *chr_str(obj_t *str, obj_t *index)
{
  bug_unless (length_str_gt(str, index));

  if (lazy_stringp(str)) {
    lazy_str_force_upto(str, index);
    return chr(c_str(str->ls.prefix)[c_num(index)]);
  } else {
    return chr(c_str(str)[c_num(index)]);
  }
}

obj_t *chr_str_set(obj_t *str, obj_t *index, obj_t *chr)
{
  bug_unless (length_str_gt(str, index));

  if (lazy_stringp(str)) {
    lazy_str_force_upto(str, index);
    str->ls.prefix->st.str[c_num(index)] = c_chr(chr);
  } else {
    str->st.str[c_num(index)] = c_chr(chr);
  }

  return chr;
}

obj_t *symbol_name(obj_t *sym)
{
  if (sym)
    type_check(sym, SYM);
  return sym ? sym->s.name : nil_string;
}

obj_t *make_sym(obj_t *name)
{
  obj_t *obj = make_obj();
  obj->s.type = SYM;
  obj->s.name = name;
  obj->s.val = nil;
  return obj;
}

obj_t *intern(obj_t *str)
{
  obj_t *iter;

  for (iter = interned_syms; iter != nil; iter = cdr(iter)) {
    obj_t *sym = car(iter);
    if (equal(symbol_name(sym), str))
      return sym;
  }

  interned_syms = cons(make_sym(str), interned_syms);
  return car(interned_syms);
}

obj_t *symbolp(obj_t *sym)
{
  return (sym == nil || (is_ptr(sym) && sym->s.type == SYM)) ? t : nil;
}

obj_t *func_f0(obj_t *env, obj_t *(*fun)(obj_t *))
{
  obj_t *obj = make_obj();
  obj->f.type = FUN;
  obj->f.functype = F0;
  obj->f.env = env;
  obj->f.f.f0 = fun;
  return obj;
}

obj_t *func_f1(obj_t *env, obj_t *(*fun)(obj_t *, obj_t *))
{
  obj_t *obj = make_obj();
  obj->f.type = FUN;
  obj->f.functype = F1;
  obj->f.env = env;
  obj->f.f.f1 = fun;
  return obj;
}

obj_t *func_f2(obj_t *env, obj_t *(*fun)(obj_t *, obj_t *, obj_t *))
{
  obj_t *obj = make_obj();
  obj->f.type = FUN;
  obj->f.functype = F2;
  obj->f.env = env;
  obj->f.f.f2 = fun;
  return obj;
}

obj_t *func_f3(obj_t *env, obj_t *(*fun)(obj_t *, obj_t *, obj_t *, obj_t *))
{
  obj_t *obj = make_obj();
  obj->f.type = FUN;
  obj->f.functype = F3;
  obj->f.env = env;
  obj->f.f.f3 = fun;
  return obj;
}

obj_t *func_f4(obj_t *env, obj_t *(*fun)(obj_t *, obj_t *, obj_t *, obj_t *,
                                         obj_t *))
{
  obj_t *obj = make_obj();
  obj->f.type = FUN;
  obj->f.functype = F4;
  obj->f.env = env;
  obj->f.f.f4 = fun;
  return obj;
}

obj_t *func_n0(obj_t *(*fun)(void))
{
  obj_t *obj = make_obj();
  obj->f.type = FUN;
  obj->f.functype = N0;
  obj->f.env = nil;
  obj->f.f.n0 = fun;
  return obj;
}

obj_t *func_n1(obj_t *(*fun)(obj_t *))
{
  obj_t *obj = make_obj();
  obj->f.type = FUN;
  obj->f.functype = N1;
  obj->f.env = nil;
  obj->f.f.n1 = fun;
  return obj;
}

obj_t *func_n2(obj_t *(*fun)(obj_t *, obj_t *))
{
  obj_t *obj = make_obj();
  obj->f.type = FUN;
  obj->f.functype = N2;
  obj->f.env = nil;
  obj->f.f.n2 = fun;
  return obj;
}

obj_t *func_n3(obj_t *(*fun)(obj_t *, obj_t *, obj_t *))
{
  obj_t *obj = make_obj();
  obj->f.type = FUN;
  obj->f.functype = N3;
  obj->f.f.n3 = fun;
  return obj;
}

obj_t *func_n4(obj_t *(*fun)(obj_t *, obj_t *, obj_t *, obj_t *))
{
  obj_t *obj = make_obj();
  obj->f.type = FUN;
  obj->f.functype = N4;
  obj->f.f.n4 = fun;
  return obj;
}


obj_t *apply(obj_t *fun, obj_t *arglist)
{
  obj_t *arg[4], **p = arg;

  type_check (fun, FUN);

  type_assert (listp(arglist),
               (lit("apply arglist ~s is not a list"), arglist, nao));

  *p++ = car(arglist); arglist = cdr(arglist);
  *p++ = car(arglist); arglist = cdr(arglist);
  *p++ = car(arglist); arglist = cdr(arglist);
  *p++ = car(arglist); arglist = cdr(arglist);

  switch (fun->f.functype) {
  case F0:
    return fun->f.f.f0(fun);
  case F1:
    return fun->f.f.f1(fun, arg[0]);
  case F2:
    return fun->f.f.f2(fun, arg[0], arg[1]);
  case F3:
    return fun->f.f.f3(fun, arg[0], arg[1], arg[2]);
  case F4:
    return fun->f.f.f4(fun, arg[0], arg[1], arg[2], arg[3]);
  case N0:
    return fun->f.f.n0();
  case N1:
    return fun->f.f.n1(arg[0]);
  case N2:
    return fun->f.f.n2(arg[0], arg[1]);
  case N3:
    return fun->f.f.n3(arg[0], arg[1], arg[2]);
  case N4:
    return fun->f.f.n4(arg[0], arg[1], arg[2], arg[3]);
  case FINTERP:
    internal_error("unsupported function type");
  }

  internal_error("corrupt function type field");
}

obj_t *funcall(obj_t *fun)
{
  type_check(fun, FUN);

  switch (fun->f.functype) {
  case F0:
    return fun->f.f.f0(fun->f.env);
  case N0:
    return fun->f.f.n0();
  default:
    abort();
  }
}

obj_t *funcall1(obj_t *fun, obj_t *arg)
{
  type_check(fun, FUN);

  switch (fun->f.functype) {
  case F1:
    return fun->f.f.f1(fun->f.env, arg);
  case N1:
    return fun->f.f.n1(arg);
  default:
    abort();
  }
}

obj_t *funcall2(obj_t *fun, obj_t *arg1, obj_t *arg2)
{
  type_check(fun, FUN);

  switch (fun->f.functype) {
  case F2:
    return fun->f.f.f2(fun->f.env, arg1, arg2);
  case N2:
    return fun->f.f.n2(arg1, arg2);
  default:
    abort();
  }
}

obj_t *reduce_left(obj_t *fun, obj_t *list, obj_t *init, obj_t *key)
{
  if (!key)
    key = identity_f;

  for (; list; list = cdr(list))
    init = funcall2(fun, init, funcall1(key, car(list)));

  return init;
}

obj_t *do_bind2(obj_t *fcons, obj_t *arg2)
{
  return funcall2(car(fcons), cdr(fcons), arg2);
}

obj_t *bind2(obj_t *fun2, obj_t *arg)
{
  return func_f1(cons(fun2, arg), do_bind2);
}

obj_t *do_bind2other(obj_t *fcons, obj_t *arg1)
{
  return funcall2(car(fcons), arg1, cdr(fcons));
}

obj_t *bind2other(obj_t *fun2, obj_t *arg2)
{
  return func_f1(cons(fun2, arg2), do_bind2other);
}


static obj_t *do_chain(obj_t *fun1_list, obj_t *arg)
{
  for (; fun1_list; fun1_list = cdr(fun1_list))
    arg = funcall1(car(fun1_list), arg);

  return arg;
}

obj_t *chain(obj_t *fun1_list)
{
  return func_f1(fun1_list, do_chain);
}

obj_t *vector(obj_t *alloc)
{
  long alloc_plus = c_num(alloc) + 2;
  obj_t *vec = make_obj();
  obj_t **v = (obj_t **) chk_malloc(alloc_plus * sizeof *v);
  vec->v.type = VEC;
  vec->v.vec = v + 2;
  v[0] = alloc;
  v[1] = zero;
  return vec;
}

obj_t *vec_get_fill(obj_t *vec)
{
  type_check(vec, VEC);
  return vec->v.vec[vec_fill];
}

obj_t *vec_set_fill(obj_t *vec, obj_t *fill)
{
  type_check(vec, VEC);

  {
    long new_fill = c_num(fill);
    long old_fill = c_num(vec->v.vec[vec_fill]);
    long old_alloc = c_num(vec->v.vec[vec_alloc]);
    long fill_delta = new_fill - old_fill;
    long alloc_delta = new_fill - old_alloc;

    if (alloc_delta > 0) {
      long new_alloc = max(new_fill, 2*old_alloc);
      obj_t **newvec = (obj_t **) chk_realloc(vec->v.vec - 2,
                                              (new_alloc + 2)*sizeof *newvec);
      vec->v.vec = newvec + 2;
      vec->v.vec[vec_alloc] = num(new_alloc);
    }

    if (fill_delta > 0) {
      long i;
      for (i = old_fill; i < new_fill; i++)
        vec->v.vec[i] = nil;
    }

    vec->v.vec[vec_fill] = fill;
  }

  return vec;
}


obj_t **vecref_l(obj_t *vec, obj_t *ind)
{
  type_check(vec, VEC);
  range_bug_unless (c_num(ind) < c_num(vec->v.vec[vec_fill]));
  return vec->v.vec + c_num(ind);
}

obj_t *vec_push(obj_t *vec, obj_t *item)
{
  obj_t *fill = vec_get_fill(vec);
  vec_set_fill(vec, plus(fill, one));
  *vecref_l(vec, fill) = item;
  return fill;
}

static obj_t *make_lazycons(obj_t *func)
{
  obj_t *obj = make_obj();
  obj->lc.type = LCONS;
  obj->lc.car = obj->lc.cdr = nil;
  obj->lc.func = func;
  return obj;
}

static obj_t *lazy_stream_func(obj_t *env, obj_t *lcons)
{
  obj_t *stream = car(env);
  obj_t *next = cdr(env) ? pop(cdr_l(env)) : get_line(stream);
  obj_t *ahead = get_line(stream);

  lcons->lc.car = next;
  lcons->lc.cdr = if2(ahead, make_lazycons(lcons->lc.func));
  lcons->lc.func = nil;

  if (!next || !ahead)
    close_stream(stream, t);

  if (ahead)
    push(ahead, cdr_l(env));

  return next;
}

obj_t *lazy_stream_cons(obj_t *stream)
{
  obj_t *first = get_line(stream);

  if (!first) {
    close_stream(stream, t);
    return nil;
  }

  return make_lazycons(func_f1(cons(stream, cons(first, nil)),
                       lazy_stream_func));
}

obj_t *lazy_str(obj_t *lst, obj_t *term, obj_t *limit)
{
  obj_t *obj = make_obj();
  obj->ls.type = LSTR;
  obj->ls.opts = nil; /* Must init before calling something that can gc! */

  term = or2(term, string(L"\n"));

  if (nullp(lst)) {
    obj->ls.prefix = null_string;
    obj->ls.list = nil;
  } else {
    obj->ls.prefix = cat_str(list(first(lst), term, nao), nil);
    obj->ls.list = rest(lst);
    limit = if2(limit, minus(limit, one));
  }

  obj->ls.opts = cons(term, limit);

  return obj;
}

obj_t *lazy_str_force(obj_t *lstr)
{
  obj_t *lim;
  type_check(lstr, LSTR);
  lim = cdr(lstr->ls.opts);

  while ((!lim || gt(lim, zero)) && lstr->ls.list) {
    obj_t *next = pop(&lstr->ls.list);
    obj_t *term = car(lstr->ls.opts);
    lstr->ls.prefix = cat_str(list(lstr->ls.prefix, next, term, nao), nil);
    if (lim)
      lim = minus(lim, one);
  }

  if (lim)
    *cdr_l(lstr->ls.opts) = lim;

  return lstr->ls.prefix;
}

obj_t *lazy_str_force_upto(obj_t *lstr, obj_t *index)
{
  obj_t *lim;
  type_check(lstr, LSTR);
  lim = cdr(lstr->ls.opts);

  while (ge(index, length_str(lstr->ls.prefix)) && lstr->ls.list &&
         or2(nullp(lim),gt(lim,zero)))
  {
    obj_t *next = pop(&lstr->ls.list);
    obj_t *term = car(lstr->ls.opts);
    lstr->ls.prefix = cat_str(list(lstr->ls.prefix, next, term, nao), nil);
    if (lim)
      lim = minus(lim, one);
  }

  if (lim)
    *cdr_l(lstr->ls.opts) = lim;
  return lt(index, length_str(lstr->ls.prefix));
}

obj_t *length_str_gt(obj_t *str, obj_t *len)
{
  type_check2 (str, STR, LSTR);

  switch (str->t.type) {
  case STR:
    return gt(length_str(str), len);
  case LSTR:
    lazy_str_force_upto(str, len);
    return gt(length_str(str->ls.prefix), len);
  default:
    internal_error("unexpected type value");
  }
}

obj_t *length_str_ge(obj_t *str, obj_t *len)
{
  type_check2 (str, STR, LSTR);

  switch (str->t.type) {
  case STR:
    return ge(length_str(str), len);
  case LSTR:
    lazy_str_force_upto(str, len);
    return ge(length_str(str->ls.prefix), len);
  default:
    internal_error("unexpected type value");
  }
}

obj_t *length_str_lt(obj_t *str, obj_t *len)
{
  type_check2 (str, STR, LSTR);

  switch (str->t.type) {
  case STR:
    return lt(length_str(str), len);
  case LSTR:
    lazy_str_force_upto(str, len);
    return lt(length_str(str->ls.prefix), len);
  default:
    internal_error("unexpected type value");
  }
}

obj_t *length_str_le(obj_t *str, obj_t *len)
{
  type_check2 (str, STR, LSTR);

  switch (str->t.type) {
  case STR:
    return le(length_str(str), len);
  case LSTR:
    lazy_str_force_upto(str, len);
    return le(length_str(str->ls.prefix), len);
  default:
    internal_error("unexpected type value");
  }
}

obj_t *lazy_str_get_trailing_list(obj_t *lstr, obj_t *index)
{
  type_check(lstr, LSTR);

  /* Force lazy string up through the index position */
  if (ge(index, length_str(lstr->ls.prefix)))
    lazy_str_force_upto(lstr, index);

  {
    obj_t *split_suffix = split_str(sub_str(lstr->ls.prefix, index, nil),
                                    or2(car(lstr->ls.opts), string(L"\n")));

    return nappend2(split_suffix, lstr->ls.list);
  }
}

obj_t *cobj(void *handle, obj_t *cls_sym, struct cobj_ops *ops)
{
  obj_t *obj = make_obj();
  obj->co.type = COBJ;
  obj->co.handle = handle;
  obj->co.ops = ops;
  obj->co.cls = cls_sym;
  return obj;
}

void cobj_print_op(obj_t *obj, obj_t *out)
{
  put_string(out, lit("#<"));
  obj_print(obj->co.cls, out);
  format(out, lit(": ~p>"), obj->co.handle, nao);
}

obj_t *assoc(obj_t *list, obj_t *key)
{
  while (list) {
    obj_t *elem = car(list);
    if (equal(car(elem), key))
      return elem;
    list = cdr(list);
  }

  return nil;
}

obj_t *acons_new(obj_t *list, obj_t *key, obj_t *value)
{
  obj_t *existing = assoc(list, key);

  if (existing) {
    *cdr_l(existing) = value;
    return list;
  } else {
    return cons(cons(key, value), list);
  }
}

obj_t **acons_new_l(obj_t **list, obj_t *key)
{
  obj_t *existing = assoc(*list, key);

  if (existing) {
    return cdr_l(existing);
  } else {
    obj_t *new = cons(key, nil);
    *list = cons(new, *list);
    return cdr_l(new);
  }
}

obj_t *alist_remove(obj_t *list, obj_t *keys)
{
  obj_t **plist = &list;

  while (*plist) {
    if (memq(car(car(*plist)), keys))
      *plist = cdr(*plist);
    else
      plist = cdr_l(*plist);
  }

  return list;
}

obj_t *alist_remove1(obj_t *list, obj_t *key)
{
  obj_t **plist = &list;

  while (*plist) {
    if (eq(car(car(*plist)), key))
      *plist = cdr(*plist);
    else
      plist = cdr_l(*plist);
  }

  return list;
}

obj_t *copy_cons(obj_t *c)
{
  return cons(car(c), cdr(c));
}

obj_t *copy_alist(obj_t *list)
{
  return mapcar(func_n1(copy_cons), list);
}

obj_t *mapcar(obj_t *fun, obj_t *list)
{
  list_collect_decl (out, iter);

  for (; list; list = cdr(list))
    list_collect (iter, funcall1(fun, car(list)));

  return out;
}

obj_t *mappend(obj_t *fun, obj_t *list)
{
  list_collect_decl (out, iter);

  for (; list; list = cdr(list))
    list_collect_append (iter, funcall1(fun, car(list)));

  return out;
}

obj_t *merge(obj_t *list1, obj_t *list2, obj_t *lessfun, obj_t *keyfun)
{
  list_collect_decl (out, ptail);

  while (list1 && list2) {
    obj_t *el1 = funcall1(keyfun, first(list1));
    obj_t *el2 = funcall1(keyfun, first(list2));

    if (funcall2(lessfun, el1, el2)) {
      obj_t *next = cdr(list1);
      *cdr_l(list1) = nil;
      list_collect_append(ptail, list1);
      list1 = next;
    } else {
      obj_t *next = cdr(list2);
      *cdr_l(list2) = nil;
      list_collect_append(ptail, list2);
      list2 = next;
    }
  }

  if (list1)
    list_collect_append(ptail, list1);
  else
    list_collect_append(ptail, list2);

  return out;
}

static obj_t *do_sort(obj_t *list, obj_t *lessfun, obj_t *keyfun)
{
  if (list == nil)
    return nil;
  if (!cdr(list))
    return list;
  if (!cdr(cdr(list))) {
    if (funcall2(lessfun, funcall1(keyfun, first(list)),
                          funcall1(keyfun, second(list))))
    {
      return list;
    } else {
      obj_t *cons2 = cdr(list);
      *cdr_l(cons2) = list;
      *cdr_l(list) = nil;
      return cons2;
    }
  }

  {
    obj_t *bisect, *iter;
    obj_t *list2;

    for (iter = cdr(cdr(list)), bisect = list; iter;
         bisect = cdr(bisect), iter = cdr(cdr(iter)))
      ; /* empty */

    list2 = cdr(bisect);
    *cdr_l(bisect) = nil;

    return merge(sort(list, lessfun, keyfun),
                 sort(list2, lessfun, keyfun),
                 lessfun, keyfun);
  }
}

obj_t *sort(obj_t *list, obj_t *lessfun, obj_t *keyfun)
{
  if (!keyfun)
    keyfun = identity_f;

  return do_sort(list, lessfun, keyfun);
}

static void obj_init(void)
{
  /*
   * No need to GC-protect the convenience variables which hold the interned
   * symbols, because the interned_syms list holds a reference to all the
   * symbols.
   */

  protect(&interned_syms, &zero, &one,
          &two, &negone, &maxint, &minint,
          &null_string, &nil_string,
          &null_list, &equal_f,
          &identity_f, &prog_string,
          (obj_t **) 0);

  nil_string = lit("nil");

  null = intern(lit("null"));
  t = intern(lit("t"));
  cons_t = intern(lit("cons"));
  str_t = intern(lit("str"));
  chr_t = intern(lit("chr"));
  num_t = intern(lit("num"));
  sym_t = intern(lit("sym"));
  fun_t = intern(lit("fun"));
  vec_t = intern(lit("vec"));
  stream_t = intern(lit("stream"));
  hash_t = intern(lit("hash"));
  lcons_t = intern(lit("lcons"));
  lstr_t = intern(lit("lstr"));
  cobj_t = intern(lit("cobj"));
  var = intern(lit("$var"));
  regex = intern(lit("$regex"));
  set = intern(lit("set"));
  cset = intern(lit("cset"));
  wild = intern(lit("wild"));
  oneplus = intern(lit("1+"));
  zeroplus = intern(lit("0+"));
  optional = intern(lit("?"));
  compound = intern(lit("compound"));
  or = intern(lit("or"));
  quasi = intern(lit("$quasi"));
  skip = intern(lit("skip"));
  trailer = intern(lit("trailer"));
  block = intern(lit("block"));
  next = intern(lit("next"));
  freeform = intern(lit("freeform"));
  fail = intern(lit("fail"));
  accept = intern(lit("accept"));
  all = intern(lit("all"));
  some = intern(lit("some"));
  none = intern(lit("none"));
  maybe = intern(lit("maybe"));
  cases = intern(lit("cases"));
  collect = intern(lit("collect"));
  until = intern(lit("until"));
  coll = intern(lit("coll"));
  define = intern(lit("define"));
  output = intern(lit("output"));
  single = intern(lit("single"));
  frst = intern(lit("first"));
  lst = intern(lit("last"));
  empty = intern(lit("empty"));
  repeat = intern(lit("repeat"));
  rep = intern(lit("rep"));
  flattn = intern(lit("flatten"));
  forget = intern(lit("forget"));
  local = intern(lit("local"));
  mrge = intern(lit("merge"));
  bind = intern(lit("bind"));
  cat = intern(lit("cat"));
  args = intern(lit("args"));
  try = intern(lit("try"));
  catch = intern(lit("catch"));
  finally = intern(lit("finally"));
  nothrow = intern(lit("nothrow"));
  throw = intern(lit("throw"));
  defex = intern(lit("defex"));
  error = intern(lit("error"));
  type_error = intern(lit("type_error"));
  internal_err = intern(lit("internal_error"));
  numeric_err = intern(lit("numeric_error"));
  range_err = intern(lit("range_error"));
  query_error = intern(lit("query_error"));
  file_error = intern(lit("file_error"));
  process_error = intern(lit("process_error"));

  interned_syms = cons(nil, interned_syms);

  zero = num(0);
  one  = num(1);
  two = num(2);
  negone = num(-1);
  maxint = num(NUM_MAX);
  minint = num(NUM_MIN);

  null_string = lit("");

  null_list = cons(nil, nil);

  equal_f = func_f2(nil, equal_tramp);
  identity_f = func_f1(nil, identity_tramp);
  prog_string = string(progname);
}

void obj_print(obj_t *obj, obj_t *out)
{
  if (obj == nil) {
    put_string(out, lit("nil"));
    return;
  }

  switch (type(obj)) {
  case CONS:
  case LCONS:
    {
      obj_t *iter;
      put_char(out, chr('('));
      for (iter = obj; consp(iter); iter = cdr(iter)) {
        obj_print(car(iter), out);
        if (nullp(cdr(iter))) {
          put_char(out, chr(')'));
        } else if (consp(cdr(iter))) {
          put_char(out, chr(' '));
        } else {
          put_string(out, lit(" . "));
          obj_print(cdr(iter), out);
          put_char(out, chr(')'));
        }
      }
    }
    return;
  case LIT:
  case STR:
    {
      const wchar_t *ptr;
      put_char(out, chr('"'));
      for (ptr = c_str(obj); *ptr; ptr++) {
        switch (*ptr) {
        case '\a': put_string(out, lit("\\a")); break;
        case '\b': put_string(out, lit("\\b")); break;
        case '\t': put_string(out, lit("\\t")); break;
        case '\n': put_string(out, lit("\\n")); break;
        case '\v': put_string(out, lit("\\v")); break;
        case '\f': put_string(out, lit("\\f")); break;
        case '\r': put_string(out, lit("\\r")); break;
        case '"': put_string(out, lit("\\\"")); break;
        case '\\': put_string(out, lit("\\\\")); break;
        case 27: put_string(out, lit("\\e")); break;
        default:
          if (iswprint(*ptr))
            put_char(out, chr(*ptr));
          else
            format(out, lit("\\~03o"), num(*ptr), nao);
        }
      }
      put_char(out, chr('"'));
    }
    return;
  case CHR:
    {
      wchar_t ch = c_chr(obj);

      put_char(out, chr('\''));
      switch (ch) {
      case '\a': put_string(out, lit("\\a")); break;
      case '\b': put_string(out, lit("\\b")); break;
      case '\t': put_string(out, lit("\\t")); break;
      case '\n': put_string(out, lit("\\n")); break;
      case '\v': put_string(out, lit("\\v")); break;
      case '\f': put_string(out, lit("\\f")); break;
      case '\r': put_string(out, lit("\\r")); break;
      case '"': put_string(out, lit("\\\"")); break;
      case '\\': put_string(out, lit("\\\\")); break;
      case 27: put_string(out, lit("\\e")); break;
      default:
        if (iswprint(ch))
          put_char(out, chr(ch));
        else
          format(out, lit("\\~03o"), num(ch), nao);
      }
      put_char(out, chr('\''));
    }
    return;
  case NUM:
    format(out, lit("~s"), obj, nao);
    return;
  case SYM:
    put_string(out, symbol_name(obj));
    return;
  case FUN:
    format(out, lit("#<function: f~a>"), num(obj->f.functype), nao);
    return;
  case VEC:
    {
      long i, fill = c_num(obj->v.vec[vec_fill]);
      put_string(out, lit("#("));
      for (i = 0; i < fill; i++) {
        obj_print(obj->v.vec[i], out);
        if (i < fill - 1)
          put_char(out, chr(' '));
      }
      put_char(out, chr(')'));
    }
    return;
  case LSTR:
    obj_print(obj->ls.prefix, out);
    put_string(out, lit("#<... lazy string>"));
    return;
  case COBJ:
    obj->co.ops->print(obj, out);
    return;
  }

  format(out, lit("#<garbage: ~p>"), (void *) obj, nao);
}

void obj_pprint(obj_t *obj, obj_t *out)
{
  if (obj == nil) {
    put_string(out, lit("nil"));
    return;
  }

  switch (type(obj)) {
  case CONS:
  case LCONS:
    {
      obj_t *iter;
      put_char(out, chr('('));
      for (iter = obj; consp(iter); iter = cdr(iter)) {
        obj_pprint(car(iter), out);
        if (nullp(cdr(iter))) {
          put_char(out, chr(')'));
        } else if (consp(cdr(iter))) {
          put_char(out, chr(' '));
        } else {
          put_string(out, lit(" . "));
          obj_pprint(cdr(iter), out);
          put_char(out, chr(')'));
        }
      }
    }
    return;
  case LIT:
  case STR:
    put_string(out, obj);
    return;
  case CHR:
    put_char(out, obj);
    return;
  case NUM:
    format(out, lit("~s"), obj, nao);
    return;
  case SYM:
    put_string(out, symbol_name(obj));
    return;
  case FUN:
    format(out, lit("#<function: f~a>"), num(obj->f.functype), nao);
    return;
  case VEC:
    {
      long i, fill = c_num(obj->v.vec[vec_fill]);
      put_string(out, lit("#("));
      for (i = 0; i < fill; i++) {
        obj_pprint(obj->v.vec[i], out);
        if (i < fill - 1)
          put_char(out, chr(' '));
      }
      put_char(out, chr(')'));
    }
    return;
  case LSTR:
    obj_pprint(obj->ls.prefix, out);
    put_string(out, lit("..."));
    return;
  case COBJ:
    obj->co.ops->print(obj, out);
    return;
  }

  format(out, lit("#<garbage: ~p>"), (void *) obj, nao);
}

void init(const wchar_t *pn, void *(*oom)(void *, size_t),
          obj_t **stack_bottom)
{
  int gc_save;
  progname = pn;
  gc_save = gc_state(0);

  oom_realloc = oom;
  gc_init(stack_bottom);
  obj_init();
  uw_init();
  stream_init();

  gc_state(gc_save);
}

void dump(obj_t *obj, obj_t *out)
{
  obj_print(obj, out);
  put_char(out, chr('\n'));
}

/*
 * Handy function for debugging in gdb,
 * so we don't have to keep typing:
 * (gdb) p dump(something, stdout)
 */
void d(obj_t *obj)
{
  dump(obj, std_output);
}

obj_t *snarf(obj_t *in)
{
  list_collect_decl (list, iter);
  obj_t *str;

  while ((str = get_line(in)) != 0)
    list_collect (iter, str);

  return list;
}
