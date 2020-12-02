(method (private) "LFlow;.flow:(LSink;LSource;)V"
 (
  LINE(1)
  (invoke-virtual (v3) "LSource;.source:()LData;")
  (move-result-object v0)

  (move-object v5 v0)

  LINE(2)
  (invoke-virtual (v2 v5) "LSink;.sink:(LData;)V")
  (return-void)
 )
)
