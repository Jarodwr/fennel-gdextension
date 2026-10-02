;; extend-node.fnl — Fennel macro for defining Godot script classes.
;;
;; Available as a global macro — no import needed. Usage:
;;
;;   (local MyNode
;;     (extend-node gd.Node3D
;;       (tool)
;;       (export speed (gd.export_range 1.0 100.0 0.5))
;;       (signal pinged)
;;       (override-method _ready [] (print "ready"))
;;       (method greet [name] (print name))
;;       (async-method _on_load [] (gd.await self :loaded))
;;       (rpc-method sync_pos ["any_peer" "reliable"] [pos] (set self.position pos))))
;;   MyNode
;;
;; Supported sub-forms:
;;   (tool)  /  (tool false)                            — enable tool mode
;;   (signal <name>)                                    — untyped signal
;;   (signal <name> <argname> <type> ...)               — typed signal args (pairs)
;;   (export <name> <gd.export*-expr>)                  — exported property
;;   (export <name> <type>)                             — typed export, default constructed
;;   (export <name> <type> <default>)                   — typed export with explicit default
;;   (method <name> [params...] body...)                — exposed method, lua_pcall; self is implicit
;;   (override-method <name> [params...] body...)       — Godot virtual override, lua_pcall, ClassDB inference; self is implicit
;;   (async-method <name> [params...] body...)          — exposed method, coroutine (supports gd.await); self is implicit
;;   (rpc-method <name> [flags...] [params...] body...) — RPC method; self is implicit

(fn make-method-entry [methods-sym name params body-forms flags]
  ;; flags: {:async bool :override bool :rpc expr}
  ;; self is always injected as implicit first param by C++, so we prepend it here.
  ;; sym bypasses Fennel's hygiene check for this intentional capture.
  (table.insert params 1 (sym :self))
  (let [fn-form `(fn ,params)]
    (each [_ b (ipairs body-forms)]
      (table.insert fn-form b))
    `(tset ,methods-sym ,name
       {:fn ,fn-form
        :async ,(or flags.async false)
        :override ,(or flags.override false)
        :rpc ,(or flags.rpc nil)})))

(fn extend-node [base ...]
  (let [signals-sym (gensym :sigs)
        exports-sym (gensym :exps)
        methods-sym (gensym :mths)
        stmts []]

    (var tool-val false)

    (table.insert stmts `(local ,signals-sym {}))
    (table.insert stmts `(local ,exports-sym {}))
    (table.insert stmts `(local ,methods-sym {}))

    (each [_ form (ipairs [...])]
      (let [head (tostring (. form 1))]
        (if
          (= head :tool)
          (set tool-val (if (> (length form) 1) (. form 2) true))

          (= head :signal)
          (let [sig-name (tostring (. form 2))
                args-sym (gensym :sig-args)]
            (table.insert stmts `(local ,args-sym []))
            (var i 3)
            (while (<= (+ i 1) (length form))
              (table.insert stmts `(table.insert ,args-sym ,(tostring (. form i))))
              (table.insert stmts `(table.insert ,args-sym ,(. form (+ i 1))))
              (set i (+ i 2)))
            (table.insert stmts `(tset ,signals-sym ,sig-name ,args-sym)))

          (= head :export)
          (if (= (length form) 4)
            (table.insert stmts
              `(tset ,exports-sym ,(tostring (. form 2))
                 (gd.export_typed ,(. form 3) ,(. form 4))))
            (table.insert stmts
              `(tset ,exports-sym ,(tostring (. form 2)) ,(. form 3))))

          (= head :method)
          (let [name (tostring (. form 2))
                params (. form 3)
                body (icollect [i v (ipairs form)] (if (> i 3) v))]
            (table.insert stmts
              (make-method-entry methods-sym name params body {})))

          (= head :override-method)
          (let [name (tostring (. form 2))
                params (. form 3)
                body (icollect [i v (ipairs form)] (if (> i 3) v))]
            (table.insert stmts
              (make-method-entry methods-sym name params body {:override true})))

          (= head :async-method)
          (let [name (tostring (. form 2))
                params (. form 3)
                body (icollect [i v (ipairs form)] (if (> i 3) v))]
            (table.insert stmts
              (make-method-entry methods-sym name params body {:async true})))

          (= head :rpc-method)
          (let [name (tostring (. form 2))
                flags (. form 3)
                params (. form 4)
                body (icollect [i v (ipairs form)] (if (> i 4) v))
                rpc-call `(gd.rpc)]
            (each [_ flag (ipairs flags)]
              (table.insert rpc-call flag))
            (table.insert stmts
              (make-method-entry methods-sym name params body {:rpc rpc-call})))

          (error (.. "extend-node: unknown form '" head "'")))))

    (table.insert stmts
      `{:extends ,base
        :tool ,tool-val
        :__signals ,signals-sym
        :__exports ,exports-sym
        :__methods ,methods-sym})

    (let [do-form `(do)]
      (each [_ s (ipairs stmts)]
        (table.insert do-form s))
      do-form)))

{: extend-node}
