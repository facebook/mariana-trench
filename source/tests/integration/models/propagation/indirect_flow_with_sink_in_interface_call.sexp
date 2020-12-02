(method (public) "LInterface;.sink:(LData;)V"
 (
   LINE(1)
   (return-void)
 )
)
(super "LInterface;")
(method (public) "LImplementation;.sink:(LData;)V"
 (
  (load-param v0)
  (load-param v1)

  LINE(1)
  (invoke-virtual (v0 v1) "LSink;.sink:(LData;)V")
  (return-void)
 )
)
(method (public) "LFlow;.flow:()V"
 (
  LINE(1)
  (invoke-virtual (v0) "LSource;.source:()LData;")
  (move-result-object v0)

  LINE(2)
  (invoke-virtual (v2 v0) "LInterface;.sink:(LData;)V")
  (return-void)
 )
)
