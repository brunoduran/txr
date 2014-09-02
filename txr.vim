" VIM Syntax file for txr
" Kaz Kylheku <kaz@kylheku.com>

" INSTALL-HOWTO:
"
" 1. Create the directory .vim/syntax in your home directory and
"    put this file there.
" 2. In your .vimrc, add this command to associate *.txr files
"    with the txr filetype.
"    :au BufRead,BufNewFile *.txr set filetype=txr | set lisp
"
" If you want syntax highlighting to be on automatically (for any language)
" you need to add ":syntax on" in your .vimrc also. But you knew that already!
"
" This file is generated by the genvim.txr script in the TXR source tree.

syn case match
syn spell toplevel

setlocal iskeyword=a-z,A-Z,48-57,!,$,&,*,+,-,<,=,>,?,\\,_,~,/

syn keyword txr_keyword contained accept all and assert
syn keyword txr_keyword contained bind block cases cat
syn keyword txr_keyword contained catch choose close coll
syn keyword txr_keyword contained collect defex deffilter define
syn keyword txr_keyword contained do elif else end
syn keyword txr_keyword contained eof eol fail filter
syn keyword txr_keyword contained finally flatten forget freeform
syn keyword txr_keyword contained fuzz gather if last
syn keyword txr_keyword contained load local maybe merge
syn keyword txr_keyword contained next none or output
syn keyword txr_keyword contained rebind rep repeat require
syn keyword txr_keyword contained set skip some text
syn keyword txr_keyword contained throw trailer try until
syn keyword txr_keyword contained var

