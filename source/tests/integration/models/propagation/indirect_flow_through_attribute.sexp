(method (private) "LFlow;.into_attribute:()V"
 (
  (load-param v0)

  LINE(1)
  (invoke-virtual (v1) "LSource;.source:()LData;")
  (move-result-object v1)

  LINE(2)
  (iput-object v1 v0 "LFlow;.field:LData;")

  (return-void)
 )
)
(method (private) "LFlow;.attribute_into_sink:(LSink;LSource;)V"
 (
  (load-param v0)

  LINE(1)
  (invoke-virtual (v0) "LFlow;.into_attribute:()V")

  LINE(2)
  (invoke-virtual (v0 v0) "LSink;.sink:(LData;)V")

  (return-void)
 )
)
