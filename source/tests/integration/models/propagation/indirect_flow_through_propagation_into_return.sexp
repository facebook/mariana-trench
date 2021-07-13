(method (private) "LFlow;.flow:()V"
 (
  LINE(1)
  (invoke-virtual (v0) "LSource;.source:()LData;")
  (move-result-object v0)

  LINE(2)
  (invoke-virtual (v2 v0) "LData;.propagation:(LData;)LData;")
  (move-result-object v1)

  LINE(3)
  (invoke-virtual (v2 v1) "LSink;.sink:(LData;)V")
  (return-void)
 )
)
