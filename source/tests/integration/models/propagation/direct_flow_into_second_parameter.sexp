(method (private) "LFlow;.flow:(LSink;LSource;)V"
 (
  LINE(1)
  (invoke-virtual (v3) "LSource;.source:()LData;")
  (move-result-object v0)

  (const v3 0)

  LINE(2)
  (invoke-virtual (v2 v3 v0) "LSink;.sink_in_second_parameter:(II)V")
  (return-void)
 )
)
