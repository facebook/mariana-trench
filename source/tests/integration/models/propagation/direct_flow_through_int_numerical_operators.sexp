(method (private) "LFlow;.flow:()V"
 (
  LINE(1)
  (invoke-virtual (v0) "LSource;.source:()LData;")
  (move-result-object v1)

  LINE(2)
  (add-int/lit v2 v1 1)

  LINE(3)
  (mul-int/lit v3 v2 3)

  LINE(4)
  (div-int/lit v3 2)

  LINE(5)
  (move-result-pseudo-object v4)

  LINE(6)
  (invoke-virtual (v0 v4) "LSink;.sink:(LData;)V")
  (return-void)
 )
)
