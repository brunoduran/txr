/* Copyright 2011-2016
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
#include <dirent.h>
#include <stdarg.h>
#include <wchar.h>
#include <signal.h>
#include "config.h"
#include "lib.h"
#include "gc.h"
#include "args.h"
#include "signal.h"
#include "unwind.h"
#include "stream.h"
#include "parser.h"
#include "eval.h"
#include "txr.h"
#include "debug.h"

int opt_debugger;
int debug_depth;
static int step_mode;
static int next_depth = -1;
static val breakpoints;
static val last_command;
static int cols = 80;

val debug_quit_s;

/* C99 inline instantiations. */
#if __STDC_VERSION__ >= 199901L
val debug_check(val form, val bindings, val data, val line,
                val pos, val base);
void debug_init(void);
#endif

static void help(val stream)
{
  put_string(lit("commands:\n"
                 "? - help     q - quit    c - continue  s - step into form\n"
                 "h - help     f - finish form           n - step over form\n"
                 "v - show variable binding environment  o - show current form\n"
                 "b - set breakpoint by line number      i - show current data\n"
                 "d - delete breakpoint                  w - backtrace\n"
                 "l - list breakpoints                   g - set loglevel\n"),
             stream);
}

static void show_bindings(val env, val stream)
{
  val level = zero;
  put_string(lit("bindings:\n"), stream);

  for (;; level = plus(level, one)) {
    if (nilp(env))
      break;
    else if (consp(env)) {
      format(stream, lit("~d: ~s\n"), level, env, nao);
      break;
    } else if (type(env) == ENV) {
      format(stream, lit("~d: ~s\n"), level, env->e.vbindings, nao);
      env = env->e.up_env;
    } else {
      format(stream, lit("invalid environment object: ~s\n"), env, nao);
      break;
    }
  }
}

val debug(val ctx, val bindings, val data, val line, val pos, val base)
{
  uses_or2;
  val form = ctx_form(ctx);
  val rl = source_loc(form);
  cons_bind (lineno, file, rl);

  if (consp(data))
    data = car(data);
  else if (data == t)
    data = nil;

  if (!step_mode && !memqual(rl, breakpoints)
      && (debug_depth > next_depth))
  {
    return nil;
  } else {
    val print_form = t;
    val print_data = t;

    for (;;) {
      val input, command;

      if (print_form) {
        format(std_debug, lit("stopped at line ~d of ~a\n"),
               lineno, file, nao);
        format(std_debug, lit("form: ~s\n"), form, nao);
        format(std_debug, lit("depth: ~s\n"), num(debug_depth), nao);
        print_form = nil;
      }

      if (print_data) {
        int lim = cols * 8;

        if (data && pos) {
          val half = num((lim - 8) / 2);
          val full = num((lim - 8));
          val prefix, suffix;

          if (lt(pos, half)) {
            prefix = sub_str(data, zero, pos);
            suffix = sub_str(data, pos, full);
          } else {
            prefix = sub_str(data, minus(pos, half), pos);
            suffix = sub_str(data, pos, plus(pos, half));
          }

          format(std_debug, lit("data (~d:~d):\n~s . ~s\n"),
                 line, plus(pos, base), prefix, suffix, nao);
        } else if (data && length_str_ge(data, num(lim - 2))) {
          format(std_debug, lit("data (~d):\n~s...~s\n"), line,
                 sub_str(data, zero, num(lim/2 - 4)),
                 sub_str(data, num(-(lim/2 - 3)), t), nao);
        } else {
          format(std_debug, lit("data (~d):\n~s\n"), line, data, nao);
        }
        print_data = nil;
      }

      format(std_debug, lit("txr> "), nao);
      flush_stream(std_debug);

      input = split_str_set(or2(get_line(std_input), lit("q")), lit("\t "));
      command = if3(equal(first(input), null_string),
                    or2(last_command, lit("")), first(input));
      last_command = command;

      if (equal(command, lit("?")) || equal(command, lit("h"))) {
        help(std_debug);
        continue;
      } else if (equal(command, null_string)) {
        continue;
      } else if (equal(command, lit("c"))) {
        step_mode = 0;
        next_depth = -1;
        return nil;
      } else if (equal(command, lit("s"))) {
        step_mode = 1;
        return nil;
      } else if (equal(command, lit("n"))) {
        step_mode = 0;
        next_depth = debug_depth;
        return nil;
      } else if (equal(command, lit("f"))) {
        step_mode = 0;
        next_depth = debug_depth - 1;
        return nil;
      } else if (equal(command, lit("v"))) {
        show_bindings(bindings, std_debug);
      } else if (equal(command, lit("o"))) {
        print_form = t;
      } else if (equal(command, lit("i"))) {
        print_data = t;
      } else if (equal(command, lit("b")) || equal(command, lit("d")) ||
                 equal(command, lit("g")))
      {
        if (!rest(input)) {
          format(std_debug, lit("~s needs arguments\n"), command, nao);
          continue;
        } else {
          val n = int_str(second(input), num(10));
          val l = cons(n, or2(third(input), file));

          if (!n) {
            format(std_debug, lit("~s needs <line> [ <file> ]\n"),
                   command, nao);
            continue;
          }

          if (equal(command, lit("b"))) {
            breakpoints = remqual(l, breakpoints, nil);
            push(l, &breakpoints);
          } else if (equal(command, lit("d"))) {
            val breakpoints_old = breakpoints;
            breakpoints = remqual(l, breakpoints, nil);
            if (breakpoints == breakpoints_old)
              format(std_debug, lit("no such breakpoint\n"));
          } else {
            opt_loglevel = c_num(n);
          }
        }
      } else if (equal(command, lit("l"))) {
        format(std_debug, lit("breakpoints: ~s\n"), breakpoints, nao);
      } else if (equal(command, lit("w"))) {
        format(std_debug, lit("backtrace:\n"), nao);
        {
          uw_frame_t *iter;

          for (iter = uw_current_frame(); iter != 0; iter = iter->uw.up) {
            if (iter->uw.type == UW_DBG) {
              if (iter->db.ub_p_a_pairs)
                format(std_debug, lit("(~s ~s ~s)\n"), iter->db.func,
                       args_copy_to_list(iter->db.args),
                       iter->db.ub_p_a_pairs, nao);
              else
                format(std_debug, lit("(~s ~s)\n"), iter->db.func,
                       args_copy_to_list(iter->db.args), nao);
            }
          }
        }
      } else if (equal(command, lit("q"))) {
        uw_throwf(debug_quit_s, lit("terminated via debugger"), nao);
      } else {
        format(std_debug, lit("unrecognized command: ~a\n"), command, nao);
      }
    }

    return nil;
  }
}

debug_state_t debug_set_state(int depth, int step)
{
  debug_state_t old = { next_depth, step_mode };
  next_depth = depth;
  step_mode = step;
  return old;
}

void debug_restore_state(debug_state_t state)
{
  next_depth = state.next_depth;
  step_mode = state.step_mode;
}

void debug_init(void)
{
  step_mode = 1;
  protect(&breakpoints, &last_command, convert(val *, 0));
  debug_quit_s = intern(lit("debug-quit"), user_package);
  {
    char *columns = getenv("COLUMNS");
    if (columns)
      cols = atoi(columns);
    if (cols < 40)
      cols = 40;
  }
}
