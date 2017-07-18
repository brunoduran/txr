/* Copyright 2009-2017
 * Kaz Kylheku <kaz@kylheku.com>
 * Vancouver, Canada
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <wctype.h>
#include <limits.h>
#include <stdarg.h>
#include <dirent.h>
#include <errno.h>
#include <wchar.h>
#include <math.h>
#include <time.h>
#include <signal.h>
#include <sys/time.h>
#include <assert.h>
#include "config.h"
#include ALLOCA_H
#ifdef HAVE_GETENVIRONMENTSTRINGS
#define NOMINMAX
#include <windows.h>
#endif
#include "lib.h"
#include "gc.h"
#include "arith.h"
#include "rand.h"
#include "hash.h"
#include "signal.h"
#include "unwind.h"
#include "args.h"
#include "stream.h"
#include "utf8.h"
#include "filter.h"
#include "eval.h"
#include "sysif.h"
#include "regex.h"
#include "parser.h"
#include "syslog.h"
#include "glob.h"
#include "ftw.h"
#include "termios.h"
#include "cadr.h"
#include "struct.h"
#include "itypes.h"
#include "buf.h"
#include "ffi.h"
#include "txr.h"

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

#if !HAVE_POSIX_SIGS
int async_sig_enabled = 0;
#endif

val packages;

val system_package_var, keyword_package_var, user_package_var;
val package_alist_s;
val package_s, system_package_s, keyword_package_s, user_package_s;

val null_s, t, cons_s, str_s, chr_s, fixnum_s, sym_s, pkg_s, fun_s, vec_s;
val lit_s, stream_s, hash_s, hash_iter_s, lcons_s, lstr_s, cobj_s, cptr_s;
val atom_s, integer_s, number_s, sequence_s, string_s;
val env_s, bignum_s, float_s, range_s, rcons_s, buf_s;
val var_s, expr_s, regex_s, chset_s, set_s, cset_s, wild_s, oneplus_s;
val nongreedy_s;
val quote_s, qquote_s, unquote_s, splice_s;
val sys_qquote_s, sys_unquote_s, sys_splice_s;
val zeroplus_s, optional_s, compl_s, compound_s;
val or_s, and_s, quasi_s, quasilist_s;
val skip_s, trailer_s, block_s, next_s, freeform_s, fail_s, accept_s;
val all_s, some_s, none_s, maybe_s, cases_s, collect_s, until_s, coll_s;
val define_s, output_s, single_s, first_s, last_s, empty_s;
val repeat_s, rep_s, flatten_s, forget_s;
val local_s, merge_s, bind_s, rebind_s, cat_s;
val try_s, catch_s, finally_s, throw_s, defex_s, deffilter_s;
val eof_s, eol_s, assert_s, name_s;
val error_s, type_error_s, internal_error_s, panic_s;
val numeric_error_s, range_error_s;
val query_error_s, file_error_s, process_error_s, syntax_error_s;
val timeout_error_s, system_error_s;
val warning_s, defr_warning_s, restart_s, continue_s;
val gensym_counter_s, nullify_s, from_list_s, lambda_set_s, length_s;

val nothrow_k, args_k, colon_k, auto_k, fun_k;
val wrap_k, reflect_k;

val null_string;
val nil_string;
val null_list;

val identity_f, equal_f, eql_f, eq_f, car_f, cdr_f, null_f;
val list_f, less_f, greater_f;

val prog_string;

val time_s, time_local_s, time_utc_s, time_string_s, time_parse_s;
val year_s, month_s, day_s, hour_s, min_s, sec_s, dst_s, gmtoff_s, zone_s;

static val env_list;
static val recycled_conses;

mem_t *(*oom_realloc)(mem_t *, size_t);

/* C99 inline instantiations. */
#if __STDC_VERSION__ >= 199901L
loc mkloc_fun(val *ptr, val obj);
cnum tag(val obj);
int is_ptr(val obj);
int is_num(val obj);
int is_chr(val obj);
int is_lit(val obj);
type_t type(val obj);
val auto_str(const wchli_t *str);
val static_str(const wchli_t *str);
wchar_t *litptr(val obj);
val num_fast(cnum n);
mp_int *mp(val bign);
val chr(wchar_t ch);
val eq(val a, val b);
val null(val v);
int null_or_missing_p(val v);
val default_null_arg(val arg);
#endif

const seq_kind_t seq_kind_tab[MAXTYPE+1] = {
  SEQ_NIL,      /* NIL */
  SEQ_NOTSEQ,   /* NUM */
  SEQ_NOTSEQ,   /* CHR */
  SEQ_VECLIKE,  /* LIT */
  SEQ_LISTLIKE, /* CONS */
  SEQ_VECLIKE,  /* STR */
  SEQ_NOTSEQ,   /* SYM */
  SEQ_NOTSEQ,   /* PKG */
  SEQ_NOTSEQ,   /* FUN */
  SEQ_VECLIKE,  /* VEC */
  SEQ_LISTLIKE, /* LCONS */
  SEQ_VECLIKE,  /* LSTR */
  SEQ_NOTSEQ,   /* COBJ */
  SEQ_NOTSEQ,   /* CPTR */
  SEQ_NOTSEQ,   /* ENV */
  SEQ_NOTSEQ,   /* BGNUM */
  SEQ_NOTSEQ,   /* FLNUM */
  SEQ_NOTSEQ,   /* RNG */
  SEQ_VECLIKE,  /* BUF */
};

val identity(val obj)
{
  return obj;
}

static val code2type(int code)
{
  switch (convert(type_t, code)) {
  case NIL: return null_s;
  case CONS: return cons_s;
  case STR: return str_s;
  case LIT: return lit_s;
  case CHR: return chr_s;
  case NUM: return fixnum_s;
  case SYM: return sym_s;
  case PKG: return pkg_s;
  case FUN: return fun_s;
  case VEC: return vec_s;
  case LCONS: return lcons_s;
  case LSTR: return lstr_s;
  case COBJ: return cobj_s;
  case CPTR: return cptr_s;
  case ENV: return env_s;
  case BGNUM: return bignum_s;
  case FLNUM: return float_s;
  case RNG: return range_s;
  case BUF: return buf_s;
  }
  return nil;
}

val typeof(val obj)
{
  switch (tag(obj)) {
  case TAG_NUM:
    return fixnum_s;
  case TAG_CHR:
    return chr_s;
  case TAG_LIT:
    return lit_s;
  case TAG_PTR:
    {
      int typecode = type(obj);

      if (typecode == COBJ) {
        return obj->co.cls;
      } else {
        val typesym = code2type(typecode);
        if (!typesym)
          internal_error("corrupt type field");
        return typesym;
      }
    }
  default:
    internal_error("invalid type tag");
  }
}

val subtypep(val sub, val sup)
{
  if (sub == nil || sup == t) {
    return t;
  } else if (sub == sup) {
    return t;
  } else if (sup == atom_s) {
    return tnil(sub != cons_s && sub != lcons_s);
  } else if (sup == integer_s) {
    return tnil(sub == fixnum_s || sub == bignum_s);
  } else if (sup == number_s) {
    return tnil(sub == fixnum_s || sub == bignum_s ||
                sub == integer_s || sub == float_s);
  } else if (sup == cons_s) {
    return tnil(sub == lcons_s);
  } else if (sup == sym_s) {
    return tnil(sub == null_s);
  } else if (sup == list_s) {
    return tnil(sub == null_s || sub == cons_s || sub == lcons_s);
  } else if (sup == sequence_s) {
    return tnil(sub == str_s || sub == lit_s || sub == lstr_s ||
                sub == vec_s || sub == null_s || sub == cons_s ||
                sub == list_s);
  } else if (sup == string_s) {
    return tnil(sub == str_s || sub == lit_s || sub == lstr_s);
  } else if (sup == stream_s) {
    return tnil(sub == stdio_stream_s);
  } else {
    val sub_struct = find_struct_type(sub);
    val sup_struct = find_struct_type(sup);

    if (sub_struct && sup_struct) {
      do {
        sub_struct = super(sub_struct);
        if (sub_struct == sup_struct)
          return t;
      } while (sub_struct);
      return nil;
    }

    return eq(sub, sup);
  }
}

val typep(val obj, val type)
{
  return subtypep(typeof(obj), type);
}

seq_info_t seq_info(val obj)
{
  seq_info_t ret;
  type_t to = type(obj);

  if (to != COBJ) {
    ret.obj = obj;
    ret.type = to;
    ret.kind = seq_kind_tab[to];
    return ret;
  }

  ret.obj = obj = nullify(obj);

  if (!obj || (ret.type = to = type(obj)) != COBJ) {
    ret.kind = seq_kind_tab[to];
  } else {
    val cls = obj->co.cls;

    if (cls == hash_s) {
      ret.kind = SEQ_HASHLIKE;
    } else if (cls == carray_s) {
      ret.kind = SEQ_VECLIKE;
    } else if (structp(cls)) {
      if (maybe_slot(obj, length_s))
        ret.kind = SEQ_VECLIKE;
      if (maybe_slot(obj, car_s))
        ret.kind = SEQ_LISTLIKE;
      else
      ret.kind = SEQ_NOTSEQ;
    } else {
      ret.kind = SEQ_NOTSEQ;
    }
  }

  return ret;
}

val throw_mismatch(val obj, type_t t)
{
  type_mismatch(lit("~s is not of type ~s"), obj, code2type(t), nao);
}

val type_check2(val obj, int t1, int t2)
{
  if (!is_ptr(obj) || (obj->t.type != t1 && obj->t.type != t2))
    type_mismatch(lit("~s is not of type ~s or ~s"), obj,
                  code2type(t1), code2type(t2), nao);
  return t;
}

val type_check3(val obj, int t1, int t2, int t3)
{
  if (!is_ptr(obj) || (obj->t.type != t1 && obj->t.type != t2
                       && obj->t.type != t3))
    type_mismatch(lit("~s is not of type ~s, ~s nor ~s"), obj,
                  code2type(t1), code2type(t2), code2type(t3), nao);
  return t;
}

val class_check(val cobj, val class_sym)
{
  type_assert (is_ptr(cobj) && cobj->t.type == COBJ &&
              (cobj->co.cls == class_sym || subtypep(cobj->co.cls, class_sym)),
               (lit("~s is not of type ~s"), cobj, class_sym, nao));
  return t;
}

val car(val cons)
{
  switch (type(cons)) {
  case NIL:
    return nil;
  case CONS:
    return cons->c.car;
  case LCONS:
    if (cons->lc.func == nil) {
      return cons->lc.car;
    } else {
      sig_check_fast();
      funcall1(cons->lc.func, cons);
      cons->lc.func = nil;
      return cons->lc.car;
    }
  case VEC:
    if (zerop(cons->v.vec[vec_length]))
      return nil;
    return cons->v.vec[0];
  case STR:
  case LIT:
  case LSTR:
    if (zerop(length_str(cons)))
      return nil;
    return chr_str(cons, zero);
  case COBJ:
    if (structp(cons))
      return funcall1(slot(cons, car_s), cons);
  default:
    type_mismatch(lit("~s is not a cons"), cons, nao);
  }
}

val cdr(val cons)
{
  switch (type(cons)) {
  case NIL:
    return nil;
  case CONS:
    return cons->c.cdr;
  case LCONS:
    if (cons->lc.func == nil) {
      return cons->lc.cdr;
    } else {
      sig_check_fast();
      funcall1(cons->lc.func, cons);
      cons->lc.func = nil;
      return cons->lc.cdr;
    }
  case VEC:
  case STR:
  case LIT:
  case LSTR:
    if (le(length(cons), one))
      return nil;
    return sub(cons, one, t);
  case COBJ:
    if (structp(cons))
      return funcall1(slot(cons, cdr_s), cons);
  default:
    type_mismatch(lit("~s is not a cons"), cons, nao);
  }
}

val rplaca(val cons, val new_car)
{
  switch (type(cons)) {
  case CONS:
    set(mkloc(cons->c.car, cons), new_car);
    return cons;
  case LCONS:
    set(mkloc(cons->lc.car, cons), new_car);
    return cons;
  case VEC:
  case STR:
  case LSTR:
    refset(cons, zero, new_car);
    return cons;
  default:
    type_mismatch(lit("rplaca: cannot modify ~s"), cons, nao);
  }
}

val rplacd(val cons, val new_cdr)
{
  switch (type(cons)) {
  case CONS:
    set(mkloc(cons->c.cdr, cons), new_cdr);
    return cons;
  case LCONS:
    set(mkloc(cons->lc.cdr, cons), new_cdr);
    return cons;
  case VEC:
  case STR:
  case LSTR:
    replace(cons, new_cdr, one, t);
    return cons;
  default:
    type_mismatch(lit("rplacd: cannot modify ~s"), cons, nao);
  }
}

val sys_rplaca(val cons, val new_car)
{
  (void) rplaca(cons, new_car);
  return new_car;
}

val sys_rplacd(val cons, val new_car)
{
  (void) rplacd(cons, new_car);
  return new_car;
}

loc car_l(val cons)
{
  switch (type(cons)) {
  case CONS:
    return mkloc(cons->c.car, cons);
  case LCONS:
    if (cons->lc.func) {
      sig_check_fast();
      funcall1(cons->lc.func, cons);
      cons->lc.func = nil;
    }
    return mkloc(cons->lc.car, cons);
  default:
    type_mismatch(lit("~s is not a cons"), cons, nao);
  }
}

loc cdr_l(val cons)
{
  switch (type(cons)) {
  case CONS:
    return mkloc(cons->c.cdr, cons);
  case LCONS:
    if (cons->lc.func) {
      sig_check_fast();
      funcall1(cons->lc.func, cons);
      cons->lc.func = nil;
    }
    return mkloc(cons->lc.cdr, cons);
  default:
    type_mismatch(lit("~s is not a cons"), cons, nao);
  }
}

val first(val cons)
{
  return car(cons);
}

val rest(val cons)
{
  return cdr(cons);
}

val second(val obj)
{
  return ref(obj, one);
}

val third(val obj)
{
  return ref(obj, two);
}

val fourth(val obj)
{
  return ref(obj, three);
}

val fifth(val obj)
{
  return ref(obj, four);
}

val sixth(val obj)
{
  return ref(obj, num_fast(5));
}

val seventh(val obj)
{
  return ref(obj, num_fast(6));
}

val eighth(val obj)
{
  return ref(obj, num_fast(7));
}

val ninth(val obj)
{
  return ref(obj, num_fast(8));
}

val tenth(val obj)
{
  return ref(obj, num_fast(9));
}

val conses(val list)
{
  list_collect_decl (out, ptail);

  if (listp(list))
    for (; consp(list); list = cdr(list))
      ptail = list_collect(ptail, list);
  else
    for (; list; list = cdr(list))
      ptail = list_collect(ptail, list);

  return out;
}

static val lazy_conses_func(val env, val lcons)
{
  val fun = lcons_fun(lcons);
  rplaca(lcons, env);
  func_set_env(fun, env = cdr(env));

  if (env)
    rplacd(lcons, make_lazy_cons(fun));
  else
    rplacd(lcons, nil);
  return nil;
}

val lazy_conses(val list)
{
  if (!list)
    return nil;
  return make_lazy_cons(func_f1(list, lazy_conses_func));
}

val listref(val list, val ind)
{
  gc_hint(list);
  if (lt(ind, zero)) {
    ind = plus(ind, length_list(list));
    if (lt(ind, zero))
      return nil;
  }
  for (; gt(ind, zero); ind = minus(ind, one))
    list = cdr(list);
  return car(list);
}

loc listref_l(val list, val ind)
{
  val olist = list;
  val oind = ind;

  if (lt(ind, zero))
    ind = plus(ind, length_list(list));

  for (; gt(ind, zero) && list; ind = minus(ind, one))
    list = cdr(list);
  if (ge(ind, zero) && consp(list))
    return car_l(list);

  uw_throwf(error_s, lit("~s has no assignable location at ~s"),
            olist, oind,  nao);
}

loc tail(val cons)
{
  while (cdr(cons))
    cons = cdr(cons);
  return cdr_l(cons);
}

loc term(loc head)
{
  while (consp(deref(head)))
    head = cdr_l(deref(head));
  return head;
}

loc lastcons(val list)
{
  loc ret = nulloc;

  gc_hint(list);

  while (consp(cdr(list))) {
    ret = cdr_l(list);
    list = cdr(list);
  }
  return ret;
}

val last(val seq, val n)
{
  if (null_or_missing_p(n)) {
    if (listp(seq)) {
      loc p = lastcons(seq);
      return nullocp(p) ? seq : deref(p);
    }
    return sub(seq, negone, t);
  } else {
    if (listp(seq))
      return nthlast(n, seq);
    return if3(plusp(n),
               sub(seq, neg(n), t),
               sub(seq, t, t));
  }
}

val nthcdr(val pos, val list)
{
  cnum n = c_num(pos);

  if (n < 0)
    uw_throwf(error_s, lit("nthcdr: negative index ~s given"), pos, nao);

  gc_hint(list);

  while (n-- > 0)
    list = cdr(list);

  return list;
}

val nth(val pos, val list)
{
  return car(nthcdr(pos, list));
}

val nthlast(val pos, val list)
{
  val iter = list;

  while (plusp(pos) && consp(list)) {
    list = cdr(list);
    pos = pred(pos);
  }

  if (plusp(pos))
    return iter;

  if (list == iter) {
    while (consp(iter))
      iter = cdr(iter);
  } else {
    while (consp(list)) {
      iter = cdr(iter);
      list = cdr(list);
    }
  }

  return iter;
}

val butlastn(val n, val list)
{
  val tail = nthlast(n, list);
  return ldiff(list, tail);
}

loc ltail(loc cons)
{
  while (cdr(deref(cons)))
    cons = cdr_l(deref(cons));
  return cons;
}

val pop(val *plist)
{
  val ret = car(*plist);
  *plist = cdr(*plist);
  return ret;
}

val rcyc_pop(val *plist)
{
  val rcyc = *plist;
  val ret = rcyc->c.car;
  *plist = rcyc->c.cdr;
  rcyc_cons(rcyc);
  return ret;
}

val upop(val *plist, val *pundo)
{
  *pundo = *plist;
  return pop(plist);
}

val push(val value, val *plist)
{
  /* Unsafe for mutating object fields: use mpush macro. */
  return *plist = cons(value, *plist);
}

val copy_list(val list)
{
  list_collect_decl (out, ptail);

  list = nullify(list);

  while (consp(list)) {
    ptail = list_collect(ptail, car(list));
    list = cdr(list);
  }

  ptail = list_collect_nconc(ptail, list);

  return out;
}

val make_like(val list, val thatobj)
{
  if (list != thatobj) {
    switch (type(thatobj)) {
    case VEC:
      return vec_list(list);
    case STR:
    case LIT:
    case LSTR:
      if (!opt_compat || opt_compat > 101) {
        if (!list)
          return null_string;
      }
      if (is_chr(car(list)))
        return cat_str(list, nil);
      break;
    case COBJ:
      if (structp(thatobj)) {
        val from_list_meth = maybe_slot(thatobj, from_list_s);
        if (from_list_meth)
          return funcall1(from_list_meth, list);
      }
      if (thatobj->co.cls == carray_s)
        return carray_list(list, carray_type(thatobj), nil);
      break;
    case NIL:
    case CONS:
    case LCONS:
    default:
      break;
    }
  }

  return list;
}

val toseq(val seq)
{
  seq_info_t si = seq_info(seq);

  switch (si.kind) {
  case SEQ_NIL:
    return nil;
  case SEQ_LISTLIKE:
  case SEQ_VECLIKE:
    return (si.type == COBJ) ? si.obj : nullify(si.obj);
  default:
    return cons(si.obj, nil);
  }
}

val tolist(val seq)
{
  switch (type(seq)) {
  case VEC:
    return list_vec(seq);
  case STR:
  case LIT:
  case LSTR:
    return list_str(seq);
  case COBJ:
    return mapcar_listout(identity_f, seq);
  case NIL:
  case CONS:
  case LCONS:
  default:
    return seq;
  }
}

val nullify(val seq)
{
  switch (type(seq)) {
  case NIL:
    return nil;
  case CONS:
  case LCONS:
    return seq;
  case LIT:
  case STR:
    return c_str(seq)[0] ? seq : nil;
  case LSTR:
    return if3(length_str_gt(seq, zero), seq, nil);
  case VEC:
    return if3(length_vec(seq) != zero, seq, nil);
  case COBJ:
    if (structp(seq)) {
      val nullify_meth = maybe_slot(seq, nullify_s);
      if (nullify_meth)
        return funcall1(nullify_meth, seq);
    }
  default:
    return seq;
  }
}

val seqp(val obj)
{
  switch (type(obj)) {
  case NIL:
  case CONS:
  case LCONS:
  case VEC:
  case STR:
  case LSTR:
  case LIT:
    return t;
  default:
    return nil;
  }
}

loc list_collect(loc ptail, val obj)
{
  switch (type(deref(ptail))) {
  case NIL:
    set(ptail, cons(obj, nil));
    return ptail;
  case CONS:
  case LCONS:
    ptail = tail(deref(ptail));
    set(ptail, cons(obj, nil));
    return ptail;
  case VEC:
    replace_vec(deref(ptail), cons(obj, nil), t, t);
    return ptail;
  case STR:
  case LIT:
  case LSTR:
    replace_str(deref(ptail), cons(obj, nil), t, t);
    return ptail;
  default:
    uw_throwf(error_s, lit("cannot append ~s"), deref(ptail), nao);
  }
}

loc list_collect_nconc(loc ptail, val obj)
{
  val tailobj = deref(ptail);
  obj = nullify(obj);

  switch (type(tailobj)) {
  case NIL:
    set(ptail, obj);
    return ptail;
  case CONS:
  case LCONS:
    ptail = tail(tailobj);
    set(ptail, obj);
    return ptail;
  case VEC:
    replace_vec(tailobj, obj, t, t);
    return ptail;
  case STR:
  case LIT:
  case LSTR:
    replace_str(tailobj, obj, t, t);
    return ptail;
  case COBJ:
    set(ptail, tolist(tailobj));
    ptail = tail(deref(ptail));
    set(ptail, tolist(obj));
    return ptail;
  default:
    uw_throwf(error_s, lit("cannot nconc ~s to ~s"), obj, tailobj, nao);
  }
}

loc list_collect_append(loc ptail, val obj)
{
  val tailobj = deref(ptail);
  obj = nullify(obj);

  switch (type(tailobj)) {
  case NIL:
    set(ptail, obj);
    return ptail;
  case CONS:
  case LCONS:
    set(ptail, copy_list(tailobj));
    ptail = tail(deref(ptail));
    set(ptail, obj);
    return ptail;
  case VEC:
    set(ptail, copy_vec(tailobj));
    replace_vec(deref(ptail), obj, t, t);
    return ptail;
  case STR:
  case LIT:
  case LSTR:
    set(ptail, copy_str(tailobj));
    replace_str(deref(ptail), obj, t, t);
    return ptail;
  case COBJ:
    set(ptail, tolist(tailobj));
    ptail = tail(deref(ptail));
    set(ptail, tolist(obj));
    return ptail;
  default:
    uw_throwf(error_s, lit("cannot append to ~s"), tailobj, nao);
  }
}

loc list_collect_nreconc(loc ptail, val obj)
{
  val rev = nreverse(nullify(obj));

  switch (type(deref(ptail))) {
  case CONS:
  case LCONS:
    ptail = tail(deref(ptail));
    /* fallthrough */
  case NIL:
    set(ptail, rev);
    switch (type(obj)) {
    case CONS:
    case LCONS:
      return cdr_l(obj);
    default:
      return ptail;
    }
  case VEC:
    replace_vec(deref(ptail), rev, t, t);
    return ptail;
  case STR:
  case LIT:
  case LSTR:
    replace_str(deref(ptail), rev, t, t);
    return ptail;
  default:
    uw_throwf(error_s, lit("cannot nconc ~s to ~s"), obj, deref(ptail), nao);
  }
}

static val revlist(val in, val *tail)
{
  val rev = nil;

  *tail = nil;

  if (in) {
    *tail = rev = cons(car(in), rev);
    in = cdr(in);
  }

  while (in) {
    rev = cons(car(in), rev);
    in = cdr(in);
  }

  return rev;
}

loc list_collect_revappend(loc ptail, val obj)
{
  val last;
  obj = nullify(obj);

  switch (type(deref(ptail))) {
  case CONS:
  case LCONS:
    set(ptail, copy_list(deref(ptail)));
    ptail = tail(deref(ptail));
    /* fallthrough */
  case NIL:
    switch (type(obj)) {
    case CONS:
    case LCONS:
      set(ptail, revlist(obj, &last));
      return cdr_l(last);
    case NIL:
      return ptail;
    default:
      set(ptail, reverse(obj));
      return ptail;
    }
    set(ptail, obj);
    return ptail;
  case VEC:
    set(ptail, copy_vec(deref(ptail)));
    replace_vec(deref(ptail), reverse(obj), t, t);
    return ptail;
  case STR:
  case LIT:
  case LSTR:
    set(ptail, copy_str(deref(ptail)));
    replace_str(deref(ptail), reverse(obj), t, t);
    return ptail;
  default:
    uw_throwf(error_s, lit("cannot append to ~s"), deref(ptail), nao);
  }
}

val nreverse(val in)
{
  seq_info_t si = seq_info(in);

  switch (si.kind) {
  case SEQ_NIL:
    return nil;
  case SEQ_LISTLIKE:
    {
      val rev = nil;
      val in = si.obj;

      while (in) {
        val temp = cdr(in);
        set(cdr_l(in), rev);
        rev = in;
        in = temp;
      }

      return rev;
    }
  case SEQ_VECLIKE:
    {
      val in = si.obj;
      cnum len = c_num(length(in));
      cnum i;

      for (i = 0; i < len / 2; i++) {
        cnum j = len - i - 1;
        val tmp = ref(in, num_fast(i));
        refset(in, num_fast(i), ref(in, num_fast(j)));
        refset(in, num_fast(j), tmp);
      }

      return in;
    }
  default:
    uw_throwf(error_s, lit("nreverse: cannot reverse ~s"), in, nao);
  }
}

val reverse(val seq_in)
{
  seq_info_t si = seq_info(seq_in);

  switch (si.kind) {
  case SEQ_NIL:
    return nil;
  case SEQ_LISTLIKE:
    {
      val rev = nil;
      val in = si.obj;

      while (in) {
        rev = cons(car(in), rev);
        in = cdr(in);
      }

      return make_like(rev, seq_in);
    }
  case SEQ_VECLIKE:
    if (si.type == LSTR)
      si.obj = lazy_str_force(si.obj);

    {
      val obj = copy(si.obj);
      cnum len = c_num(length(si.obj));
      cnum i;

      for (i = 0; i < len / 2; i++) {
        cnum j = len - i - 1;
        val tmp = ref(obj, num_fast(i));
        refset(obj, num_fast(i), ref(obj, num_fast(j)));
        refset(obj, num_fast(j), tmp);
      }

      return obj;
    }
  default:
    uw_throwf(error_s, lit("reverse: cannot reverse ~s"), in, nao);
  }
}

val append2(val list1, val list2)
{
  list_collect_decl (out, ptail);

  ptail = list_collect_append (ptail, list1);
  ptail = list_collect_append (ptail, list2);

  return out;
}

val appendv(struct args *lists)
{
  cnum index = 0;
  list_collect_decl (out, ptail);

  while (args_more(lists, index)) {
    val item = args_get(lists, &index);
    ptail = list_collect_append(ptail, item);
  }

  return out;
}

val nappend2(val list1, val list2)
{
  list_collect_decl (out, ptail);

  ptail = list_collect_nconc (ptail, list1);
  ptail = list_collect_nconc (ptail, list2);

  return out;
}

val revappend(val list1, val list2)
{
  list_collect_decl (out, ptail);

  ptail = list_collect_revappend(ptail, list1);
  ptail = list_collect_nconc(ptail, list2);

  return out;
}

val nreconc(val list1, val list2)
{
  list_collect_decl (out, ptail);

  ptail = list_collect_nreconc(ptail, list1);
  ptail = list_collect_nconc(ptail, list2);

  return out;
}

val nconcv(struct args *lists)
{
  cnum index = 0;
  list_collect_decl (out, ptail);

  while (args_more(lists, index))
    ptail = list_collect_nconc(ptail, args_get(lists, &index));

  return out;
}

val sub_list(val list, val from, val to)
{
  val len = nil;

  if (!list)
    return nil;

  if (null_or_missing_p(from))
    from = zero;
  else if (from == t)
    from = nil;
  else if (lt(from, zero)) {
    from = plus(from, len = length(list));
    if (to == zero)
      to = nil;
  }

  if (to == t || null_or_missing_p(to))
    to = nil;
  else if (!null_or_missing_p(to) && lt(to, zero))
    to = plus(to, if3(len, len, len = length(list)));

  gc_hint(list);

  if (to && from && gt(from, to)) {
    return nil;
  } else if (!to || (len && ge(to, len)))  {
    val i;

    for (i = zero; list; list = cdr(list), i = plus(i, one)) {
      if (from && ge(i, from))
        break;
    }
    return list;
  } else {
    val i;
    list_collect_decl (out, ptail);

    for (i = zero; list; list = cdr(list), i = plus(i, one)) {
      if (ge(i, to))
        break;
      if (from && ge(i, from))
        ptail = list_collect(ptail, car(list));
    }

    return out;
  }
}

val replace_list(val list, val items, val from, val to)
{
  val len = nil;

  items = toseq(items);

  if (!list)
    return items;

  if (listp(from)) {
    val where = from;
    val seq = list;
    val idx = zero;

    if (!missingp(to))
      uw_throwf(error_s,
                lit("replace-list: to-arg not applicable when from-arg is a list"),
                nao);

    for (; seq && where && items; seq = cdr(seq), idx = plus(idx, one)) {
      val wh = nil;

      for (; where && lt(wh = car(where), idx); where = cdr(where))
        ; /* empty */

      if (eql(wh, idx))
        rplaca(seq, pop(&items));
    }

    return list;
  } else if (vectorp(from)) {
    val where = from;
    val seq = list;
    val idx = zero;
    val wlen = length_vec(where);
    val widx = zero;

    if (!missingp(to))
      uw_throwf(error_s,
                lit("replace-str: to-arg not applicable when from-arg is a vector"),
                nao);

    for (; seq && items && lt(widx, wlen); seq = cdr(seq), idx = plus(idx, one)) {
      val wh = nil;

      for (; lt(widx, wlen) && lt(wh = vecref(where, widx), idx); widx = plus(widx, one))
        ; /* empty */

      if (eql(wh, idx))
        rplaca(seq, pop(&items));
    }

    return list;
  } else if (missingp(from)) {
    from = zero;
  } else if (from == t) {
    from = nil;
  } else if (lt(from, zero)) {
    from = plus(from, len ? len : (len = length(list)));
    if (to == zero)
      to = len;
  }

  if (to == t || null_or_missing_p(to))
    to = nil;
  if (to && lt(to, zero))
    to = plus(to, len ? len : (len = length(list)));

  gc_hint(list);

  if (!to || (len && ge(to, len)))  {
    if (from && zerop(from)) {
      return (listp(items)) ? items : list_vec(items);
    } else {
      val i;
      list_collect_decl (out, ptail);

      for (i = zero; list; list = cdr(list), i = plus(i, one)) {
        if (from && ge(i, from))
          break;
        ptail = list_collect(ptail, car(list));
      }

      ptail = list_collect_nconc(ptail, if3(listp(items),
                                            items, list_vec(items)));
      return out;
    }
  } else {
    val i;
    list_collect_decl (out, ptail);

    for (i = zero; list; list = cdr(list), i = plus(i, one)) {
      if (ge(i, to))
        break;
      if (from && lt(i, from))
        ptail = list_collect(ptail, car(list));
    }

    ptail = list_collect_nconc(ptail, append2(if3(listp(items), items,
                                                  list_vec(items)),
                                      list));
    return out;
  }
}

static val lazy_appendv_func(val env, val lcons)
{
  cons_bind (last, lists, env);
  val nonempty = nil;

  while (lists) {
    nonempty = nullify(pop(&lists));
    if (nonempty)
      break;
  }

  rplaca(lcons, last);

  if (nilp(lists)) {
    rplacd(lcons, nonempty);
    return nil;
  }

  if (atom(nonempty))
    uw_throwf(error_s, lit("append*: cannot append to atom ~s"),
              nonempty, nao);

  rplacd(env, lists);

  {
    loc ptail = ltail(mkcloc(nonempty));
    rplaca(env, car(deref(ptail)));
    set(ptail, make_lazy_cons(lcons_fun(lcons)));
    rplacd(lcons, nonempty);
  }
  return nil;
}

val lazy_appendv(struct args *args)
{
  val nonempty = nil;
  cnum index = 0;

  while (args_more(args, index)) {
    nonempty = args_get(args, &index);
    if (nonempty)
      break;
  }

  if (!args_more(args, index))
    return nonempty;

  if (atom(nonempty))
    uw_throwf(error_s, lit("append*: cannot append to atom ~s"),
              nonempty, nao);

  {
    loc ptail = ltail(mkcloc(nonempty));
    set(ptail, make_lazy_cons(func_f1(cons(car(deref(ptail)),
                                           args_get_rest(args, index)),
                                      lazy_appendv_func)));
    return nonempty;
  }
}

val lazy_appendl(val lists)
{
  args_decl_list(args, ARGS_MIN, lists);
  return lazy_appendv(args);
}

val ldiff(val list1, val list2)
{
  val list_orig = list1;
  list_collect_decl (out, ptail);

  list1 = nullify(list1);
  list2 = nullify(list2);

  switch (type(list2)) {
  case NIL:
  case CONS:
  case LCONS:
    while (list1 && list1 != list2) {
      ptail = list_collect(ptail, car(list1));
      list1 = cdr(list1);
    }
    break;
  default:
    while (list1 && !equal(list1, list2)) {
      ptail = list_collect(ptail, car(list1));
      list1 = cdr(list1);
    }
    break;
  }

  return make_like(out, list_orig);
}

val memq(val obj, val list)
{
  val list_orig = list;
  list = nullify(list);
  gc_hint(list);
  while (list && car(list) != obj)
    list = cdr(list);
  return make_like(list, list_orig);
}

val rmemq(val obj, val list)
{
  val list_orig = list;
  val found = nil;
  list = nullify(list);
  gc_hint(list);
  while (list && (found = (car(list) != obj ? list : found)))
    list = cdr(list);
  return make_like(found, list_orig);
}

val memql(val obj, val list)
{
  val list_orig = list;
  list = nullify(list);
  gc_hint(list);
  while (list && !eql(car(list), obj))
    list = cdr(list);
  return make_like(list, list_orig);
}

val rmemql(val obj, val list)
{
  val list_orig = list;
  val found = nil;
  list = nullify(list);
  gc_hint(list);
  while (list && (found = (eql(car(list), obj) ? list : found)))
    list = cdr(list);
  return make_like(found, list_orig);
}

