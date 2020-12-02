(method (private) "LFlow;.flow:()V"
 (
  LINE(1)
  (invoke-virtual (v0) "LSource;.source:()LData;")
  (move-result-object v1)

  LINE(2)
  (const-wide v0 3)
  (mul-double v2 v1 v0)

  LINE(3)
  (const-wide v0 1)
  (add-double v3 v2 v0)

  LINE(4)
  (const-wide v0 2)
  (div-double v4 v3 v0)

  LINE(5)
  (const-wide v0 1)
  (sub-double v5 v0 v4)

  LINE(6)
  (invoke-virtual (v0 v4) "LSink;.sink:(LData;)V")
  (return-void)
 )
)
