(method (private) "LFlow;.flow:()V"
 (
  LINE(1)
  (invoke-virtual (v0) "LSource;.source:()LData;")
  (move-result-object v0)

  LINE(2)
  (invoke-virtual (v0) "LData;.propagation_this:()LData;")
  (move-result-object v3)

  LINE(3)
  (invoke-virtual (v2 v3) "LSink;.sink:(LData;)V")
  (return-void)
 )
)
