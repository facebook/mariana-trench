(method (private) "LFlow;.flow:(LSink;LSource;)V"
 (
  LINE(1)
  (const v0 0)

  LINE(2)
  (if-eqz v0 :else)
  (new-instance "LData;")
  (move-result-object v5)

  LINE(3)
  (invoke-direct (v2) "LData;.<init>:()V")
  (goto :out)

  LINE(4)
  (:else)
  (invoke-virtual (v3) "LSource;.source:()LData;")
  (move-result-object v5)

  LINE(5)
  (:out)
  (invoke-virtual (v4 v5) "LSink;.sink:(LData;)V")

  (return-void)
 )
)
