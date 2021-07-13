(method (private) "LFlow;.into_sink:(LData;LData;)V"
 (
  (load-param v0)
  (load-param v4)
  (load-param v5)

  LINE(1)
  (invoke-virtual (v0 v4) "LSink;.sink:(LData;)V")
  (return-void)
 )
)
(method (private) "LFlow;.indirect:()V"
 (
  LINE(1)
  (invoke-virtual (v0) "LSource;.source:()LData;")
  (move-result-object v1)

  LINE(2)
  (invoke-virtual (v0 v1 v2) "LFlow;.into_sink:(LData;LData;)V")
  (return-void)
 )
)