syn keyword txl_keyword contained * *args* *e* *flo-dig*
syn keyword txl_keyword contained *flo-epsilon* *flo-max* *flo-min* *full-args*
syn keyword txl_keyword contained *gensym-counter* *keyword-package* *pi* *random-state*
syn keyword txl_keyword contained *self-path* *stddebug* *stderr* *stdin*
syn keyword txl_keyword contained *stdlog* *stdnull* *stdout* *txr-version*
syn keyword txl_keyword contained *user-package* + - /
syn keyword txl_keyword contained /= < <= =
syn keyword txl_keyword contained > >= abs abs-path-p
syn keyword txl_keyword contained acons acons-new aconsql-new acos
syn keyword txl_keyword contained ado alist-nremove alist-remove all
syn keyword txl_keyword contained and andf ap apf
syn keyword txl_keyword contained append append* append-each append-each*
syn keyword txl_keyword contained apply ash asin assoc
syn keyword txl_keyword contained assql atan atan2 atom
syn keyword txl_keyword contained bignump bit block boundp
syn keyword txl_keyword contained break-str call car caseq
syn keyword txl_keyword contained caseql casequal cat-str cat-vec
syn keyword txl_keyword contained catch cdr ceil chain
syn keyword txl_keyword contained chdir chr-isalnum chr-isalpha chr-isascii
syn keyword txl_keyword contained chr-iscntrl chr-isdigit chr-isgraph chr-islower
syn keyword txl_keyword contained chr-isprint chr-ispunct chr-isspace chr-isupper
syn keyword txl_keyword contained chr-isxdigit chr-num chr-str chr-str-set
syn keyword txl_keyword contained chr-tolower chr-toupper chrp close-stream
syn keyword txl_keyword contained closelog cmp-str collect-each collect-each*
syn keyword txl_keyword contained comb compl-span-str cond cons
syn keyword txl_keyword contained conses conses* consp copy
syn keyword txl_keyword contained copy-alist copy-cons copy-hash copy-list
syn keyword txl_keyword contained copy-str copy-vec cos count-if
syn keyword txl_keyword contained countq countql countqual cum-norm-dist
syn keyword txl_keyword contained daemon dec defmacro defsymacro
syn keyword txl_keyword contained defun defvar del delay
syn keyword txl_keyword contained delete-package do dohash downcase-str
syn keyword txl_keyword contained dwim each each* empty
syn keyword txl_keyword contained env env-fbind env-hash env-vbind
syn keyword txl_keyword contained eq eql equal errno
syn keyword txl_keyword contained error eval evenp exit
syn keyword txl_keyword contained exp expt exptmod false
syn keyword txl_keyword contained fbind fboundp fifth filter-equal
syn keyword txl_keyword contained filter-string-tree find find-if find-max
syn keyword txl_keyword contained find-min find-package first fixnump
syn keyword txl_keyword contained flatten flatten* flet flip
syn keyword txl_keyword contained flo-int flo-str floatp floor
syn keyword txl_keyword contained flush-stream for for* force
syn keyword txl_keyword contained format fourth fun func-get-env
syn keyword txl_keyword contained func-get-form func-set-env functionp gcd
syn keyword txl_keyword contained gen generate gensym get-byte
syn keyword txl_keyword contained get-char get-hash-userdata get-line get-lines
syn keyword txl_keyword contained get-list-from-stream get-sig-handler get-string get-string-from-stream
syn keyword txl_keyword contained gethash getitimer getpid getppid
syn keyword txl_keyword contained giterate group-by gun hash
syn keyword txl_keyword contained hash-alist hash-construct hash-count hash-diff
syn keyword txl_keyword contained hash-eql hash-equal hash-isec hash-keys
syn keyword txl_keyword contained hash-pairs hash-uni hash-update hash-update-1
syn keyword txl_keyword contained hash-values hashp html-decode html-encode
syn keyword txl_keyword contained iapply identity ido if
syn keyword txl_keyword contained iff iffi inc inhash
syn keyword txl_keyword contained int-flo int-str integerp intern
syn keyword txl_keyword contained interp-fun-p interpose ip ipf
syn keyword txl_keyword contained isqrt itimer-prov itimer-real itimer-virtual
syn keyword txl_keyword contained juxt keep-if keep-if* keywordp
syn keyword txl_keyword contained kill labels lambda last
syn keyword txl_keyword contained lazy-str lazy-str-force lazy-str-force-upto lazy-str-get-trailing-list
syn keyword txl_keyword contained lazy-stream-cons lazy-stringp lbind lcons-fun
syn keyword txl_keyword contained lconsp ldiff length length-list
syn keyword txl_keyword contained length-str length-str-< length-str-<= length-str->
syn keyword txl_keyword contained length-str->= length-vec let let*
syn keyword txl_keyword contained link lisp-parse list list*
syn keyword txl_keyword contained list-str list-vector listp log
syn keyword txl_keyword contained log-alert log-auth log-authpriv log-cons
syn keyword txl_keyword contained log-crit log-daemon log-debug log-emerg
syn keyword txl_keyword contained log-err log-info log-ndelay log-notice
syn keyword txl_keyword contained log-nowait log-odelay log-perror log-pid
syn keyword txl_keyword contained log-user log-warning log10 log2
syn keyword txl_keyword contained logand logior lognot logtest
syn keyword txl_keyword contained logtrunc logxor macro-form-p macro-time
syn keyword txl_keyword contained macroexpand macroexpand-1 macrolet major
syn keyword txl_keyword contained make-catenated-stream make-env make-hash make-lazy-cons
syn keyword txl_keyword contained make-like make-package make-random-state make-similar-hash
syn keyword txl_keyword contained make-string-byte-input-stream make-string-input-stream make-string-output-stream make-strlist-output-stream
syn keyword txl_keyword contained make-sym make-time make-time-utc make-trie
syn keyword txl_keyword contained makedev mapcar mapcar* mapdo
syn keyword txl_keyword contained maphash mappend mappend* mask
syn keyword txl_keyword contained match-fun match-regex match-regex-right match-str
syn keyword txl_keyword contained match-str-tree max member member-if
syn keyword txl_keyword contained memq memql memqual merge
syn keyword txl_keyword contained min minor mkdir mknod
syn keyword txl_keyword contained mkstring mod multi multi-sort
syn keyword txl_keyword contained n-choose-k n-perm-k nconc nilf
syn keyword txl_keyword contained none not nreverse null
syn keyword txl_keyword contained nullify num-chr num-str numberp
syn keyword txl_keyword contained oddp op open-command open-directory
syn keyword txl_keyword contained open-file open-files open-files* open-pipe
syn keyword txl_keyword contained open-process open-tail openlog or
syn keyword txl_keyword contained orf packagep partition-by perm
syn keyword txl_keyword contained pop pos pos-if pos-max
syn keyword txl_keyword contained pos-min posq posql posqual
syn keyword txl_keyword contained pprinl pprint pprof prinl
syn keyword txl_keyword contained print prof prog1 progn
syn keyword txl_keyword contained prop proper-listp push pushhash
syn keyword txl_keyword contained put-byte put-char put-line put-lines
syn keyword txl_keyword contained put-string put-strings pwd qquote
syn keyword txl_keyword contained quasi quasilist quote rand
syn keyword txl_keyword contained random random-fixnum random-state-p range
syn keyword txl_keyword contained range* range-regex rcomb read
syn keyword txl_keyword contained readlink real-time-stream-p reduce-left reduce-right
syn keyword txl_keyword contained ref refset regex-compile regex-parse
syn keyword txl_keyword contained regexp regsub rehome-sym remhash
syn keyword txl_keyword contained remove-if remove-if* remove-path remq
syn keyword txl_keyword contained remq* remql remql* remqual
syn keyword txl_keyword contained remqual* rename-path repeat replace
syn keyword txl_keyword contained replace-list replace-str replace-vec rest
syn keyword txl_keyword contained ret retf return return-from
syn keyword txl_keyword contained reverse rlcp rperm rplaca
syn keyword txl_keyword contained rplacd run s-ifblk s-ifchr
syn keyword txl_keyword contained s-ifdir s-ififo s-iflnk s-ifmt
syn keyword txl_keyword contained s-ifreg s-ifsock s-irgrp s-iroth
syn keyword txl_keyword contained s-irusr s-irwxg s-irwxo s-irwxu
syn keyword txl_keyword contained s-isgid s-isuid s-isvtx s-iwgrp
syn keyword txl_keyword contained s-iwoth s-iwusr s-ixgrp s-ixoth
syn keyword txl_keyword contained s-ixusr search search-regex search-str
syn keyword txl_keyword contained search-str-tree second seek-stream select
syn keyword txl_keyword contained seqp set set-diff set-hash-userdata
syn keyword txl_keyword contained set-sig-handler sethash setitimer setlogmask
syn keyword txl_keyword contained sh sig-abrt sig-alrm sig-bus
syn keyword txl_keyword contained sig-check sig-chld sig-cont sig-fpe
syn keyword txl_keyword contained sig-hup sig-ill sig-int sig-io
syn keyword txl_keyword contained sig-iot sig-kill sig-lost sig-pipe
syn keyword txl_keyword contained sig-poll sig-prof sig-pwr sig-quit
syn keyword txl_keyword contained sig-segv sig-stkflt sig-stop sig-sys
syn keyword txl_keyword contained sig-term sig-trap sig-tstp sig-ttin
syn keyword txl_keyword contained sig-ttou sig-urg sig-usr1 sig-usr2
syn keyword txl_keyword contained sig-vtalrm sig-winch sig-xcpu sig-xfsz
syn keyword txl_keyword contained sin sixth size-vec some
syn keyword txl_keyword contained sort source-loc source-loc-str span-str
syn keyword txl_keyword contained splice split-str split-str-set sqrt
syn keyword txl_keyword contained stat stdlib str< str<=
syn keyword txl_keyword contained str= str> str>= stream-get-prop
syn keyword txl_keyword contained stream-set-prop streamp string-extend string-lt
syn keyword txl_keyword contained stringp sub sub-list sub-str
syn keyword txl_keyword contained sub-vec symacrolet symbol-function symbol-name
syn keyword txl_keyword contained symbol-package symbol-value symbolp symlink
syn keyword txl_keyword contained sys-qquote sys-splice sys-unquote syslog
syn keyword txl_keyword contained tan tf third throw
syn keyword txl_keyword contained throwf time time-fields-local time-fields-utc
syn keyword txl_keyword contained time-string-local time-string-utc time-usec tofloat
syn keyword txl_keyword contained toint tok-str tok-where tostring
syn keyword txl_keyword contained tostringp transpose tree-bind tree-case
syn keyword txl_keyword contained tree-find trie-add trie-compress trie-lookup-begin
syn keyword txl_keyword contained trie-lookup-feed-char trie-value-at trim-str true
syn keyword txl_keyword contained trunc tuples typeof unget-byte
syn keyword txl_keyword contained unget-char uniq unless unquote
syn keyword txl_keyword contained until upcase-str update url-decode
syn keyword txl_keyword contained url-encode usleep uw-protect vec
syn keyword txl_keyword contained vec-push vec-set-length vecref vector
syn keyword txl_keyword contained vector-list vectorp when where
syn keyword txl_keyword contained while with-saved-vars zerop zip

