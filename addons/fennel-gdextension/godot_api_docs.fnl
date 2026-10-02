;; godot_api_docs.fnl
;; Generates a fennel-ls global-docs table from extension_api.json bundled with
;; lua-gdextension. Used by addons/fennel-gdextension/lsp.fnl — prefer requiring
;; that instead of this file directly.

;; Locate our own directory so we can find json.lua and extension_api.json
;; regardless of where the LSP process was started from.
(local addon-dir
  (: (. (debug.getinfo 1 :S) :source) :match "^@?(.+)[/\\\\][^/\\\\]+$"))

(set package.path (.. package.path ";" addon-dir "/?.lua"))
(local json (require :json))

(local api
  (with-open [f (assert (io.open (.. addon-dir "/extension_api.json") :r)
                        "godot_api_docs: could not open extension_api.json")]
    (json.decode (f:read :*a))))

(local docs {})

(fn set-doc [key sig doc]
  (tset docs key {:signature sig :doc doc}))

(fn args-str [arguments]
  (if (or (= arguments nil) (= (length arguments) 0))
      ""
      (.. " " (table.concat (icollect [_ a (ipairs arguments)] a.name) " "))))

(fn longest-constructor [constructors]
  (var best nil)
  (var best-len -1)
  (each [_ c (ipairs constructors)]
    (let [n (if c.arguments (length c.arguments) 0)]
      (when (> n best-len)
        (set best c)
        (set best-len n))))
  best)

;; Root namespace entry
(set-doc "gd" "gd" "Godot API namespace. All engine types and functions live here.")

;; ── Classes ──────────────────────────────────────────────────────────────────
(each [_ cls (ipairs api.classes)]
  (local n cls.name)
  (set-doc (.. "gd." n) (.. "gd." n)
           (if cls.inherits (.. "extends " cls.inherits) nil))
  (set-doc (.. "gd." n ".new") (.. "(gd." n ".new)") "Instantiate a new object.")
  (when cls.methods
    (each [_ m (ipairs cls.methods)]
      (local sep (if m.is_static "." ":"))
      (set-doc (.. "gd." n "." m.name)
               (.. "(gd." n sep m.name (args-str m.arguments) ")")
               nil))))

;; ── Builtin classes (Vector3, Color, String, …) ──────────────────────────────
(each [_ cls (ipairs api.builtin_classes)]
  (local n cls.name)
  (set-doc (.. "gd." n) (.. "gd." n) nil)
  (when (and cls.constructors (> (length cls.constructors) 0))
    (let [c (longest-constructor cls.constructors)]
      (set-doc (.. "gd." n ".new")
               (.. "(gd." n ".new" (args-str c.arguments) ")")
               nil)))
  (when cls.methods
    (each [_ m (ipairs cls.methods)]
      (local sep (if m.is_static "." ":"))
      (set-doc (.. "gd." n "." m.name)
               (.. "(gd." n sep m.name (args-str m.arguments) ")")
               nil))))

;; ── Singletons (Engine, OS, RenderingServer, …) ──────────────────────────────
(each [_ s (ipairs api.singletons)]
  (set-doc (.. "gd." s.name) (.. "gd." s.name) "Godot singleton."))

;; ── Utility functions (sin, cos, lerp, …) ────────────────────────────────────
(each [_ f (ipairs api.utility_functions)]
  (set-doc (.. "gd." f.name)
           (.. "(gd." f.name (args-str f.arguments) ")")
           (if f.return_type (.. "-> " f.return_type) nil)))

;; ── Global enums (Side, Corner, Key, …) ──────────────────────────────────────
(each [_ e (ipairs api.global_enums)]
  (set-doc (.. "gd." e.name) (.. "gd." e.name)
           (.. "enum { "
               (table.concat (icollect [_ v (ipairs e.values)] v.name) ", ")
               " }")))

;; ── lua-gdextension script API ────────────────────────────────────────────────
(set-doc "gd.rpc"
         "(gd.rpc mode...)"
         "Build an RPC config for use with rpc-method. Flags: \"any_peer\", \"authority\", \"call_local\", \"call_remote\", \"unreliable\", \"unreliable_ordered\", \"reliable\", or a channel number.")
(set-doc "gd.export"
         "(gd.export default-value)"
         "Declares an exported property visible in the Godot inspector.")
(set-doc "gd.export_range"
         "(gd.export_range min max step)"
         "Declares an exported numeric property with an inspector range hint.")
(set-doc "gd.signal"
         "(gd.signal)"
         "Declares a signal on the script class.")

docs
