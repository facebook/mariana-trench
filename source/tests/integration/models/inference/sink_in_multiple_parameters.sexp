(method (private) "LFlow;.flow:(LData;LData;)V"
 (
  (load-param v0)
  (load-param v1)
  (load-param v2)

  LINE(1)
  (invoke-virtual (v0 v1) "LSink;.sink:(LData;)V")

  LINE(3)
  (invoke-virtual (v0 v2) "LSink;.sink:(LData;)V")
  (return-void)
 )
)
