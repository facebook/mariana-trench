(method (private) "LFlow;.flow:(LSink;LSource;)V"
 (
  LINE(1)
  (invoke-virtual (v3) "LSource;.source:()LData;")
  (move-result-object v0)

  LINE(2)
  (iput-object v0 v5 "LData;.field:LData;")

  LINE(2)
  (iget-object v5 "LData;.other_field:LData;")
  (move-result-pseudo-object v1)

  LINE(3)
  (invoke-virtual (v2 v1) "LSink;.sink:(LData;)V")
  (return-void)
 )
)
