(method (private) "LFlow;.flow:(LSink;LSource;)V"
 (
  LINE(1)
  (invoke-virtual (v3) "LSource;.source:()LData;")
  (move-result-object v0)

  LINE(2)
  (invoke-static (v0) "LSink;.static_sink:(LData;)V")
  (return-void)
 )
)
