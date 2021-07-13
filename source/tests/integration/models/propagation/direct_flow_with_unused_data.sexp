(method (private) "LFlow;.flow:(LSink;LSource;)V"
 (
  LINE(1)
  (invoke-virtual (v3) "LSource;.source:()LData;")
  (move-result-object v0)

  LINE(2)
  (new-instance "LData;")
  (move-result-object v1)
  (invoke-direct (v1) "LData;.<init>:()V")

  LINE(3)
  (invoke-virtual (v2 v0) "LSink;.sink:(LData;)V")
  (return-void)
 )
)