val memqual(val obj, val list)
{
  val list_orig = list;
  list = nullify(list);
  gc_hint(list);
  while (list && !equal(car(list), obj))
    list = cdr(list);
  return make_like(list, list_orig);
}

val rmemqual(val obj, val list)
{
  val list_orig = list;
  val found = nil;
  list = nullify(list);
  gc_hint(list);
  while (list && (found = (equal(car(list), obj) ? list : found)))
    list = cdr(list);
  return make_like(found, list_orig);
}

val member(val item, val list, val testfun, val keyfun)
{
  testfun = default_arg(testfun, equal_f);
  keyfun = default_arg(keyfun, identity_f);

  list = nullify(list);

  gc_hint(list);

  for (; list; list = cdr(list)) {
    val elem = car(list);
    val key = funcall1(keyfun, elem);

    if (funcall2(testfun, item, key))
      return list;
  }

  return nil;
}

val rmember(val item, val list, val testfun, val keyfun)
{
  val found = nil;
  testfun = default_arg(testfun, equal_f);
  keyfun = default_arg(keyfun, identity_f);

  list = nullify(list);

  gc_hint(list);

  for (; list; list = cdr(list)) {
    val elem = car(list);
    val key = funcall1(keyfun, elem);

    if (funcall2(testfun, item, key))
      found = list;
  }

  return found;
}

val member_if(val pred, val list, val key)
{
  key = default_arg(key, identity_f);
  list = nullify(list);

  gc_hint(list);

  for (; list; list = cdr(list)) {
    val item = car(list);
    val subj = funcall1(key, item);

    if (funcall1(pred, subj))
      return list;
  }

  return nil;
}

val rmember_if(val pred, val list, val key)
{
  val found = nil;
  key = default_arg(key, identity_f);
  list = nullify(list);

  gc_hint(list);

  for (; list; list = cdr(list)) {
    val item = car(list);
    val subj = funcall1(key, item);

    if (funcall1(pred, subj))
      found = list;
  }

  return found;
}

static val rem_impl(val (*eqfun)(val, val), val name,
                    val obj, val seq_in, val keyfun_in)
{
  val keyfun = default_null_arg(keyfun_in);

  switch (type(seq_in)) {
  case NIL:
    return nil;
  case CONS:
  case LCONS:
  case COBJ:
    {
      list_collect_decl (out, ptail);
      val list = seq_in;
      val lastmatch = cons(nil, list);

      gc_hint(list);

      for (; list; list = cdr(list)) {
        val elem = car(list);
        val key = keyfun ? funcall1(keyfun, elem) : elem;

        if (eqfun(key, obj)) {
          ptail = list_collect_nconc(ptail, ldiff(cdr(lastmatch), list));
          lastmatch = list;
        }
      }
      ptail = list_collect_nconc(ptail, cdr(lastmatch));
      return out;
    }
  case LIT:
  case STR:
  case LSTR:
    {
      val out = mkustring(zero);
      val str = seq_in;
      cnum len = c_num(length_str(str)), i;

      for (i = 0; i < len; i++) {
        val elem = chr_str(str, num_fast(i));
        val key = keyfun ? funcall1(keyfun, elem) : elem;

        if (!eqfun(key, obj))
          string_extend(out, elem);
      }

      return out;
    }
  case VEC:
    {
      val out = vector(zero, nil);
      val vec = seq_in;
      cnum len = c_num(length_vec(vec)), i;

      for (i = 0; i < len; i++) {
        val elem = vecref(vec, num_fast(i));
        val key = keyfun ? funcall1(keyfun, elem) : elem;

        if (!eqfun(key, obj))
          vec_push(out, elem);
      }

      return out;
    }
  default:
    uw_throwf(error_s, lit("~a: ~s isn't a sequence"), name, seq_in, nao);
  }
}

val remove_if(val pred, val seq_in, val keyfun_in)
{
  val keyfun = default_null_arg(keyfun_in);

  switch (type(seq_in)) {
  case NIL:
    return nil;
  case CONS:
  case LCONS:
  case COBJ:
    {
      list_collect_decl (out, ptail);
      val list = seq_in;
      val lastmatch = cons(nil, list);

      gc_hint(list);

      for (; list; list = cdr(list)) {
        val elem = car(list);
        val key = keyfun ? funcall1(keyfun, elem) : elem;

        if (funcall1(pred, key)) {
          ptail = list_collect_nconc(ptail, ldiff(cdr(lastmatch), list));
          lastmatch = list;
        }
      }
      ptail = list_collect_nconc(ptail, cdr(lastmatch));
      return out;
    }
  case LIT:
  case STR:
  case LSTR:
    {
      val out = mkustring(zero);
      val str = seq_in;
      cnum len = c_num(length_str(str)), i;

      for (i = 0; i < len; i++) {
        val elem = chr_str(str, num_fast(i));
        val key = keyfun ? funcall1(keyfun, elem) : elem;

        if (!funcall1(pred, key))
          string_extend(out, elem);
      }

      return out;
    }
  case VEC:
    {
      val out = vector(zero, nil);
      val vec = seq_in;
      cnum len = c_num(length_vec(vec)), i;

      for (i = 0; i < len; i++) {
        val elem = vecref(vec, num_fast(i));
        val key = keyfun ? funcall1(keyfun, elem) : elem;

        if (!funcall1(pred, key))
          vec_push(out, elem);
      }

      return out;
    }
  default:
    uw_throwf(error_s, lit("remove-if: ~s isn't a sequence"), seq_in, nao);
  }
}


val remq(val obj, val seq, val keyfun)
{
  return rem_impl(eq, lit("remq"), obj, seq, keyfun);
}

val remql(val obj, val seq, val keyfun)
{
  return rem_impl(eql, lit("remq"), obj, seq, keyfun);
}

val remqual(val obj, val seq, val keyfun)
{
  return rem_impl(equal, lit("remqual"), obj, seq, keyfun);
}

val keepq(val obj, val seq, val keyfun)
{
  return rem_impl(neq, lit("keepq"), obj, seq, keyfun);
}

val keepql(val obj, val seq, val keyfun)
{
  return rem_impl(neql, lit("keepql"), obj, seq, keyfun);
}

val keepqual(val obj, val seq, val keyfun)
{
  return rem_impl(nequal, lit("keepqual"), obj, seq, keyfun);
}

val keep_if(val pred, val seq, val keyfun)
{
  return remove_if(notf(pred), seq, keyfun);
}

static val rem_lazy_rec(val obj, val list, val env, val func);

static val rem_lazy_func(val env, val lcons)
{
  cons_bind (pred, list, env);
  val rest = rem_lazy_rec(pred, list, env, lcons_fun(lcons));
  rplacd(lcons, rem_lazy_rec(pred, list, env, lcons_fun(lcons)));
  return rest;
}

static val rem_lazy_rec(val pred, val list, val env, val func)
{
  while (list && funcall1(pred, car(list)))
    list = cdr(list);
  if (!list)
    return nil;
  if (!env)
    func = func_f1(cons(pred, cdr(list)), rem_lazy_func);
  else
    rplacd(env, cdr(list));
  return make_half_lazy_cons(func, car(list));
}

val remq_lazy(val obj, val list)
{
  return rem_lazy_rec(curry_12_1(eq_f, obj), nullify(list), nil, nil);
}

val remql_lazy(val obj, val list)
{
  return rem_lazy_rec(curry_12_1(eql_f, obj), nullify(list), nil, nil);
}

val remqual_lazy(val obj, val list)
{
  return rem_lazy_rec(curry_12_1(equal_f, obj), nullify(list), nil, nil);
}

val remove_if_lazy(val pred, val list, val key)
{
  val pred_key = chain(default_arg(key, identity_f), pred, nao);
  return rem_lazy_rec(pred_key, nullify(list), nil, nil);
}

val keep_if_lazy(val pred, val list, val key)
{
  val pred_key = chain(default_arg(key, identity_f), pred, null_f, nao);
  return rem_lazy_rec(pred_key, nullify(list), nil, nil);
}

val tree_find(val obj, val tree, val testfun)
{
  if (funcall2(default_arg(testfun, equal_f), obj, tree))
    return t;
  else if (consp(tree))
    return some_satisfy(tree, curry_123_2(func_n3(tree_find),
                                          obj, testfun), nil);
  return nil;
}

val countqual(val obj, val list)
{
  val count = zero;

  list = nullify(list);

  gc_hint(list);

  for (; list; list = cdr(list))
    if (equal(car(list), obj))
      count = plus(count, one);

  return count;
}

val countql(val obj, val list)
{
  val count = zero;

  list = nullify(list);

  gc_hint(list);

  for (; list; list = cdr(list))
    if (eql(car(list), obj))
      count = plus(count, one);

  return count;
}

val countq(val obj, val list)
{
  val count = zero;

  list = nullify(list);

  gc_hint(list);

  for (; list; list = cdr(list))
    if (car(list) == obj)
      count = plus(count, one);

  return count;
}

val count_if(val pred, val list, val key)
{
  val count = zero;

  key = default_arg(key, identity_f);
  list = nullify(list);

  gc_hint(list);

  for (; list; list = cdr(list)) {
    val subj = funcall1(key, car(list));
    val satisfies = funcall1(pred, subj);

    if (satisfies)
      count = plus(count, one);
  }

  return count;
}

val some_satisfy(val list, val pred, val key)
{
  pred = default_arg(pred, identity_f);
  key = default_arg(key, identity_f);
  list = nullify(list);

  gc_hint(list);

  for (; list; list = cdr(list)) {
    val item;
    if ((item = funcall1(pred, funcall1(key, car(list)))) != nil)
      return item;
  }

  return nil;
}

val all_satisfy(val list, val pred, val key)
{
  val item = t;

  pred = default_arg(pred, identity_f);
  key = default_arg(key, identity_f);
  list = nullify(list);

  gc_hint(list);

  for (; list; list = cdr(list)) {
    if ((item = funcall1(pred, funcall1(key, car(list)))) == nil)
      return nil;
  }

  return item;
}

val none_satisfy(val list, val pred, val key)
{
  pred = default_arg(pred, identity_f);
  key = default_arg(key, identity_f);
  list = nullify(list);

  gc_hint(list);

  for (; list; list = cdr(list)) {
    if (funcall1(pred, funcall1(key, car(list))))
      return nil;
  }

  return t;
}

val multi(val func, struct args *lists)
{
  val transposed = mapcarv(list_f, lists);
  val processed = funcall1(func, transposed);
  return mapcarl(list_f, processed);
}

val flatten(val list)
{
  if (list == nil)
    return nil;

  if (atom(list))
    return cons(list, nil);

  return mappend(func_n1(flatten), list);
}

/*
 * Return the embedded list whose car is the non-nil atom in the given nested
 * list, updating the escape stack in the process. If no such atom is found in
 * the list, try to retreate up the stack to find it in the surrounding
 * structure, finally returning nil if nothing is found.
 */
static val lazy_flatten_scan(val list, val *escape)
{
  for (;;) {
    if (list) {
      val a = car(list);
      if (nilp(a)) {
        list = cdr(list);
      } else if (atom(a)) {
        return list;
      } else do {
        push(cdr(list), escape); /* safe mutation: *escape is a local var */
        list = a;
        a = car(list);
      } while (consp(a));
      return list;
    } else if (*escape) {
      list = pop(escape);
    } else {
      return nil;
    }
  }
}

static val lazy_flatten_func(val env, val lcons)
{
  cons_bind (list, escape, env);
  val atom = car(list);
  val next = lazy_flatten_scan(cdr(list), &escape);

  rplaca(lcons, atom);
  rplaca(env, next);
  rplacd(env, escape);

  if (next)
    rplacd(lcons, make_lazy_cons(lcons_fun(lcons)));

  return nil;
}

val lazy_flatten(val list)
{
  if (atom(list)) {
    return cons(list, nil);
  } else {
    val escape = nil;
    val next = lazy_flatten_scan(list, &escape);

    if (!next)
      return nil;

    return make_lazy_cons(func_f1(cons(next, escape), lazy_flatten_func));
  }
}

val flatcar(val tree)
{
  if (atom(tree))
    return cons(tree, nil);
  if (cdr(tree))
    return nappend2(flatcar(car(tree)), flatcar(cdr(tree)));
  return flatcar(car(tree));
}

static val lazy_flatcar_scan(val tree, val *cont)
{
  for (;;) {
    val a = car(tree);
    val d = cdr(tree);

    if (d != nil)
      *cont = cons(d, *cont);

    if (atom(a))
      return a;

    tree = a;
  }
}

static val lazy_flatcar_func(val env, val lcons)
{
  val tree = car(env);
  val cont = nil;
  val atom = lazy_flatcar_scan(tree, &cont);

  rplaca(lcons, atom);
  rplaca(env, cont);

  if (cont)
    rplacd(lcons, make_lazy_cons(lcons_fun(lcons)));

  return nil;
}

val lazy_flatcar(val tree)
{
  if (atom(tree)) {
    return cons(tree, nil);
  } else {
    val cont = nil;
    val nextatom = lazy_flatcar_scan(tree, &cont);

    if (!cont)
      return cons(nextatom, nil);

    return make_lazy_cons(func_f1(cons(tree, nil), lazy_flatcar_func));
  }
}


static val tuples_func(val env, val lcons)
{
  list_collect_decl (out, ptail);
  cons_bind (seq_in, envr, env);
  cons_bind (n, fill, envr);
  val seq = seq_in;
  val count;

  for (count = n; count != zero && seq; count = minus(count, one))
    ptail = list_collect(ptail, pop(&seq));

  if (!missingp(fill))
    for (; gt(count, zero); count = minus(count, one))
      ptail = list_collect(ptail, fill);

  rplaca(env, seq);

  if (seq)
    rplacd(lcons, make_lazy_cons(lcons_fun(lcons)));
  rplaca(lcons, make_like(out, seq_in));

  return nil;
}

val tuples(val n, val seq, val fill)
{
  seq = nullify(seq);

  if (!seq)
    return nil;

  return make_lazy_cons(func_f1(cons(seq, cons(n, fill)),
                                tuples_func));
}

static val partition_by_func(val env, val lcons)
{
  list_collect_decl (out, ptail);
  cons_bind (flast_seq, func, env);
  cons_bind (flast, seq_in, flast_seq);
  val seq = seq_in;
  val fnext = nil;

  ptail = list_collect(ptail, pop(&seq));

  while (seq) {
    val next = car(seq);
    fnext = funcall1(func, next);

    if (!equal(flast, fnext))
      break;

    ptail = list_collect(ptail, next);

    seq = cdr(seq);
    flast = fnext;
  }

  rplaca(flast_seq, fnext);
  rplacd(flast_seq, seq);

  if (seq)
    rplacd(lcons, make_lazy_cons(lcons_fun(lcons)));

  rplaca(lcons, make_like(out, seq_in));
  return nil;
}

val partition_by(val func, val seq)
{
  seq = nullify(seq);

  if (!seq)
    return nil;

  return make_lazy_cons(func_f1(cons(cons(funcall1(func, car(seq)), seq),
                                     func),
                                partition_by_func));
}

static val partition_func(val env, val lcons)
{
  cons_bind (seq, indices_base, env);
  cons_bind (indices, base, indices_base);

  for (;;) {
    if (indices) {
      val index = pop(&indices);
      val index_rebased = minus(index, base);

      if (le(index_rebased, zero)) {
        continue;
      } else {
        val first = sub(seq, zero, index_rebased);
        val rest = nullify(sub(seq, index_rebased, t));

        rplaca(env, rest);
        rplaca(indices_base, indices);
        rplacd(indices_base, index);

        if (rest)
          rplacd(lcons, make_lazy_cons(lcons_fun(lcons)));

        rplaca(lcons, first);
      }
    } else {
      rplaca(lcons, seq);
    }
    break;
  }

  return nil;
}

static val split_func(val env, val lcons)
{
  cons_bind (seq, indices_base, env);
  cons_bind (indices, base, indices_base);

  for (;;) {
    if (indices) {
      val index = pop(&indices);
      val index_rebased = minus(index, base);

      if (lt(index_rebased, zero)) {
        continue;
      } else {
        val first = sub(seq, zero, index_rebased);
        val rsub = sub(seq, index_rebased, t);
        val rest = nullify(rsub);

        rplaca(env, rest);
        rplaca(indices_base, indices);
        rplacd(indices_base, index);

        rplacd(lcons, if3(rest,
                          make_lazy_cons(lcons_fun(lcons)),
                          cons(rsub, nil)));

        rplaca(lcons, first);
      }
    } else {
      rplaca(lcons, seq);
    }
    break;
  }

  return nil;
}

static val split_star_func(val env, val lcons)
{
  cons_bind (seq, indices_base, env);
  cons_bind (indices, base, indices_base);

  for (;;) {
    if (indices) {
      val index = pop(&indices);
      val index_rebased = minus(index, base);

      if (lt(index_rebased, zero)) {
        continue;
      } else {
        val first = sub(seq, zero, index_rebased);
        val rsub = sub(seq, succ(index_rebased), t);
        val rest = nullify(rsub);

        rplaca(env, rest);
        rplaca(indices_base, indices);
        rplacd(indices_base, succ(index));

        rplacd(lcons, if3(rest,
                          make_lazy_cons(lcons_fun(lcons)),
                          cons(rsub, nil)));

        rplaca(lcons, first);
      }
    } else {
      rplaca(lcons, seq);
    }
    break;
  }

  return nil;
}

static val partition_split_common(val seq, val indices,
                                  val (*split_fptr)(val env, val lcons))
{
  seq = nullify(seq);

  if (!seq)
    return nil;

  if (functionp(indices))
    indices = funcall1(indices, seq);

  indices = nullify(indices);

  if (!indices)
    return cons(seq, nil);

  if (!seqp(indices))
    indices = cons(indices, nil);

  {
    val len = nil;
    list_collect_decl (out, ptail);

    if (!opt_compat || opt_compat > 170) {
      for (; indices; indices = cdr(indices)) {
        val idx_raw = car(indices);
        val idx = if3(minusp(idx_raw),
                      plus(idx_raw, if3(len, len, len = length(seq))),
                      idx_raw);
        if (!minusp(idx))
          ptail = list_collect(ptail, idx);
      }
    }

    return make_lazy_cons(func_f1(cons(seq, cons(out, zero)), split_fptr));
  }
}

val partition(val seq, val indices)
{
  return partition_split_common(seq, indices, partition_func);
}

val split(val seq, val indices)
{
  return partition_split_common(seq, indices, split_func);
}

val split_star(val seq, val indices)
{
  return partition_split_common(seq, indices, split_star_func);
}

static val partition_star_func(val env, val lcons)
{
  for (;;) {
    cons_bind (seq, indices_base, env);
    cons_bind (indices, base, indices_base);

    if (indices) {
      val index = pop(&indices);
      val index_rebased = minus(index, base);
      val first = nullify(sub(seq, zero, index_rebased));

      seq = nullify(sub(seq, plus(index_rebased, one), t));

      base = plus(index, one);

      while (seq && eql(car(indices), base)) {
        seq = nullify(cdr(seq));
        base = plus(base, one);
        pop(&indices);
      }

      rplaca(env, seq);
      rplaca(indices_base, indices);
      rplacd(indices_base, base);

      if (!first)
        continue;

      rplaca(lcons, first);

      if (seq)
        rplacd(lcons, make_lazy_cons(lcons_fun(lcons)));
    } else {
      rplaca(lcons, seq);
    }

    break;
  }

  return nil;
}

val partition_star(val seq, val indices)
{
  val base = zero;
  seq = nullify(seq);
  indices = nullify(indices);

  if (!seq)
    return nil;

  if (functionp(indices))
    indices = funcall1(indices, seq);

  if (!indices)
    return cons(seq, nil);

  if (indices == zero)
    return cons(nullify(rest(seq)), nil);

  if (!seqp(indices))
    indices = cons(indices, nil);

  {
    val len = nil;
    list_collect_decl (tindices, ptail);

    if (!opt_compat || opt_compat > 170) {
      for (; indices; indices = cdr(indices)) {
        val idx_raw = car(indices);
        val idx = if3(minusp(idx_raw),
                      plus(idx_raw, if3(len, len, len = length(seq))),
                      idx_raw);
        if (!minusp(idx))
          ptail = list_collect(ptail, idx);
      }
    }

    while (tindices && lt(car(tindices), zero))
      pop(&tindices);

    while (tindices && eql(car(tindices), base)) {
      seq = nullify(cdr(seq));
      if (!seq)
        return nil;
      base = plus(base, one);
      pop(&tindices);
    }

    if (!tindices)
      return cons(seq, nil);

    return make_lazy_cons(func_f1(cons(seq, cons(tindices, base)),
                                  partition_star_func));
  }
}

cnum c_num(val num);

val eql(val left, val right)
{
  /* eql is the same as eq except that numbers
     are compared by value, and ranges are
     specially treated also. This means that bignum and
     floating point objects which are distinct are
     treated through the equal function.
     Two ranges are eql if they are the same object,
     or if their corresponding parts are eql. */
  if (left == right)
    return t;

  switch (type(left)) {
  case BGNUM:
  case FLNUM:
    return equal(left, right);
  case RNG:
    if (type(right) == RNG &&
        eql(from(left), from(right)) &&
        eql(to(left), to(right)))
      return t;
    /* fallthrough */
  default:
    return nil;
  }
}

val equal(val left, val right)
{
  if (left == right)
    return t;

  switch (type(left)) {
  case NIL:
  case CHR:
  case NUM:
    break;
  case CONS:
  case LCONS:
    if (type(right) == CONS || type(right) == LCONS)
    {
      if (equal(car(left), car(right)) && equal(cdr(left), cdr(right)))
        return t;
      return nil;
    }
    break;
  case LIT:
    switch (type(right)) {
    case LIT:
      return wcscmp(litptr(left), litptr(right)) == 0 ? t : nil;
    case STR:
      return wcscmp(litptr(left), right->st.str) == 0 ? t : nil;
    case LSTR:
      lazy_str_force(right);
      return equal(left, right->ls.prefix);
    case COBJ:
      break;
    default:
      return nil;
    }
    break;
  case STR:
    switch (type(right)) {
    case LIT:
      return wcscmp(left->st.str, litptr(right)) == 0 ? t : nil;
    case STR:
      return wcscmp(left->st.str, right->st.str) == 0 ? t : nil;
    case LSTR:
      lazy_str_force(right);
      return equal(left, right->ls.prefix);
    case COBJ:
      break;
    default:
      return nil;
    }
    break;
  case SYM:
  case PKG:
  case ENV:
    break;
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
      case N5: return (left->f.f.n5 == right->f.f.n5) ? t : nil;
      case N6: return (left->f.f.n6 == right->f.f.n6) ? t : nil;
      case N7: return (left->f.f.n7 == right->f.f.n7) ? t : nil;
      case N8: return (left->f.f.n8 == right->f.f.n8) ? t : nil;
      }
      return nil;
    }
    break;
  case VEC:
    if (type(right) == VEC) {
      cnum i, length;
      if (!equal(left->v.vec[vec_length], right->v.vec[vec_length]))
        return nil;
      length = c_num(left->v.vec[vec_length]);
      for (i = 0; i < length; i++) {
        if (!equal(left->v.vec[i], right->v.vec[i]))
          return nil;
      }
      return t;
    }
    break;
  case LSTR:
    switch (type(right)) {
    case LIT:
    case STR:
    case LSTR:
      lazy_str_force(left);
      return equal(left->ls.prefix, right);
    case COBJ:
      break;
    default:
      return nil;
    }
    return nil;
  case BGNUM:
    if (type(right) == BGNUM) {
      if (mp_cmp(mp(left), mp(right)) == MP_EQ)
        return t;
      return nil;
    }
    break;
  case FLNUM:
    if (type(right) == FLNUM) {
      if (left->fl.n == right->fl.n)
        return t;
      return nil;
    }
    break;
  case RNG:
    if (type(right) == RNG) {
      if (equal(from(left), from(right)) &&
          equal(to(left), to(right)))
        return t;
      return nil;
    }
    break;
  case BUF:
    if (type(right) == BUF) {
      cnum ll = c_num(left->b.len);
      cnum rl = c_num(right->b.len);
      if (ll == rl && memcmp(left->b.data, right->b.data, ll) == 0)
        return t;
    }
    break;
  case COBJ:
    if (left->co.ops->equalsub) {
      val lsub = left->co.ops->equalsub(left);
      if (lsub)
        return equal(lsub, right);
    }

    if (type(right) == COBJ && left->co.ops == right->co.ops)
      return left->co.ops->equal(left, right);

    return nil;
  case CPTR:
    if (type(right) == CPTR && left->co.ops == right->co.ops)
      return left->co.ops->equal(left, right);
  }

  if (type(right) != COBJ)
    return nil;

  if (right->co.ops->equalsub) {
    val rsub = right->co.ops->equalsub(right);
    if (rsub)
      return equal(left, rsub);
  }

  return nil;
}

alloc_bytes_t malloc_bytes;

mem_t *chk_malloc(size_t size)
{
  mem_t *ptr = convert(mem_t *, malloc(size));

  assert (!async_sig_enabled);

  if (size && ptr == 0)
    ptr = convert(mem_t *, oom_realloc(0, size));
  malloc_bytes += size;
  return ptr;
}

mem_t *chk_malloc_gc_more(size_t size)
{
  mem_t *ptr = convert(mem_t *, malloc(size));
  assert (!async_sig_enabled);
  if (size && ptr == 0)
    ptr = convert(mem_t *, oom_realloc(0, size));
  return ptr;
}

mem_t *chk_calloc(size_t n, size_t size)
{
  mem_t *ptr = convert(mem_t *, calloc(n, size));
  cnum total = convert(cnum, size) * convert(cnum, n);

  assert (!async_sig_enabled);

  if (size && ptr == 0) {
    ptr = convert(mem_t *, oom_realloc(0, total));
    memset(ptr, 0, total);
  }
  malloc_bytes += total;
  return ptr;
}

mem_t *chk_realloc(mem_t *old, size_t size)
{
  mem_t *newptr = convert(mem_t *, realloc(old, size));

  assert (!async_sig_enabled);

  if (size != 0 && newptr == 0)
    newptr = oom_realloc(old, size);
  malloc_bytes += size;
  return newptr;
}

mem_t *chk_grow_vec(mem_t *old, size_t oldelems, size_t newelems,
                    size_t elsize)
{
  static const size_t no_oflow = convert(size_t, -1) >> (CHAR_BIT * sizeof(size_t) / 2);
  size_t bytes = newelems * elsize;

  if (elsize == 0)
    internal_error("elsize == 0");

  if (newelems <= oldelems ||
      ((newelems > no_oflow || elsize > no_oflow) && bytes / elsize != newelems))
    uw_throw(error_s, lit("array size overflow"));

  return chk_realloc(old, bytes);
}

static size_t bounding_pow_two(size_t s)
{
  s -= 1;

  s |= (s >> 1);
  s |= (s >> 2);
  s |= (s >> 4);
  s |= (s >> 8);
  s |= (s >> 16);

  if (sizeof (size_t) * CHAR_BIT > 32) {
    size_t t = s >> 16; /* suppress warning about s >> 32 */
    s |= (t >> 16);
  }

  return s + 1;
}

mem_t *chk_manage_vec(mem_t *old, size_t oldfilled, size_t newfilled,
                      size_t elsize, mem_t *fillval)
{
  if (newfilled == 0) {
    free(old);
    return 0;
  } else {
    static const size_t no_oflow = convert(size_t, -1) >> (CHAR_BIT * sizeof(size_t) / 2);
    size_t oldbytes = oldfilled * elsize;
    size_t newbytes = newfilled * elsize;
    size_t oldsize = bounding_pow_two(oldbytes);
    size_t newsize = bounding_pow_two(newbytes);

    if (((newfilled > no_oflow || elsize > no_oflow) &&
         newbytes / elsize != newfilled) ||
        (newsize < newbytes))
      uw_throw(error_s, lit("array size overflow"));

    if (oldsize != newsize)
      old = chk_realloc(old, newsize);

    if (fillval)
      while (oldbytes < newbytes) {
        memcpy(old + oldbytes, fillval, elsize);
        oldbytes += elsize;
      }

    return old;
  }
}

wchar_t *chk_wmalloc(size_t nwchar)
{
  return coerce(wchar_t *, chk_xalloc(nwchar, sizeof (wchar_t),
                                      lit("string operation")));
}

wchar_t *chk_strdup(const wchar_t *str)
{
  size_t nchar = wcslen(str) + 1;
  wchar_t *copy = chk_wmalloc(nchar);
  wmemcpy(copy, str, nchar);
  return copy;
}

char *chk_strdup_utf8(const char *str)
{
  size_t nchar = strlen(str) + 1;
  char *copy = coerce(char *, chk_malloc(nchar));
  memcpy(copy, str, nchar);
  return copy;
}

unsigned char *chk_strdup_8bit(const wchar_t *str)
{
  size_t nchar = wcslen(str) + 1, i;
  unsigned char *copy = coerce(unsigned char *, chk_malloc(nchar));
  for (i = 0; i < nchar; i++) {
    if (str[i] < 0 || str[i] > 255) {
      free(copy);
      uw_throwf(error_s, lit("cannot coerce ~s to 8 bit"),
                string(str), nao);
    }
    copy[i] = str[i];
  }
  return copy;
}

mem_t *chk_copy_obj(mem_t *orig, size_t size)
{
  mem_t *copy = chk_malloc(size);
  memcpy(copy, orig, size);
  return copy;
}

mem_t *chk_xalloc(ucnum m, ucnum n, val self)
{
  ucnum mn = m * n;
  size_t size = mn;

  if ((m > 0 && mn / m != n) || (ucnum) size != mn)
    uw_throwf(error_s, lit("~s: memory allocation size overflow"),
              self, nao);

  return chk_malloc(size);
}

val cons(val car, val cdr)
{
  val obj;

  if (recycled_conses) {
    obj = recycled_conses;
    recycled_conses = recycled_conses->c.cdr;
#if CONFIG_GEN_GC
    gc_mutated(obj);
#endif
  } else {
    obj = make_obj();
    obj->c.type = CONS;
  }

  obj->c.car = car;
  obj->c.cdr = cdr;
  return obj;
}

val make_lazy_cons(val func)
{
  val obj = make_obj();
  obj->lc.type = LCONS;
  obj->lc.car = obj->lc.cdr = nil;
  obj->lc.func = func;
  return obj;
}

val make_half_lazy_cons(val func, val car)
{
  val obj = make_obj();
  obj->lc.type = LCONS;
  obj->lc.car = car;
  obj->lc.cdr = nil;
  obj->lc.func = func;
  return obj;
}

void rcyc_cons(val cons)
{
  cons->c.cdr = recycled_conses;
  cons->c.car = nil;
  recycled_conses = cons;
}

void rcyc_list(val list)
{
  if (list) {
    val rl_orig = recycled_conses;
    recycled_conses = list;

    while (list->c.cdr)
      list = list->c.cdr;

    list->c.cdr = rl_orig;
  }
}

void rcyc_empty(void)
{
  recycled_conses = nil;
}

val lcons_fun(val lcons)
{
  type_check(lcons, LCONS);
  return lcons->lc.func;
}

val list(val first, ...)
{
  va_list vl;
  val list = nil;
  val array[32], *ptr = array;

  if (first != nao) {
    val next = first;

    va_start (vl, first);

    do {
      *ptr++ = next;
      if (ptr == array + 32)
        internal_error("runaway arguments in list function");
      next = va_arg(vl, val);
    } while (next != nao);

    va_end (vl);

    while (ptr > array)
      list = cons(*--ptr, list);
  }

  return list;
}

val listv(struct args *args)
{
  return args_get_list(args);
}

val consp(val obj)
{
  type_t ty = type(obj);
  return (ty == CONS || ty == LCONS) ? t : nil;
}

val lconsp(val obj)
{
  return type(obj) == LCONS ? t : nil;
}

val atom(val obj)
{
  return if3(consp(obj), nil, t);
}

val listp(val obj)
{
  return if2(obj == nil || consp(obj), t);
}

val endp(val obj)
{
  if (obj == nil)
    return t;
  if (consp(obj))
    return nil;
  uw_throwf(error_s, lit("endp: list improperly terminated by ~s"),
            obj, nao);
}

val proper_list_p(val obj)
{
  while (consp(obj))
    obj = cdr(obj);

  return (obj == nil) ? t : nil;
}

val length_list(val list)
{
  cnum len = 0;
  val bn_len;

  gc_hint(list);

  while (consp(list) && len < INT_PTR_MAX) {
    len++;
    list = cdr(list);
  }

  if (len < INT_PTR_MAX)
    return num(len);

  bn_len = num(INT_PTR_MAX);

  while (consp(list)) {
    bn_len = succ(bn_len);
    list = cdr(list);
  }

  return bn_len;
}

static val length_proper_list(val list)
{
  cnum len = 0;
  val bn_len;

  gc_hint(list);

  while (list && len < INT_PTR_MAX) {
    len++;
    list = cdr(list);
  }

  if (len < INT_PTR_MAX)
    return num(len);

  bn_len = num(INT_PTR_MAX);

  while (list) {
    bn_len = succ(bn_len);
    list = cdr(list);
  }

  return bn_len;
}

val getplist(val list, val key)
{
  gc_hint(list);

  for (; list; list = cdr(cdr(list))) {
    val ind = first(list);
    if (ind == key)
      return second(list);
  }

  return nil;
}

val getplist_f(val list, val key, loc found)
{
  gc_hint(list);

  for (; list; list = cdr(cdr(list))) {
    val ind = first(list);
    if (ind == key) {
      deref(found) = t;
      return second(list);
    }
  }

  deref(found) = nil;
  return nil;
}

val memp(val key, val plist)
{
  for (; plist; plist = cddr(plist)) {
    if (car(plist) == key)
      return plist;
  }
  return nil;
}

val plist_to_alist(val list)
{
  list_collect_decl (out, ptail);

  for (; list; list = cdr(cdr(list))) {
    val ind = first(list);
    val prop = second(list);
    ptail = list_collect(ptail, cons(ind, prop));
  }

  return out;
}

val improper_plist_to_alist(val list, val boolean_keys)
{
  list_collect_decl (out, ptail);

  for (; list; list = cdr(list)) {
    val ind = first(list);

    if (memqual(ind, boolean_keys)) {
      ptail = list_collect(ptail, cons(ind, t));
    } else {
      val prop = second(list);
      ptail = list_collect(ptail, cons(ind, prop));
      list = cdr(list);
    }
  }

  return out;
}

val num(cnum n)
{
  if (n >= NUM_MIN && n <= NUM_MAX)
    return coerce(val, (n << TAG_SHIFT) | TAG_NUM);
  return bignum(n);
}

cnum c_num(val num)
{
  switch (type(num)) {
  case CHR: case NUM:
    return coerce(cnum, num) >> TAG_SHIFT;
  case BGNUM:
    if (in_int_ptr_range(num)) {
      int_ptr_t out;
      mp_get_intptr(mp(num), &out);
      return out;
    }
    uw_throwf(error_s, lit("~s is out of cnum range"), num, nao);
  default:
    type_mismatch(lit("~s is not an integer"), num, nao);
  }
}

