(method (private) "LFlow;.flow:(LData;)V"
 (
  (load-param v0)

  LINE(2)
  (iget-object v0 "LFlow;.field:LData;")
  (move-result-pseudo-object v1)

  LINE(1)
  (invoke-virtual (v0 v1) "LSink;.sink:(LData;)V")
  (return-void)
 )
)
