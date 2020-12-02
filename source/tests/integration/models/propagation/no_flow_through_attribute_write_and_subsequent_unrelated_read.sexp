(method (private) "LFlow;.flow:(LData;)V"
 (
  (load-param v0)
  (load-param v1)

  LINE(1)
  (invoke-virtual (v0) "LSource;.source:()LData;")
  (move-result-object v2)

  (iput-object v2 v1 "LData;.field:LData;")

  LINE(2)
  (iget-object v1 "LData;.other_field:LData;")
  (move-result-pseudo-object v3)

  LINE(3)
  (invoke-virtual (v0 v3) "LSink;.sink:(LData;)V")
  (return-void)
 )
)
