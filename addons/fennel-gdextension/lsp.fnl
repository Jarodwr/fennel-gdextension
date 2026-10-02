;; lsp.fnl — fennel-ls configuration for lua-gdextension projects.
;;
;; Require this from your project's .lsp.fnl to get:
;;   - Godot API global docs (gd.*, gd.Node3D, gd.lerp, …)
;;   - extend-node global macro + analysis hook (method/rpc-method/export/signal body analysis)
;;
;; Minimal usage:
;;   (require :addons.fennel-gdextension.lsp)

(local godot-docs (require :addons.fennel-gdextension.godot_api_docs))

(fn method-hook [result i form params-child]
  ;; Open a scope with implicit self + user params bound, then analyze body.
  (let [params-node (. form.children params-child)]
    (table.insert result {:type :scope-open :span form.span})
    (table.insert result {:type :bind :name :self :span form.span})
    (when (and params-node params-node.children)
      (each [_ param (ipairs params-node.children)]
        (when (= param.kind :sym)
          (table.insert result {:type :bind :name param.value :span param.span}))))
    (for [j (+ params-child 1) (length form.children)]
      (table.insert result {:type :analyze-child-at :parent i :child j}))
    (table.insert result {:type :scope-close})))

(fn extend-node-hook [call]
  ;; call.children layout for (extend-node gd.Node3D ...):
  ;;   [1] extend-node (head symbol)
  ;;   [2] base class (e.g. gd.Node3D)
  ;;   [3+] sub-forms: (tool) (signal ...) (export ...) (method ...) (rpc-method ...)
  (let [result []]

    ;; Analyze the base class expression (e.g. gd.Node3D)
    (table.insert result {:type :analyze :index 2})

    (for [i 3 (length call.children)]
      (let [form (. call.children i)
            head (and form.children (. form.children 1))]
        (when head
          (if
            ;; (method name [params] body...) / (override-method ...) / (async-method ...)
            ;; self is implicit; child 3 = user params, children 4+ = body
            (or (= head.value :method) (= head.value :override-method)
                (= head.value :async-method))
            (method-hook result i form 3)

            ;; (rpc-method name [flags] [params] body...)
            ;; self is implicit; child 4 = user params, children 5+ = body
            (= head.value :rpc-method)
            (method-hook result i form 4)

            ;; (export name type) or (export name type default) or (export name expr)
            ;; child 3 = type/expr, child 4 = default (optional)
            (= head.value :export)
            (do
              (table.insert result {:type :analyze-child-at :parent i :child 3})
              (when (>= (length form.children) 4)
                (table.insert result {:type :analyze-child-at :parent i :child 4})))

            ;; (signal name argname type argname type ...)
            ;; types are at children 4, 6, 8, ... (argname/type pairs starting at child 3)
            (= head.value :signal)
            (do
              (var j 4)
              (while (<= j (length form.children))
                (table.insert result {:type :analyze-child-at :parent i :child j})
                (set j (+ j 2))))

            ;; (tool) / (tool false) — no sub-expressions to analyze
            nil))))

    result))

(fn require-tscn-hook [call]
  ;; Nothing to bind — the result is a value returned to the caller.
  ;; Returning [] suppresses default macro-depth analysis of the path string.
  [])

(fn autoload-hook [call]
  ;; Bind the singleton name so the LSP treats it as a known identifier.
  (let [name-node (. call.children 2)]
    (when (and name-node name-node.value)
      [{:type :bind :name name-node.value :span name-node.span}])))

{:global-docs godot-docs
 :global-macros {:extend-node true :require-tscn true :autoload true}
 :macro-hooks {:extend-node extend-node-hook
               :require-tscn require-tscn-hook
               :autoload autoload-hook}
 :warn-unhooked-macros false}
