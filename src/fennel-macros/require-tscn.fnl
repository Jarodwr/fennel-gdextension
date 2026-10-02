;; require-tscn.fnl — global macros for Godot resource loading.
;;
;; (require-tscn "res://scenes/player.tscn")
;;   Reads and parses the .tscn file at script load time; returns a table:
;;   {:nodes [{:name str :type str :parent str :script str|nil :properties {}}...]
;;    :resources {id {:path str :type str}}}
;;
;; (autoload :GameManager)
;;   No-op at runtime (Godot has already set up the singleton).
;;   Purely a declaration so the fennel-ls LSP knows the name is valid.

;; ── Runtime parser (defined globally so macro-emitted code can call it) ──────

(global __fennel_gdext_parse_tscn
  (fn [src]
    (local nodes [])
    (local resources {})
    (var current nil)
    (each [line (src:gmatch "[^\n]+")]
      (if
        (line:match "^%[ext_resource")
        (let [id   (line:match "id=\"([^\"]+)\"")
              path (line:match "path=\"([^\"]+)\"")
              type (line:match "type=\"([^\"]+)\"")]
          (when (and id path)
            (tset resources id {:path path :type (or type "")})))

        (line:match "^%[node")
        (let [name   (line:match "name=\"([^\"]+)\"")
              type   (line:match "type=\"([^\"]+)\"")
              parent (line:match "parent=\"([^\"]+)\"")]
          (when name
            (set current {:name name
                          :type (or type "")
                          :parent (or parent ".")
                          :properties {}})
            (table.insert nodes current)))

        (and current (line:match "^script%s*="))
        (let [id (line:match "ExtResource%(\"([^\"]+)\"%)")]
          (when (and id (. resources id))
            (tset current :script (. (. resources id) :path))))

        (and current (line:match "^[%S]+ ="))
        (let [k (line:match "^([%S]+)%s*=")
              v (line:match "=%s*(.+)$")]
          (when (and k v (not= k "script"))
            (tset current.properties k v)))))
    {:nodes nodes :resources resources}))

;; ── Macros ────────────────────────────────────────────────────────────────────

(fn require-tscn [path-sym]
  (local path (tostring path-sym))
  ;; Validate the file exists at Fennel compile time for early error feedback.
  (let [f (io.open path)]
    (assert f (.. "require-tscn: file not found: " path))
    (f:close))
  ;; Emit load-time code that reads the file and calls the globally-defined parser.
  `(let [f# (assert (io.open ,path) ,(.. "require-tscn: cannot open " path))
         src# (f#:read :*a)]
     (f#:close)
     (__fennel_gdext_parse_tscn src#)))

(fn autoload [name-sym]
  ;; No-op at runtime — Godot has already injected the singleton into _G.
  ;; The fennel-ls hook reads the name and binds it so the LSP knows it exists.
  nil)

{: require-tscn : autoload}