val flo(double n)
{
  val obj = make_obj();
  obj->fl.type = FLNUM;
  obj->fl.n = n;
  return obj;
}

double c_flo(val num)
{
  type_check(num, FLNUM);
  return num->fl.n;
}

val fixnump(val num)
{
  return (is_num(num)) ? t : nil;
}

val bignump(val num)
{
  return (type(num) == BGNUM) ? t : nil;
}

val integerp(val num)
{
  switch (tag(num)) {
  case TAG_NUM:
    return t;
  case TAG_PTR:
    if (num == nil)
      return nil;
    if (num->t.type == BGNUM)
      return t;
    /* fallthrough */
  default:
    return nil;
  }
}

val floatp(val num)
{
  return (type(num) == FLNUM) ? t : nil;
}

val numberp(val num)
{
  switch (tag(num)) {
  case TAG_NUM:
    return t;
  case TAG_PTR:
    if (num == nil)
      return nil;
    if (num->t.type == BGNUM || num->t.type == FLNUM)
      return t;
    /* fallthrough */
  default:
    return nil;
  }
}

val nary_op(val (*cfunc)(val, val), struct args *args, val emptyval)
{
  val fi, se, re;
  cnum index = 0;

  if (!args_more(args, index))
    return emptyval;

  fi = args_get(args, &index);

  if (!args_more(args, index))
    return fi;

  se = args_get(args, &index);

  if (!args_more(args, index))
    return cfunc(fi, se);

  re = args_get_rest(args, index);

  return reduce_left(func_n2(cfunc), re, cfunc(fi, se), nil);
}

val plusv(struct args *nlist)
{
  return nary_op(plus, nlist, zero);
}

val minusv(val minuend, struct args *nlist)
{
  cnum index = 0;
  val fi;

  if (!args_more(nlist, index))
    return neg(minuend);

  fi = args_get(nlist, &index);

  if (!args_more(nlist, index))
    return minus(minuend, fi);

  return reduce_left(func_n2(minus), args_get_list(nlist),
                     minus(minuend, fi), nil);
}

val mulv(struct args *nlist)
{
  return nary_op(mul, nlist, one);
}

val divv(val dividend, struct args *nlist)
{
  cnum index = 0;
  val fi;

  if (!args_more(nlist, index))
    return divi(one, dividend);

  fi = args_get(nlist, &index);

  if (!args_more(nlist, index))
    return divi(dividend, fi);

  return reduce_left(func_n2(divi), args_get_list(nlist),
                     divi(dividend, fi), nil);
}

val logandv(struct args *nlist)
{
  return nary_op(logand, nlist, negone);
}

val logiorv(struct args *nlist)
{
  return nary_op(logior, nlist, zero);
}

val gtv(val first, struct args *rest)
{
  cnum index = 0;

  while (args_more(rest, index)) {
    val elem = args_get(rest, &index);
    if (!gt(first, elem))
      return nil;
    first = elem;
  }

  return t;
}

val ltv(val first, struct args *rest)
{
  cnum index = 0;

  while (args_more(rest, index)) {
    val elem = args_get(rest, &index);
    if (!lt(first, elem))
      return nil;
    first = elem;
  }

  return t;
}

val gev(val first, struct args *rest)
{
  cnum index = 0;

  while (args_more(rest, index)) {
    val elem = args_get(rest, &index);
    if (!ge(first, elem))
      return nil;
    first = elem;
  }

  return t;
}

val lev(val first, struct args *rest)
{
  cnum index = 0;

  while (args_more(rest, index)) {
    val elem = args_get(rest, &index);
    if (!le(first, elem))
      return nil;
    first = elem;
  }

  return t;
}

val numeqv(val first, struct args *rest)
{
  cnum index = 0;

  while (args_more(rest, index)) {
    val elem = args_get(rest, &index);
    if (!numeq(first, elem))
      return nil;
    first = elem;
  }

  return t;
}

val numneqv(struct args *args)
{
  val i, j;
  val list = args_get_list(args);

  for (i = list; i; i = cdr(i))
    for (j = cdr(i); j; j = cdr(j))
      if (numeq(car(i), car(j)))
        return nil;

  return t;
}

val max2(val a, val b)
{
  return if3(less(a, b), b, a);
}

val min2(val a, val b)
{
  return if3(less(a, b), a, b);
}

val maxv(val first, struct args *rest)
{
  return reduce_left(func_n2(max2), args_get_list(rest), first, nil);
}

val minv(val first, struct args *rest)
{
  return reduce_left(func_n2(min2), args_get_list(rest), first, nil);
}

val maxl(val first, val rest)
{
  args_decl_list(args, ARGS_MIN, rest);
  return maxv(first, args);
}

val minl(val first, val rest)
{
  args_decl_list(args, ARGS_MIN, rest);
  return minv(first, args);
}

val clamp(val low, val high, val num)
{
  return max2(low, min2(high, num));
}

val exptv(struct args *nlist)
{
  return reduce_right(func_n2(expt), args_get_list(nlist), one, nil);
}

val gcdv(struct args *nlist)
{
  if (!args_more(nlist, 0))
    return zero;
  if (!args_two_more(nlist, 0))
    return abso(args_atz(nlist, 0));
  return reduce_left(func_n2(gcd), args_get_list(nlist), colon_k, nil);
}

val lcmv(struct args *nlist)
{
  if (!args_more(nlist, 0))
    return one;
  if (!args_two_more(nlist, 0))
    return abso(args_atz(nlist, 0));
  return reduce_left(func_n2(lcm), args_get_list(nlist), colon_k, nil);
}

val string_own(wchar_t *str)
{
  val obj = make_obj();
  obj->st.type = STR;
  obj->st.str = str;
  obj->st.len = nil;
  obj->st.alloc = nil;
  return obj;
}

val string(const wchar_t *str)
{
  val obj = make_obj();
  obj->st.type = STR;
  obj->st.str = coerce(wchar_t *, chk_strdup(str));
  obj->st.len = nil;
  obj->st.alloc = nil;
  return obj;
}

val string_utf8(const char *str)
{
  val obj = make_obj();
  obj->st.type = STR;
  obj->st.str = utf8_dup_from(str);
  obj->st.len = nil;
  obj->st.alloc = nil;
  return obj;
}

val string_8bit(const unsigned char *str)
{
  size_t l = strlen(coerce(const char *, str)), i;
  wchar_t *wstr = chk_wmalloc(l + 1);
  for (i = 0; i <= l; i++)
    wstr[i] = str[i];
  return string_own(wstr);
}

val string_8bit_size(const unsigned char *str, size_t sz)
{
  size_t i;
  wchar_t *wstr = chk_wmalloc(sz + 1);
  for (i = 0; i < sz; i++)
    wstr[i] = str[i];
  wstr[i] = 0;
  return string_own(wstr);
}

val mkstring(val len, val ch_in)
{
  size_t l = if3(minusp(len),
                 (uw_throwf(error_s, lit("mkstring: negative size ~s specified"),
                            len, nao), 0),
                 c_num(len));
  wchar_t *str = chk_wmalloc(l + 1);
  val s = string_own(str);
  val ch = default_arg_strict(ch_in, chr(' '));
  wmemset(str, c_chr(ch), l);
  str[l] = 0;
  s->st.len = len;
  s->st.alloc = plus(len, one);
  return s;
}

val mkustring(val len)
{
  cnum l = if3(minusp(len),
               (uw_throwf(error_s, lit("mkustring: negative size ~s specified"),
                          len, nao), 0),
               c_num(len));
  wchar_t *str = chk_wmalloc(l + 1);
  val s = string_own(str);
  str[l] = 0;
  s->st.len = len;
  s->st.alloc = plus(len, one);
  return s;
}

val init_str(val str, const wchar_t *data)
{
  wmemcpy(str->st.str, data, c_num(str->st.len));
  return str;
}

static val copy_lazy_str(val lstr);

val copy_str(val str)
{
  return if3(lazy_stringp(str),
             copy_lazy_str(str),
             string(c_str(str)));
}

val upcase_str(val str)
{
  val len = length_str(str);
  wchar_t *dst = chk_wmalloc(c_num(len) + 1);
  const wchar_t *src = c_str(str);
  val out = string_own(dst);

  while ((*dst++ = towupper(*src++)))
    ;

  return out;
}

val downcase_str(val str)
{
  val len = length_str(str);
  wchar_t *dst = chk_wmalloc(c_num(len) + 1);
  const wchar_t *src = c_str(str);
  val out = string_own(dst);

  while ((*dst++ = towlower(*src++)))
    ;

  return out;
}

val string_extend(val str, val tail)
{
  type_check(str, STR);
  {
    cnum len = c_num(length_str(str));
    cnum oalloc = c_num(str->st.alloc), alloc = oalloc;
    val needed;
    val room = zero;

    if (stringp(tail))
      needed = length_str(tail);
    else if (chrp(tail))
      needed = one;
    else if (fixnump(tail))
      needed = tail;
    else
      uw_throwf(error_s, lit("string-extend: tail ~s bad type"), str, nao);

    room = num(alloc - len - 1);

    while (gt(needed, room) && alloc < NUM_MAX) {
      if (alloc > NUM_MAX / 2) {
        alloc = NUM_MAX;
      } else {
        alloc *= 2;
      }
      room = num(alloc - len - 1);
    }

    if (gt(needed, room))
      uw_throw(error_s, lit("string-extend: overflow"));

    if (alloc != oalloc) {
      str->st.str = coerce(wchar_t *, chk_grow_vec(coerce(mem_t *, str->st.str),
                                                   oalloc, alloc,
                                                   sizeof *str->st.str));
      set(mkloc(str->st.alloc, str), num(alloc));
    }

    set(mkloc(str->st.len, str), plus(str->st.len, needed));

    if (stringp(tail)) {
      wmemcpy(str->st.str + len, c_str(tail), c_num(needed) + 1);
    } else if (chrp(tail)) {
      str->st.str[len] = c_chr(tail);
      str->st.str[len + 1] = 0;
    }
  }

  return str;
}

val stringp(val str)
{
  switch (type(str)) {
  case LIT:
  case STR:
  case LSTR:
    return t;
  default:
    return nil;
  }
}

val lazy_stringp(val str)
{
  return type(str) == LSTR ? t : nil;
}

val length_str(val str)
{
  if (tag(str) == TAG_LIT) {
    return num(wcslen(c_str(str)));
  } else {
    type_check2 (str, STR, LSTR);

    if (str->ls.type == LSTR) {
      lazy_str_force(str);
      return length_str(str->ls.prefix);
    }

    if (!str->st.len) {
      set(mkloc(str->st.len, str), num(wcslen(str->st.str)));
      set(mkloc(str->st.alloc, str), plus(str->st.len, one));
    }
    return str->st.len;
  }
}

const wchar_t *c_str(val obj)
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

val search_str(val haystack, val needle, val start_num, val from_end)
{
  from_end = default_null_arg(from_end);
  start_num = default_arg(start_num, zero);

  if (length_str_lt(haystack, start_num)) {
    return nil;
  } else {
    val h_is_lazy = lazy_stringp(haystack);
    cnum start = c_num(start_num);
    cnum good = -1, pos = -1;
    const wchar_t *n = c_str(needle), *h;

    if (!h_is_lazy) {
      h = c_str(haystack);

      if (start < 0)
        start += wcslen(h);

    nonlazy:
      do {
        const wchar_t *f = wcsstr(h + start, n);

        if (f)
          pos = f - h;
        else
          pos = -1;
      } while (pos != -1 && (good = pos) != -1 && from_end && h[start++]);
    } else {
      size_t ln = c_num(length_str(needle));

      if (start < 0) {
        lazy_str_force(haystack);
        h = c_str(haystack->ls.prefix);
        start += wcslen(h);
        goto nonlazy;
      }

      do {
        lazy_str_force_upto(haystack, plus(num(start + 1), length_str(needle)));
        h = c_str(haystack->ls.prefix);

        if (!wcsncmp(h + start, n, ln))
          good = start;
      } while (h[start++] && (from_end || good == -1));
    }
    return (good == -1) ? nil : num(good);
  }
}

val search_str_tree(val haystack, val tree, val start_num, val from_end)
{
  if (stringp(tree)) {
    val result = search_str(haystack, tree, start_num, from_end);
    if (result)
      return cons(result, length_str(tree));
  } else if (consp(tree)) {
    val it = nil, minpos = nil, maxlen = nil;

    for (; tree; tree = cdr(tree)) {
      val result = search_str_tree(haystack, car(tree), start_num, from_end);
      if (result) {
        cons_bind (pos, len, result);
        if (!it || lt(pos, minpos) || (pos == minpos && gt(len, maxlen))) {
          minpos = pos;
          maxlen = len;
          it = result;
        }
      }
    }

    return it;
  }

  return nil;
}

val match_str(val bigstr, val str, val pos)
{
  val i, p;

  pos = default_arg(pos, zero);

  if (ge(pos, zero)) {
    for (i = zero;
         length_str_gt(bigstr, p = plus(pos, i)) && length_str_gt(str, i);
         i = plus(i, one))
    {
      if (chr_str(bigstr, p) != chr_str(str, i))
        return nil;
    }

    return length_str_le(str, i) ? t : nil;
  } else {
    pos = plus(pos, length(bigstr));
    pos = plus(minus(pos, length(str)), one);

    for (i = minus(length(str), one);
         ge(i, zero) && ge(p = plus(pos, i), zero);
         i = minus(i, one))
    {
      if (chr_str(bigstr, p) != chr_str(str, i))
        return nil;
    }

    return lt(i, zero) ? t : nil;
  }
}

val match_str_tree(val bigstr, val tree, val pos)
{
  pos = default_arg(pos, zero);

  if (stringp(tree)) {
    if (match_str(bigstr, tree, pos))
      return length_str(tree);
  } else if (consp(tree)) {
    val maxlen = nil;

    for (; tree; tree = cdr(tree)) {
      val result = match_str_tree(bigstr, car(tree), pos);
      if (result && (!maxlen || gt(result, maxlen)))
        maxlen = result;
    }

    return maxlen;
  }

  return nil;
}

static val lazy_sub_str(val lstr, val from, val to)
{
  val len = nil;
  val len_pfx = length_str(lstr->ls.prefix);

  if (null_or_missing_p(from)) {
    from = zero;
  } else if (from == t) {
    return null_string;
  } else {
    if (lt(from, zero)) {
      from = plus(from, len = length_str(lstr));
      from = max2(zero, from);

      if (to == zero)
        to = t;
    }

    if (ge(from, len_pfx)) {
      if (!lazy_str_force_upto(lstr, from))
        return null_string;
    }
  }

  if (null_or_missing_p(to) || to == t) {
    to = t;
  } else {
    if (lt(to, zero)) {
      to = plus(to, len = length_str(lstr));
      to = max(zero, to);
    }

    if (ge(to, len_pfx))
      if (!lazy_str_force_upto(lstr, minus(to, one)))
        to = t;
  }

  {
    val pfxsub = sub_str(lstr->ls.prefix, from, to);

    if (to != t) {
      return pfxsub;
    } else {
      val lsub = make_obj();
      lsub->ls.type = LSTR;
      lsub->ls.prefix = pfxsub;
      lsub->ls.list = lstr->ls.list;
      lsub->ls.props = coerce(struct lazy_string_props *,
                              chk_copy_obj(coerce(mem_t *, lstr->ls.props),
                                           sizeof *lstr->ls.props));
      return lsub;
    }
  }
}

val sub_str(val str_in, val from, val to)
{
  val len = nil;

  if (lazy_stringp(str_in))
    return lazy_sub_str(str_in, from, to);

  len = length_str(str_in);

  if (null_or_missing_p(from))
    from = zero;
  else if (from == t)
    return null_string;
  else if (lt(from, zero)) {
    from = plus(from, len);
    if (to == zero)
      to = len;
  }

  if (null_or_missing_p(to) || to == t)
    to = len;
  else if (lt(to, zero))
    to = plus(to, len);

  from = max2(zero, min2(from, len));
  to = max2(zero, min2(to, len));

  if (ge(from, to)) {
    return null_string;
  } else {
    size_t nchar = c_num(to) - c_num(from) + 1;
    wchar_t *sub = chk_wmalloc(nchar);
    const wchar_t *str = c_str(str_in);
    wcsncpy(sub, str + c_num(from), nchar);
    sub[nchar-1] = 0;
    return string_own(sub);
  }
}

val replace_str(val str_in, val items, val from, val to)
{
  val itseq = toseq(items);
  val len = length_str(str_in);

  if (type(str_in) != STR) {
    uw_throwf(error_s, lit("replace-str: ~s of type ~s is not "
                           "a modifiable string"),
              str_in, typeof(str_in), nao);
  }

  if (listp(from)) {
    val where = from;
    val len = length_str(str_in);

    if (!missingp(to))
      uw_throwf(error_s,
                lit("replace-str: to-arg not applicable when from-arg is a list"),
                nao);

    for (; where && itseq; where = cdr(where)) {
      val wh = car(where);
      if (ge(wh, len))
        break;
      chr_str_set(str_in, wh, pop(&itseq));
    }

    return str_in;
  } else if (vectorp(from)) {
    val where = from;
    val len = length_str(str_in);
    val wlen = length_vec(from);
    val i;

    if (!missingp(to))
      uw_throwf(error_s,
                lit("replace-str: to-arg not applicable when from-arg is a vector"),
                nao);

    for (i = zero; lt(i, wlen) && itseq; i = plus(i, one)) {
      val wh = vecref(where, i);
      if (ge(wh, len))
        break;
      chr_str_set(str_in, wh, pop(&itseq));
    }

    return str_in;
  } else if (missingp(from)) {
    from = zero;
  } else if (from == t) {
    from = len;
  } else if (lt(from, zero)) {
    from = plus(from, len);
    if (to == zero)
      to = len;
  }

  if (null_or_missing_p(to) || to == t)
    to = len;
  else if (lt(to, zero))
    to = plus(to, len);

  from = max2(zero, min2(from, len));
  to = max2(zero, min2(to, len));


  {
    val len_rep = minus(to, from);
    val len_it = length(itseq);

    if (gt(len_rep, len_it)) {
      val len_diff = minus(len_rep, len_it);
      cnum t = c_num(to);
      cnum l = c_num(len);

      wmemmove(str_in->st.str + t - c_num(len_diff),
               str_in->st.str + t, (l - t) + 1);
      set(mkloc(str_in->st.len, str_in), minus(len, len_diff));
      to = plus(from, len_it);
    } else if (lt(len_rep, len_it)) {
      val len_diff = minus(len_it, len_rep);
      cnum t = c_num(to);
      cnum l = c_num(len);

      string_extend(str_in, plus(len, len_diff));
      wmemmove(str_in->st.str + t + c_num(len_diff),
               str_in->st.str + t, (l - t) + 1);
      to = plus(from, len_it);
    }

    if (zerop(len_it))
      return str_in;
    if (stringp(itseq)) {
      wmemcpy(str_in->st.str + c_num(from), c_str(itseq), c_num(len_it));
    } else {
      val iter;
      cnum f = c_num(from);
      cnum t = c_num(to);
      cnum s;

      if (listp(itseq)) {
        for (iter = itseq; iter && f != t; iter = cdr(iter), f++)
          str_in->st.str[f] = c_chr(car(iter));
      } else if (vectorp(itseq)) {
        for (s = 0; f != t; f++, s++)
          str_in->st.str[f] = c_chr(vecref(itseq, num(s)));
      } else {
        uw_throwf(error_s, lit("replace-str: source object ~s not supported"),
                  itseq, nao);
      }
    }
  }

  return str_in;
}

