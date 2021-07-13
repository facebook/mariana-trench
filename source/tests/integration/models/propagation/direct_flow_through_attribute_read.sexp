(method (private) "LFlow;.flow:(LSink;LSource;)V"
 (
  LINE(1)
  (invoke-virtual (v3) "LSource;.source:()LData;")
  (move-result-object v0)

  LINE(2)
  (iget-object v0 "LData;.field:LData;")
  (move-result-pseudo-object v5)

  LINE(3)
  (invoke-virtual (v2 v5) "LSink;.sink:(LData;)V")
  (return-void)
 )
)
