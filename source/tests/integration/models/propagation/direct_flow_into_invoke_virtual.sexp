(method (private) "LFlow;.flow:(LSink;LSource;)V"
 (
  LINE(1)
  (invoke-virtual (v3) "LSource;.source:()LData;")
  (move-result-object v0)

  LINE(2)
  (invoke-virtual (v2 v0) "LSink;.sink:(LData;)V")
  (return-void)
 )
)