val cat_str(val list, val sep)
{
  cnum total = 0;
  val iter;
  wchar_t *str, *ptr;
  cnum len_sep = (!null_or_missing_p(sep)) ? c_num(length_str(sep)) : 0;

  for (iter = list; iter != nil; iter = cdr(iter)) {
    val item = car(iter);
    if (!item)
      continue;
    if (stringp(item)) {
      cnum ntotal = total + c_num(length_str(item));

      if (len_sep && cdr(iter))
        ntotal += len_sep;

      if (ntotal < total)
        goto oflow;

      total = ntotal;

      continue;
    }
    if (chrp(item)) {
      cnum ntotal = total + 1;

      if (len_sep && cdr(iter))
        ntotal += len_sep;

      if (ntotal < total)
        goto oflow;

      total = ntotal;

      continue;
    }
    uw_throwf(error_s, lit("cat-str: ~s is not a character or string"),
              item, nao);
  }

  str = chk_wmalloc(total + 1);

  for (ptr = str, iter = list; iter != nil; iter = cdr(iter)) {
    val item = car(iter);

    if (!item)
      continue;
    if (stringp(item)) {
      cnum len = c_num(length_str(item));
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

oflow:
  uw_throwf(error_s, lit("cat-str: string length overflow"), nao);
}

static val vscat(val sep, va_list vl1, va_list vl2)
{
  cnum total = 0;
  val item, next;
  wchar_t *str, *ptr;
  cnum len_sep = (!null_or_missing_p(sep)) ? c_num(length_str(sep)) : 0;

  for (item = va_arg(vl1, val); item != nao; item = next)
  {
    next = va_arg(vl1, val);

    if (stringp(item)) {
      cnum ntotal = total + c_num(length_str(item));

      if (len_sep && next != nao)
        ntotal += len_sep;

      if (ntotal < total)
        goto oflow;

      total = ntotal;

      continue;
    }
    if (chrp(item)) {
      cnum ntotal = total + 1;

      if (len_sep && next != nao)
        ntotal += len_sep;

      if (ntotal < total)
        goto oflow;

      total = ntotal;

      continue;
    }
    uw_throwf(error_s, lit("scat: ~s is not a character or string"),
              item, nao);
  }

  str = chk_wmalloc(total + 1);

  for (ptr = str, item = va_arg(vl2, val); item != nao; item = next)
  {
    next = va_arg(vl2, val);

    if (stringp(item)) {
      cnum len = c_num(length_str(item));
      wmemcpy(ptr, c_str(item), len);
      ptr += len;
    } else {
      *ptr++ = c_chr(item);
    }

    if (len_sep && next != nao) {
      wmemcpy(ptr, c_str(sep), len_sep);
      ptr += len_sep;
    }
  }
  *ptr = 0;

  return string_own(str);

oflow:
  uw_throwf(error_s, lit("scat: string length overflow"), nao);
}

val scat(val sep, ...)
{
  va_list vl1, vl2;
  val ret;
  va_start (vl1, sep);
  va_start (vl2, sep);
  ret = vscat(sep, vl1, vl2);
  va_end (vl1);
  va_end (vl2);
  return ret;
}

val split_str_keep(val str, val sep, val keep_sep)
{
  keep_sep = default_null_arg(keep_sep);

  if (regexp(sep)) {
    list_collect_decl (out, iter);
    val pos = zero;
    val slen = length(str);

    for (;;) {
      cons_bind (new_pos, len, search_regex(str, sep, pos, nil));

      if (len == zero && new_pos != slen)
        new_pos = plus(new_pos, one);

      iter = list_collect(iter, sub_str(str, pos, new_pos));
      pos = new_pos;

      if (len && pos != slen) {
        pos = plus(pos, len);
        if (keep_sep)
          iter = list_collect(iter, sub_str(str, new_pos, pos));
        continue;
      }
      break;
    }

    return out;
  } else {
    size_t len_sep = c_num(length_str(sep));

    if (len_sep == 0) {
      if (opt_compat && opt_compat <= 100) {
        return list_str(str);
      } else {
        const wchar_t *cstr = c_str(str);

        if (*cstr) {
          list_collect_decl (out, iter);

          for (; *cstr; cstr++) {
            val piece = mkustring(one);
            init_str(piece, cstr);
            iter = list_collect(iter, piece);
            if (keep_sep && *(cstr+1))
              iter = list_collect(iter, null_string);
          }

          gc_hint(str);

          return out;
        } else {
          return cons(str, nil);
        }
      }
    } else {
      const wchar_t *cstr = c_str(str);
      const wchar_t *csep = c_str(sep);

      list_collect_decl (out, iter);

      for (;;) {
        const wchar_t *psep = wcsstr(cstr, csep);
        size_t span = (psep != 0) ? psep - cstr : wcslen(cstr);
        val piece = mkustring(num(span));
        init_str(piece, cstr);
        iter = list_collect(iter, piece);
        cstr += span;
        if (psep != 0) {
          cstr += len_sep;
          if (keep_sep)
            iter = list_collect(iter, sep);
          continue;
        }
        break;
      }

      gc_hint(sep);
      gc_hint(str);

      return out;
    }
  }
}

val split_str(val str, val sep)
{
  return split_str_keep(str, sep, nil);
}

val split_str_set(val str, val set)
{
  const wchar_t *cstr = c_str(str);
  const wchar_t *cset = c_str(set);
  list_collect_decl (out, iter);

  for (;;) {
    size_t span = wcscspn(cstr, cset);
    val piece = mkustring(num(span));
    init_str(piece, cstr);
    iter = list_collect(iter, piece);
    cstr += span;
    if (*cstr) {
      cstr++;
      continue;
    }
    break;
  }

  gc_hint(set);
  gc_hint(str);

  return out;
}

val tok_str(val str, val tok_regex, val keep_sep)
{
  list_collect_decl (out, iter);
  val pos = zero;
  val last_end = zero;
  val slen = length(str);
  int prev_empty = 1;

  keep_sep = default_null_arg(keep_sep);

  if (opt_compat && opt_compat <= 155) for (;;) {
    cons_bind (new_pos, len, search_regex(str, tok_regex, pos, nil));

    if (len == zero && new_pos != slen)
      new_pos = plus(new_pos, one);

    if (new_pos == slen || !len) {
      if (keep_sep)
        iter = list_collect(iter, sub_str(str, pos, t));
      break;
    }

    if (keep_sep)
      iter = list_collect(iter, sub_str(str, pos, new_pos));

    pos = plus(new_pos, len);
    iter = list_collect(iter, sub_str(str, new_pos, pos));
  } else for (;;) {
    cons_bind (new_pos, len, search_regex(str, tok_regex, pos, nil));

    if (!len || (new_pos == slen && !prev_empty)) {
      if (keep_sep)
        iter = list_collect(iter, sub_str(str, last_end, t));
      break;
    }

    if (len != zero || prev_empty) {
      if (keep_sep)
        iter = list_collect(iter, sub_str(str, last_end, new_pos));
      last_end = plus(new_pos, len);
      iter = list_collect(iter, sub_str(str, new_pos, last_end));
      prev_empty = (len == zero);
    } else {
      prev_empty = 1;
    }

    pos = plus(new_pos, len);

    if (len == zero)
      pos = succ(pos);
  }

  return out;
}

val tok_where(val str, val tok_regex)
{
  list_collect_decl (out, iter);
  val pos = zero;
  int prev_empty = 1;

  if (opt_compat && opt_compat <= 155) for (;;) {
    val range = range_regex(str, tok_regex, pos, nil);

    if (range) {
      range_bind (match_start, match_end, range);

      iter = list_collect(iter, range);

      pos = match_end;

      if (match_end == match_start)
        pos = plus(pos, one);
      continue;
    }

    break;
  } else for (;;) {
    val range = range_regex(str, tok_regex, pos, nil);

    if (range) {
      range_bind (match_start, match_end, range);

      if (match_end != match_start || prev_empty) {
        iter = list_collect(iter, range);
        prev_empty = (match_end == match_start);
      } else {
        prev_empty = 1;
      }

      pos = match_end;

      if (match_end == match_start)
        pos = plus(pos, one);
      continue;
    }

    break;
  }

  return out;
}

val list_str(val str)
{
  const wchar_t *cstr = c_str(str);
  list_collect_decl (out, iter);

  while (*cstr)
    iter = list_collect(iter, chr(*cstr++));

  gc_hint(str);

  return out;
}

val trim_str(val str)
{
  const wchar_t *start = c_str(str);
  const wchar_t *end = start + c_num(length_str(str));

  if (opt_compat && opt_compat <= 148) {
    while (start[0] && iswspace(start[0]))
      start++;

    while (end > start && iswspace(end[-1]))
      end--;
  } else {
    while (start[0] && wcschr(L" \t\n", start[0]))
      start++;

    while (end > start && wcschr(L" \t\n", end[-1]))
      end--;
  }

  if (end == start) {
    return null_string;
  } else {
    size_t len = end - start;
    wchar_t *buf = chk_wmalloc(len + 1);
    wmemcpy(buf, start, len);
    buf[len] = 0;
    return string_own(buf);
  }
}

val cmp_str(val astr, val bstr)
{
   switch (TYPE_PAIR(type(astr), type(bstr))) {
   case TYPE_PAIR(LIT, LIT):
   case TYPE_PAIR(STR, STR):
   case TYPE_PAIR(LIT, STR):
   case TYPE_PAIR(STR, LIT):
     return num_fast(wcscmp(c_str(astr), c_str(bstr)));
   case TYPE_PAIR(LSTR, LIT):
   case TYPE_PAIR(LSTR, STR):
   case TYPE_PAIR(LIT, LSTR):
   case TYPE_PAIR(STR, LSTR):
   case TYPE_PAIR(LSTR, LSTR):
     {
       val i;
       for (i = zero;
            length_str_lt(astr, i) && length_str_lt(bstr, i);
            i = plus(i, one))
       {
         val ach = chr_str(astr, i);
         val bch = chr_str(bstr, i);

         if (ach < bch)
           return one;
         else if (ach < bch)
           return one;
       }
       if (length_str_lt(bstr, i))
         return negone;
       if (length_str_lt(astr, i))
         return negone;
       return zero;
     }
   default:
     uw_throwf(error_s, lit("cmp-str: invalid operands ~s ~s"),
               astr, bstr, nao);
  }
}

val str_eq(val astr, val bstr)
{
  return if2(cmp_str(astr, bstr) == zero, t);
}

val str_lt(val astr, val bstr)
{
  return if2(cmp_str(astr, bstr) == negone, t);
}

val str_gt(val astr, val bstr)
{
  return if2(cmp_str(astr, bstr) == one, t);
}

val str_le(val astr, val bstr)
{
  val cmp = cmp_str(astr, bstr);
  return if2(cmp == zero || cmp == negone, t);
}

val str_ge(val astr, val bstr)
{
  val cmp = cmp_str(astr, bstr);
  return if2(cmp == zero || cmp == one, t);
}

val int_str(val str, val base)
{
  const wchar_t *wcs = c_str(str);
  wchar_t *ptr;
  long value;
  cnum b = c_num(default_arg(base, num_fast(10)));
  int zerox = 0, octzero = 0, minus = 0, flip = 0;

  switch (wcs[0]) {
  case '-':
    minus = 1;
    /* fallthrough */
  case '+':
    switch (wcs[1]) {
    case '0':
      switch (wcs[2]) {
      case 'x': case 'X':
        zerox = 1;
        wcs += 3;
        flip = minus;
        break;
      default:
        octzero = 1;
        break;
      }
    }
    break;
  case '0':
    switch (wcs[1]) {
    case 'x': case 'X':
      zerox = 1;
      wcs += 2;
    default:
      octzero = 1;
    }
    break;
  }

  if (base == chr('c')) {
    b = (zerox ? 16 : (octzero ? 8 : 10));
  } else if (b == 16) {
    /* If base is 16, strtoul and its siblings
       still recognize the 0x prefix. We don't want that;
       except if base is the character #\c. Otherwise,
       it is a zero with trailing junk. */
    if (zerox)
      return zero;
  } else if (b < 2 || b > 36) {
     uw_throwf(error_s, lit("int-str: invalid base ~s"), base, nao);
  }

  /* TODO: detect if we have wcstoll */
  value = wcstol(wcs, &ptr, b);

  if (value == 0 && ptr == wcs)
    return nil;

  if ((value == LONG_MAX || value == LONG_MIN) && errno == ERANGE) {
    val bignum = make_bignum();
    size_t size;
    unsigned char *ucs = utf8_dup_to_buf(wcs, &size, 1);
    mp_err err = mp_read_radix(mp(bignum), ucs, b);

    free(ucs); /* TODO: make wchar_t version of mp_read_radix. */
    gc_hint(str);

    if (err != MP_OKAY)
      return nil;

    if (flip)
      mp_neg(mp(bignum), mp(bignum));
    /* If wcstol overflowed, but the range of long is smaller than
       that of fixnums, that means that the value might not
       actually be a bignum, and so we must normalize.
       We do not need this on our usual target platforms, where NUM_MAX is
       never larger than LONG_MAX. */
    return (LONG_MAX < NUM_MAX) ? normalize(bignum) : bignum;
  }

  if (flip)
    value = -value;

  if (value >= NUM_MIN && value <= NUM_MAX)
    return num(value);

  return bignum_from_long(value);
}

val flo_str(val str)
{
  const wchar_t *wcs = c_str(str);
  wchar_t *ptr;

  /* TODO: detect if we have wcstod */
  double value = wcstod(wcs, &ptr);
  if (value == 0 && ptr == wcs)
    return nil;
  if ((value == HUGE_VAL || value == -HUGE_VAL) && errno == ERANGE)
    return nil;
  return flo(value);
}

val num_str(val str)
{
  const wchar_t *wcs = c_str(str);
  const wchar_t *nws = wcs + wcsspn(wcs, L"\f\n\r\t\v");
  const wchar_t *dig = nws + wcsspn(wcs, L"+-");

  if (wcsspn(dig, L"0123456789") == wcsspn(dig, L"0123456789eE."))
    return int_str(str, nil);
  return flo_str(str);
}

enum less_handling {
  less_false,
  less_true,
  less_compare,
  less_cannot,
};

static enum less_handling less_tab[MAXTYPE+1][MAXTYPE+1];

static void less_tab_init(void)
{
  int l, r;
  static int type_prec[MAXTYPE+1];

  type_prec[NIL] = 4;
  type_prec[NUM] = 1;
  type_prec[CHR] = 1;
  type_prec[LIT] = 3;
  type_prec[CONS] = 5;
  type_prec[STR] = 3;
  type_prec[SYM] = 4;
  type_prec[LCONS] = 5;
  type_prec[LSTR] = 3;
  type_prec[BGNUM] = 1;
  type_prec[FLNUM] = 1;
  type_prec[RNG] = 2;

  for (l = 0; l <= MAXTYPE; l++)
    for (r = 0; r <= MAXTYPE; r++) {
      int l_prec = type_prec[l];
      int r_prec = type_prec[r];

      if (l_prec == 0 || r_prec == 0)
        less_tab[l][r] = less_cannot;
      else if (l_prec == r_prec)
        less_tab[l][r] = less_compare;
      else if (l_prec < r_prec)
        less_tab[l][r] = less_true;
    }
}

val less(val left, val right)
{
  type_t l_type, r_type;

  if (left == right)
    return nil;

tail:
  l_type = type(left);
  r_type = type(right);

  if (l_type == COBJ && left->co.ops->equalsub) {
    val lsub = left->co.ops->equalsub(left);
    if (lsub) {
      left = lsub;
      goto tail;
    }
  }

  if (r_type == COBJ && right->co.ops->equalsub) {
    val rsub = right->co.ops->equalsub(right);
    if (rsub) {
      right = rsub;
      goto tail;
    }
  }

  switch (less_tab[l_type][r_type]) {
  case less_false:
    return nil;
  case less_true:
    return t;
  case less_compare:
    break;
  case less_cannot:
    uw_throwf(type_error_s, lit("less: cannot compare ~s and ~s"),
              left, right, nao);
  }

  switch (l_type) {
  case NUM:
  case CHR:
  case BGNUM:
  case FLNUM:
    return lt(left, right);
  case LIT:
  case STR:
  case LSTR:
    return str_lt(left, right);
  case NIL:
    return str_lt(nil_string, symbol_name(right));
  case SYM:
    return str_lt(left->s.name, symbol_name(right));
  case CONS:
  case LCONS:
    for (;;) {
      val carl = car(left);
      val carr = car(right);

      if (less(carl, carr))
        return t;

      if (equal(carl, carr)) {
        val cdrl = cdr(left);
        val cdrr = cdr(right);

        if (consp(cdrl) && consp(cdrr)) {
          left = cdrl;
          right = cdrr;
          continue;
        }

        return less(cdrl, cdrr);
      }
      break;
    }
    return nil;
  case VEC:
    {
      cnum i;
      cnum lenl = c_num(length_vec(left));
      cnum lenr = c_num(length_vec(right));
      cnum len = min(lenl, lenr);

      for (i = 0; i < len; i++) {
        val litem = vecref(left, num_fast(i));
        val ritem = vecref(right, num_fast(i));

        if (less(litem, ritem))
          return t;

        if (!equal(litem, ritem))
          return nil;
      }

      return tnil(lenl < lenr);
    }
  case RNG:
    {
      val fl = from(left);
      val fr = from(right);

      if (less(fl, fr))
        return t;

      if (equal(fl, fr))
        return less(to(left), to(right));

      return nil;
    }
  default:
    internal_error("unhandled case in less function");
  }
}

val greater(val left, val right)
{
  return less(right, left);
}

val lequal(val left, val right)
{
  uses_or2;
  return or2(equal(left, right), less(left, right));
}

val gequal(val left, val right)
{
  uses_or2;
  return or2(equal(left, right), less(right, left));
}

val lessv(val first, struct args *rest)
{
  cnum index = 0;

  while (args_more(rest, index)) {
    val elem = args_get(rest, &index);
    if (!less(first, elem))
      return nil;
    first = elem;
  }

  return t;
}

val greaterv(val first, struct args *rest)
{
  cnum index = 0;

  while (args_more(rest, index)) {
    val elem = args_get(rest, &index);
    if (!less(elem, first))
      return nil;
    first = elem;
  }

  return t;
}

val lequalv(val first, struct args *rest)
{
  cnum index = 0;

  while (args_more(rest, index)) {
    val elem = args_get(rest, &index);
    if (!equal(first, elem) && !less(first, elem))
      return nil;
    first = elem;
  }

  return t;
}

val gequalv(val first, struct args *rest)
{
  cnum index = 0;

  while (args_more(rest, index)) {
    val elem = args_get(rest, &index);
    if (!equal(first, elem) && !less(elem, first))
      return nil;
    first = elem;
  }

  return t;
}


val chrp(val chr)
{
  return (is_chr(chr)) ? t : nil;
}

wchar_t c_chr(val chr)
{
  if (!is_chr(chr))
    type_mismatch(lit("~s is not a character"), chr, nao);
  return convert(wchar_t, coerce(cnum, chr) >> TAG_SHIFT);
}

val chr_isalnum(val ch)
{
  return tnil(iswalnum(c_chr(ch)));
}

val chr_isalpha(val ch)
{
  return tnil(iswalpha(c_chr(ch)));
}

val chr_isascii(val ch)
{
  return tnil(c_chr(ch) >= 0 && c_chr(ch) < 128);
}

val chr_iscntrl(val ch)
{
  return tnil(iswcntrl(c_chr(ch)));
}

val chr_isdigit(val ch)
{
  return if2(iswdigit(c_chr(ch)), t);
}

val chr_digit(val ch)
{
  return if2(iswdigit(c_chr(ch)), minus(ch, chr('0')));
}

val chr_isgraph(val ch)
{
  return tnil(iswgraph(c_chr(ch)));
}

val chr_islower(val ch)
{
  return tnil(iswlower(c_chr(ch)));
}

val chr_isprint(val ch)
{
  return tnil(iswprint(c_chr(ch)));
}

val chr_ispunct(val ch)
{
  return tnil(iswpunct(c_chr(ch)));
}

val chr_isspace(val ch)
{
  return tnil(iswspace(c_chr(ch)));
}

val chr_isblank(val ch)
{
  return tnil(ch == chr(' ') || ch == chr('\t'));
}

val chr_isunisp(val ch)
{
  return tnil(wcschr(spaces, c_chr(ch)));
}

val chr_isupper(val ch)
{
  return tnil(iswupper(c_chr(ch)));
}

val chr_isxdigit(val ch)
{
  return tnil(iswxdigit(c_chr(ch)));
}

val chr_xdigit(val ch)
{
  wchar_t cc = c_chr(ch);

  if ('0' <= cc && cc <= '9')
    return num_fast(cc - '0');

  if ('A' <= cc && cc <= 'F')
    return num_fast(cc - 'A' + 10);

  if ('a' <= cc && cc <= 'a')
    return num_fast(cc - 'a' + 10);

  return nil;
}

val chr_toupper(val ch)
{
  return chr(towupper(c_chr(ch)));
}

val chr_tolower(val ch)
{
  return chr(towlower(c_chr(ch)));
}

val int_chr(val ch)
{
  return num_fast(c_chr(ch));
}

val chr_int(val num)
{
  cnum n = c_num(num);
  if (n < 0 || n > 0x10FFFF)
    uw_throwf(numeric_error_s,
              lit("chr-num: ~s is out of character range"), num, nao);
  return chr(n);
}

val chr_str(val str, val ind)
{
  cnum index = c_num(ind);

  if (index < 0) {
    ind = plus(length_str(str), ind);
    index = c_num(ind);
  }

  if (index < 0 || !length_str_gt(str, ind))
    uw_throwf(error_s, lit("chr-str: ~s is out of range for string ~s"),
              ind, str, nao);

  if (lazy_stringp(str)) {
    lazy_str_force_upto(str, ind);
    return chr(c_str(str->ls.prefix)[index]);
  } else {
    return chr(c_str(str)[index]);
  }
}

val chr_str_set(val str, val ind, val chr)
{
  cnum index = c_num(ind);

  if (is_lit(str)) {
    uw_throwf(error_s, lit("chr-str-set: cannot modify literal string ~s"),
              str, nao);
  }

  if (index < 0) {
    ind = plus(length_str(str), ind);
    index = c_num(ind);
  }

  if (index < 0 || !length_str_gt(str, ind))
    uw_throwf(error_s, lit("chr-str-set: ~s is out of range for string ~s"),
              ind, str, nao);


  if (lazy_stringp(str)) {
    lazy_str_force_upto(str, ind);
    str->ls.prefix->st.str[index] = c_chr(chr);
  } else {
    str->st.str[index] = c_chr(chr);
  }

  return chr;
}

val span_str(val str, val set)
{
  const wchar_t *cstr = c_str(str);
  const wchar_t *cset = c_str(set);
  size_t span = wcsspn(cstr, cset);
  return num(span);
}

val compl_span_str(val str, val set)
{
  const wchar_t *cstr = c_str(str);
  const wchar_t *cset = c_str(set);
  size_t span = wcscspn(cstr, cset);
  return num(span);
}

val break_str(val str, val set)
{
  const wchar_t *cstr = c_str(str);
  const wchar_t *cset = c_str(set);
  const wchar_t *brk = wcspbrk(cstr, cset);
  if (!brk)
    return nil;
  return num(brk - cstr);
}

val symbol_name(val sym)
{
  if (sym)
    type_check(sym, SYM);
  return sym ? sym->s.name : nil_string;
}

static void symbol_setname(val sym, val name)
{
  type_check(sym, SYM);
  sym->s.name = name;
}

val symbol_package(val sym)
{
  if (sym == nil)
    return user_package_var;
  type_check(sym, SYM);
  return sym->s.package;
}

val make_sym(val name)
{
  if (t && !stringp(name)) {
    uw_throwf(error_s, lit("make-sym: name ~s isn't a string"), name, nao);
  } else {
    val obj = make_obj();
    obj->s.type = SYM;
    obj->s.name = name;
    obj->s.package = nil;
    obj->s.slot_cache = 0;
    return obj;
  }
}

val gensym(val prefix)
{
  prefix = default_arg(prefix, lit("g"));
  loc gs_loc = lookup_var_l(nil, gensym_counter_s);
  val name = format(nil, lit("~a~,04d"), prefix,
                    set(gs_loc, plus(deref(gs_loc), one)), nao);
  return make_sym(name);
}

val make_package(val name)
{
  if (find_package(name)) {
    uw_throwf(error_s, lit("make-package: ~s exists already"), name, nao);
  } else if (t && !stringp(name)) {
    uw_throwf(error_s, lit("make-package: name ~s isn't a string"), name, nao);
  } else if (length(name) == zero) {
    uw_throwf(error_s, lit("make-package: package name can't be empty string"),
              nao);
  } else {
    val obj = make_obj();
    obj->pk.type = PKG;
    obj->pk.name = name;
    obj->pk.symhash = nil; /* make_hash calls below could trigger gc! */
    obj->pk.hidhash = nil;
    obj->pk.symhash = make_hash(nil, nil, lit("t")); /* don't have t yet! */
    obj->pk.hidhash = make_hash(nil, nil, lit("t"));

    mpush(cons(name, obj), cur_package_alist_loc);
    return obj;
  }
}

val packagep(val obj)
{
  return type(obj) == PKG ? t : nil;
}

static val lookup_package(val name)
{
  return cdr(assoc(name, deref(cur_package_alist_loc)));
}

val find_package(val package)
{
  if (stringp(package) || (symbolp(package) &&
                           (package = symbol_name(package))))
    return lookup_package(package);
  return nil;
}

static val get_package(val fname, val package, val missing_ok)
{
  if (missing_ok && null_or_missing_p(package))
    return cur_package;
  if (stringp(package) || (symbolp(package) &&
                           (package = symbol_name(package))))
  {
    val p = lookup_package(package);
    if (!p)
      uw_throwf(error_s, lit("~a: no such package: ~s"), fname, package, nao);
    return p;
  }
  type_check (package, PKG);
  return package;
}

val delete_package(val package_in)
{
  val package = get_package(lit("delete-package"), package_in, nil);
  val iter;
  loc cpll = cur_package_alist_loc;
  set(cpll, alist_nremove1(deref(cpll), package->pk.name));
  for (iter = deref(cpll); iter; iter = cdr(iter))
    unuse_package(package, cdar(iter));
  return nil;
}

val package_alist(void)
{
  return deref(cur_package_alist_loc);
}

val package_name(val package)
{
  type_check (package, PKG);
  return package->pk.name;
}

val package_symbols(val package_in)
{
  val package = get_package(lit("package-symbols"), package_in, t);
  return hash_values(package->pk.symhash);
}

val package_local_symbols(val package_in)
{
  val package = get_package(lit("package-local-symbols"), package_in, t);
  list_collect_decl (out, ptail);
  val hiter = hash_begin(package->pk.symhash);
  val cell;

  while ((cell = hash_next(hiter))) {
    val sym = cdr(cell);
    if (symbol_package(sym) == package)
      ptail = list_collect(ptail, sym);
  }

  return out;
}

val package_foreign_symbols(val package_in)
{
  val package = get_package(lit("package-foreign-symbols"), package_in, t);
  list_collect_decl (out, ptail);
  val hiter = hash_begin(package->pk.symhash);
  val cell;

  while ((cell = hash_next(hiter))) {
    val sym = cdr(cell);
    if (symbol_package(sym) != package)
      ptail = list_collect(ptail, sym);
  }

  return out;
}

static void prot_sym_check(val func, val symname, val package)
{
  extern val *protected_sym[];
  val **iter = protected_sym;

  if (package == user_package ||
      package == system_package ||
      package == keyword_package)
  {
    val sym = gethash(package->pk.symhash, symname);

    for (; sym && *iter; iter++) {
      if (**iter == sym)
        uw_throwf(error_s,
                  lit("~a: cannot remove built-in symbol ~s "
                      "from ~a package"),
                  func, sym, package_name(package), nao);
    }
  }
}

val use_sym(val symbol, val package_in)
{
  val self = lit("use-sym");
  val package = get_package(self, package_in, t);

  if (symbol_package(symbol) != package) {
    val name = symbol_name(symbol);
    val found;
    val existing = gethash_f(package->pk.symhash, name, mkcloc(found));

    if (found && symbol_package(existing) == package) {
      if (existing == nil)
        uw_throwf(error_s, lit("~a: cannot hide ~s"), self, existing, nao);
      prot_sym_check(self, name, package);
      sethash(package->pk.hidhash, name, existing);
      existing->s.package = nil;
    }

    sethash(package->pk.symhash, name, symbol);
  }

  return symbol;
}

val unuse_sym(val symbol, val package_in)
{
  val self = lit("unuse-sym");
  val package = get_package(self, package_in, t);
  val name = symbol_name(symbol);
  val found_visible, found_hidden;
  val visible = gethash_f(package->pk.symhash, name, mkcloc(found_visible));
  val hidden = gethash_f(package->pk.hidhash, name, mkcloc(found_hidden));

  if (!found_visible || visible != symbol)
    return nil;

  if (symbol_package(symbol) == package)
    uw_throwf(error_s, lit("~a: cannot unuse ~s from its home package ~s"),
              self, symbol, package, nao);

  if (found_hidden) {
    remhash(package->pk.hidhash, name);
    sethash(package->pk.symhash, name, hidden);
    set(mkloc(hidden->s.package, hidden), package);
    return hidden;
  }

  remhash(package->pk.symhash, name);
  return symbol;
}

static val resolve_package_designators(val fname, val designator_list)
{
  if (consp(designator_list)) {
    list_collect_decl (out, ptail);

    for (; designator_list; designator_list = cdr(designator_list))
      ptail = list_collect(ptail, get_package(fname, car(designator_list), nil));

    return out;
  }
  return cons(get_package(fname, designator_list, nil), nil);
}

val use_package(val use_list, val package_in)
{
  val self = lit("use-package");
  val package = get_package(self, package_in, t);
  val use_package_list = resolve_package_designators(self, use_list);
  val iter;

  if (package == keyword_package)
    uw_throwf(error_s, lit("~a: keyword package cannot use packages"),
              self, nao);

  if (memq(keyword_package, use_package_list))
    uw_throwf(error_s, lit("~a: keyword package cannot be target of use"),
              self, nao);

  if (memq(package, use_package_list))
    uw_throwf(error_s, lit("~a: invalid request for package ~s to use itself"),
              self, package, nao);

  for (iter = use_package_list; iter; iter = cdr(iter)) {
    val use_syms = package_local_symbols(car(iter));
    val use_iter;
    for (use_iter = use_syms; use_iter; use_iter = cdr(use_iter))
      use_sym(car(use_iter), package);
  }

  return use_package_list;
}

val unuse_package(val unuse_list, val package_in)
{
  val self = lit("unuse-package");
  val package = get_package(self, package_in, t);
  val unuse_package_list = resolve_package_designators(self, unuse_list);
  val iter;

  if (memq(package, unuse_package_list))
    uw_throwf(error_s, lit("~a: invalid request for package ~s to unuse itself"),
              self, package, nao);

  for (iter = unuse_package_list; iter; iter = cdr(iter)) {
    val unuse_syms = package_local_symbols(car(iter));
    val unuse_iter;
    for (unuse_iter = unuse_syms; unuse_iter; unuse_iter = cdr(unuse_iter))
      unuse_sym(car(unuse_iter), package);
  }

  return unuse_package_list;
}

/* symbol_visible assumes the perspective that package
 * is the current package!
 */
val symbol_visible(val package, val sym)
{
  val name = symbol_name(sym);
  type_check (package, PKG);

  if (sym->s.package == package)
    return t;

  {
    val cell = gethash_e(package->pk.symhash, name);

    if (cell)
      return eq(cdr(cell), sym);
  }

  {
    val fallback = get_hash_userdata(package->pk.symhash);

    for (; fallback; fallback = cdr(fallback)) {
      val fb_pkg = car(fallback);
      val cell = gethash_e(fb_pkg->pk.symhash, name);

      if (cell)
        return eq(cdr(cell), sym);
    }
  }

  return nil;
}

val find_symbol(val str, val package_in)
{
  val self = lit("find-symbol");
  val package = get_package(self, package_in, nil);
  val found, sym;

  if (!stringp(str))
    uw_throwf(error_s, lit("~s: name ~s isn't a string"), self, str, nao);

  if ((sym = gethash_f(package->pk.symhash, str, mkcloc(found))) || found)
    return sym;

  return zero;
}

val intern(val str, val package_in)
{
  val new_p;
  loc place;
  val package = get_package(lit("intern"), package_in, t);

  if (!stringp(str))
    uw_throwf(error_s, lit("intern: name ~s isn't a string"), str, nao);

  place = gethash_l(package->pk.symhash, str, mkcloc(new_p));

  if (!new_p) {
    return deref(place);
  } else {
    val newsym = make_sym(str);
    newsym->s.package = package;
    return set(place, newsym);
  }
}

val unintern(val symbol, val package_in)
{
  val unint = lit("unintern");
  val package = get_package(unint, package_in, t);
  val name = symbol_name(symbol);
  val found_visible, found_hidden;
  val visible = gethash_f(package->pk.symhash, name, mkcloc(found_visible));
  val hidden = gethash_f(package->pk.hidhash, name, mkcloc(found_hidden));

  prot_sym_check(unint, name, package);

  if (!found_visible || visible != symbol) {
    if (found_hidden && hidden == symbol) {
      remhash(package->pk.hidhash, name);
      return hidden;
    }
    return nil;
  }

  if (found_hidden) {
    remhash(package->pk.hidhash, name);
    sethash(package->pk.symhash, name, hidden);
    set(mkloc(hidden->s.package, hidden), package);
    return hidden;
  }

  if (symbol_package(symbol) == package) {
    if (symbol == nil)
      uw_throwf(error_s, lit("unintern: cannot unintern ~s from ~s"),
                symbol, package, nao);
    symbol->s.package = nil;
  }

  remhash(package->pk.symhash, name);

  return symbol;
}

val rehome_sym(val sym, val package_in)
{
  val self = lit("rehome-sym");
  val package = get_package(self, package_in, t);
  val name = symbol_name(sym);

  if (!sym)
    uw_throwf(error_s, lit("rehome-sym: cannot rehome ~s"), sym, nao);

  prot_sym_check(self, name, sym->s.package);
  prot_sym_check(self, name, package);

  if (sym->s.package) {
    val name = symbol_name(sym);
    if (sym->s.package == package)
      return sym;
    remhash(sym->s.package->pk.symhash, name);
  }
  set(mkloc(sym->s.package, sym), package);
  sethash(package->pk.symhash, name, sym);
  remhash(package->pk.hidhash, name);
  return sym;
}

val package_fallback_list(val package_in)
{
  val package = get_package(lit("package-fallback-list"), package_in, t);
  return get_hash_userdata(package->pk.symhash);
}

val set_package_fallback_list(val package_in, val list_in)
{
  val self = lit("set-package-fallback-list");
  val package = get_package(self, package_in, t);
  val list = resolve_package_designators(self, list_in);
  return set_hash_userdata(package->pk.symhash, list);
}

val intern_fallback(val str, val package_in)
{
  val self = lit("intern-fallback");
  val package = get_package(self, package_in, nil);
  val fblist = get_hash_userdata(package->pk.symhash);

  if (!stringp(str))
    uw_throwf(error_s, lit("~s: name ~s isn't a string"), self, str, nao);

  if (fblist) {
    val found;
    val sym;

    if ((sym = gethash_f(package->pk.symhash, str, mkcloc(found))) || found)
      return sym;

    for (; fblist; fblist = cdr(fblist)) {
      val otherpkg = car(fblist);
      if ((sym = gethash_f(otherpkg->pk.symhash, str, mkcloc(found))) || found)
        return sym;
    }
  }

  {
    val new_p;
    loc place;

    place = gethash_l(package->pk.symhash, str, mkcloc(new_p));

    if (!new_p) {
      return deref(place);
    } else {
      val newsym = make_sym(str);
      newsym->s.package = package;
      return set(place, newsym);
    }
  }
}

val symbolp(val sym)
{
  switch (type(sym)) {
  case NIL:
  case SYM:
    return t;
  default:
    return nil;
  }
}

val keywordp(val sym)
{
  return tnil(sym && symbolp(sym) && sym->s.package == keyword_package_var);
}

val get_current_package(void)
{
  val pkg_binding = lookup_var(nil, package_s);
  val pkg = cdr(pkg_binding);
  if (type(pkg) != PKG) {
    rplacd(pkg_binding, user_package);
    uw_throwf(error_s, lit("variable *package* non-package "
                           "value ~s (reset to sane value)"),
              pkg, nao);
  }
  return pkg;
}

loc get_current_package_alist_loc(void)
{
  if (package_alist_s) {
    loc var_loc = lookup_var_l(nil, package_alist_s);
    if (!nullocp(var_loc))
      return var_loc;
  }
  return mkcloc(packages);
}

val func_f0(val env, val (*fun)(val))
{
  val obj = make_obj();
  obj->f.type = FUN;
  obj->f.functype = F0;
  obj->f.env = env;
  obj->f.f.f0 = fun;
  obj->f.variadic = 0;
  obj->f.fixparam = 0;
  obj->f.optargs = 0;
  return obj;
}

val func_f1(val env, val (*fun)(val, val))
{
  val obj = make_obj();
  obj->f.type = FUN;
  obj->f.functype = F1;
  obj->f.env = env;
  obj->f.f.f1 = fun;
  obj->f.variadic = 0;
  obj->f.fixparam = 1;
  obj->f.optargs = 0;
  return obj;
}

val func_f2(val env, val (*fun)(val, val, val))
{
  val obj = make_obj();
  obj->f.type = FUN;
  obj->f.functype = F2;
  obj->f.env = env;
  obj->f.f.f2 = fun;
  obj->f.variadic = 0;
  obj->f.fixparam = 2;
  obj->f.optargs = 0;
  return obj;
}

val func_f3(val env, val (*fun)(val, val, val, val))
{
  val obj = make_obj();
  obj->f.type = FUN;
  obj->f.functype = F3;
  obj->f.env = env;
  obj->f.f.f3 = fun;
  obj->f.variadic = 0;
  obj->f.fixparam = 3;
  obj->f.optargs = 0;
  return obj;
}

val func_f4(val env, val (*fun)(val, val, val, val, val))
{
  val obj = make_obj();
  obj->f.type = FUN;
  obj->f.functype = F4;
  obj->f.env = env;
  obj->f.f.f4 = fun;
  obj->f.variadic = 0;
  obj->f.fixparam = 4;
  obj->f.optargs = 0;
  return obj;
}

val func_n0(val (*fun)(void))
{
  val obj = make_obj();
  obj->f.type = FUN;
  obj->f.functype = N0;
  obj->f.env = nil;
  obj->f.f.n0 = fun;
  obj->f.variadic = 0;
  obj->f.fixparam = 0;
  obj->f.optargs = 0;
  return obj;
}

val func_n1(val (*fun)(val))
{
  val obj = make_obj();
  obj->f.type = FUN;
  obj->f.functype = N1;
  obj->f.env = nil;
  obj->f.f.n1 = fun;
  obj->f.variadic = 0;
  obj->f.fixparam = 1;
  obj->f.optargs = 0;
  return obj;
}

val func_n2(val (*fun)(val, val))
{
  val obj = make_obj();
  obj->f.type = FUN;
  obj->f.functype = N2;
  obj->f.env = nil;
  obj->f.f.n2 = fun;
  obj->f.variadic = 0;
  obj->f.fixparam = 2;
  obj->f.optargs = 0;
  return obj;
}

val func_n3(val (*fun)(val, val, val))
{
  val obj = make_obj();
  obj->f.type = FUN;
  obj->f.functype = N3;
  obj->f.env = nil;
  obj->f.f.n3 = fun;
  obj->f.variadic = 0;
  obj->f.fixparam = 3;
  obj->f.optargs = 0;
  return obj;
}

val func_n4(val (*fun)(val, val, val, val))
{
  val obj = make_obj();
  obj->f.type = FUN;
  obj->f.functype = N4;
  obj->f.env = nil;
  obj->f.f.n4 = fun;
  obj->f.variadic = 0;
  obj->f.fixparam = 4;
  obj->f.optargs = 0;
  return obj;
}

val func_n5(val (*fun)(val, val, val, val, val))
{
  val obj = make_obj();
  obj->f.type = FUN;
  obj->f.functype = N5;
  obj->f.env = nil;
  obj->f.f.n5 = fun;
  obj->f.variadic = 0;
  obj->f.fixparam = 5;
  obj->f.optargs = 0;
  return obj;
}

val func_n6(val (*fun)(val, val, val, val, val, val))
{
  val obj = make_obj();
  obj->f.type = FUN;
  obj->f.functype = N6;
  obj->f.env = nil;
  obj->f.f.n6 = fun;
  obj->f.variadic = 0;
  obj->f.fixparam = 6;
  obj->f.optargs = 0;
  return obj;
}

val func_n7(val (*fun)(val, val, val, val, val, val, val))
{
  val obj = make_obj();
  obj->f.type = FUN;
  obj->f.functype = N7;
  obj->f.env = nil;
  obj->f.f.n7 = fun;
  obj->f.variadic = 0;
  obj->f.fixparam = 7;
  obj->f.optargs = 0;
  return obj;
}

val func_n8(val (*fun)(val, val, val, val, val, val, val, val))
{
  val obj = make_obj();
  obj->f.type = FUN;
  obj->f.functype = N8;
  obj->f.env = nil;
  obj->f.f.n8 = fun;
  obj->f.variadic = 0;
  obj->f.fixparam = 8;
  obj->f.optargs = 0;
  return obj;
}


val func_f0v(val env, val (*fun)(val, varg))
{
  val obj = make_obj();
  obj->f.type = FUN;
  obj->f.functype = F0;
  obj->f.env = env;
  obj->f.f.f0v = fun;
  obj->f.variadic = 1;
  obj->f.fixparam = 0;
  obj->f.optargs = 0;
  return obj;
}

val func_f1v(val env, val (*fun)(val env, val, varg))
{
  val obj = make_obj();
  obj->f.type = FUN;
  obj->f.functype = F1;
  obj->f.env = env;
  obj->f.f.f1v = fun;
  obj->f.variadic = 1;
  obj->f.fixparam = 1;
  obj->f.optargs = 0;
  return obj;
}

val func_f2v(val env, val (*fun)(val env, val, val, varg))
{
  val obj = make_obj();
  obj->f.type = FUN;
  obj->f.functype = F2;
  obj->f.env = env;
  obj->f.f.f2v = fun;
  obj->f.variadic = 1;
  obj->f.fixparam = 2;
  obj->f.optargs = 0;
  return obj;
}

val func_f3v(val env, val (*fun)(val env, val, val, val, varg))
{
  val obj = make_obj();
  obj->f.type = FUN;
  obj->f.functype = F3;
  obj->f.env = env;
  obj->f.f.f3v = fun;
  obj->f.variadic = 1;
  obj->f.fixparam = 3;
  obj->f.optargs = 0;
  return obj;
}

val func_f4v(val env, val (*fun)(val env, val, val, val, val, varg))
{
  val obj = make_obj();
  obj->f.type = FUN;
  obj->f.functype = F4;
  obj->f.env = env;
  obj->f.f.f4v = fun;
  obj->f.variadic = 1;
  obj->f.fixparam = 4;
  obj->f.optargs = 0;
  return obj;
}

val func_n0v(val (*fun)(varg))
{
  val obj = make_obj();
  obj->f.type = FUN;
  obj->f.functype = N0;
  obj->f.env = nil;
  obj->f.f.n0v = fun;
  obj->f.variadic = 1;
  obj->f.fixparam = 0;
  obj->f.optargs = 0;
  return obj;
}

val func_n1v(val (*fun)(val, varg))
{
  val obj = make_obj();
  obj->f.type = FUN;
  obj->f.functype = N1;
  obj->f.env = nil;
  obj->f.f.n1v = fun;
  obj->f.variadic = 1;
  obj->f.fixparam = 1;
  obj->f.optargs = 0;
  return obj;
}

val func_n2v(val (*fun)(val, val, varg))
{
  val obj = make_obj();
  obj->f.type = FUN;
  obj->f.functype = N2;
  obj->f.env = nil;
  obj->f.f.n2v = fun;
  obj->f.variadic = 1;
  obj->f.fixparam = 2;
  obj->f.optargs = 0;
  return obj;
}

val func_n3v(val (*fun)(val, val, val, varg))
{
  val obj = make_obj();
  obj->f.type = FUN;
  obj->f.functype = N3;
  obj->f.env = nil;
  obj->f.f.n3v = fun;
  obj->f.variadic = 1;
  obj->f.fixparam = 3;
  obj->f.optargs = 0;
  return obj;
}

val func_n4v(val (*fun)(val, val, val, val, varg))
{
  val obj = make_obj();
  obj->f.type = FUN;
  obj->f.functype = N4;
  obj->f.env = nil;
  obj->f.f.n4v = fun;
  obj->f.variadic = 1;
  obj->f.fixparam = 4;
  obj->f.optargs = 0;
  return obj;
}

val func_n5v(val (*fun)(val, val, val, val, val, varg))
{
  val obj = make_obj();
  obj->f.type = FUN;
  obj->f.functype = N5;
  obj->f.env = nil;
  obj->f.f.n5v = fun;
  obj->f.variadic = 1;
  obj->f.fixparam = 5;
  obj->f.optargs = 0;
  return obj;
}

val func_n6v(val (*fun)(val, val, val, val, val, val, varg))
{
  val obj = make_obj();
  obj->f.type = FUN;
  obj->f.functype = N6;
  obj->f.env = nil;
  obj->f.f.n6v = fun;
  obj->f.variadic = 1;
  obj->f.fixparam = 6;
  obj->f.optargs = 0;
  return obj;
}

val func_n7v(val (*fun)(val, val, val, val, val, val, val, varg))
{
  val obj = make_obj();
  obj->f.type = FUN;
  obj->f.functype = N7;
  obj->f.env = nil;
  obj->f.f.n7v = fun;
  obj->f.variadic = 1;
  obj->f.fixparam = 7;
  obj->f.optargs = 0;
  return obj;
}

val func_n8v(val (*fun)(val, val, val, val, val, val, val, val, varg))
{
  val obj = make_obj();
  obj->f.type = FUN;
  obj->f.functype = N8;
  obj->f.env = nil;
  obj->f.f.n8v = fun;
  obj->f.variadic = 1;
  obj->f.fixparam = 8;
  obj->f.optargs = 0;
  return obj;
}

val func_n1o(val (*fun)(val), int reqargs)
{
  val obj = func_n1(fun);
  obj->f.optargs = 1 - reqargs;
  return obj;
}

val func_n2o(val (*fun)(val, val), int reqargs)
{
  val obj = func_n2(fun);
  obj->f.optargs = 2 - reqargs;
  return obj;
}

val func_n3o(val (*fun)(val, val, val), int reqargs)
{
  val obj = func_n3(fun);
  obj->f.optargs = 3 - reqargs;
  return obj;
}

val func_n4o(val (*fun)(val, val, val, val), int reqargs)
{
  val obj = func_n4(fun);
  obj->f.optargs = 4 - reqargs;
  return obj;
}

val func_n5o(val (*fun)(val, val, val, val, val), int reqargs)
{
  val obj = func_n5(fun);
  obj->f.optargs = 5 - reqargs;
  return obj;
}

val func_n6o(val (*fun)(val, val, val, val, val, val), int reqargs)
{
  val obj = func_n6(fun);
  obj->f.optargs = 6 - reqargs;
  return obj;
}

val func_n7o(val (*fun)(val, val, val, val, val, val, val), int reqargs)
{
  val obj = func_n7(fun);
  obj->f.optargs = 7 - reqargs;
  return obj;
}

val func_n8o(val (*fun)(val, val, val, val, val, val, val, val), int reqargs)
{
  val obj = func_n8(fun);
  obj->f.optargs = 8 - reqargs;
  return obj;
}

val func_n1ov(val (*fun)(val, varg), int reqargs)
{
  val obj = func_n1v(fun);
  obj->f.optargs = 1 - reqargs;
  return obj;
}

val func_n2ov(val (*fun)(val, val, varg), int reqargs)
{
  val obj = func_n2v(fun);
  obj->f.optargs = 2 - reqargs;
  return obj;
}

val func_n3ov(val (*fun)(val, val, val, varg), int reqargs)
{
  val obj = func_n3v(fun);
  obj->f.optargs = 3 - reqargs;
  return obj;
}

val func_interp(val env, val form)
{
  val obj = make_obj();
  obj->f.type = FUN;
  obj->f.functype = FINTERP;
  obj->f.env = env;
  obj->f.f.interp_fun = form;
  obj->f.variadic = 1;
  obj->f.fixparam = 0;
  obj->f.optargs = 0;
  return obj;
}

val func_get_form(val fun)
{
  type_check(fun, FUN);
  if (fun->f.functype != FINTERP)
    uw_throwf(error_s, lit("func-get-form: ~a is not an interpreted function"),
              fun, nao);
  return fun->f.f.interp_fun;
}

val func_get_env(val fun)
{
  type_check(fun, FUN);
  return fun->f.env;
}

val func_set_env(val fun, val env)
{
  type_check(fun, FUN);
  set(mkloc(fun->f.env, fun), env);
  return env;
}

val functionp(val obj)
{
  return type(obj) == FUN ? t : nil;
}

val interp_fun_p(val obj)
{
  return (functionp(obj) && obj->f.functype == FINTERP) ? t : nil;
}

static noreturn void callerror(val fun, val msg)
{
  uses_or2;

  if (functionp(fun))
    fun = format(nil, lit("~s"), or2(func_get_name(fun, nil), fun), nao);
  else
    fun = format(nil, lit("object ~s called as function"), fun, nao);

  uw_throwf(error_s, lit("~a: ~a"), fun, msg, nao);
  abort();
}

val generic_funcall(val fun, struct args *args_in)
{
  int variadic, fixparam, reqargs;
  struct args *args = args_in;

  switch (type(fun)) {
  case FUN:
    break;
  case NIL:
  case CONS:
  case LCONS:
  case VEC:
  case STR:
  case LIT:
  case LSTR:
  case BUF:
  carray:
    bug_unless (args->argc >= ARGS_MIN);
    args_normalize(args, 3);

    switch (args->fill) {
    case 0:
      callerror(fun, lit("missing required arguments"));
    case 1:
      switch (type(args->arg[0])) {
      case NIL:
      case CONS:
      case LCONS:
      case VEC:
        return sel(fun, args->arg[0]);
      case RNG:
        return sub(fun, args->arg[0]->rn.from, args->arg[0]->rn.to);
      default:
        return ref(fun, args->arg[0]);
      }
    case 2:
      return sub(fun, args->arg[0], args->arg[1]);
    default:
      callerror(fun, lit("too many arguments"));
    }
  case SYM:
    {
      val binding = lookup_fun(nil, fun);
      if (!binding)
        callerror(fun, lit("has no function binding"));
      fun = cdr(binding);
    }
    break;
  case COBJ:
    if (fun->co.cls == hash_s) {
      bug_unless (args->argc >= ARGS_MIN);
      args_normalize(args, 3);

      switch (args->fill) {
      case 0:
        callerror(fun, lit("missing required arguments"));
      case 1:
        return gethash(fun, args->arg[0]);
      case 2:
        return gethash_n(fun, args->arg[0], args->arg[1]);
      default:
        callerror(fun, lit("too many arguments"));
      }
    } else if (fun->co.cls == regex_s) {
      bug_unless (args->argc >= ARGS_MIN);
      args_normalize(args, 3);

      switch (args->fill) {
      case 0:
        callerror(fun, lit("missing required arguments"));
      case 1:
        return search_regst(z(args->arg[0]), fun, nil, nil);
      case 2:
        return search_regst(z(args->arg[1]), fun, z(args->arg[0]), nil);
      case 3:
        return search_regst(z(args->arg[2]), fun, z(args->arg[0]), z(args->arg[1]));
      default:
        callerror(fun, lit("too many arguments"));
      }
    } else if (fun->co.cls == carray_s) {
      goto carray;
    } else if (structp(fun)) {
      fun = method(fun, lambda_s);
      break;
    }
    /* fallthrough */
  default:
    callerror(fun, lit("object is not callable"));
  }

  variadic = fun->f.variadic;
  fixparam = fun->f.fixparam;
  reqargs = fixparam - fun->f.optargs;

  if (!variadic) {
    val *arg = 0;

    if (args->argc < fixparam) {
      args_decl(args_copy, fixparam);
      args_copy_zap(args_copy, args_in);
      args = args_copy;
    }

    arg = args->arg;

    args_normalize_fill(args, reqargs, fixparam);

    if (args->fill < reqargs)
      callerror(fun, lit("missing required arguments"));

    if (args->list)
      callerror(fun, lit("too many arguments"));

    switch (fun->f.functype) {
    case F0:
      return fun->f.f.f0(fun->f.env);
    case F1:
      return fun->f.f.f1(fun->f.env, z(arg[0]));
    case F2:
      return fun->f.f.f2(fun->f.env, z(arg[0]), z(arg[1]));
    case F3:
      return fun->f.f.f3(fun->f.env, z(arg[0]), z(arg[1]), z(arg[2]));
    case F4:
      return fun->f.f.f4(fun->f.env, z(arg[0]), z(arg[1]), z(arg[2]), z(arg[3]));
    case N0:
      return fun->f.f.n0();
    case N1:
      return fun->f.f.n1(z(arg[0]));
    case N2:
      return fun->f.f.n2(z(arg[0]), z(arg[1]));
    case N3:
      return fun->f.f.n3(z(arg[0]), z(arg[1]), z(arg[2]));
    case N4:
      return fun->f.f.n4(z(arg[0]), z(arg[1]), z(arg[2]), z(arg[3]));
    case N5:
      return fun->f.f.n5(z(arg[0]), z(arg[1]), z(arg[2]), z(arg[3]), z(arg[4]));
    case N6:
      return fun->f.f.n6(z(arg[0]), z(arg[1]), z(arg[2]), z(arg[3]), z(arg[4]), z(arg[5]));
    case N7:
      return fun->f.f.n7(z(arg[0]), z(arg[1]), z(arg[2]), z(arg[3]), z(arg[4]), z(arg[5]), z(arg[6]));
    case N8:
      return fun->f.f.n8(z(arg[0]), z(arg[1]), z(arg[2]), z(arg[3]), z(arg[4]), z(arg[5]), z(arg[6]), z(arg[7]));
    case FINTERP:
      internal_error("unsupported function type");
    }
  } else {
    val *arg = 0;

    if (args->argc < fixparam) {
      args_decl(args_copy, fixparam);
      args_copy_zap(args_copy, args_in);
      args = args_copy;
    }

    arg = args->arg;

    args_normalize_fill(args, reqargs, fixparam);

    if (args->fill < reqargs)
      callerror(fun, lit("missing required arguments"));

    args_clear(args);

    switch (fun->f.functype) {
    case FINTERP:
      return funcall_interp(fun, args);
    case F0:
      return fun->f.f.f0v(fun->f.env, args);
    case F1:
      return fun->f.f.f1v(fun->f.env, z(arg[0]), args);
    case F2:
      return fun->f.f.f2v(fun->f.env, z(arg[0]), z(arg[1]), args);
    case F3:
      return fun->f.f.f3v(fun->f.env, z(arg[0]), z(arg[1]), z(arg[2]), args);
    case F4:
      return fun->f.f.f4v(fun->f.env, z(arg[0]), z(arg[1]), z(arg[2]), z(arg[3]), args);
    case N0:
      return fun->f.f.n0v(args);
    case N1:
      return fun->f.f.n1v(z(arg[0]), args);
    case N2:
      return fun->f.f.n2v(z(arg[0]), z(arg[1]), args);
    case N3:
      return fun->f.f.n3v(z(arg[0]), z(arg[1]), z(arg[2]), args);
    case N4:
      return fun->f.f.n4v(z(arg[0]), z(arg[1]), z(arg[2]), z(arg[3]), args);
    case N5:
      return fun->f.f.n5v(z(arg[0]), z(arg[1]), z(arg[2]), z(arg[3]), z(arg[4]), args);
    case N6:
      return fun->f.f.n6v(z(arg[0]), z(arg[1]), z(arg[2]), z(arg[3]), z(arg[4]), z(arg[5]), args);
    case N7:
      return fun->f.f.n7v(z(arg[0]), z(arg[1]), z(arg[2]), z(arg[3]), z(arg[4]), z(arg[5]), z(arg[6]), args);
    case N8:
      return fun->f.f.n8v(z(arg[0]), z(arg[1]), z(arg[2]), z(arg[3]), z(arg[4]), z(arg[5]), z(arg[6]), z(arg[7]), args);
    }
  }

  internal_error("corrupt function type field");
}

static noreturn void wrongargs(val fun)
{
  callerror(fun, lit("wrong number of arguments"));
}

val funcall(val fun)
{
  if (type(fun) != FUN || fun->f.optargs) {
    args_decl(args, ARGS_MIN);
    return generic_funcall(fun, args);
  }

  if (fun->f.variadic) {
    args_decl(args, ARGS_MIN);

    switch (fun->f.functype) {
    case FINTERP:
      return funcall_interp(fun, args);
    case F0:
      return fun->f.f.f0v(fun->f.env, args);
    case N0:
      return fun->f.f.n0v(args);
    default:
      break;
    }
  } else {
    switch (fun->f.functype) {
    case F0:
      return fun->f.f.f0(fun->f.env);
    case N0:
      return fun->f.f.n0();
    default:
      break;
    }
  }
  wrongargs(fun);
}

val funcall1(val fun, val arg)
{
  if (type(fun) != FUN || fun->f.optargs) {
    args_decl(args, ARGS_MIN);
    args_add(args, arg);
    return generic_funcall(fun, args);
  }

  if (fun->f.variadic) {
    args_decl(args, ARGS_MIN);

    switch (fun->f.functype) {
    case FINTERP:
      args_add(args, arg);
      return funcall_interp(fun, args);
    case F0:
      args_add(args, arg);
      return fun->f.f.f0v(fun->f.env, args);
    case N0:
      args_add(args, arg);
      return fun->f.f.n0v(args);
    case F1:
      return fun->f.f.f1v(fun->f.env, z(arg), args);
    case N1:
      return fun->f.f.n1v(z(arg), args);
    default:
      break;
    }
  } else {
    switch (fun->f.functype) {
    case F1:
      return fun->f.f.f1(fun->f.env, z(arg));
    case N1:
      return fun->f.f.n1(z(arg));
    default:
      break;
    }
  }
  wrongargs(fun);
}

val funcall2(val fun, val arg1, val arg2)
{
  if (type(fun) != FUN || fun->f.optargs) {
    args_decl(args, ARGS_MIN);
    args_add2(args, arg1, arg2);
    return generic_funcall(fun, args);
  }

  if (fun->f.variadic) {
    args_decl(args, ARGS_MIN);

    switch (fun->f.functype) {
    case FINTERP:
      args_add2(args, arg1, arg2);
      return funcall_interp(fun, args);
    case F0:
      args_add2(args, arg1, arg2);
      return fun->f.f.f0v(fun->f.env, args);
    case N0:
      args_add2(args, arg1, arg2);
      return fun->f.f.n0v(args);
    case F1:
      args_add(args, arg2);
      return fun->f.f.f1v(fun->f.env, z(arg1), args);
    case N1:
      args_add(args, arg2);
      return fun->f.f.n1v(z(arg1), args);
    case F2:
      return fun->f.f.f2v(fun->f.env, z(arg1), z(arg2), args);
    case N2:
      return fun->f.f.n2v(z(arg1), z(arg2), args);
    default:
      break;
    }
  } else {
    switch (fun->f.functype) {
    case F2:
      return fun->f.f.f2(fun->f.env, z(arg1), z(arg2));
    case N2:
      return fun->f.f.n2(z(arg1), z(arg2));
    default:
      break;
    }
  }
  wrongargs(fun);
}

val funcall3(val fun, val arg1, val arg2, val arg3)
{
  if (type(fun) != FUN || fun->f.optargs) {
    args_decl(args, ARGS_MIN);
    args_add3(args, arg1, arg2, arg3);
    return generic_funcall(fun, args);
  }

  if (fun->f.variadic) {
    args_decl(args, ARGS_MIN);

    switch (fun->f.functype) {
    case FINTERP:
      args_add3(args, arg1, arg2, arg3);
      return funcall_interp(fun, args);
    case F0:
      args_add3(args, arg1, arg2, arg3);
      return fun->f.f.f0v(fun->f.env, args);
    case N0:
      args_add3(args, arg1, arg2, arg3);
      return fun->f.f.n0v(args);
    case F1:
      args_add2(args, arg2, arg3);
      return fun->f.f.f1v(fun->f.env, z(arg1), args);
    case N1:
      args_add2(args, arg2, arg3);
      return fun->f.f.n1v(z(arg1), args);
    case F2:
      args_add(args, arg3);
      return fun->f.f.f2v(fun->f.env, z(arg1), z(arg2), args);
    case N2:
      args_add(args, arg3);
      return fun->f.f.n2v(z(arg1), z(arg2), args);
    case F3:
      return fun->f.f.f3v(fun->f.env, z(arg1), z(arg2), z(arg3), args);
    case N3:
      return fun->f.f.n3v(z(arg1), z(arg2), z(arg3), args);
    default:
      break;
    }
  } else {
    switch (fun->f.functype) {
    case F3:
      return fun->f.f.f3(fun->f.env, z(arg1), z(arg2), z(arg3));
    case N3:
      return fun->f.f.n3(z(arg1), z(arg2), z(arg3));
    default:
      break;
    }
  }
  wrongargs(fun);
}

val funcall4(val fun, val arg1, val arg2, val arg3, val arg4)
{
  if (type(fun) != FUN || fun->f.optargs) {
    args_decl(args, ARGS_MIN);
    args_add4(args, arg1, arg2, arg3, arg4);
    return generic_funcall(fun, args);
  }

  if (fun->f.variadic) {
    args_decl(args, ARGS_MIN);

    switch (fun->f.functype) {
    case FINTERP:
      args_add4(args, arg1, arg2, arg3, arg4);
      return funcall_interp(fun, args);
    case F0:
      args_add4(args, arg1, arg2, arg3, arg4);
      return fun->f.f.f0v(fun->f.env, args);
    case N0:
      args_add4(args, arg1, arg2, arg3, arg4);
      return fun->f.f.n0v(args);
    case F1:
      args_add3(args, arg2, arg3, arg4);
      return fun->f.f.f1v(fun->f.env, z(arg1), args);
    case N1:
      args_add3(args, arg2, arg3, arg4);
      return fun->f.f.n1v(z(arg1), args);
    case F2:
      args_add2(args, arg3, arg4);
      return fun->f.f.f2v(fun->f.env, z(arg1), z(arg2), args);
    case N2:
      args_add2(args, arg3, arg4);
      return fun->f.f.n2v(z(arg1), z(arg2), args);
    case F3:
      args_add(args, arg4);
      return fun->f.f.f3v(fun->f.env, z(arg1), z(arg2), z(arg3), args);
    case N3:
      args_add(args, arg4);
      return fun->f.f.n3v(z(arg1), z(arg2), z(arg3), args);
    case F4:
      return fun->f.f.f4v(fun->f.env, z(arg1), z(arg2), z(arg3), z(arg4), args);
    case N4:
      return fun->f.f.n4v(z(arg1), z(arg2), z(arg3), z(arg4), args);
    default:
      break;
    }
  } else {
    switch (fun->f.functype) {
    case F4:
      return fun->f.f.f4(fun->f.env, z(arg1), z(arg2), z(arg3), z(arg4));
    case N4:
      return fun->f.f.n4(z(arg1), z(arg2), z(arg3), z(arg4));
    default:
      break;
    }
  }
  wrongargs(fun);
}

val reduce_left(val fun, val list, val init, val key)
{
  if (null_or_missing_p(key))
    key = identity_f;

  list = nullify(list);

  if (missingp(init)) {
    if (list)
      init = funcall1(key, pop(&list));
    else
      return funcall(fun);
  }

  for (; list; list = cdr(list))
    init = funcall2(fun, init, funcall1(key, car(list)));

  return init;
}

val reduce_right(val fun, val list, val init, val key)
{
  if (null_or_missing_p(key))
    key = identity_f;

  list = nullify(list);

  if (list) {
    if (missingp(init)) {
      if (!rest(list))
        return funcall1(key, first(list));
      if (!rest(rest(list)))
        return funcall2(fun, funcall1(key, first(list)),
                        funcall1(key, second(list)));
      /* fall through: no init, three or more items in list */
    } else {
      if (!rest(list))
        return funcall2(fun, funcall1(key, first(list)), init);
      /* fall through: init, and two or more items in list */
    }
  } else if (!missingp(init)) {
    return init;
  } else {
    return funcall(fun);
  }

  return funcall2(fun, funcall1(key, car(list)),
                       if3(cdr(list), reduce_right(fun, cdr(list), init, key),
                                      init));
}

static val do_curry_12_2(val fcons, val arg2)
{
  return funcall2(car(fcons), cdr(fcons), arg2);
}

val curry_12_2(val fun2, val arg)
{
  return func_f1(cons(fun2, arg), do_curry_12_2);
}

static val do_curry_12_1(val fcons, val arg1)
{
  return funcall2(car(fcons), arg1, cdr(fcons));
}

val curry_12_1(val fun2, val arg2)
{
  return func_f1(cons(fun2, arg2), do_curry_12_1);
}

static val do_curry_12_1_v(val fcons, struct args *args)
{
  return funcall2(car(fcons), args_get_list(args), cdr(fcons));
}

static val curry_12_1_v(val fun2, val arg2)
{
  return func_f0v(cons(fun2, arg2), do_curry_12_1_v);
}

static val do_curry_123_3(val fcons, val arg3)
{
  return funcall3(car(fcons), car(cdr(fcons)), cdr(cdr(fcons)), arg3);
}

val curry_123_3(val fun3, val arg1, val arg2)
{
  return func_f1(cons(fun3, cons(arg1, arg2)), do_curry_123_3);
}

static val do_curry_123_2(val fcons, val arg2)
{
  return funcall3(car(fcons), car(cdr(fcons)), arg2, cdr(cdr(fcons)));
}

val curry_123_2(val fun3, val arg1, val arg3)
{
  return func_f1(cons(fun3, cons(arg1, arg3)), do_curry_123_2);
}

static val do_curry_123_1(val fcons, val arg1)
{
  return funcall3(car(fcons), arg1, car(cdr(fcons)), cdr(cdr(fcons)));
}

val curry_123_1(val fun3, val arg2, val arg3)
{
  return func_f1(cons(fun3, cons(arg2, arg3)), do_curry_123_1);
}

static val do_curry_123_23(val fcons, val arg2, val arg3)
{
  return funcall3(car(fcons), cdr(fcons), arg2, arg3);
}

val curry_123_23(val fun3, val arg1)
{
  return func_f2(cons(fun3, arg1), do_curry_123_23);
}

static val do_curry_1234_1(val fcons, val arg1)
{
  cons_bind (fun, dr, fcons);
  cons_bind (arg2, ddr, dr);
  cons_bind (arg3, dddr, ddr);
  val arg4 = car(dddr);

  return funcall4(fun, arg1, arg2, arg3, arg4);
}

val curry_1234_1(val fun4, val arg2, val arg3, val arg4)
{
  return func_f1(list(fun4, arg2, arg3, arg4, nao), do_curry_1234_1);
}

static val do_curry_1234_34(val fcons, val arg3, val arg4)
{
  return funcall4(car(fcons), car(cdr(fcons)), cdr(cdr(fcons)), arg3, arg4);
}

val curry_1234_34(val fun4, val arg1, val arg2)
{
  return func_f2(cons(fun4, cons(arg1, arg2)), do_curry_1234_34);
}

val transposev(struct args *list)
{
  val func = list_f;

  if (!args_more(list, 0))
    return nil;

  switch (type(args_at(list, 0))) {
  case STR:
  case LSTR:
  case LIT:
    func = curry_12_1_v(func_n2(cat_str), nil);
    break;
  case VEC:
    func = func_n0v(vectorv);
    break;
  default:
    break;
  }

  return mapcarv(func, list);
}

val transpose(val list)
{
  args_decl_list(args, ARGS_MIN, copy(list));
  return make_like(transposev(args), list);
}

static val do_chain(val fun1_list, struct args *args)
{
  val arg = nil;

  fun1_list = nullify(fun1_list);

  if (fun1_list) {
    arg = generic_funcall(car(fun1_list), args);
    fun1_list = cdr(fun1_list);
  }

  for (; fun1_list; fun1_list = cdr(fun1_list))
    arg = funcall1(car(fun1_list), arg);

  return arg;
}

val chain(val first_fun, ...)
{
  va_list vl;
  list_collect_decl (out, iter);

  if (first_fun != nao) {
    val next_fun;
    va_start (vl, first_fun);
    iter = list_collect(iter, first_fun);

    while ((next_fun = va_arg(vl, val)) != nao)
      iter = list_collect(iter, next_fun);

    va_end (vl);
  }

  return func_f0v(out, do_chain);
}

val chainv(struct args *funlist)
{
  return func_f0v(args_get_list(funlist), do_chain);
}

static val do_chand(val fun1_list, struct args *args)
{
  val arg = nil;

  fun1_list = nullify(fun1_list);

  if (fun1_list) {
    arg = generic_funcall(car(fun1_list), args);
    fun1_list = cdr(fun1_list);
  }

  for (; arg && fun1_list; fun1_list = cdr(fun1_list))
    arg = funcall1(car(fun1_list), arg);

  return arg;
}


val chandv(struct args *funlist)
{
  return func_f0v(args_get_list(funlist), do_chand);
}

static val do_juxt(val funcs, struct args *args)
{
  return mapcar(curry_12_1(func_n2(apply), args_get_list(args)), funcs);
}

val juxtv(struct args *funlist)
{
  return func_f0v(args_get_list(funlist), do_juxt);
}

static val do_and(val fun1_list, struct args *args_in)
{
  cnum argc = args_in->argc;
  args_decl(args, argc);
  val ret = t;

  fun1_list = nullify(fun1_list);

  gc_hint(fun1_list);

  for (; fun1_list; fun1_list = cdr(fun1_list)) {
    args_copy(args, args_in);
    if (nilp((ret = generic_funcall(car(fun1_list), args))))
      break;
  }

  return ret;
}

val andf(val first_fun, ...)
{
  va_list vl;
  list_collect_decl (out, iter);

  if (first_fun != nao) {
    val next_fun;
    va_start (vl, first_fun);
    iter = list_collect(iter, first_fun);

    while ((next_fun = va_arg(vl, val)) != nao)
      iter = list_collect(iter, next_fun);

    va_end (vl);
  }

  return func_f0v(out, do_and);
}

val andv(struct args *funlist)
{
  return func_f0v(args_get_list(funlist), do_and);
}

static val do_swap_12_21(val fun, val left, val right)
{
  return funcall2(fun, right, left);
}

val swap_12_21(val fun)
{
  return func_f2(fun, do_swap_12_21);
}

static val do_or(val fun1_list, struct args *args_in)
{
  cnum argc = args_in->argc;
  args_decl(args, argc);
  val ret = nil;

  fun1_list = nullify(fun1_list);

  gc_hint(fun1_list);

  for (; fun1_list; fun1_list = cdr(fun1_list)) {
    args_copy(args, args_in);
    if ((ret = generic_funcall(car(fun1_list), args)))
      break;
  }

  return ret;
}

val orf(val first_fun, ...)
{
  va_list vl;
  list_collect_decl (out, iter);

  if (first_fun != nao) {
    val next_fun;
    va_start (vl, first_fun);
    iter = list_collect(iter, first_fun);

    while ((next_fun = va_arg(vl, val)) != nao)
      iter = list_collect(iter, next_fun);

    va_end (vl);
  }

  return func_f0v(out, do_or);
}

val orv(struct args *funlist)
{
  return func_f0v(args_get_list(funlist), do_or);
}

static val do_not(val fun, struct args *args)
{
  return null(apply(fun, args_get_list(args)));
}

val notf(val fun)
{
  return func_f0v(fun, do_not);
}

static val do_iff(val env, struct args *args_in)
{
  cons_bind (condfun, choices, env);
  cons_bind (thenfun, elsefun, choices);
  args_decl(args, args_in->argc);

  args_copy(args, args_in);

  return if3(generic_funcall(condfun, args_in),
             generic_funcall(thenfun, args),
             if2(elsefun, generic_funcall(elsefun, args)));
}

val iff(val condfun, val thenfun, val elsefun)
{
  thenfun = default_arg(thenfun, identity_f);
  elsefun = default_null_arg(elsefun);
  return func_f0v(cons(condfun, cons(thenfun, elsefun)), do_iff);
}

val iffi(val condfun, val thenfun, val elsefun)
{
  elsefun = default_arg(elsefun, identity_f);
  return func_f0v(cons(condfun, cons(thenfun, elsefun)), do_iff);
}

static val do_dup(val fun, val arg)
{
  val arg1 = z(arg);
  val arg2 = arg1;
  return funcall2(fun, z(arg1), z(arg2));
}

val dupl(val fun)
{
  return func_f1(fun, do_dup);
}

val vector(val length, val initval)
{
  unsigned i;
  ucnum len = c_unum(length);
  ucnum alloc_plus = len + 2;
  ucnum size = if3(alloc_plus > len, alloc_plus, -1);
  val *v = coerce(val *, chk_xalloc(size, sizeof *v, lit("vector")));
  val vec = make_obj();
  vec->v.type = VEC;
  initval = default_null_arg(initval);
#if HAVE_VALGRIND
  vec->v.vec_true_start = v;
#endif
  v += 2;
  vec->v.vec = v;
  v[vec_alloc] = length;
  v[vec_length] = length;
  for (i = 0; i < alloc_plus - 2; i++)
    vec->v.vec[i] = initval;
  return vec;
}

val vectorp(val vec)
{
  return type(vec) == VEC ? t : nil;
}

val vec_set_length(val vec, val length)
{
  type_check(vec, VEC);

  {
    cnum new_length = c_num(length);
    cnum old_length = c_num(vec->v.vec[vec_length]);
    cnum old_alloc = c_num(vec->v.vec[vec_alloc]);

    if (new_length < 0)
      uw_throwf(error_s, lit("vec-set-length: negative length ~s specified"),
                length, nao);

    if (new_length > convert(cnum, (convert(size_t, -1)/sizeof (val) - 2)))
      uw_throwf(error_s, lit("vec-set-length: cannot extend to length ~s"),
                length, nao);

    if (new_length > old_alloc) {
      cnum new_alloc = max(new_length, 2*old_alloc);
      val *newvec = coerce(val *, chk_realloc(coerce(mem_t *, vec->v.vec - 2),
                                              (new_alloc + 2) * sizeof *newvec));
      vec->v.vec = newvec + 2;
      set(mkloc(vec->v.vec[vec_alloc], vec), num(new_alloc));
#if HAVE_VALGRIND
      vec->v.vec_true_start = newvec;
#endif
    }

    if (new_length > old_length) {
      cnum i;
      for (i = old_length; i < new_length; i++)
        vec->v.vec[i] = nil;
    }

    set(mkloc(vec->v.vec[vec_length], vec), length);
  }

  return vec;
}

val vecref(val vec, val ind)
{
  cnum index = c_num(ind);
  cnum len = c_num(length_vec(vec));
  if (index < 0)
    index = len + index;
  if (index < 0 || index >= len)
    uw_throwf(error_s, lit("vecref: ~s is out of range for vector ~s"),
              ind, vec, nao);
  return vec->v.vec[index];
}

loc vecref_l(val vec, val ind)
{
  cnum index = c_num(ind);
  cnum len = c_num(length_vec(vec));
  if (index < 0)
    index = len + index;
  if (index < 0 || index >= len)
    uw_throwf(error_s, lit("vecref: ~s is out of range for vector ~s"),
              ind, vec, nao);
  return mkloc(vec->v.vec[index], vec);
}

val vec_push(val vec, val item)
{
  val length = length_vec(vec);
  vec_set_length(vec, plus(length, one));
  set(vecref_l(vec, length), item);
  return length;
}

val length_vec(val vec)
{
  type_check(vec, VEC);
  return vec->v.vec[vec_length];
}

val size_vec(val vec)
{
  type_check(vec, VEC);
  return vec->v.vec[vec_alloc];
}

val vectorv(struct args *args)
{
  cnum index = 0;
  val vec = vector(zero, nil);

  while (args_more(args, index))
    vec_push(vec, args_get(args, &index));

  return vec;
}

val vec(val first, ...)
{
  va_list vl;
  val vec = vector(zero, nil);
  val next;
  int count;

  va_start (vl, first);

  for (next = first, count = 0;
       next != nao && count < 32;
       next = va_arg(vl, val), count++)
  {
    vec_push(vec, next);
  }

  if (count == 32 && next != nao)
    internal_error("runaway arguments in vec function");

  return vec;
}

val vec_list(val list)
{
  val vec = vector(zero, nil);

  for (; list; list = cdr(list))
    vec_push(vec, car(list));

  return vec;
}

val list_vec(val vec)
{
  list_collect_decl (list, ptail);
  int i, len;

  type_check(vec, VEC);

  len = c_num(vec->v.vec[vec_length]);

  for (i = 0; i < len; i++)
    ptail = list_collect(ptail, vec->v.vec[i]);

  return list;
}

val copy_vec(val vec_in)
{
  val length = length_vec(vec_in);
  ucnum alloc_plus = c_unum(length) + 2;
  val *v = coerce(val *, chk_xalloc(alloc_plus, sizeof *v, lit("copy-vec")));
  val vec = make_obj();
  vec->v.type = VEC;
#if HAVE_VALGRIND
  vec->v.vec_true_start = v;
#endif
  v += 2;
  vec->v.vec = v;
  v[vec_alloc] = length;
  v[vec_length] = length;
  memcpy(vec->v.vec, vec_in->v.vec, (alloc_plus - 2) * sizeof *vec->v.vec);
  return vec;
}

val sub_vec(val vec_in, val from, val to)
{
  val len = length_vec(vec_in);

  if (null_or_missing_p(from))
    from = zero;
  else if (from == t)
    from = len;
  else if (lt(from, zero)) {
    from = plus(from, len);
    if (to == zero)
      to = len;
  }

  if (null_or_missing_p(to) || to == t)
    to = len;
  else if (lt(to, zero))
    to = plus(to, len);

  from = max2(zero, min2(from, len));
  to = max2(zero, min2(to, len));

  if (ge(from, to)) {
    return vector(zero, nil);
  } else {
    cnum cfrom = c_num(from);
    size_t nelem = c_num(to) - cfrom;
    val *v = coerce(val *, chk_xalloc((nelem + 2), sizeof *v, lit("sub-vec")));
    val vec = make_obj();
    vec->v.type = VEC;
#if HAVE_VALGRIND
    vec->v.vec_true_start = v;
#endif
    v += 2;
    vec->v.vec = v;
    v[vec_length] = v[vec_alloc] = num(nelem);
    memcpy(vec->v.vec, vec_in->v.vec + cfrom, nelem * sizeof *vec->v.vec);
    return vec;
  }
}

val replace_vec(val vec_in, val items, val from, val to)
{
  val it_seq = toseq(items);
  val len = length_vec(vec_in);

  if (listp(from)) {
    val where = from;
    val len = length_vec(vec_in);

    if (!missingp(to))
      uw_throwf(error_s,
                lit("replace-vec: to-arg not applicable when from-arg is a list"),
                nao);

    for (; where && it_seq; where = cdr(where)) {
      val wh = car(where);
      if (ge(wh, len))
        break;
      set(vecref_l(vec_in, wh), pop(&it_seq));
    }

    return vec_in;
  } else if (vectorp(from)) {
    val where = from;
    val len = length_vec(vec_in);
    val wlen = length_vec(from);
    val i;

    if (!missingp(to))
      uw_throwf(error_s,
                lit("replace-vec: to-arg not applicable when from-arg is a vector"),
                nao);

    for (i = zero; lt(i, wlen) && it_seq; i = plus(i, one)) {
      val wh = vecref(where, i);
      if (ge(wh, len))
        break;
      set(vecref_l(vec_in, wh), pop(&it_seq));
    }

    return vec_in;
  } else if (missingp(from)) {
    from = zero;
  } else if (from == t) {
    from = len;
  } else if (lt(from, zero)) {
    from = plus(from, len);
    if (to == zero)
      to = len;
  }

  if (null_or_missing_p(to) || to == t)
    to = len;
  else if (lt(to, zero))
    to = plus(to, len);

  from = max2(zero, min2(from, len));
  to = max2(zero, min2(to, len));

  {
    val len_rep = minus(to, from);
    val len_it = length(it_seq);

    if (gt(len_rep, len_it)) {
      val len_diff = minus(len_rep, len_it);
      cnum t = c_num(to);
      cnum l = c_num(len);

      memmove(vec_in->v.vec + t - c_num(len_diff),
              vec_in->v.vec + t,
              (l - t) * sizeof vec_in->v.vec);

      vec_in->v.vec[vec_length] = minus(len, len_diff);
      to = plus(from, len_it);
    } else if (lt(len_rep, len_it)) {
      val len_diff = minus(len_it, len_rep);
      cnum t = c_num(to);
      cnum l = c_num(len);

      vec_set_length(vec_in, plus(len, len_diff));

      memmove(vec_in->v.vec + t + c_num(len_diff),
              vec_in->v.vec + t,
              (l - t) * sizeof vec_in->v.vec);
      to = plus(from, len_it);
    }

    if (zerop(len_it))
      return vec_in;
    if (vectorp(it_seq)) {
      memcpy(vec_in->v.vec + c_num(from), it_seq->v.vec,
             sizeof *vec_in->v.vec * c_num(len_it));
      mut(vec_in);
    } else if (stringp(it_seq)) {
      cnum f = c_num(from);
      cnum t = c_num(to);
      cnum s;
      const wchar_t *str = c_str(it_seq);

      for (s = 0; f != t; f++, s++)
        vec_in->v.vec[f] = chr(str[s]);
    } else {
      val iter;
      cnum f = c_num(from);
      cnum t = c_num(to);

      for (iter = it_seq; iter && f != t; iter = cdr(iter), f++)
        vec_in->v.vec[f] = car(iter);
      mut(vec_in);
    }
  }

  return vec_in;
}

val cat_vec(val list)
{
  ucnum total = 0;
  val iter;
  val vec, *v;

  list = nullify(list);

  for (iter = list; iter != nil; iter = cdr(iter)) {
    ucnum newtot = total + c_unum(length_vec(car(iter)));
    if (newtot < total)
      goto toobig;
    total = newtot;
  }

  if (total + 2 < total)
    goto toobig;

  v = coerce(val *, chk_xalloc(total + 2, sizeof *v, lit("cat-vec")));
  vec = make_obj();
  vec->v.type = VEC;

#if HAVE_VALGRIND
  vec->v.vec_true_start = v;
#endif
  v += 2;
  vec->v.vec = v;
  v[vec_length] = v[vec_alloc] = num(total);

  for (iter = list; iter != nil; iter = cdr(iter)) {
    val item = car(iter);
    cnum len = c_num(item->v.vec[vec_length]);
    memcpy(v, item->v.vec, len * sizeof *v);
    v += len;
  }

  return vec;
toobig:
  uw_throwf(error_s, lit("cat-vec: resulting vector too large"), nao);
}

static val simple_lazy_stream_func(val stream, val lcons)
{
  if (set(mkloc(lcons->lc.car, lcons), get_line(stream)) != nil) {
    set(mkloc(lcons->lc.cdr, lcons), make_lazy_cons(lcons->lc.func));
  } else {
    close_stream(stream, t);
    lcons->lc.cdr = nil;
  }

  return nil;
}

static val lazy_stream_cont(val stream, val func, val env)
{
  val next = get_line(stream);

  if (!next) {
    close_stream(stream, t);
    return nil;
  }

  rplacd(env, next);
  return make_lazy_cons(func);
}

static val lazy_stream_func(val env, val lcons)
{
  val stream = car(env);
  val prefetched_line = cdr(env);

  set(mkloc(lcons->lc.car, lcons), prefetched_line);
  set(mkloc(lcons->lc.cdr, lcons), lazy_stream_cont(stream, lcons->lc.func, env));

  return prefetched_line;
}

val lazy_stream_cons(val stream)
{
  stream = default_arg(stream, std_input);

  if (real_time_stream_p(stream)) {
    return make_lazy_cons(func_f1(stream, simple_lazy_stream_func));
  } else {
    val first = get_line(stream);

    if (!first) {
      close_stream(stream, t);
      return nil;
    }

    return make_lazy_cons(func_f1(cons(stream, first),
                                  lazy_stream_func));
  }
}

val lazy_str(val lst, val term, val limit)
{
  val obj = make_obj();
  obj->ls.type = LSTR;

  /* Must init before calling something that can gc! */
  obj->ls.list = obj->ls.prefix = nil;
  obj->ls.props = coerce(struct lazy_string_props *,
                         chk_calloc(1, sizeof *obj->ls.props));

  term = default_arg(term, lit("\n"));
  limit = default_null_arg(limit);

  if (nilp(lst)) {
    obj->ls.prefix = null_string;
    obj->ls.list = nil;
  } else {
    val prefix = scat(nil, first(lst), term, nao);
    set(mkloc(obj->ls.prefix, obj), prefix);
    set(mkloc(obj->ls.list, obj), rest(lst));
    limit = if2(limit, minus(limit, one));
  }

  set(mkloc(obj->ls.props->term, obj), term);
  set(mkloc(obj->ls.props->limit, obj), limit);

  return obj;
}

static val copy_lazy_str(val lstr)
{
  val obj = make_obj();
  obj->ls.type = LSTR;
  obj->ls.prefix = nil;
  obj->ls.list = lstr->ls.list;
  obj->ls.props = coerce(struct lazy_string_props *,
                         chk_copy_obj(coerce(mem_t *, lstr->ls.props),
                                      sizeof *lstr->ls.props));
  obj->ls.prefix = copy_str(lstr->ls.prefix);
  return obj;
}

val lazy_str_force(val lstr)
{
  val lim, term, pfx;
  type_check(lstr, LSTR);
  lim = lstr->ls.props->limit;
  term = lstr->ls.props->term;
  pfx = lstr->ls.prefix;

  while ((!lim || gt(lim, zero)) && lstr->ls.list) {
    val next = pop(&lstr->ls.list);
    if (!next)
      break;
    string_extend(pfx, next);
    string_extend(pfx, term);
    if (lim)
      lim = minus(lim, one);
  }

  if (lim)
    set(mkloc(lstr->ls.props->limit, lstr), lim);

  return lstr->ls.prefix;
}

val lazy_str_put(val lstr, val stream)
{
  val lim, term, iter;
  type_check(lstr, LSTR);
  lim = lstr->ls.props->limit;
  term = lstr->ls.props->term;

  put_string(lstr->ls.prefix, stream);

  for (iter = lstr->ls.list; (!lim || gt(lim, zero)) && iter;
       iter = cdr(iter))
  {
    val str = car(iter);
    if (!str)
      break;
    if (lim)
      lim = pred(lim);
    put_string(str, stream);
    put_string(term, stream);
  }

  return t;
}

val lazy_str_force_upto(val lstr, val index)
{
  uses_or2;
  val lim, term, ltrm, pfx, len;
  type_check(lstr, LSTR);
  lim = lstr->ls.props->limit;
  term = lstr->ls.props->term;
  ltrm = length_str(term);
  pfx = lstr->ls.prefix;
  len = length_str(pfx);

  while (ge(index, len) && lstr->ls.list &&
         or2(null(lim),gt(lim,zero)))
  {
    val next = pop(&lstr->ls.list);
    if (!next)
      break;
    string_extend(pfx, next);
    string_extend(pfx, term);
    if (lim)
      lim = minus(lim, one);
    len = plus(len, length_str(next));
    len = plus(len, ltrm);
  }

  if (lim)
    set(mkloc(lstr->ls.props->limit, lstr), lim);

  return lt(index, len);
}

val length_str_gt(val str, val len)
{
  if (is_lit(str)) {
    const wchar_t *cstr = c_str(str);
    size_t clen = c_num(len);
    const wchar_t *nult = wmemchr(cstr, 0, clen + 1);
    return nult == 0 ? t : nil;
  } else {
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
}

val length_str_ge(val str, val len)
{
  if (is_lit(str)) {
    const wchar_t *cstr = c_str(str);
    size_t clen = c_num(len);
    const wchar_t *nult = wmemchr(cstr, 0, clen);
    return nult == 0 ? t : nil;
  } else {
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
}

val length_str_lt(val str, val len)
{
  if (is_lit(str)) {
    const wchar_t *cstr = c_str(str);
    size_t clen = c_num(len);
    const wchar_t *nult = wmemchr(cstr, 0, clen);
    return nult != 0 ? t : nil;
  } else {
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
}

val length_str_le(val str, val len)
{
  if (is_lit(str)) {
    const wchar_t *cstr = c_str(str);
    size_t clen = c_num(len);
    const wchar_t *nult = wmemchr(cstr, 0, clen + 1);
    return nult != 0 ? t : nil;
  } else {
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
}

val lazy_str_get_trailing_list(val lstr, val index)
{
  type_check(lstr, LSTR);

  /* Force lazy string up through the index position */
  if (ge(index, length_str(lstr->ls.prefix)))
    lazy_str_force_upto(lstr, index);

  {
    uses_or2;
    val split_suffix = split_str(sub_str(lstr->ls.prefix, index, nil),
                                 or2(lstr->ls.props->term, lit("\n")));

    if (!cdr(split_suffix) && equal(car(split_suffix), null_string))
      return lstr->ls.list;

    return nappend2(split_suffix, lstr->ls.list);
  }
}

val cobj(mem_t *handle, val cls_sym, struct cobj_ops *ops)
{
  val obj = make_obj();
  obj->co.type = COBJ;
  obj->co.handle = handle;
  obj->co.ops = ops;
  obj->co.cls = cls_sym;
  return obj;
}

val cobjp(val obj)
{
  return type(obj) == COBJ ? t : nil;
}

mem_t *cobj_handle(val cobj, val cls_sym)
{
  class_check(cobj, cls_sym);
  return cobj->co.handle;
}

struct cobj_ops *cobj_ops(val cobj, val cls_sym)
{
  class_check(cobj, cls_sym);
  return cobj->co.ops;
}

void cobj_print_op(val obj, val out, val pretty, struct strm_ctx *ctx)
{
  put_string(lit("#<"), out);
  obj_print_impl(obj->co.cls, out, pretty, ctx);
  format(out, lit(": ~p>"), coerce(val, obj->co.handle), nao);
}

void cptr_print_op(val obj, val out, val pretty, struct strm_ctx *ctx)
{
  put_string(lit("#<cptr"), out);
  if (obj->co.cls) {
    put_char(chr(' '), out);
    obj_print_impl(obj->co.cls, out, pretty, ctx);
  }
  format(out, lit(": ~p>"), coerce(val, obj->co.handle), nao);
}


val cobj_equal_handle_op(val left, val right)
{
  return (left->co.handle == right->co.handle) ? t : nil;
}

cnum cobj_handle_hash_op(val obj, int *count)
{
  mem_t *handle = obj->co.handle;
  return cobj_eq_hash_op(coerce(val, handle), count);
}

static struct cobj_ops cptr_ops = {
  cobj_equal_handle_op,
  cptr_print_op,
  cobj_destroy_stub_op,
  cobj_mark_op,
  cobj_handle_hash_op
};

val cptr_typed(mem_t *handle, val type_sym, struct cobj_ops *ops)
{
  val obj = make_obj();
  obj->co.type = CPTR;
  obj->co.handle = handle;
  obj->co.ops = (ops != 0 ? ops : &cptr_ops);
  obj->co.cls = type_sym;
  return obj;
}

val cptr(mem_t *ptr)
{
  return cptr_typed(ptr, nil, &cptr_ops);
}

val cptrp(val obj)
{
  return type(obj) == CPTR ? t : nil;
}

val cptr_type(val cptr)
{
  (void) cptr_handle(cptr, nil, lit("cptr-type"));
  return cptr->co.cls;
}

val cptr_int(val n, val type_sym_in)
{
  val type_sym = default_null_arg(type_sym_in);
  return cptr_typed(coerce(mem_t *, c_num(n)), type_sym, 0);
}

val cptr_obj(val obj, val type_sym_in)
{
  val type_sym = default_null_arg(type_sym_in);
  return cptr_typed(coerce(mem_t *, obj), type_sym, 0);
}

val cptr_zap(val cptr)
{
  (void) cptr_handle(cptr, nil, lit("cptr-zap"));
  cptr->co.handle = 0;
  return cptr;
}

val cptr_free(val cptr)
{
  (void) cptr_handle(cptr, nil, lit("cptr-free"));
  free(cptr->co.handle);
  cptr->co.handle = 0;
  return cptr;
}

val cptr_cast(val to_type, val cptr)
{
  mem_t *ptr = cptr_handle(cptr, nil, lit("cptr-cast"));
  return cptr_typed(ptr, to_type, 0);
}

val int_cptr(val cptr)
{
  return num(coerce(cnum, cptr_handle(cptr, nil, lit("int-cptr"))));
}

mem_t *cptr_handle(val cptr, val type_sym, val self)
{
  if (type(cptr) != CPTR) {
    uw_throwf(error_s, lit("~a: ~s isn't a cptr"), self, cptr, nao);
  } else {
    mem_t *ptr = cptr->co.handle;

    if (type_sym && cptr->co.cls != type_sym && (ptr != 0 || cptr->co.cls))
      uw_throwf(error_s, lit("~a: cptr ~s isn't of type ~s"), self, cptr,
                type_sym, nao);

    return ptr;
  }
}

mem_t *cptr_get(val cptr)
{
  return cptr_handle(cptr, nil, lit("cptr-get"));
}

mem_t **cptr_addr_of(val cptr, val type_sym, val self)
{
  (void) cptr_handle(cptr, type_sym, self);
  return &cptr->co.handle;
}

val assoc(val key, val list)
{
  list = nullify(list);

  while (list) {
    val elem = car(list);
    if (equal(car(elem), key))
      return elem;
    list = cdr(list);
  }

  return nil;
}

val assql(val key, val list)
{
  list = nullify(list);

  while (list) {
    val elem = car(list);
    if (eql(car(elem), key))
      return elem;
    list = cdr(list);
  }

  return nil;
}

val rassoc(val key, val list)
{
  list = nullify(list);

  while (list) {
    val elem = car(list);
    if (equal(cdr(elem), key))
      return elem;
    list = cdr(list);
  }

  return nil;
}

val rassql(val key, val list)
{
  list = nullify(list);

  while (list) {
    val elem = car(list);
    if (eql(cdr(elem), key))
      return elem;
    list = cdr(list);
  }

  return nil;
}

val acons(val car, val cdr, val list)
{
  return cons(cons(car, cdr), list);
}

val acons_new(val key, val value, val list)
{
  val existing = assoc(key, list);

  if (existing) {
    set(cdr_l(existing), value);
    return list;
  } else {
    return cons(cons(key, value), list);
  }
}

val acons_new_c(val key, loc new_p, loc list)
{
  val existing = assoc(key, deref(list));

  if (existing) {
    if (!nullocp(new_p))
      deref(new_p) = nil;
    return existing;
  } else {
    val nc = cons(key, nil);
    set(list, cons(nc, deref(list)));
    if (!nullocp(new_p))
      deref(new_p) = t;
    return nc;
  }
}

val aconsql_new(val key, val value, val list)
{
  val existing = assql(key, list);

  if (existing) {
    set(cdr_l(existing), value);
    return list;
  } else {
    return cons(cons(key, value), list);
  }
}

val aconsql_new_c(val key, loc new_p, loc list)
{
  val existing = assql(key, deref(list));

  if (existing) {
    if (!nullocp(new_p))
      deref(new_p) = nil;
    return existing;
  } else {
    val nc = cons(key, nil);
    set(list, cons(nc, deref(list)));
    if (!nullocp(new_p))
      deref(new_p) = t;
    return nc;
  }
}


static val alist_remove_test(val item, val key)
{
  return equal(car(item), key);
}

val alist_remove(val list, val keys)
{
  return set_diff(list, keys, func_n2(alist_remove_test), nil);
}

val alist_removev(val list, struct args *keys)
{
  return alist_remove(list, args_get_list(keys));
}

val alist_remove1(val list, val key)
{
  return alist_remove(list, cons(key, nil));
}

val alist_nremove(val list, val keys)
{
  loc plist = mkcloc(list);

  while (deref(plist)) {
    if (memqual(car(car(deref(plist))), keys))
      set(plist, cdr(deref(plist)));
    else
      plist = cdr_l(deref(plist));
  }

  return list;
}

val alist_nremovev(val list, struct args *keys)
{
  return alist_nremove(list, args_get_list(keys));
}

val alist_nremove1(val list, val key)
{
  loc plist = mkcloc(list);

  while (deref(plist)) {
    if (equal(car(car(deref(plist))), key))
      set(plist, cdr(deref(plist)));
    else
      plist = cdr_l(deref(plist));
  }

  return list;
}

val copy_cons(val c)
{
  return cons(car(c), cdr(c));
}

val copy_alist(val list)
{
  return mapcar(func_n1(copy_cons), list);
}

val mapcar_listout(val fun, val list)
{
  list_collect_decl (out, iter);

  list = nullify(list);

  for (; list; list = cdr(list))
    iter = list_collect(iter, funcall1(fun, car(list)));

  return out;
}

val mapcar(val fun, val list)
{
  return make_like(mapcar_listout(fun, list), list);
}

val mapcon(val fun, val list)
{
  list_collect_decl (out, iter);
  val list_orig = list;

  list = nullify(list);

  for (; list; list = cdr(list))
    iter = list_collect_nconc(iter, funcall1(fun, list));

  return make_like(out, list_orig);
}

val mappend(val fun, val list)
{
  list_collect_decl (out, iter);
  val list_orig = list;

  list = nullify(list);

  gc_hint(list);

  for (; list; list = cdr(list))
    iter = list_collect_append(iter, funcall1(fun, car(list)));

  return make_like(out, list_orig);
}

val mapdo(val fun, val list)
{
  list = nullify(list);

  gc_hint(list);

  for (; list; list = cdr(list))
    funcall1(fun, car(list));

  return nil;
}

static cnum calc_win_size(cnum ra)
{
  cnum ws = 2 * ra + 1;
  if (ra < 1)
    uw_throwf(error_s, lit("window-map: range must be nonnegative"), nao);
  if (ws > 1025)
    uw_throwf(error_s, lit("window-map: window size exceeds 1025"), nao);
  return ws;
}

static val window_map_list(val range, val boundary, val fun, val list, val app)
{
  cnum i, j, ra = c_num(range), ws = calc_win_size(ra);
  val iter;
  args_decl (args, ws);
  list_collect_decl (out, ptail);

  args_set_fill(args, ws);

  if (boundary == wrap_k) {
    val lcopy = take(range, list);
    while (lt(length(lcopy), range))
      lcopy = append2(lcopy, lcopy);
    boundary = append2(sub(lcopy, num_fast(-ra), t), sub(lcopy, zero, range));
  } else if (boundary == reflect_k) {
    val lcopy = take(range, list);
    while (lt(length(lcopy), range))
      lcopy = append2(lcopy, lcopy);
    boundary = nappend2(nreverse(sub(lcopy, zero, range)),
                        nreverse(sub(lcopy, num_fast(-ra), t)));
  }

  for (i = 0; i < ra; i++)
    args->arg[i] = ref(boundary, num_fast(i));

  for (iter = list; iter && i < ws; iter = cdr(iter), i++)
    args->arg[i] = car(iter);

  for (j = ra; i < ws; i++)
    args->arg[i] = ref(boundary, num_fast(j++));

  for (;;) {
    args_decl (args_cp, ws);

    args_copy(args_cp, args);

    ptail = if3(app,
                list_collect_append,
                list_collect)(ptail, generic_funcall(fun, args_cp));

    if (nilp(list = cdr(list)))
      break;

    for (i = 0; i < ws - 1; i++)
      args->arg[i] = args->arg[i + 1];

    if (iter) {
      args->arg[i] = car(iter);
      iter = cdr(iter);
    } else {
      args->arg[i] = ref(boundary, num_fast(j++));
    }
  }

  return out;
}

static val window_map_vec(val range, val boundary, val fun, val seq, val app)
{
  val list = tolist(seq);
  val out = window_map_list(range, boundary, fun, list, app);
  return make_like(out, seq);
}

val window_map(val range, val boundary, val fun, val seq)
{
  switch (type(seq)) {
  case NIL:
    return nil;
  case CONS:
  case LCONS:
    return window_map_list(range, boundary, fun, seq, nil);
  case VEC:
  case LIT:
  case STR:
  case LSTR:
    return window_map_vec(range, boundary, fun, seq, nil);
  default:
    type_mismatch(lit("window-map: ~s is not a sequence"), seq, nao);
  }
}

val window_mappend(val range, val boundary, val fun, val seq)
{
  switch (type(seq)) {
  case NIL:
    return nil;
  case CONS:
  case LCONS:
    return window_map_list(range, boundary, fun, seq, t);
  case VEC:
  case LIT:
  case STR:
  case LSTR:
    return window_map_vec(range, boundary, fun, seq, t);
  default:
    type_mismatch(lit("window-map: ~s is not a sequence"), seq, nao);
  }
}

static val lazy_interpose_func(val env, val lcons)
{
  cons_bind (sep, list, env);
  val next = cdr(list);
  val fun = lcons_fun(lcons);

  rplaca(lcons, car(list));

  if (next) {
    rplacd(env, next);
    func_set_env(fun, env);
    rplacd(lcons, cons(sep, make_lazy_cons(fun)));
  }

  return nil;
}

static val lazy_interpose(val sep, val list)
{
  return make_lazy_cons(func_f1(cons(sep, list),
                                lazy_interpose_func));
}

val interpose(val sep, val seq)
{
  switch (type(seq)) {
  case NIL:
    return nil;
  case CONS:
    {
      val next;
      list_collect_decl (out, ptail);
      for (next = cdr(seq); next; seq = next, next = cdr(seq)) {
        ptail = list_collect(ptail, car(seq));
        ptail = list_collect(ptail, sep);
        if (lconsp(next)) {
          list_collect_nconc(ptail, lazy_interpose(sep, next));
          return out;
        }
      }
      list_collect(ptail, car(seq));
      return out;
    }
  case LCONS:
    return lazy_interpose(sep, seq);
  case LIT:
  case STR:
  case LSTR:
    return cat_str(interpose(sep, tolist(seq)), nil);
  case VEC:
    return vec_list(interpose(sep, tolist(seq)));
  default:
    type_mismatch(lit("interpose: ~s is not a sequence"), seq, nao);
  }
}

val merge(val list1, val list2, val lessfun, val keyfun)
{
  list_collect_decl (out, ptail);

  while (list1 && list2) {
    val el1 = funcall1(keyfun, first(list1));
    val el2 = funcall1(keyfun, first(list2));

    if (funcall2(lessfun, el2, el1)) {
      loc pnext = cdr_l(list2);
      val next = deref(pnext);
      deref(pnext) = nil;
      ptail = list_collect_nconc(ptail, list2);
      list2 = next;
    } else {
      loc pnext = cdr_l(list1);
      val next = deref(pnext);
      deref(pnext) = nil;
      ptail = list_collect_nconc(ptail, list1);
      list1 = next;
    }
  }

  if (list1)
    ptail = list_collect_nconc(ptail, list1);
  else
    ptail = list_collect_nconc(ptail, list2);

  return out;
}

static val sort_list(val list, val lessfun, val keyfun)
{
  if (list == nil)
    return nil;
  if (!cdr(list))
    return list;
  if (!cdr(cdr(list))) {
    if (funcall2(lessfun, funcall1(keyfun, second(list)),
                          funcall1(keyfun, first(list))))
    {
      val cons2 = cdr(list);
      /* This assignment is a dangerous mutation since the list
         may contain mixtures of old and new objects, and
         so we could be reversing a newer->older pointer
         relationship. */
      set(cdr_l(cons2), list);
      deref(cdr_l(list)) = nil;
      return cons2;
    } else {
      return list;
    }
  }

  {
    val bisect, iter;
    val list2;

    for (iter = cdr(cdr(list)), bisect = list; iter;
         bisect = cdr(bisect), iter = cdr(cdr(iter)))
      ; /* empty */

    list2 = cdr(bisect);
    deref(cdr_l(bisect)) = nil;

    return merge(sort_list(list, lessfun, keyfun),
                 sort_list(list2, lessfun, keyfun),
                 lessfun, keyfun);
  }
}

static void swap(val vec, val i, val j)
{
  if (i != j) {
    val temp = ref(vec, i);
    refset(vec, i, ref(vec, j));
    refset(vec, j, temp);
  }
}

static cnum med_of_three(val vec, val lessfun, val keyfun, cnum from, cnum to,
                         val *pkval)
{
  cnum mid = from + (to - from) / 2;
  val fval = ref(vec, num_fast(from));
  val mval = ref(vec, num_fast(mid));
  val tval = ref(vec, num_fast(to - 1));
  val fkval = funcall1(keyfun, fval);
  val mkval = funcall1(keyfun, mval);
  val tkval = funcall1(keyfun, tval);

  if (funcall2(lessfun, fkval, mkval)) {
    if (funcall2(lessfun, mkval, tval)) {
      *pkval = mkval;
      return mid;
    } else if (funcall2(lessfun, fkval, tkval)) {
      *pkval = tkval;
      return to - 1;
    } else {
      *pkval = fkval;
      return from;
    }
  } else {
    if (funcall2(lessfun, fkval, tval)) {
      *pkval = fkval;
      return from;
    } else if (funcall2(lessfun, mkval, tkval)) {
      *pkval = tkval;
      return to - 1;
    } else {
      *pkval = mkval;
      return mid;
    }
  }
}

static cnum middle_pivot(val vec, val lessfun, val keyfun, cnum from, cnum to,
                         val *pkval)
{
    cnum pivot = from + (to - from) / 2;
    val pval = ref(vec, num_fast(pivot));
    *pkval = funcall1(keyfun, pval);
    return pivot;
}

static void quicksort(val vec, val lessfun, val keyfun, cnum from, cnum to)
{
  while (to - from >= 2) {
    val pkval;
    cnum i, j;
    cnum pivot = if3(to - from > 15,
                     med_of_three(vec, lessfun, keyfun, from, to, &pkval),
                     middle_pivot(vec, lessfun, keyfun, from, to, &pkval));

    swap(vec, num_fast(pivot), num_fast(to - 1));

    for (j = from, i = from; i < to - 1; i++)
      if (funcall2(lessfun, funcall1(keyfun, ref(vec, num_fast(i))), pkval))
        swap(vec, num_fast(i), num_fast(j++));

    swap(vec, num_fast(j), num_fast(to - 1));

    if (j - from > to - j) {
      quicksort(vec, lessfun, keyfun, j + 1, to);
      to = j;
    } else {
      quicksort(vec, lessfun, keyfun, from, j);
      from = j + 1;
    }
  }
}

static void sort_vec(val vec, val lessfun, val keyfun)
{
  cnum len = c_num(length(vec));
  quicksort(vec, lessfun, keyfun, 0, len);
}

val sort(val seq_in, val lessfun, val keyfun)
{
  val seq_orig = seq_in;
  val seq = nullify(seq_in);

  if (!seq)
    return make_like(nil, seq_orig);

  keyfun = default_arg(keyfun, identity_f);
  lessfun = default_arg(lessfun, less_f);

  if (consp(seq)) {
    /* The list could have a mixture of generation 0 and 1
       objects. Sorting the list could reverse some of the
       pointers between the generations resulting in a backpointer.
       Thus we better inform the collector about this object. */
    return sort_list(seq, lessfun, keyfun);
  }

  sort_vec(seq, lessfun, keyfun);
  return seq;
}

val shuffle(val seq)
{
  switch (type(seq)) {
  case NIL:
    return nil;
  case CONS:
  case LCONS:
    if (cdr(seq))
    {
      val v = shuffle(vec_list(seq));
      val i, l;

      for (l = seq, i = zero; l; i = succ(i), l = cdr(l))
        rplaca(l, ref(v, i));
    }
    return seq;
  case LIT:
    uw_throwf(error_s, lit("shuffle: ~s is a literal"), seq, nao);
  case STR:
  case LSTR:
  case VEC:
    {
      val rs = random_state;
      val n = length(seq);
      val i;

      if (n == zero || n == one)
        return seq;

      for (i = pred(n); ge(i, one); i = pred(i)) {
        val j = random(rs, succ(i));
        val t = ref(seq, i);
        refset(seq, i, ref(seq, j));
        refset(seq, j, t);
      }

      return seq;
    }
  default:
    type_mismatch(lit("shuffle: ~s is not a sequence"), seq, nao);
  }
}

static val multi_sort_less(val funcs_cons, val llist, val rlist)
{
  cons_bind (funcs, key_funcs, funcs_cons);
  val less = t;

  while (funcs) {
    val func = pop(&funcs);
    val test = pop(&key_funcs);
    val left = if3(test, funcall1(test, pop(&llist)), pop(&llist));
    val right = if3(test, funcall1(test, pop(&rlist)), pop(&rlist));

    if (funcall2(func, left, right))
      break;

    if (funcall2(func, right, left)) {
      less = nil;
      break;
    }
  }

  return less;
}

val multi_sort(val lists, val funcs, val key_funcs)
{
  val tuples = mapcarl(list_f, nullify(lists));

  key_funcs = default_null_arg(key_funcs);

  if (functionp(funcs))
    funcs = cons(funcs, nil);

  tuples = sort_list(tuples, func_f2(cons(funcs, key_funcs),
                                     multi_sort_less), identity_f);

  return mapcarl(list_f, tuples);
}

val sort_group(val seq, val keyfun, val lessfun)
{
  val kf = default_arg(keyfun, identity_f);
  val lf = default_arg(lessfun, less_f);
  val seq_copy = copy(seq);
  val sorted = sort(seq_copy, lf, kf);
  return partition_by(kf, sorted);
}

val unique(val seq, val keyfun, struct args *hashv_args)
{
  val hash = hashv(hashv_args);
  val kf = default_arg(keyfun, identity_f);

  list_collect_decl (out, ptail);

  if (vectorp(seq) || stringp(seq)) {
    cnum i, len;

    for (i = 0, len = c_num(length(seq)); i < len; i++) {
      val new_p;
      val v = ref(seq, num_fast(i));

      (void) gethash_c(hash, funcall1(kf, v), mkcloc(new_p));

      if (new_p)
        ptail = list_collect(ptail, v);
    }
  } else {
    for (; seq; seq = cdr(seq)) {
      val new_p;
      val v = car(seq);

      (void) gethash_c(hash, funcall1(kf, v), mkcloc(new_p));

      if (new_p)
        ptail = list_collect(ptail, v);
    }
  }

  return make_like(out, seq);
}

val uniq(val seq)
{
  args_decl(hashv_args, ARGS_MIN);
  args_add(hashv_args, equal_based_k);
  return unique(seq, identity_f, hashv_args);
}

val find(val item, val list, val testfun, val keyfun)
{
  testfun = default_arg(testfun, equal_f);
  keyfun = default_arg(keyfun, identity_f);

  list = nullify(list);

  gc_hint(list);

  for (; list; list = cdr(list)) {
    val elem = car(list);
    val key = funcall1(keyfun, elem);

    if (funcall2(testfun, item, key))
      return elem;
  }

  return nil;
}

val rfind(val item, val list, val testfun, val keyfun)
{
  val found = nil;
  testfun = default_arg(testfun, equal_f);
  keyfun = default_arg(keyfun, identity_f);

  list = nullify(list);

  gc_hint(list);

  for (; list; list = cdr(list)) {
    val elem = car(list);
    val key = funcall1(keyfun, elem);

    if (funcall2(testfun, item, key))
      found = elem;
  }

  return found;
}

val find_max(val seq, val testfun, val keyfun)
{

  seq = nullify(seq);

  if (!seq)
    return nil;

  testfun = default_arg(testfun, greater_f);
  keyfun = default_arg(keyfun, identity_f);

  switch (type(seq)) {
  case COBJ:
    if (seq->co.cls == hash_s) {
      val hiter = hash_begin(seq);
      val cell = hash_next(hiter);
      val maxelt = cell;
      val maxkey = if2(cell, funcall1(keyfun, cell));

      while (cell && (cell = hash_next(hiter))) {
        val key = funcall1(keyfun, cell);
        if (funcall2(testfun, key, maxkey)) {
          maxkey = key;
          maxelt = cell;
        }
      }

      return maxelt;
    }
    /* fallthrough */
  case CONS:
  case LCONS:
    {
      val maxelt = car(seq);
      val maxkey = funcall1(keyfun, maxelt);

      gc_hint(seq);

      for (seq = cdr(seq); seq; seq = cdr(seq)) {
        val elt = car(seq);
        val key = funcall1(keyfun, elt);
        if (funcall2(testfun, key, maxkey)) {
          maxkey = key;
          maxelt = elt;
        }
      }
      return maxelt;
    }
  case STR:
  case LSTR:
  case LIT:
  case VEC:
    {
      val maxelt = ref(seq, zero);
      val maxkey = funcall1(keyfun, maxelt);
      val len = length(seq);
      val i;

      for (i = zero; lt(i, len); i = succ(i)) {
        val elt = ref(seq, i);
        val key = funcall1(keyfun, elt);
        if (funcall2(testfun, key, maxkey)) {
          maxkey = key;
          maxelt = elt;
        }
      }
      return maxelt;
    }
  default:
    uw_throwf(error_s, lit("find-max: unsupported object ~s"), seq, nao);
  }
}

val find_min(val seq, val testfun, val keyfun)
{
  return find_max(seq, default_arg(testfun, less_f), keyfun);
}

val find_if(val pred, val list, val key)
{
  key = default_arg(key, identity_f);
  list = nullify(list);

  gc_hint(list);

  for (; list; list = cdr(list)) {
    val item = car(list);
    val subj = funcall1(key, item);

    if (funcall1(pred, subj))
      return item;
  }

  return nil;
}

val rfind_if(val pred, val list, val key)
{
  val found = nil;
  key = default_arg(key, identity_f);
  list = nullify(list);

  gc_hint(list);

  for (; list; list = cdr(list)) {
    val item = car(list);
    val subj = funcall1(key, item);

    if (funcall1(pred, subj))
      found = item;
  }

  return found;
}

val posqual(val obj, val list)
{
  val pos = zero;

  list = nullify(list);

  gc_hint(list);

  for (; list; list = cdr(list), pos = plus(pos, one))
    if (equal(car(list), obj))
      return pos;

  return nil;
}

val rposqual(val obj, val list)
{
  val pos = zero;
  val found = nil;

  list = nullify(list);

  gc_hint(list);

  for (; list; list = cdr(list), pos = plus(pos, one))
    if (equal(car(list), obj))
      found = pos;

  return found;
}

val posql(val obj, val list)
{
  val pos = zero;

  list = nullify(list);

  gc_hint(list);

  for (; list; list = cdr(list), pos = plus(pos, one))
    if (eql(car(list), obj))
      return pos;

  return nil;
}

val rposql(val obj, val list)
{
  val pos = zero;
  val found = nil;

  list = nullify(list);

  gc_hint(list);

  for (; list; list = cdr(list), pos = plus(pos, one))
    if (eql(car(list), obj))
      pos = found;

  return pos;
}

val posq(val obj, val list)
{
  val pos = zero;

  list = nullify(list);

  gc_hint(list);

  for (; list; list = cdr(list), pos = plus(pos, one))
    if (car(list) == obj)
      return pos;

  return nil;
}

val rposq(val obj, val list)
{
  val pos = zero;
  val found = nil;

  list = nullify(list);

  gc_hint(list);

  for (; list; list = cdr(list), pos = plus(pos, one))
    if (car(list) == obj)
      found = pos;

  return found;
}

val pos(val item, val list, val testfun, val keyfun)
{
  val pos = zero;
  testfun = default_arg(testfun, equal_f);
  keyfun = default_arg(keyfun, identity_f);
  list = nullify(list);

  gc_hint(list);

  for (; list; list = cdr(list), pos = plus(pos, one)) {
    val elem = car(list);
    val key = funcall1(keyfun, elem);

    if (funcall2(testfun, item, key))
      return pos;
  }

  return nil;
}

val rpos(val item, val list, val testfun, val keyfun)
{
  val pos = zero;
  val found = nil;
  testfun = default_arg(testfun, equal_f);
  keyfun = default_arg(keyfun, identity_f);
  list = nullify(list);

  gc_hint(list);

  for (; list; list = cdr(list), pos = plus(pos, one)) {
    val elem = car(list);
    val key = funcall1(keyfun, elem);

    if (funcall2(testfun, item, key))
      found = pos;
  }

  return found;
}

val pos_if(val pred, val list, val key)
{
  val pos = zero;
  key = default_arg(key, identity_f);
  list = nullify(list);

  gc_hint(list);

  for (; list; list = cdr(list), pos = plus(pos, one)) {
    val item = car(list);
    val subj = funcall1(key, item);

    if (funcall1(pred, subj))
      return pos;
  }

  return nil;
}

val rpos_if(val pred, val list, val key)
{
  val pos = zero;
  val found = nil;
  key = default_arg(key, identity_f);
  list = nullify(list);

  gc_hint(list);

  for (; list; list = cdr(list), pos = plus(pos, one)) {
    val item = car(list);
    val subj = funcall1(key, item);

    if (funcall1(pred, subj))
      found = pos;
  }

  return found;
}

val pos_max(val seq, val testfun, val keyfun)
{
  val pos = zero;
  val maxkey;
  val maxpos = zero;

  seq = nullify(seq);

  if (!seq)
    return nil;

  gc_hint(seq);

  testfun = default_arg(testfun, greater_f);
  keyfun = default_arg(keyfun, identity_f);

  maxkey = funcall1(keyfun, car(seq));

  for (seq = cdr(seq); seq; seq = cdr(seq)) {
    val key = funcall1(keyfun, car(seq));
    pos = plus(pos, one);
    if (funcall2(testfun, key, maxkey)) {
      maxkey = key;
      maxpos = pos;
    }
  }

  return maxpos;
}

val pos_min(val seq, val testfun, val keyfun)
{
  return pos_max(seq, default_arg(testfun, less_f), keyfun);
}

val mismatch(val left, val right, val testfun_in, val keyfun_in)
{
  val testfun = default_arg(testfun_in, equal_f);
  val keyfun = default_arg(keyfun_in, identity_f);

  switch (type(left)) {
  case NIL:
    switch (type(right)) {
    case NIL:
      return nil;
    case CONS:
    case LCONS:
      return zero;
    case VEC:
    case LIT:
    case STR:
      return if3(length(right) == zero, nil, zero);
    case LSTR:
      return if3(length_str_lt(right, one), nil, zero);
    default:
      break;
    }
    break;
  case CONS:
  case LCONS:
  default:
    switch (type(right)) {
    case NIL:
      return zero;
    case CONS:
    case LCONS:
      {
        val pos = zero;

        gc_hint(left);
        gc_hint(right);

        for (; !endp(left) && !endp(right);
             left = cdr(left), right = cdr(right), pos = succ(pos))
        {
          val lelt = funcall1(keyfun, car(left));
          val relt = funcall1(keyfun, car(right));
          if (!funcall2(testfun, lelt, relt))
            break;
        }

        return if3(left || right, pos, nil);
      }
    case VEC:
    case LIT:
    case STR:
    case LSTR:
      {
        val pos = zero;
        val rlen = length(right);

        gc_hint(left);

        for (; !endp(left) && lt(pos, rlen);
             left = cdr(left), pos = succ(pos))
        {
          val lelt = funcall1(keyfun, car(left));
          val relt = funcall1(keyfun, ref(right, pos));
          if (!funcall2(testfun, lelt, relt))
            break;
        }

        return if3(left || lt(pos, rlen), pos, nil);
      }
    default:
      break;
    }
    break;
  case STR:
  case LSTR:
  case LIT:
  case VEC:
    switch (type(right)) {
    case NIL:
      return if3(length(left) == zero, nil, zero);
    case CONS:
    case LCONS:
      return mismatch(right, left, testfun, keyfun);
    case VEC:
    case LIT:
    case STR:
    case LSTR:
      {
        val llen = length(left);
        val rlen = length(right);
        val pos = zero;

        for (; lt(pos, llen) && lt(pos, rlen); pos = succ(pos))
        {
          val lelt = funcall1(keyfun, ref(left, pos));
          val relt = funcall1(keyfun, ref(right, pos));
          if (!funcall2(testfun, lelt, relt))
            break;
        }

        return if3(lt(pos, llen) || lt(pos, rlen), pos, nil);
      }
    default:
      break;
    }
    break;
  }

  uw_throwf(error_s, lit("mismatch: invalid arguments ~!~s and ~s"),
            left, right, nao);
}

val rmismatch(val left, val right, val testfun_in, val keyfun_in)
{
  val testfun = default_arg(testfun_in, equal_f);
  val keyfun = default_arg(keyfun_in, identity_f);

  switch (type(left)) {
  case NIL:
    switch (type(right)) {
    case NIL:
      return nil;
    case CONS:
    case LCONS:
      return negone;
    case VEC:
    case LIT:
    case STR:
      return if3(length(right) == zero, nil, negone);
    case LSTR:
      return if3(length_str_lt(right, one), nil, negone);
    default:
      break;
    }
    break;
  case CONS:
  case LCONS:
  default:
    switch (type(right)) {
    case NIL:
      return negone;
    case CONS:
    case LCONS:
      {
        val mm = mismatch(reverse(left), reverse(right), testfun, keyfun);
        return if2(mm, minus(negone, mm));
      }
    case VEC:
    case LIT:
    case STR:
    case LSTR:
      {
        val rleft = reverse(left);
        val rlen = length(right);
        val rpos = pred(rlen);

        for (; !endp(rleft) && !minusp(rpos);
             rleft = cdr(rleft), rpos = pred(rpos))
        {
          val lelt = funcall1(keyfun, car(rleft));
          val relt = funcall1(keyfun, ref(right, rpos));
          if (!funcall2(testfun, lelt, relt))
            break;
        }

        return if2(rleft || !minusp(rpos), minus(rpos, rlen));
      }
    default:
      break;
    }
    break;
  case STR:
  case LSTR:
  case LIT:
  case VEC:
    switch (type(right)) {
    case NIL:
      return if3(length(left) == zero, nil, zero);
    case CONS:
    case LCONS:
      return rmismatch(right, left, testfun, keyfun);
    case VEC:
    case LIT:
    case STR:
    case LSTR:
      {
        val llen = length(left);
        val rlen = length(right);
        val lpos = pred(llen);
        val rpos = pred(rlen);

        for (; !minusp(lpos) && !minusp(rpos);
             lpos = pred(lpos), rpos = pred(rpos))
        {
          val lelt = funcall1(keyfun, ref(left, lpos));
          val relt = funcall1(keyfun, ref(right, rpos));
          if (!funcall2(testfun, lelt, relt))
            break;
        }

        return if2(!minusp(lpos) || !minusp(rpos), minus(lpos, llen));
      }
    default:
      break;
    }
    break;
  }

  uw_throwf(error_s, lit("rmismatch: invalid arguments ~!~s and ~s"),
            left, right, nao);
}

val starts_with(val little, val big, val testfun, val keyfun)
{
  val mm = mismatch(little, big, testfun, keyfun);
  return tnil(!mm || eql(mm, length(little)));
}

val ends_with(val little, val big, val testfun, val keyfun)
{
  val mm = rmismatch(little, big, testfun, keyfun);
  return tnil(!mm || eql(pred(neg(mm)), length(little)));
}

static val take_list_fun(val env, val lcons)
{
  cons_bind (list, count, env);

  rplaca(lcons, pop(&list));

  if3(le((count = pred(count)), zero) || list == nil,
      rplacd(lcons, nil),
      rplacd(lcons, make_lazy_cons(lcons_fun(lcons))));

  rplaca(env, list);
  rplacd(env, count);
  return nil;
}

val take(val count, val seq)
{
  switch (type(seq)) {
  case NIL:
    return nil;
  case CONS:
  case LCONS:
    if (le(count, zero))
      return nil;
    return make_lazy_cons(func_f1(cons(seq, count), take_list_fun));
  case LSTR:
  case LIT:
  case STR:
  case VEC:
    return sub(seq, zero, count);
  default:
    type_mismatch(lit("take: ~s is not a sequence"), seq, nao);
  }
}

static val take_while_list_fun(val env, val lcons)
{
  cons_bind (list, cell, env);
  cons_bind (pred, keyfun, cell);

  rplaca(lcons, pop(&list));

  if (!list || !funcall1(pred, funcall1(keyfun, car(list))))
    rplacd(lcons, nil);
  else
    rplacd(lcons, make_lazy_cons(lcons_fun(lcons)));

  rplaca(env, list);
  return nil;
}

val take_while(val pred, val seq, val keyfun)
{
  switch (type(seq)) {
  case NIL:
    return nil;
  case CONS:
  case LCONS:
    keyfun = default_arg(keyfun, identity_f);
    if (!funcall1(pred, funcall1(keyfun, (car(seq)))))
      return nil;
    return make_lazy_cons(func_f1(cons(seq, cons(pred, keyfun)),
                                  take_while_list_fun));
  case LSTR:
  case LIT:
  case STR:
  case VEC:
    {
      val pos = pos_if(notf(pred), seq, keyfun);
      if (!pos)
        return seq;
      return sub(seq, zero, pos);
    }
  default:
    type_mismatch(lit("take-while: ~s is not a sequence"), seq, nao);
  }
}

static val take_until_list_fun(val env, val lcons)
{
  cons_bind (list, cell, env);
  cons_bind (pred, keyfun, cell);
  val item = pop(&list);

  rplaca(lcons, item);

  if (!list || funcall1(pred, funcall1(keyfun, item)))
    rplacd(lcons, nil);
  else
    rplacd(lcons, make_lazy_cons(lcons_fun(lcons)));

  rplaca(env, list);
  return nil;
}

val take_until(val pred, val seq, val keyfun)
{
  switch (type(seq)) {
  case NIL:
    return nil;
  case CONS:
  case LCONS:
    keyfun = default_arg(keyfun, identity_f);
    return make_lazy_cons(func_f1(cons(seq, cons(pred, keyfun)),
                                  take_until_list_fun));
  case LSTR:
  case LIT:
  case STR:
  case VEC:
    {
      val pos = pos_if(pred, seq, keyfun);
      if (!pos)
        return seq;
      return sub(seq, zero, succ(pos));
    }
  default:
    type_mismatch(lit("take-until: ~s is not a sequence"), seq, nao);
  }
}

val drop(val count, val seq)
{
  if (le(count, zero))
    return seq;
  return sub(seq, count, t);
}

val drop_while(val pred, val seq, val keyfun)
{
  switch (type(seq)) {
  case NIL:
    return nil;
  case CONS:
  case LCONS:
    keyfun = default_arg(keyfun, identity_f);
    while (seq && funcall1(pred, funcall1(keyfun, car(seq))))
      pop(&seq);
    return seq;
  case LSTR:
  case LIT:
  case STR:
  case VEC:
    {
      val pos = pos_if(notf(pred), seq, keyfun);
      if (!pos)
        return make_like(nil, seq);
      return sub(seq, pos, t);
    }
  default:
    type_mismatch(lit("drop-while: ~s is not a sequence"), seq, nao);
  }
}

val drop_until(val pred, val seq, val keyfun)
{
  switch (type(seq)) {
  case NIL:
    return nil;
  case CONS:
  case LCONS:
    {
      val key = default_arg(keyfun, identity_f);
      val item;

      do {
        item = pop(&seq);
      } while (!funcall1(pred, funcall1(key, item)));

      return seq;
    }
  case LSTR:
  case LIT:
  case STR:
  case VEC:
    {
      val pos = pos_if(pred, seq, keyfun);
      if (!pos)
        return seq;
      return sub(seq, succ(pos), t);
    }
  default:
    type_mismatch(lit("drop-until: ~s is not a sequence"), seq, nao);
  }
}

val in(val seq, val item, val testfun, val keyfun)
{
  switch (type(seq)) {
  case NIL:
    return nil;
  case COBJ:
    if (seq->co.cls == hash_s) {
      val found;
      gethash_f(seq, item, mkcloc(found));
      return if3(found, t, nil);
    }
    /* fallthrough */
  case CONS:
  case LCONS:
    {
      testfun = default_arg(testfun, equal_f);
      keyfun = default_arg(keyfun, identity_f);

      seq = nullify(seq);

      gc_hint(seq);

      for (; seq; seq = cdr(seq)) {
        val elem = car(seq);
        val key = funcall1(keyfun, elem);

        if (funcall2(testfun, item, key))
          return t;
      }

      return nil;
    }
  case LIT:
  case STR:
  case LSTR:
    {
      testfun = default_arg(testfun, equal_f);
      keyfun = default_arg(keyfun, identity_f);
      val len = length_str(seq);
      val ind;

      for (ind = zero; lt(ind, len); ind = plus(ind, one)) {
        val elem = chr_str(seq, ind);
        val key = funcall1(keyfun, elem);

        if (funcall2(testfun, item, key))
          return t;
      }

      return nil;
    }
  case VEC:
    {
      testfun = default_arg(testfun, equal_f);
      keyfun = default_arg(keyfun, identity_f);
      val len = length_vec(seq);
      val ind;

      for (ind = zero; lt(ind, len); ind = plus(ind, one)) {
        val elem = vecref(seq, ind);
        val key = funcall1(keyfun, elem);

        if (funcall2(testfun, item, key))
          return t;
      }

      return nil;
    }
  default:
    type_mismatch(lit("in: ~s is not a sequence or hash"), seq, nao);
  }
}

val set_diff(val list1, val list2, val testfun, val keyfun)
{
  list_collect_decl (out, ptail);
  val list_orig = list1;

  list1 = nullify(list1);
  list2 = nullify(list2);

  testfun = default_arg(testfun, equal_f);
  keyfun = default_arg(keyfun, identity_f);

  for (; list1; list1 = cdr(list1)) {
    /* optimization: list2 is a tail of list1, and so we
       are done, unless the application has a weird test function. */
    if (list1 == list2) {
      break;
    } else {
      val item = car(list1);
      val list1_key = funcall1(keyfun, item);

      if (!member(list1_key, list2, testfun, keyfun))
        ptail = list_collect(ptail, item);
    }
  }

  return make_like(out, list_orig);
}

val copy(val seq)
{
  switch (type(seq)) {
  case NIL:
    return nil;
  case CONS:
  case LCONS:
    return copy_list(seq);
  case LIT:
  case STR:
  case LSTR:
    return copy_str(seq);
  case VEC:
    return copy_vec(seq);
  case COBJ:
    if (seq->co.cls == hash_s)
      return copy_hash(seq);
    if (seq->co.cls == random_state_s)
      return make_random_state(seq, nil);
    if (seq->co.cls == carray_s)
      return copy_carray(seq);
    if (structp(seq))
      return copy_struct(seq);
    /* fallthrough */
  default:
    type_mismatch(lit("copy: cannot copy object of type ~s"),
                  typeof(seq), nao);
  }
}

static val length_proper_list(val list);

val length(val seq)
{
  switch (type(seq)) {
  case NIL:
    return num(0);
  case CONS:
  case LCONS:
    return length_list(seq);
  case LIT:
  case STR:
  case LSTR:
    return length_str(seq);
  case VEC:
    return length_vec(seq);
  case RNG:
    return minus(to(seq), from(seq));
  case BUF:
    return length_buf(seq);
  case COBJ:
    if (seq->co.cls == hash_s)
      return hash_count(seq);
    if (seq->co.cls == carray_s)
      return length_carray(seq);
    if (structp(seq)) {
      val length_meth = maybe_slot(seq, length_s);

      if (length_meth)
        return funcall1(length_meth, seq);

      if (maybe_slot(seq, car_s))
        return length_proper_list(nullify(seq));

      type_mismatch(lit("length: ~s has no length or car method"), seq, nao);
    }
    /* fallthrough */
  default:
    type_mismatch(lit("length: ~s is not a sequence"), seq, nao);
  }
}

val empty(val seq)
{
  switch (type(seq)) {
  case NIL:
    return t;
  case CONS:
  case LCONS:
    return nil;
  case LIT:
  case STR:
    return if2(c_str(seq)[0] == 0, t);
  case LSTR:
    return length_str_le(seq, zero);
  case VEC:
    return eq(length_vec(seq), zero);
  case RNG:
    return eql(from(seq), to(seq));
  case COBJ:
    if (seq->co.cls == hash_s)
      return eq(hash_count(seq), zero);
    if (structp(seq)) {
      val length_meth = maybe_slot(seq, length_s);
      val nullify_meth = if2(nilp(length_meth), maybe_slot(seq, nullify_s));
      if (length_meth)
        return eq(funcall1(length_meth, seq), zero);
      return if3(nullify_meth && funcall1(nullify_meth, seq), nil, seq);
    }
  default:
    type_mismatch(lit("empty: ~s is not a sequence"), seq, nao);
  }
}

val sub(val seq, val from, val to)
{
  switch (type(seq)) {
  case NIL:
    return nil;
  case COBJ:
    if (seq->co.cls == carray_s)
      return carray_sub(seq, from, to);
    seq = nullify(seq);
    /* fallthrough */
  case CONS:
  case LCONS:
    return sub_list(seq, from, to);
  case LIT:
  case STR:
  case LSTR:
    return sub_str(seq, from, to);
  case VEC:
    return sub_vec(seq, from, to);
  default:
    type_mismatch(lit("sub: ~s is not a sequence"), seq, nao);
  }
}

val ref(val seq, val ind)
{
  switch (type(seq)) {
  case NIL:
    return nil;
  case COBJ:
    if (seq->co.cls == hash_s)
      return gethash(seq, ind);
    if (seq->co.cls == carray_s)
      return carray_ref(seq, ind);
    if (structp(seq)) {
      val lambda_meth = maybe_slot(seq, lambda_s);
      if (lambda_meth)
        return funcall2(lambda_meth, seq, ind);
    }
    /* fallthrough */
  case CONS:
  case LCONS:
    return listref(seq, ind);
  case LIT:
  case STR:
  case LSTR:
    return chr_str(seq, ind);
  case VEC:
    return vecref(seq, ind);
  case BUF:
    return buf_get_uchar(seq, ind);
  default:
    type_mismatch(lit("ref: ~s is not a sequence"), seq, nao);
  }
}

val refset(val seq, val ind, val newval)
{
  switch (type(seq)) {
  case NIL:
  case CONS:
  case LCONS:
    return set(listref_l(seq, ind), newval);
  case LIT:
  case STR:
  case LSTR:
    return chr_str_set(seq, ind, newval);
  case VEC:
    return set(vecref_l(seq, ind), newval);
  case BUF:
    return buf_put_uchar(seq, ind, newval);
  case COBJ:
    if (seq->co.cls == hash_s)
      return sethash(seq, ind, newval);
    if (seq->co.cls == carray_s)
      return carray_refset(seq, ind, newval);
    if (structp(seq)) {
      val lambda_set_meth = maybe_slot(seq, lambda_set_s);
      if (lambda_set_meth)
        return funcall3(lambda_set_meth, seq, ind, newval);
    }
    /* fallthrough */
  default:
    type_mismatch(lit("ref: ~s is not a sequence"), seq, nao);
  }
  return newval;
}

val replace(val seq, val items, val from, val to)
{
  switch (type(seq)) {
  case NIL:
  case CONS:
  case LCONS:
    return replace_list(seq, items, from, to);
  case LIT:
  case STR:
  case LSTR:
    return replace_str(seq, items, from, to);
  case VEC:
    return replace_vec(seq, items, from, to);
  case COBJ:
    if (seq->co.cls == carray_s)
      return carray_replace(seq, items, from, to);
    /* fallthrough */
  default:
    type_mismatch(lit("replace: ~s is not a sequence"), seq, nao);
  }
}

val dwim_set(val place_p, val seq, varg vargs)
{
  switch (type(seq)) {
  case COBJ:
    if (type(seq) == COBJ) {
      if (seq->co.cls == hash_s) {
        cnum nva = args_count(vargs);

        if (nva < 2)
          uw_throwf(error_s, lit("sethash: missing required arguments"), nao);

        if (nva > 3)
            uw_throwf(error_s, lit("sethash: too many arguments"), nao);

        if (nva == 2) {
          args_normalize(vargs, 2);
          (void) sethash(seq, vargs->arg[0], vargs->arg[1]);
        } else {
          args_normalize(vargs, 3);
          (void) sethash(seq, vargs->arg[0], vargs->arg[2]);
        }

        return seq;
      }
      if (structp(seq)) {
        (void) funcall(method_args(seq, lambda_set_s, vargs));
        return seq;
      }
    }
    /* fallthrough */
  default:
    {
      cnum index = 0;
      val ind_range, newval;
      if (!args_two_more(vargs, 0))
        uw_throwf(error_s, lit("index/range assignment: missing required arguments"), nao);
      ind_range = args_get(vargs, &index);
      newval = args_get(vargs, &index);

      switch (type(ind_range)) {
      case NIL:
      case CONS:
      case LCONS:
      case VEC:
        if (!place_p && listp(seq))
          goto notplace;
        return replace(seq, newval, ind_range, colon_k);
      case RNG:
        {
          range_bind (x, y, ind_range);
          if (!place_p && listp(seq))
            goto notplace;
          return replace(seq, newval, x, y);
        }
      default:
        (void) refset(seq, ind_range, newval);
        return seq;
      }
    }
  }
notplace:
  uw_throwf(error_s, lit("range assignment: list form must be place"), nao);
}

val dwim_del(val place_p, val seq, val ind_range)
{
  switch (type(seq)) {
  case NIL:
  case CONS:
  case LCONS:
    if (!place_p)
      uw_throwf(error_s, lit("index/range delete: list form must be place"),
                nao);
    break;
  case COBJ:
    if (seq->co.cls == hash_s) {
      (void) remhash(seq, ind_range);
      return seq;
    }
    if (structp(seq))
      uw_throwf(error_s, lit("index/range delete: not supported for structs"),
                nao);
  default:
    break;
  }

  if (rangep(ind_range)) {
    return replace(seq, nil, from(ind_range), to(ind_range));
  } else {
    return replace(seq, nil, ind_range, succ(ind_range));
  }
}

val butlast(val seq, val idx)
{
  if (listp(seq)) {
    return butlastn(default_arg(idx, one), seq);
  } else {
    val nidx = if3(null_or_missing_p(idx), negone, neg(idx));
    return sub(seq, zero, if3(plusp(nidx), zero, nidx));
  }
}

val update(val seq, val fun)
{
  switch (type(seq)) {
  case NIL:
    break;
  case CONS:
  case LCONS:
    {
      val iter = seq;

      while (consp(iter)) {
        rplaca(iter, funcall1(fun, car(iter)));
        iter = cdr(iter);
      }
    }
    break;
  case LIT:
  case STR:
  case LSTR:
  case VEC:
    {
      val len = length(seq);
      val i;
      for (i = zero; lt(i, len); i = plus(i, one))
        refset(seq, i, funcall1(fun, ref(seq, i)));
    }
    break;
  case COBJ:
    if (hashp(seq))
      return hash_update(seq, fun);
    /* fallthrough */
  default:
    type_mismatch(lit("update: ~s is not a sequence"), seq, nao);
  }

  return seq;
}

static val search_list(val seq, val key, val testfun, val keyfun)
{
  val siter, kiter;
  val pos = zero;

  switch (type(key)) {
  case NIL:
    return pos;
  case CONS:
  case LCONS:
  case LIT:
  case STR:
  case LSTR:
  case VEC:
    /* TODO: optimize me */
    gc_hint(seq);

    for (; seq; seq = cdr(seq)) {
      for (siter = seq, kiter = key;
           siter && kiter;
           siter = cdr(siter), kiter = cdr(kiter))
      {
        if (!funcall2(testfun,
                      funcall1(keyfun, car(siter)),
                      funcall1(keyfun, car(kiter))))
        {
          pos = plus(pos, one);
          break;
        }
      }

      if (!kiter)
        return pos;

      if (!siter)
        break;
    }
    break;
  default:
    type_mismatch(lit("search: ~s is not a sequence"), seq, nao);
  }

  return nil;
}

val search(val seq, val key, val testfun, val keyfun)
{
  testfun = default_arg(testfun, equal_f);
  keyfun = default_arg(keyfun, identity_f);
  seq = nullify(seq);

  switch (type(seq)) {
  case NIL:
    return if3(length(key) == zero, zero, nil);
  case CONS:
  case LCONS:
  case LIT:
  case STR:
  case LSTR:
  case VEC:
  case COBJ:
    /* TODO: optimize me */
    return search_list(seq, key, testfun, keyfun);
  default:
    type_mismatch(lit("search: ~s is not a sequence"), seq, nao);
  }
}

static val rsearch_list(val seq, val key, val testfun, val keyfun)
{
  val siter, kiter;
  val pos = zero;
  val found = nil;

  switch (type(key)) {
  case NIL:
    return pos;
  case CONS:
  case LCONS:
  case LIT:
  case STR:
  case LSTR:
  case VEC:
    /* TODO: optimize me */
    gc_hint(seq);

    for (; seq; seq = cdr(seq)) {
      for (siter = seq, kiter = key;
           siter && kiter;
           siter = cdr(siter), kiter = cdr(kiter))
      {
        if (!funcall2(testfun,
                      funcall1(keyfun, car(siter)),
                      funcall1(keyfun, car(kiter))))
        {
          pos = plus(pos, one);
          break;
        }
      }

      if (!kiter)
        found = pos;

      if (!siter)
        break;
    }
    break;
  default:
    type_mismatch(lit("rsearch: ~s is not a sequence"), seq, nao);
  }

  return found;
}

val rsearch(val seq, val key, val testfun, val keyfun)
{
  testfun = default_arg(testfun, equal_f);
  keyfun = default_arg(keyfun, identity_f);
  seq = nullify(seq);

  switch (type(seq)) {
  case NIL:
    return if3(length(key) == zero, zero, nil);
  case CONS:
  case LCONS:
  case LIT:
  case STR:
  case LSTR:
  case VEC:
  case COBJ:
    /* TODO: optimize me */
    return rsearch_list(seq, key, testfun, keyfun);
  default:
    type_mismatch(lit("rsearch: ~s is not a sequence"), seq, nao);
  }
}

val where(val func, val seq)
{
  list_collect_decl (out, ptail);

  if (opt_compat && opt_compat <= 100) {
    val f = seq, s = func;
    func = s;
    seq = f;
  }

  seq = nullify(seq);

  switch (type(seq)) {
  case COBJ:
    if (seq->co.cls == hash_s) {
      val hiter = hash_begin(seq);
      val cell;

      while ((cell = hash_next(hiter)))
        if (funcall1(func, cdr(cell)))
          ptail = list_collect(ptail, car(cell));
      break;
    }
    /* fallthrough */
  case NIL:
  case CONS:
  case LCONS:
    {
      val idx = zero;

      gc_hint(seq);

      for (; seq; seq = cdr(seq), idx = plus(idx, one)) {
        val elt = car(seq);
        if (funcall1(func, elt))
          ptail = list_collect(ptail, idx);
      }
    }
    break;
  case LIT:
  case STR:
  case LSTR:
  case VEC:
    {
      val idx;
      val len = length(seq);

      gc_hint(seq);

      for (idx = zero; lt(idx, len); idx = plus(idx, one)) {
        val elt = ref(seq, idx);
        if (funcall1(func, elt))
          ptail = list_collect(ptail, idx);
      }
    }
    break;
  default:
    type_mismatch(lit("where: ~s is not a sequence"), seq, nao);
  }

  return out;
}

val sel(val seq_in, val where_in)
{
  list_collect_decl (out, ptail);
  val seq = nullify(seq_in);
  val where = if3(functionp(where_in),
                  funcall1(where_in, seq),
                  nullify(where_in));

  switch (type(seq)) {
  case NIL:
    return nil;
  case COBJ:
    if (hashp(seq))
    {
      val newhash = make_similar_hash(seq);

      for (; where; where = cdr(where)) {
        val found;
        loc pfound = mkcloc(found);
        val key = car(where);
        val value = gethash_f(seq, key, pfound);

        if (found)
          sethash(newhash, key, value);
      }

      return newhash;
    }
    if (seq->co.cls == carray_s)
      goto carray;
    /* fallthrough */
  case CONS:
  case LCONS:
    {
      val idx = zero;

      for (; seq && where; seq = cdr(seq), idx = plus(idx, one)) {
        val wh = nil;

        for (; where && lt(wh = car(where), idx); where = cdr(where))
          ; /* empty */

        if (eql(wh, idx))
          ptail = list_collect (ptail, car(seq));
      }
    }
    break;
  carray:
  default:
    {
      val len = length(seq);
      for (; where; where = cdr(where)) {
        val wh = car(where);
        if (ge(wh, len))
          break;
        ptail = list_collect (ptail, ref(seq, car(where)));
      }
    }
    break;
  }
  return make_like(out, seq_in);
}

static val do_relate(val env, val arg)
{
  cons_bind (dom, rng, env);
  val pos = posqual(arg, dom);
  return if3(pos, ref(rng, pos), arg);
}

static val do_relate_dfl(val env, val arg)
{
  val dom = env->v.vec[0];
  val rng = env->v.vec[1];
  val dfl = env->v.vec[2];
  val pos = posqual(arg, dom);
  return if3(pos, ref(rng, pos), dfl);
}


val relate(val domain_seq, val range_seq, val dfl_val)
{
  return if3(missingp(dfl_val),
             func_f1(cons(domain_seq, range_seq), do_relate),
             func_f1(vec(domain_seq, range_seq, dfl_val, nao),
                     do_relate_dfl));
}

val rcons(val from, val to)
{
  val obj = make_obj();
  obj->rn.type = RNG;
  obj->rn.from = from;
  obj->rn.to = to;
  return obj;
}

val rangep(val obj)
{
  return type(obj) == RNG ? t : nil;
}

val from(val range)
{
  type_check(range, RNG);
  return range->rn.from;
}

val to(val range)
{
  type_check(range, RNG);
  return range->rn.to;
}

val set_from(val range, val from)
{
  type_check(range, RNG);
  set(mkloc(range->rn.from, range), from);
  return range;
}

val set_to(val range, val to)
{
  type_check(range, RNG);
  set(mkloc(range->rn.to, range), to);
  return range;
}

val env(void)
{
  if (env_list) {
    return env_list;
  } else {
    list_collect_decl (out, ptail);
#if HAVE_ENVIRON
    extern char **environ;
    char **iter = environ;

    for (; *iter != 0; iter++)
      ptail = list_collect(ptail, string_utf8(*iter));

    return env_list = out;
#elif HAVE_GETENVIRONMENTSTRINGS
    wchar_t *env = GetEnvironmentStringsW();
    wchar_t *iter = env;

    if (iter == 0)
      uw_throwf(error_s, lit("out of memory"), nao);

    for (; *iter; iter += wcslen(iter) + 1)
      ptail = list_collect(ptail, string(iter));

    FreeEnvironmentStringsW(env);

    return env_list = out;
#else
    uw_throwf(error_s, lit("environment strings not available"), nao);
#endif
  }
}

static void obj_init(void)
{
  /*
   * No need to GC-protect the convenience variables which hold the interned
   * symbols, because the interned_syms list holds a reference to all the
   * symbols.
   */

  protect(&packages, &system_package_var, &keyword_package_var,
          &user_package_var, &null_string, &nil_string,
          &null_list, &equal_f, &eq_f, &eql_f,
          &car_f, &cdr_f, &null_f, &list_f,
          &identity_f, &less_f, &greater_f, &prog_string, &env_list,
          convert(val *, 0));

  nil_string = lit("nil");
  null_string = lit("");
  null_list = cons(nil, nil);

  hash_s = make_sym(lit("hash"));
  system_package = make_package(lit("sys"));
  keyword_package = make_package(lit("keyword"));
  user_package = make_package(lit("usr"));

  rehome_sym(hash_s, user_package);

  /* nil can't be interned because it's not a SYM object;
     it works as a symbol because the nil case is handled by
     symbol-manipulating function. */
  rplacd(gethash_c(user_package->pk.symhash, nil_string, nulloc), nil);

  /* t can't be interned, because intern needs t in order to do its job. */
  t = cdr(rplacd(gethash_c(user_package->pk.symhash,
                           lit("t"), nulloc), make_sym(lit("t"))));
  set(mkloc(t->s.package, t), user_package);

  null_s = intern(lit("null"), user_package);
  cons_s = intern(lit("cons"), user_package);
  str_s = intern(lit("str"), user_package);
  lit_s = intern(lit("lit"), user_package);
  chr_s = intern(lit("chr"), user_package);
  fixnum_s = intern(lit("fixnum"), user_package);
  sym_s = intern(lit("sym"), user_package);
  pkg_s = intern(lit("pkg"), user_package);
  fun_s = intern(lit("fun"), user_package);
  vec_s = intern(lit("vec"), user_package);
  stream_s = intern(lit("stream"), user_package);
  hash_s = intern(lit("hash"), user_package);
  hash_iter_s = intern(lit("hash-iter"), user_package);
  lcons_s = intern(lit("lcons"), user_package);
  lstr_s = intern(lit("lstr"), user_package);
  cobj_s = intern(lit("cobj"), user_package);
  cptr_s = intern(lit("cptr"), user_package);
  atom_s = intern(lit("atom"), user_package);
  integer_s = intern(lit("integer"), user_package);
  number_s = intern(lit("number"), user_package);
  sequence_s = intern(lit("sequence"), user_package);
  string_s = intern(lit("string"), user_package);
  env_s = intern(lit("env"), user_package);
  bignum_s = intern(lit("bignum"), user_package);
  float_s = intern(lit("float"), user_package);
  range_s = intern(lit("range"), user_package);
  rcons_s = intern(lit("rcons"), user_package);
  buf_s = intern(lit("buf"), user_package);
  var_s = intern(lit("var"), system_package);
  expr_s = intern(lit("expr"), system_package);
  regex_s = intern(lit("regex"), user_package);
  nongreedy_s = intern(lit("ng0+"), user_package);
  quote_s = intern(lit("quote"), user_package);
  qquote_s = intern(lit("qquote"), user_package);
  unquote_s = intern(lit("unquote"), user_package);
  splice_s = intern(lit("splice"), user_package);
  sys_qquote_s = intern(lit("qquote"), system_package);
  sys_unquote_s = intern(lit("unquote"), system_package);
  sys_splice_s = intern(lit("splice"), system_package);
  chset_s = intern(lit("chset"), system_package);
  set_s = intern(lit("set"), user_package);
  cset_s = intern(lit("cset"), user_package);
  wild_s = intern(lit("wild"), user_package);
  oneplus_s = intern(lit("1+"), user_package);
  zeroplus_s = intern(lit("0+"), user_package);
  optional_s = intern(lit("?"), user_package);
  compl_s = intern(lit("~"), user_package);
  compound_s = intern(lit("compound"), user_package);
  or_s = intern(lit("or"), user_package);
  and_s = intern(lit("and"), user_package);
  quasi_s = intern(lit("quasi"), system_package);
  quasilist_s = intern(lit("quasilist"), system_package);
  skip_s = intern(lit("skip"), user_package);
  trailer_s = intern(lit("trailer"), user_package);
  block_s = intern(lit("block"), user_package);
  next_s = intern(lit("next"), user_package);
  freeform_s = intern(lit("freeform"), user_package);
  fail_s = intern(lit("fail"), user_package);
  accept_s = intern(lit("accept"), user_package);
  all_s = intern(lit("all"), user_package);
  some_s = intern(lit("some"), user_package);
  none_s = intern(lit("none"), user_package);
  maybe_s = intern(lit("maybe"), user_package);
  cases_s = intern(lit("cases"), user_package);
  collect_s = intern(lit("collect"), user_package);
  until_s = intern(lit("until"), user_package);
  coll_s = intern(lit("coll"), user_package);
  define_s = intern(lit("define"), user_package);
  output_s = intern(lit("output"), user_package);
  single_s = intern(lit("single"), user_package);
  first_s = intern(lit("first"), user_package);
  last_s = intern(lit("last"), user_package);
  empty_s = intern(lit("empty"), user_package);
  repeat_s = intern(lit("repeat"), user_package);
  rep_s = intern(lit("rep"), user_package);
  flatten_s = intern(lit("flatten"), user_package);
  forget_s = intern(lit("forget"), user_package);
  local_s = intern(lit("local"), user_package);
  merge_s = intern(lit("merge"), user_package);
  bind_s = intern(lit("bind"), user_package);
  rebind_s = intern(lit("rebind"), user_package);
  cat_s = intern(lit("cat"), user_package);
  try_s = intern(lit("try"), user_package);
  catch_s = intern(lit("catch"), user_package);
  finally_s = intern(lit("finally"), user_package);
  throw_s = intern(lit("throw"), user_package);
  defex_s = intern(lit("defex"), user_package);
  deffilter_s = intern(lit("deffilter"), user_package);
  eof_s = intern(lit("eof"), user_package);
  eol_s = intern(lit("eol"), user_package);
  error_s = intern(lit("error"), user_package);
  type_error_s = intern(lit("type-error"), user_package);
  internal_error_s = intern(lit("internal-error"), user_package);
  panic_s = intern(lit("panic"), user_package);
  numeric_error_s = intern(lit("numeric-error"), user_package);
  range_error_s = intern(lit("range-error"), user_package);
  query_error_s = intern(lit("query-error"), user_package);
  file_error_s = intern(lit("file-error"), user_package);
  process_error_s = intern(lit("process-error"), user_package);
  syntax_error_s = intern(lit("syntax-error"), user_package);
  system_error_s = intern(lit("system-error"), user_package);
  timeout_error_s = intern(lit("timeout-error"), user_package);
  assert_s = intern(lit("assert"), user_package);
  warning_s = intern(lit("warning"), user_package);
  defr_warning_s = intern(lit("defr-warning"), user_package);
  restart_s = intern(lit("restart"), user_package);
  continue_s = intern(lit("continue"), user_package);
  name_s = intern(lit("name"), user_package);
  nullify_s = intern(lit("nullify"), user_package);
  from_list_s = intern(lit("from-list"), user_package);
  lambda_set_s = intern(lit("lambda-set"), user_package);
  length_s = intern(lit("length"), user_package);

  args_k = intern(lit("args"), keyword_package);
  nothrow_k = intern(lit("nothrow"), keyword_package);
  colon_k = intern(lit(""), keyword_package);
  auto_k = intern(lit("auto"), keyword_package);
  fun_k = intern(lit("fun"), keyword_package);
  wrap_k = intern(lit("wrap"), keyword_package);
  reflect_k = intern(lit("reflect"), keyword_package);

  equal_f = func_n2(equal);
  eq_f = func_n2(eq);
  eql_f = func_n2(eql);
  identity_f = func_n1(identity);
  car_f = func_n1(car);
  cdr_f = func_n1(cdr);
  null_f = func_n1(null);
  list_f = func_n0v(listv);
  less_f = func_n2(less);
  greater_f = func_n2(greater);
  prog_string = string(progname);
}

static val simple_qref_args_p(val args, val pos)
{
  if (nilp(args)) {
    return if2(ge(pos, two), t);
  } else if (!consp(args)) {
    return nil;
  } else {
    val arg = car(args);
    if (symbolp(arg) || (consp(arg) &&
                         car(arg) != qref_s &&
                         car(arg) != uref_s))
    {
      return simple_qref_args_p(cdr(args), succ(pos));
    }
    return nil;
  }
}

void out_str_char(wchar_t ch, val out, int *semi_flag)
{
  if (*semi_flag && (iswxdigit(ch) || ch == ';'))
    put_char(chr(';'), out);

  *semi_flag = 0;

  switch (ch) {
  case '\a': put_string(lit("\\a"), out); break;
  case '\b': put_string(lit("\\b"), out); break;
  case '\t': put_string(lit("\\t"), out); break;
  case '\n': put_string(lit("\\n"), out); break;
  case '\v': put_string(lit("\\v"), out); break;
  case '\f': put_string(lit("\\f"), out); break;
  case '\r': put_string(lit("\\r"), out); break;
  case '"': put_string(lit("\\\""), out); break;
  case '\\': put_string(lit("\\\\"), out); break;
  case 27: put_string(lit("\\e"), out); break;
  default:
    {
      val fmt = nil;

      if ((ch < 0x20) || (ch >= 0x7F && ch < 0xA0))
        fmt = lit("\\x~,02X");
      else if ((ch >= 0xD800 && ch < 0xE000) || ch == 0xFFFE || ch == 0xFFFF)
        fmt = lit("\\x~,04X");
      else if (ch >= 0xFFFF)
        fmt = lit("\\x~,06X");
      else
        put_char(chr(ch), out);

      if (fmt) {
        format(out, fmt, num(ch), nao);
        *semi_flag = 1;
      }
    }
  }
}

static void out_str_readable(const wchar_t *ptr, val out, int *semi_flag)
{
  for (; *ptr; ptr++)
    out_str_char(*ptr, out, semi_flag);
}

static void out_lazy_str(val lstr, val out)
{
  int semi_flag = 0;
  val lim, term, iter;
  const wchar_t *wcterm;

  type_check(lstr, LSTR);
  lim = lstr->ls.props->limit;
  term = lstr->ls.props->term;
  wcterm = c_str(term);

  put_char(chr('"'), out);

  out_str_readable(c_str(lstr->ls.prefix), out, &semi_flag);

  for (iter = lstr->ls.list; (!lim || gt(lim, zero)) && iter;
       iter = cdr(iter))
  {
    val str = car(iter);
    if (!str)
      break;
    out_str_readable(c_str(str), out, &semi_flag);
    out_str_readable(wcterm, out, &semi_flag);
    if (lim)
      lim = pred(lim);
  }

  put_char(chr('"'), out);
}

static void out_quasi_str_sym(val name, val mods, val rem_args,
                              val out, struct strm_ctx *ctx)
{
  val next_elem = car(rem_args);
  val next_elem_char = and3(next_elem && !mods, stringp(next_elem),
                            chr_str(next_elem, zero));
  val osstrm = make_string_output_stream();
  val namestr = (obj_print_impl(name, osstrm, nil, ctx),
                 get_string_from_stream(osstrm));
  int need_brace = mods ||
    (next_elem_char &&
     (chr_isalpha(next_elem_char) ||
      chr_isdigit(next_elem_char) ||
      next_elem_char == chr('_'))) ||
    (span_str(namestr, lit("0123456789_"
                          "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                          "abcdefghijklmnopqrstuvwxyz"))
     != length(namestr));
  put_char(chr('@'), out);
  if (need_brace)
    put_char(chr('{'), out);
  obj_print_impl(namestr, out, t, ctx);
  while (mods) {
    put_char(chr(' '), out);
    obj_print_impl(car(mods), out, nil, ctx);
    mods = cdr(mods);
  }
  if (need_brace)
    put_char(chr('}'), out);
}

static void out_quasi_str(val args, val out, struct strm_ctx *ctx)
{
  val iter, next;

  for (iter = cdr(args); iter; iter = next) {
    val elem = car(iter);
    next = cdr(iter);

    if (stringp(elem)) {
      int semi_flag = 0;
      out_str_readable(c_str(elem), out, &semi_flag);
    } else if (consp(elem)) {
      val sym = car(elem);
      if (sym == var_s)
      {
        out_quasi_str_sym(second(elem), third(elem), next,
                          out, ctx);
      } else {
        put_char(chr('@'), out);
        obj_print_impl(elem, out, nil, ctx);
      }
    } else if (symbolp(elem)) {
      out_quasi_str_sym(elem, nil, next, out, ctx);
    } else {
      obj_print_impl(elem, out, nil, ctx);
    }
  }
}

INLINE int circle_print_eligible(val obj)
{
  return is_ptr(obj) && (!symbolp(obj) || !symbol_package(obj));
}

static int unquote_star_check(val obj, val pretty)
{
  if (!obj || !symbolp(obj))
    return 0;
  if (car(obj->s.name) != chr('*'))
    return 0;
  return pretty || symbol_visible(cur_package, obj);
}

val obj_print_impl(val obj, val out, val pretty, struct strm_ctx *ctx)
{
  val ret = obj;

  if (ctx && circle_print_eligible(obj)) {
    val cell = gethash_c(ctx->obj_hash, obj, nulloc);
    val label = cdr(cell);

    if (label == t) {
      val counter = succ(ctx->counter);
      ctx->counter = counter;
      rplacd(cell, counter);
      format(out, lit("#~s="), counter, nao);
    } else if (integerp(label)) {
      format(out, lit("#~s#"), label, nao);
      return ret;
    } else if (!label) {
      rplacd(cell, colon_k);
    }
  }

  switch (type(obj)) {
  case NIL:
    put_string(if3(get_indent_mode(out) == num_fast(indent_code),
                   lit("()"), lit("nil")), out);
    break;
  case CONS:
  case LCONS:
    {
      val sym = car(obj);
      val save_mode = test_set_indent_mode(out, num_fast(indent_off),
                                           num_fast(indent_data));
      val save_indent = nil;
      int two_elem = consp(cdr(obj)) && !cddr(obj);

      if (sym == quote_s && two_elem) {
        put_char(chr('\''), out);
        obj_print_impl(second(obj), out, pretty, ctx);
      } else if (sym == sys_qquote_s && two_elem) {
        put_char(chr('^'), out);
        obj_print_impl(second(obj), out, pretty, ctx);
      } else if (sym == sys_unquote_s && two_elem) {
        val arg = second(obj);
        put_char(chr(','), out);
        if (unquote_star_check(arg, pretty))
          put_char(chr(' '), out);
        obj_print_impl(second(obj), out, pretty, ctx);
      } else if (sym == sys_splice_s && two_elem) {
        put_string(lit(",*"), out);
        obj_print_impl(second(obj), out, pretty, ctx);
      } else if (sym == vector_lit_s && two_elem) {
        put_string(lit("#"), out);
        obj_print_impl(second(obj), out, pretty, ctx);
      } else if (sym == hash_lit_s) {
        put_string(lit("#H"), out);
        obj_print_impl(rest(obj), out, pretty, ctx);
      } else if (sym == var_s && two_elem &&
                 (symbolp(second(obj)) || integerp(second(obj))))
      {
        put_char(chr('@'), out);
        obj_print_impl(second(obj), out, pretty, ctx);
      } else if (sym == expr_s && two_elem && consp(second(obj))) {
        put_char(chr('@'), out);
        obj_print_impl(second(obj), out, pretty, ctx);
      } else if (sym == rcons_s && consp(cdr(obj))
                 && consp(cddr(obj)) && !(cdddr(obj)))
      {
        obj_print_impl(second(obj), out, pretty, ctx);
        put_string(lit(".."), out);
        obj_print_impl(third(obj), out, pretty, ctx);
      } else if (sym == qref_s && simple_qref_args_p(cdr(obj), zero)) {
        val iter, next;
        for (iter = cdr(obj); iter; iter = next) {
          next = cdr(iter);
          obj_print_impl(car(iter), out, pretty, ctx);
          if (next)
            put_string(lit("."), out);
          iter = next;
        }
      } else if (sym == uref_s && simple_qref_args_p(cdr(obj), one)) {
        val iter;
        for (iter = cdr(obj); iter; iter = cdr(iter)) {
          put_string(lit("."), out);
          obj_print_impl(car(iter), out, pretty, ctx);
        }
      } else if (sym == quasi_s && consp(cdr(obj))) {
        put_char(chr('`'), out);
        out_quasi_str(obj, out, ctx);
        put_char(chr('`'), out);
      } else if (sym == quasilist_s && consp(cdr(obj))) {
        val args = cdr(obj);
        put_string(lit("#`"), out);
        if (args) {
          out_quasi_str(car(args), out, ctx);
          args = cdr(args);
        }
        while (args) {
          put_char(chr(' '), out);
          out_quasi_str(car(args), out, ctx);
          args = cdr(args);
        }
        put_char(chr('`'), out);
      } else {
        val iter;
        val closepar = chr(')');
        val indent = zero;

        if (sym == dwim_s && consp(cdr(obj))) {
          put_char(chr('['), out);
          obj = cdr(obj);
          closepar = chr(']');
        } else {
          put_char(chr('('), out);
        }

        if (sym == lambda_s && consp(cdr(obj)) && symbolp(second(obj))) {
          indent = one;
          save_indent = inc_indent(out, indent);
          set_indent_mode(out, num_fast(indent_code));
          obj_print_impl(sym, out, pretty, ctx);
          if (second(obj)) {
            put_string(lit(" (. "), out);
            obj_print_impl(second(obj), out, pretty, ctx);
            put_char(chr(')'), out);
          } else {
            put_string(lit(" ()"), out);
          }
          iter = cdr(obj);
          goto finish;
        } else if (special_operator_p(sym) || macro_form_p(obj, nil)) {
          indent = one;
          set_indent_mode(out, num_fast(indent_code));
        } else if (fboundp(sym)) {
          obj_print_impl(sym, out, pretty, ctx);
          indent = one;
          save_indent = inc_indent(out, indent);
          set_indent_mode(out, num_fast(indent_code));
          iter = obj;
          goto finish;
        }

        save_indent = inc_indent(out, indent);

        for (iter = obj; consp(iter); iter = cdr(iter)) {
          val d;
          {
            val a = car(iter);
            val unq = nil;

            if (a == sys_unquote_s)
              unq = lit(". ,");
            else if (a == sys_splice_s)
              unq = lit(". ,*");

            if (unq) {
              val d = cdr(iter);
              val ad = car(d);

              if (consp(d) && !cdr(d)) {
                put_string(unq, out);
                if (a == sys_unquote_s && unquote_star_check(ad, pretty))
                  put_char(chr(' '), out);
                obj_print_impl(ad, out, pretty, ctx);
                put_char(closepar, out);
                break;
              }
            }

            obj_print_impl(a, out, pretty, ctx);
          }
finish:
          d = cdr(iter);
          if (nilp(d)) {
            put_char(closepar, out);
          } else if (ctx && gethash(ctx->obj_hash, d)) {
            iter = nil;
            goto dot;
          } else if (consp(d)) {
            width_check(out, chr(' '));
          } else {
dot:
            put_string(lit(" . "), out);
            obj_print_impl(d, out, pretty, ctx);
            put_char(closepar, out);
          }
        }
      }

      if (save_indent)
        set_indent(out, save_indent);
      set_indent_mode(out, save_mode);
      break;
    }
  case LIT:
  case STR:
    if (pretty) {
      put_string(obj, out);
    } else {
      int semi_flag = 0;
      put_char(chr('"'), out);

      out_str_readable(c_str(obj), out, &semi_flag);

      put_char(chr('"'), out);
    }
    break;
  case CHR:
    if (pretty) {
      put_char(obj, out);
    } else {
      wchar_t ch = c_chr(obj);
      val fmt = nil;

      put_string(lit("#\\"), out);
      switch (ch) {
      case '\0': put_string(lit("nul"), out); break;
      case '\a': put_string(lit("alarm"), out); break;
      case '\b': put_string(lit("backspace"), out); break;
      case '\t': put_string(lit("tab"), out); break;
      case '\n': put_string(lit("newline"), out); break;
      case '\v': put_string(lit("vtab"), out); break;
      case '\f': put_string(lit("page"), out); break;
      case '\r': put_string(lit("return"), out); break;
      case 27: put_string(lit("esc"), out); break;
      case ' ': put_string(lit("space"), out); break;
      case 0xDC00: put_string(lit("pnul"), out); break;
      default:
        if ((ch < 0x20) || (ch >= 0x7F && ch < 0xA0))
          fmt = lit("x~,02X");
        else if ((ch >= 0xD800 && ch < 0xE000) || ch == 0xFFFE || ch == 0xFFFF)
          fmt = lit("x~,04X");
        else if (ch >= 0xFFFF)
          fmt = lit("x~,06X");
        else
          put_char(chr(ch), out);

        if (fmt)
          format(out, fmt, num(ch), nao);
      }
    }
    break;
  case NUM:
  case BGNUM:
    format(out, lit("~s"), obj, nao);
    break;
  case FLNUM:
    {
      val fmt = cdr(lookup_var(nil,
                               if3(pretty,
                                   pprint_flo_format_s, print_flo_format_s)));
      format(out, fmt, obj, nao);
    }
    break;
  case SYM:
    if (!pretty) {
      if (!obj->s.package) {
        put_string(lit("#:"), out);
      } else if (obj->s.package == keyword_package) {
        put_char(chr(':'), out);
      } else if (!symbol_visible(cur_package, obj)) {
        put_string(obj->s.package->pk.name, out);
        put_char(chr(':'), out);
      }
    }
    put_string(symbol_name(obj), out);
    break;
  case PKG:
    format(out, lit("#<package: ~a>"), obj->pk.name, nao);
    break;
  case FUN:
    {
      struct func *f = &obj->f;
      if (f->functype == FINTERP) {
        val fun = f->f.interp_fun;
        format(out, lit("#<interpreted fun: ~s ~s>"),
               car(fun), cadr(fun), nao);
      } else {
        format(out, lit("#<intrinsic fun: ~a param"),
               num_fast(f->fixparam - f->optargs), nao);
        if (f->optargs)
          format(out, lit(" + ~a optional"),
               num_fast(f->optargs), nao);
        if (obj->f.variadic)
          put_string(lit(" + variadic>"), out);
        else
          put_char(chr('>'), out);
      }
    }
    break;
  case VEC:
    {
      cnum i, length = c_num(obj->v.vec[vec_length]);
      val save_mode = test_set_indent_mode(out, num_fast(indent_off),
                                           num_fast(indent_data));
      val save_indent;

      put_string(lit("#("), out);

      save_indent = inc_indent(out, zero);

      for (i = 0; i < length; i++) {
        val elem = obj->v.vec[i];
        obj_print_impl(elem, out, pretty, ctx);
        if (i < length - 1)
          width_check(out, chr(' '));
      }

      put_char(chr(')'), out);

      set_indent(out, save_indent);
      set_indent_mode(out, save_mode);
    }
    break;
  case LSTR:
    if (pretty) {
      lazy_str_put(obj, out);
    } else {
      out_lazy_str(obj, out);
    }
    break;
  case COBJ:
  case CPTR:
    obj->co.ops->print(obj, out, pretty, ctx);
    break;
  case ENV:
    format(out, lit("#<environment: ~p>"), obj, nao);
    break;
  case RNG:
    format(out, if3(pretty, lit("#R(~a ~a)"), lit("#R(~s ~s)")),
           from(obj), to(obj), nao);
    break;
  case BUF:
    if (pretty)
      buf_pprint(obj, out);
    else
      buf_print(obj, out);
    break;
  default:
    format(out, lit("#<garbage: ~p>"), obj, nao);
    break;
  }

  return ret;
}

static void populate_obj_hash(val obj, struct strm_ctx *ctx)
{
tail:
  if (circle_print_eligible(obj)) {
    if (ctx->obj_hash_prev) {
      val prev_cell;
      val label = gethash_f(ctx->obj_hash_prev, obj, mkcloc(prev_cell));

      if (label == colon_k)
        uw_throwf(error_s, lit("print: unexpected duplicate object "
                               "(misbehaving print method?)"), nao);
      if (prev_cell)
        return;
    } else {
      val new_p;
      val cell = gethash_c(ctx->obj_hash, obj, mkcloc(new_p));

      if (!new_p) {
        rplacd(cell, t);
        return;
      }
    }
  } else {
    return;
  }

  switch (type(obj)) {
  case CONS:
  case LCONS:
    {
      populate_obj_hash(car(obj), ctx);
      obj = cdr(obj);
      goto tail;
    }
  case VEC:
    {
      cnum i;
      cnum l = c_num(length_vec(obj));

      for (i = 0; i < l; i++) {
        val in = num(i);
        populate_obj_hash(vecref(obj, in), ctx);
      }

      break;
    }
  case RNG:
    {
      populate_obj_hash(from(obj), ctx);
      obj = to(obj);
      goto tail;
    }
  case COBJ:
    if (hashp(obj)) {
      val iter = hash_begin(obj);
      val cell;
      while ((cell = hash_next(iter))) {
        populate_obj_hash(car(cell), ctx);
        populate_obj_hash(cdr(cell), ctx);
      }
      obj = get_hash_userdata(obj);
      goto tail;
    } else if (structp(obj)) {
      val stype = struct_type(obj);
      val iter;

      for (iter = slots(stype); iter; iter = cdr(iter)) {
        val sn = car(iter);
        populate_obj_hash(slot(obj, sn), ctx);
      }
    }
    break;
  case FUN:
    if (obj->f.functype == FINTERP) {
      val fun = obj->f.f.interp_fun;
      populate_obj_hash(car(fun), ctx);
      obj = cadr(fun);
      goto tail;
    }
  default:
    break;
  }
}

static void obj_hash_merge(val parent_hash, val child_hash)
{
  val iter, cell;

  for (iter = hash_begin(child_hash); (cell = hash_next(iter));) {
    val new_p;
    val pcell = gethash_c(parent_hash, car(cell), mkcloc(new_p));
    if (new_p)
      rplacd(pcell, cdr(cell));
    else
      uw_throwf(error_s, lit("print: unexpected duplicate object "
                             "(internal error?)"), nao);
  }
}

val obj_print(val obj, val out, val pretty)
{
  val ret = nil;
  val save_mode = get_indent_mode(out);
  val save_indent = get_indent(out);
  struct strm_ctx *ctx_orig = get_ctx(out);
  struct strm_ctx *volatile ctx = ctx_orig, ctx_struct;
  uw_frame_t cont_guard;

  if (ctx_orig)
    uw_push_guard(&cont_guard, 1);

  uw_simple_catch_begin;

  if (ctx) {
    if (cdr(lookup_var(nil, print_circle_s))) {
      ctx->obj_hash_prev = ctx->obj_hash;
      ctx->obj_hash = make_hash(nil, nil, nil);
      populate_obj_hash(obj, ctx);
      obj_hash_merge(ctx->obj_hash_prev, ctx->obj_hash);
      ctx->obj_hash = ctx->obj_hash_prev;
      ctx->obj_hash_prev = nil;
    } else {
      ctx = 0;
    }
  } else {
    if (print_circle_s && cdr(lookup_var(nil, print_circle_s))) {
      ctx = &ctx_struct;
      ctx->obj_hash = make_hash(nil, nil, nil);
      ctx->obj_hash_prev = nil;
      ctx->counter = zero;
      get_set_ctx(out, ctx);
      populate_obj_hash(obj, ctx);
    }
  }

  ret = obj_print_impl(obj, out, pretty, ctx);

  uw_unwind {
    set_indent_mode(out, save_mode);
    set_indent(out, save_indent);
    if (ctx != ctx_orig)
      get_set_ctx(out, ctx_orig);
  }

  uw_catch_end;

  if (ctx_orig)
    uw_pop_frame(&cont_guard);

  return ret;
}

val print(val obj, val stream, val pretty)
{
  return obj_print(obj, default_arg(stream, std_output),
                   default_null_arg(pretty));
}

val pprint(val obj, val stream)
{
  return obj_print(obj, default_arg(stream, std_output), t);
}

val tostring(val obj)
{
  val ss = make_string_output_stream();
  obj_print(obj, ss, nil);
  return get_string_from_stream(ss);
}

val tostringp(val obj)
{
  val ss = make_string_output_stream();
  pprint(obj, ss);
  return get_string_from_stream(ss);
}

val display_width(val obj)
{
  if (stringp(obj)) {
    const wchar_t *s = c_str(obj);
    cnum width = 0;
    for (; *s; s++) {
      if (iswcntrl(*s))
        continue;
      width += 1 + wide_display_char_p(*s);
    }
    return num(width);
  } else if (chrp(obj)) {
    wchar_t ch = c_chr(obj);
    if (iswcntrl(ch))
      return zero;
    return num_fast(1 + wide_display_char_p(ch));
  }

  uw_throwf(type_error_s, lit("display-width: ~s isn't a character or string"),
            obj, nao);
}

val time_sec(void)
{
  struct timeval tv;
  if (gettimeofday(&tv, 0) == -1)
    return nil;
  return num(tv.tv_sec);
}

val time_sec_usec(void)
{
  struct timeval tv;
  if (gettimeofday(&tv, 0) == -1)
    return nil;
  return cons(num(tv.tv_sec), num(tv.tv_usec));
}

#if !HAVE_GMTIME_R
struct tm *gmtime_r(const time_t *timep, struct tm *result);
struct tm *localtime_r(const time_t *timep, struct tm *result);

struct tm *gmtime_r(const time_t *timep, struct tm *result)
{
  struct tm *hack = gmtime(timep);
  *result = *hack;
  return hack;
}

struct tm *localtime_r(const time_t *timep, struct tm *result)
{
  struct tm *hack = localtime(timep);
  *result = *hack;
  return hack;
}
#endif

static val string_time(struct tm *(*break_time_fn)(const time_t *, struct tm *),
                       char *format, time_t time)
{
  char buffer[512] = "";
  struct tm broken_out_time;

  if (break_time_fn(&time, &broken_out_time) == 0)
    return nil;

#if HAVE_TM_ZONE
  if (strcmp(broken_out_time.TM_ZONE, "GMT") == 0)
    broken_out_time.TM_ZONE = "UTC";
#endif

  if (strftime(buffer, sizeof buffer, format, &broken_out_time) == 0)
    buffer[0] = 0;

  {
    wchar_t *wctime = utf8_dup_from(buffer);
    return string_own(wctime);
  }
}

val time_string_local(val time, val format)
{
  time_t secs = c_num(time);
  const wchar_t *wcfmt = c_str(format);
  char *u8fmt = utf8_dup_to(wcfmt);
  val timestr = string_time(localtime_r, u8fmt, secs);
  free(u8fmt);
  return timestr;
}

val time_string_utc(val time, val format)
{
  time_t secs = c_num(time);
  const wchar_t *wcfmt = c_str(format);
  char *u8fmt = utf8_dup_to(wcfmt);
  val timestr = string_time(gmtime_r, u8fmt, secs);
  free(u8fmt);
  return timestr;
}

static val broken_time_list(struct tm *tms)
{
  return list(num(tms->tm_year + 1900),
              num_fast(tms->tm_mon + 1),
              num_fast(tms->tm_mday),
              num_fast(tms->tm_hour),
              num_fast(tms->tm_min),
              num_fast(tms->tm_sec),
              tms->tm_isdst ? t : nil,
              nao);
}

static void tm_to_time_struct(val time_struct, struct tm *ptm)
{
  slotset(time_struct, year_s, num(ptm->tm_year + 1900));
  slotset(time_struct, month_s, num_fast(ptm->tm_mon + 1));
  slotset(time_struct, day_s, num_fast(ptm->tm_mday));
  slotset(time_struct, hour_s, num_fast(ptm->tm_hour));
  slotset(time_struct, min_s, num_fast(ptm->tm_min));
  slotset(time_struct, sec_s, num_fast(ptm->tm_sec));
  slotset(time_struct, dst_s, tnil(ptm->tm_isdst));
#if HAVE_TM_GMTOFF
  slotset(time_struct, gmtoff_s, num_fast(ptm->TM_GMTOFF));
#endif
#if HAVE_TM_ZONE
  slotset(time_struct, zone_s, if2(ptm->TM_ZONE, string_utf8(ptm->TM_ZONE)));
#endif
}

static val broken_time_struct(struct tm *tms)
{
  args_decl(args, ARGS_MIN);
  val ts = make_struct(time_s, nil, args);

  tm_to_time_struct(ts, tms);

  return ts;
}

val time_fields_local(val time)
{
  struct tm tms;
  time_t secs = c_num(time);

  if (localtime_r(&secs, &tms) == 0)
    return nil;

  return broken_time_list(&tms);
}

val time_fields_utc(val time)
{
  struct tm tms;
  time_t secs = c_num(time);

  if (gmtime_r(&secs, &tms) == 0)
    return nil;

  return broken_time_list(&tms);
}

val time_struct_local(val time)
{
  struct tm tms;
  time_t secs = c_num(time);

  if (localtime_r(&secs, &tms) == 0)
    return nil;

  return broken_time_struct(&tms);
}

val time_struct_utc(val time)
{
  struct tm tms;
  time_t secs = c_num(time);

  if (gmtime_r(&secs, &tms) == 0)
    return nil;

  return broken_time_struct(&tms);
}

static void time_fields_to_tm(struct tm *ptm,
                              val year, val month, val day,
                              val hour, val min, val sec, val dst)
{
  uses_or2;
  ptm->tm_year = c_num(or2(year, zero)) - 1900;
  ptm->tm_mon = c_num(or2(month, zero)) - 1;
  ptm->tm_mday = c_num(or2(day, zero));
  ptm->tm_hour = c_num(or2(hour, zero));
  ptm->tm_min = c_num(or2(min, zero));
  ptm->tm_sec = c_num(or2(sec, zero));

  if (!dst)
    ptm->tm_isdst = 0;
  else if (dst == auto_k)
    ptm->tm_isdst = -1;
  else
    ptm->tm_isdst = 1;

#if HAVE_TM_GMTOFF
  ptm->TM_GMTOFF = 0;
#endif
#if HAVE_TM_ZONE
  ptm->TM_ZONE = 0;
#endif
}

static void time_struct_to_tm(struct tm *ptm, val time_struct, val strict)
{
  val year = slot(time_struct, year_s);
  val month = slot(time_struct, month_s);
  val day = slot(time_struct, day_s);
  val hour = slot(time_struct, hour_s);
  val min = slot(time_struct, min_s);
  val sec = slot(time_struct, sec_s);
  val dst = slot(time_struct, dst_s);

  if (!strict) {
    year = (year ? year : zero);
    month = (month ? month : zero);
    day = (day ? day : zero);
    hour = (hour ? hour : zero);
    min = (min ? min : zero);
    sec = (sec ? sec : zero);
  }

  time_fields_to_tm(ptm, year, month, day, hour, min, sec, dst);
}

static val make_time_impl(time_t (*pmktime)(struct tm *),
                          val year, val month, val day,
                          val hour, val minute, val second,
                          val isdst)
{
  struct tm local = { 0 };
  time_t time;

  time_fields_to_tm(&local, year, month, day,
                    hour, minute, second, isdst);
  time = pmktime(&local);

  return time == -1 ? nil : num(time);
}

val make_time(val year, val month, val day,
              val hour, val minute, val second,
              val isdst)
{
  return make_time_impl(mktime, year, month, day, hour, minute, second, isdst);
}

#if HAVE_STRPTIME

static struct tm epoch_tm(void)
{
  struct tm ep = { 0 };
  ep.tm_year = 70;
  ep.tm_mday = 1;
  return ep;
}

static int strptime_wrap(val string, val format, struct tm *ptms)
{
  const wchar_t *w_str = c_str(string);
  const wchar_t *w_fmt = c_str(format);
  char *str = utf8_dup_to(w_str);
  char *fmt = utf8_dup_to(w_fmt);
  char *ptr = strptime(str, fmt, ptms);
  int ret = ptr != 0;
  free(fmt);
  free(str);
  return ret;
}

val time_parse(val format, val string)
{
  struct tm tms = epoch_tm();
  int ret = strptime_wrap(string, format, &tms);
  return ret ? broken_time_struct(&tms) : nil;
}

#endif

#if !HAVE_SETENV

void setenv(const char *name, const char *value, int overwrite)
{
  int len = strlen(name)+1+strlen(value)+1;
  char *str = (char *) chk_malloc(len);
  (void) overwrite;
  sprintf(str, "%s=%s", name, value);
  putenv(str);
}

void unsetenv(const char *name)
{
  setenv(name, "", 1);
}

#endif


#if !HAVE_TIMEGM

static time_t timegm_hack(struct tm *tm)
{
    time_t ret;
    char *tz;

    tz = getenv("TZ");
    setenv("TZ", "UTC", 1);
#if HAVE_TZSET
    tzset();
#endif
    ret = mktime(tm);
    if (tz)
        setenv("TZ", tz, 1);
    else
        unsetenv("TZ");
#if HAVE_TZSET
    tzset();
#endif

    env_list = nil;
    return ret;
}
#endif

val make_time_utc(val year, val month, val day,
                  val hour, val minute, val second,
                  val isdst)
{
#if HAVE_TIMEGM
  time_t (*pmktime)(struct tm *) = timegm;
#else
  time_t (*pmktime)(struct tm *) = timegm_hack;
#endif

  return make_time_impl(pmktime, year, month, day, hour, minute, second, isdst);
}

static val time_meth(val utc_p, val time_struct)
{
  val year = slot(time_struct, year_s);
  val month = slot(time_struct, month_s);
  val day = slot(time_struct, day_s);
  val hour = slot(time_struct, hour_s);
  val min = slot(time_struct, min_s);
  val sec = slot(time_struct, sec_s);
  val dst = slot(time_struct, dst_s);

  return (utc_p ? make_time_utc : make_time)(year, month, day,
                                             hour, min, sec, dst);
}

static val time_string_meth(val time_struct, val format)
{
  struct tm tms = { 0 };
  time_struct_to_tm(&tms, time_struct, t);
  char buffer[512] = "";
  char *fmt = utf8_dup_to(c_str(format));

  if (strftime(buffer, sizeof buffer, fmt, &tms) == 0)
    buffer[0] = 0;

  free(fmt);

  return string_own(utf8_dup_from(buffer));
}

#if HAVE_STRPTIME

static val time_parse_meth(val time_struct, val format, val string)
{
  struct tm tms = { 0 };
  time_struct_to_tm(&tms, time_struct, nil);
  val ret = nil;

  {
    const wchar_t *w_str = c_str(string);
    const wchar_t *w_fmt = c_str(format);
    char *str = utf8_dup_to(w_str);
    char *fmt = utf8_dup_to(w_fmt);
    char *ptr = strptime(str, fmt, &tms);

    if (ptr != 0) {
      tm_to_time_struct(time_struct, &tms);
      ret = string_utf8(ptr);
    }

    free(fmt);
    free(str);
  }

  return ret;
}

val time_parse_local(val format, val string)
{
  struct tm tms = epoch_tm();
  if (!strptime_wrap(string, format, &tms))
    return nil;
  return num(mktime(&tms));
}

val time_parse_utc(val format, val string)
{
  struct tm tms = epoch_tm();
  if (!strptime_wrap(string, format, &tms))
    return nil;
#if HAVE_TIMEGM
  return num(timegm(&tms));
#else
  return num(timegm_hack(&tms));
#endif
}

#endif

static void time_init(void)
{
  val time_st;

  time_s = intern(lit("time"), user_package);
  time_local_s = intern(lit("time-local"), user_package);
  time_utc_s = intern(lit("time-utc"), user_package);
  time_string_s = intern(lit("time-string"), user_package);
  time_parse_s = intern(lit("time-parse"), user_package);
  year_s = intern(lit("year"), user_package);
  month_s = intern(lit("month"), user_package);
  day_s = intern(lit("day"), user_package);
  hour_s = intern(lit("hour"), user_package);
  min_s = intern(lit("min"), user_package);
  sec_s = intern(lit("sec"), user_package);
  dst_s = intern(lit("dst"), user_package);
  gmtoff_s = intern(lit("gmtoff"), user_package);
  zone_s = intern(lit("zone"), user_package);

  time_st = make_struct_type(time_s, nil,
                             list(time_local_s, time_utc_s,
                                  time_string_s, time_parse_s, nao),
                             list(year_s, month_s, day_s,
                                  hour_s, min_s, sec_s, dst_s,
                                  gmtoff_s, zone_s, nao),
                             nil, nil, nil, nil);

  static_slot_set(time_st, time_local_s, func_f1(nil, time_meth));
  static_slot_set(time_st, time_utc_s, func_f1(t, time_meth));
  static_slot_set(time_st, time_string_s, func_n2(time_string_meth));
#if HAVE_STRPTIME
  static_slot_set(time_st, time_parse_s, func_n3(time_parse_meth));
#endif
}

void init(mem_t *(*oom)(mem_t *, size_t), val *stack_bottom)
{
  int gc_save;
  gc_save = gc_state(0);

  oom_realloc = oom;
  gc_init(stack_bottom);
  obj_init();
  uw_init();
  eval_init();
  hash_init();
  struct_init();
  itypes_init();
  buf_init();
  ffi_init();
  sysif_init();
  arith_init();
  rand_init();
  stream_init();
#if HAVE_POSIX_SIGS
  sig_init();
#endif
  filter_init();
  regex_init();
  gc_late_init();
  parse_init();
  uw_late_init();
  less_tab_init();
#if HAVE_SYSLOG
  syslog_init();
#endif
#if HAVE_GLOB
  glob_init();
#endif
#if HAVE_FTW
  ftw_init();
#endif
#if HAVE_TERMIOS
  termios_init();
#endif
  cadr_init();
  time_init();

  gc_state(gc_save);
}

int compat_fixup(int compat_ver)
{
  if (compat_ver < 97)
    return 97;

  if (compat_ver == 97) {
    symbol_setname(type_error_s, lit("type_error"));
    symbol_setname(internal_error_s, lit("internal_error"));
    symbol_setname(numeric_error_s, lit("numeric_error"));
    symbol_setname(range_error_s, lit("range_error"));
    symbol_setname(query_error_s, lit("query_error"));
    symbol_setname(file_error_s, lit("file_error"));
    symbol_setname(process_error_s, lit("process_error"));
  }

  eval_compat_fixup(compat_ver);
  rand_compat_fixup(compat_ver);
  return 0;
}

void dump(val obj, val out)
{
  obj_print(obj, out, nil);
  put_char(chr('\n'), out);
}

/*
 * Handy function for debugging in gdb,
 * so we don't have to keep typing:
 * (gdb) p dump(something, stdout)
 */
void d(val obj)
{
  int save = gc_state(0);
  dump(obj, std_output);
  gc_state(save);
}

/*
 * Function for software breakpoints.
 * Debugging routines call here when a breakpoint
 * is requested. For this to work, set a breakpoint
 * on this function.
 */
void breakpt(void)
{
}
