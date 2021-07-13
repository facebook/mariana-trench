(method (private) "LFlow;.flow:()V"
 (
  LINE(1)
  (invoke-virtual (v0) "LSource;.source:()LData;")
  (move-result-object v1)

  LINE(2)
  (invoke-virtual (v0) "LSource;.source:()LData;")
  (move-result-object v2)

  LINE(3)
  (new-instance "LData;")
  (move-result-pseudo-object v3)

  LINE(4)
  (invoke-direct (v3 v1 v2) "LData;.<init>:(LData;LData;)V")

  LINE(5)
  (invoke-virtual (v4 v3) "LSink;.sink:(LData;)V")
  (return-void)
 )
)
