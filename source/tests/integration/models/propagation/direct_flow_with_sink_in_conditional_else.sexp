(method (private) "LFlow;.flow:(LSink;LSource;)V"
 (
  LINE(1)
  (invoke-virtual (v3) "LSource;.source:()LData;")
  (move-result-object v0)

  LINE(2)
  (const v4 0)

  LINE(3)
  (if-eqz v4 :else)
  (return-void)
  (goto :out)

  LINE(4)
  (:else)
  (invoke-virtual (v2 v0) "LSink;.sink:(LData;)V")
  LINE(5)
  (:out)
  (return-void)
 )
)