syn match txr_error "@[\t ]*[*]\?[\t ]*."
syn match txr_nested_error "[^\t `]\+" contained
syn match txr_hashbang "^#!.*"
syn match txr_atat "@[ \t]*@"
syn match txr_comment "@[ \t]*[#;].*"
syn match txr_contin "@[ \t]*\\$"
syn match txr_char "@[ \t]*\\."
syn match txr_char "@[ \t]*\\x[0-9A-Fa-f]\+"
syn match txr_char "@[ \t]*\\[0-9]\+"
syn match txr_variable "@[ \t]*[*]\?[ \t]*[A-Za-z_][A-Za-z0-9_]*"
syn match txr_metanum "@[0-9]\+"
syn match txr_splicevar "@[ \t,*]*[A-Za-z_][A-Za-z0-9_]*"
syn match txr_regdir "@[ \t]*/\(\\/\|[^/]\|\\\n\)*/"

syn match txr_chr "#\\x[A-Fa-f0-9]\+" contained
syn match txr_chr "#\\o[0-9]\+" contained
syn match txr_chr "#\\[^ \t\nA-Za-z0-9_]" contained
syn match txr_chr "#\\[A-Za-z0-9_]\+" contained
syn match txr_ncomment ";.*" contained

syn match txr_dot "\." contained
syn match txr_num "#x[+\-]\?[0-9A-Fa-f]\+" contained
syn match txr_num "#o[+\-]\?[0-7]\+" contained
syn match txr_num "#b[+\-]\?[0-1]\+" contained
syn match txr_ident "[A-Za-z0-9!$%&*+\-<=>?\\_~]*[A-Za-z!$#%&*+\-<=>?\\^_~][A-Za-z0-9!$#%&*+\-<=>?\\^_~]*" contained
syn match txl_ident "[:@][A-Za-z0-9!$%&*+\-<=>?\\\^_~/]\+" contained
syn match txl_ident "[A-Za-z0-9!$%&*+\-<=>?\\_~/]*[A-Za-z!$#%&*+\-<=>?\\^_~/][A-Za-z0-9!$#%&*+\-<=>?\\^_~/]*" contained
syn match txr_num "[+\-]\?[0-9]\+\([^A-Za-z0-9!$#%&*+\-<=>?\\^_~/]\|\n\)"me=e-1 contained
syn match txr_badnum "[+\-]\?[0-9]*[.][0-9]\+\([eE][+\-]\?[0-9]\+\)\?[A-Za-z!$#%&*+\-<=>?\\^_~/]\+" contained
syn match txr_num "[+\-]\?[0-9]*[.][0-9]\+\([eE][+\-]\?[0-9]\+\)\?\([^A-Za-z0-9!$#%&*+\-<=>?\\^_~/]\|\n\)"me=e-1 contained
syn match txr_num "[+\-]\?[0-9]\+\([eE][+\-]\?[0-9]\+\)\([^A-Za-z0-9!$#%&*+\-<=>?\\^_~/]\|\n\)"me=e-1 contained
syn match txl_ident ":" contained
syn match txl_splice "[ \t,]\|,[*]" contained

