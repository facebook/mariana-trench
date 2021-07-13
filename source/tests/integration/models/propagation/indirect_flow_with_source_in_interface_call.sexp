(abstract method (public) "LInterface;.source:()LData;"
 (
   LINE(1)
   (return-void)
 )
)
(super "LInterface;")
(method (public) "LImplementation;.source:()LData;"
 (
  LINE(1)
  (invoke-virtual (v3) "LSource;.source:()LData;")
  (move-result-object v0)

  LINE(2)
  (return-object v0)
 )
)
(method (public) "LFlow;.flow:()V"
 (
  LINE(1)
  (invoke-virtual (v0) "LInterface;.source:()LData;")
  (move-result-object v0)

  LINE(2)
  (invoke-virtual (v2 v0) "LSink;.sink:(LData;)V")
  (return-void)
 )
)
