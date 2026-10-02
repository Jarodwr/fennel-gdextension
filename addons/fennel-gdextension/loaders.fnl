;; loaders.fnl — package loader definitions for lua-gdextension projects.
;;
;; Returns a table of loader functions without registering them.
;; The host (lsp.fnl config consumer, or init.fnl for the Godot runtime)
;; is responsible for inserting these into package.searchers.

(fn parse-tscn [src]
  ;; Parse a .tscn file into a node tree.
  ;; Returns {:nodes [...] :resources {...}} where each node is:
  ;;   {:name str :type str :parent str :script str|nil :properties {}}
  (local nodes [])
  (local resources {})
  (var current nil)

  (each [line (src:gmatch "[^\n]+")]
    (if
      ;; [ext_resource type="Script" path="res://foo.fnl" id="1_abc"]
      (line:match "^%[ext_resource")
      (let [id   (line:match "id=\"([^\"]+)\"")
            path (line:match "path=\"([^\"]+)\"")
            type (line:match "type=\"([^\"]+)\"")]
        (when (and id path)
          (tset resources id {:path path :type type})))

      ;; [node name="Foo" type="Node3D" parent="."]
      (line:match "^%[node")
      (let [name   (line:match "name=\"([^\"]+)\"")
            type   (line:match "type=\"([^\"]+)\"")
            parent (line:match "parent=\"([^\"]+)\"")]
        (when name
          (set current {:name name :type (or type "") :parent (or parent ".") :properties {}})
          (table.insert nodes current)))

      ;; script = ExtResource("1_abc")
      (and current (line:match "^script%s*="))
      (let [id (line:match "ExtResource%(\"([^\"]+)\"%)")]
        (when (and id (. resources id))
          (tset current :script (. resources id :path))))

      ;; property = value  (everything else on a node section)
      (and current (line:match "^%S+ ="))
      (let [k (line:match "^([%S]+)%s*=")
            v (line:match "=%s*(.+)$")]
        (when (and k v (not= k "script"))
          (tset current.properties k v)))))

  {:nodes nodes :resources resources})

(fn tscn-loader [module-name]
  ;; Searcher function: maps "path.to.scene" → "res://path/to/scene.tscn".
  ;; Returns a loader thunk if the file exists, nil otherwise.
  (local path (.. "res://" (module-name:gsub "%." "/") ".tscn"))
  (local f (io.open path))
  (when f
    (local src (f:read :*a))
    (f:close)
    (fn [] (parse-tscn src))))

[tscn-loader]
