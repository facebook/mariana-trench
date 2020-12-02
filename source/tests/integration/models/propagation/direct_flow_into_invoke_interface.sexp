(method (private) "LFlow;.flow:(LSink;LSource;)V"
 (
  LINE(1)
  (invoke-virtual (v3) "LSource;.source:()LData;")
  (move-result-object v0)

  LINE(2)
  (invoke-interface (v2 v0) "LSink;.interface_sink:(LData;)V")
  (return-void)
 )
)
