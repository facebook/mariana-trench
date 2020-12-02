(method (private) "LFlow;.flow:()V"
 (
  LINE(1)
  (const v0 1)

  LINE(2)
  (if-eqz v0 :else)
  (invoke-virtual (v1) "LSource;.source:()LData;")
  (move-result-object v0)
  (goto :out)

  LINE(3)
  (:else)
  (invoke-virtual (v1) "LSource;.source:()LData;")
  (move-result-object v0)

  LINE(4)
  (:out)
  (invoke-virtual (v1 v0) "LSink;.sink:(LData;)V")

  (return-void)
 )
)
