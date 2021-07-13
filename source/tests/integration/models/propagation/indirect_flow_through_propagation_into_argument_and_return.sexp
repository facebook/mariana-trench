(method (private) "LFlow;.flow:()V"
 (
  (load-param v0)

  LINE(1)
  (invoke-virtual (v1) "LSource;.source:()LData;")
  (move-result-object v2)

  LINE(2)
  (invoke-virtual (v0 v2) "LFlow;.propagate_argument_into_this:(LSource;)LFlow;")
  (move-result-object v3)

  LINE(3)
  (invoke-virtual (v1 v3) "LSink;.sink:(LData;)V")
  (return-void)
 )
)
(method (private) "LFlow;.propagate_argument_into_this:(LSource;)LFlow;"
 (
  (load-param v0)
  (load-param v1)

  LINE(4)
  (iput-object v1 v0 "LFlow;.field:LSource;")

  LINE(5)
  (return-object v0)
 )
)
