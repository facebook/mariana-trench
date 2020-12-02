(method (private) "LFlow;.flow:(LSink;LSource;)V"
 (
  LINE(1)
  (invoke-virtual (v3) "LSource;.source:()LData;")
  (move-result-object v0)

  LINE(2)
  (iput-object v0 v5 "LData;.field:LData;")

  LINE(3)
  (invoke-virtual (v2 v5) "LSink;.sink:(LData;)V")
  (return-void)
 )
)