syn match txr_unquote "," contained
syn match txr_splice ",\*" contained
syn match txr_quote "'" contained
syn match txr_quote "\^" contained
syn match txr_dotdot "\.\." contained
syn match txr_metaat "@" contained

syn region txr_bracevar matchgroup=Delimiter start="@[ \t]*[*]\?{" matchgroup=Delimiter end="}" contains=txr_num,txr_ident,txr_string,txr_list,txr_bracket,txr_mlist,txr_mbracket,txr_regex,txr_quasilit,txr_chr,txl_splice,txr_nested_error
syn region txr_directive matchgroup=Delimiter start="@[ \t]*(" matchgroup=Delimiter end=")" contains=txr_keyword,txr_string,txr_list,txr_bracket,txr_mlist,txr_mbracket,txr_quasilit,txr_num,txr_badnum,txl_ident,txl_regex,txr_string,txr_chr,txr_quote,txr_unquote,txr_splice,txr_dot,txr_dotdot,txr_metaat,txr_ncomment,txr_nested_error
syn region txr_list contained matchgroup=Delimiter start="#\?H\?(" matchgroup=Delimiter end=")" contains=txl_keyword,txr_string,txl_regex,txr_num,txr_badnum,txl_ident,txr_metanum,txr_list,txr_bracket,txr_mlist,txr_mbracket,txr_quasilit,txr_chr,txr_quote,txr_unquote,txr_splice,txr_dot,txr_dotdot,txr_metaat,txr_ncomment,txr_nested_error
syn region txr_bracket contained matchgroup=Delimiter start="\[" matchgroup=Delimiter end="\]" contains=txl_keyword,txr_string,txl_regex,txr_num,txr_badnum,txl_ident,txr_metanum,txr_list,txr_bracket,txr_mlist,txr_mbracket,txr_quasilit,txr_chr,txr_quote,txr_unquote,txr_splice,txr_dot,txr_dotdot,txr_metaat,txr_ncomment,txr_nested_error
syn region txr_mlist contained matchgroup=Delimiter start="@[ \t]*(" matchgroup=Delimiter end=")" contains=txl_keyword,txr_string,txl_regex,txr_num,txr_badnum,txl_ident,txr_metanum,txr_list,txr_bracket,txr_mlist,txr_mbracket,txr_quasilit,txr_chr,txr_quote,txr_unquote,txr_splice,txr_dot,txr_dotdot,txr_metaat,txr_ncomment,txr_nested_error
syn region txr_mbracket matchgroup=Delimiter start="@[ \t]*\[" matchgroup=Delimiter end="\]" contains=txl_keyword,txr_string,txl_regex,txr_num,txr_badnum,txl_ident,txr_metanum,txr_list,txr_bracket,txr_mlist,txr_mbracket,txr_quasilit,txr_chr,txr_quote,txr_unquote,txr_splice,txr_dot,txr_dotdot,txr_metaat,txr_ncomment,txr_nested_error
syn region txr_string contained start=+#\?\*\?"+ skip=+\\\\\|\\"\|\\\n+ end=+"\|\n+
syn region txr_quasilit contained start=+#\?\*\?`+ skip=+\\\\\|\\`\|\\\n+ end=+`\|\n+ contains=txr_splicevar,txr_metanum,txr_bracevar,txr_mlist,txr_mbracket
syn region txr_regex contained start="/" skip="\\\\\|\\/\|\\\n" end="/\|\n"
syn region txl_regex contained start="#/" skip="\\\\\|\\/\|\\\n" end="/\|\n"

hi def link txr_at Special
hi def link txr_atstar Special
hi def link txr_atat Special
hi def link txr_comment Comment
hi def link txr_ncomment Comment
hi def link txr_hashbang Preproc
hi def link txr_contin Preproc
hi def link txr_char String
hi def link txr_keyword Keyword
hi def link txl_keyword Type
hi def link txr_string String
hi def link txr_chr String
hi def link txr_quasilit String
hi def link txr_regex String
hi def link txl_regex String
hi def link txr_regdir String
hi def link txr_variable Identifier
hi def link txr_splicevar Identifier
hi def link txr_metanum Identifier
hi def link txr_ident Identifier
hi def link txl_ident Identifier
hi def link txr_num Number
hi def link txr_badnum Error
hi def link txr_quote Special
hi def link txr_unquote Special
hi def link txr_splice Special
hi def link txr_dot Special
hi def link txr_dotdot Special
hi def link txr_metaat Special
hi def link txr_munqspl Special
hi def link txl_splice Special
hi def link txr_error Error
hi def link txr_nested_error Error

let b:current_syntax = "lisp"
