(method (private) "LFlow;.flow:()V"
 (
  LINE(1)
  (const v0 10)
  (new-array v0 "[LData;") ; create an array of length 10
  (move-result-pseudo-object v1)

  LINE(2)
  (invoke-virtual (v0) "LSource;.source:()LData;")
  (move-result-object v2)

  LINE(3)
  ; v1[0] = v2
  (const v0 0)
  (aput v2 v1 v0)

  LINE(4)
  ; v3 = read v1[0]
  (const v0 0)
  (aget v1 v0)
  (move-result-pseudo v3)

  LINE(5)
  (invoke-virtual (v0 v3) "LSink;.sink:(LData;)V")

  LINE(6)
  (return-void)
 )
)
