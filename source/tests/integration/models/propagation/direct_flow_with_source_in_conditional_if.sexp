(method (private) "LFlow;.flow:(LSink;LSource;)V"
 (
  LINE(1)
  (const v0 0)
  (const v1 0)

  LINE(2)
  (if-eqz v0 :else)
  (invoke-virtual (v3) "LSource;.source:()LData;")
  (move-result-object v5)
  (goto :out)

  LINE(3)
  (:else)
  (new-instance "LData;")
  (move-result-object v5)

  LINE(4)
  (invoke-direct (v2) "LData;.<init>:()V")

  LINE(5)
  (:out)
  (invoke-virtual (v4 v5) "LSink;.sink:(LData;)V")

  (return-void)
 )
)
