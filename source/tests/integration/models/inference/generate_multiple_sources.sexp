(method (private) "LFlow;.flow:()LData;"
 (
  LINE(1)
  (const v0 1)

  LINE(2)
  (if-eqz v0 :else)
  (invoke-virtual (v1) "LSource;.source:()LData;")
  (move-result-object v0)

  LINE(3)
  (return-object v0)
  (goto :out)

  LINE(4)
  (:else)
  (invoke-virtual (v1) "LSource;.source:()LData;")
  (move-result-object v0)

  LINE(5)
  (:out)
  (return-object v0)
 )
)
