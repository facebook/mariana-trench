(method (private) "LFlow;.into_sink:(LData;)V"
 (
  (load-param v0)
  (load-param v1)

  LINE(1)
  (invoke-virtual (v0 v1) "LSink;.sink:(LData;)V")
  (return-void)
 )
)
(method (private) "LFlow;.propagation:(LData;)LData;"
 (
  (load-param v0)
  (load-param v1)

  LINE(1)
  (return-object v1)
 )
)
(method (private) "LFlow;.indirect:()V"
 (
  LINE(1)
  (invoke-virtual (v0) "LSource;.source:()LData;")
  (move-result-object v1)

  LINE(2)
  (invoke-virtual (v0 v1) "LFlow;.propagation:(LData;)LData;")
  (move-result-object v2)

  LINE(3)
  (invoke-virtual (v0 v2) "LFlow;.into_sink:(LData;)V")
  (return-void)
 )
)
