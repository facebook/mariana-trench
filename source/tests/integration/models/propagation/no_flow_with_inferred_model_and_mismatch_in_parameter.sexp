(method (private) "LFlow;.into_sink:(LData;LData;)V"
 (
  (load-param v0)
  (load-param v4)
  (load-param v5)

  LINE(1)
  (invoke-virtual (v0 v5) "LSink;.sink:(LData;)V")
  (return-void)
 )
)
(method (private) "LFlow;.indirect:()V"
 (
  LINE(1)
  (invoke-virtual (v0) "LSource;.source:()LData;")
  (move-result-object v2)

  LINE(2)
  (invoke-virtual (v0 v2 v1)
  "LFlow;.into_sink:(LData;LData;)V") (return-void)
 )
)
