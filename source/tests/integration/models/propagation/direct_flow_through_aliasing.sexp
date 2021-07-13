(method (private) "LFlow;.flow:()V"
 (
  LINE(1)
  ; v1 = new Data;
  (new-instance "LData;")
  (move-result-object v1)

  LINE(2)
  ; v2 = v1
  (move v2 v1)

  LINE(3)
  ; v2.field = Source.source()
  (invoke-virtual (v0) "LSource;.source:()LData;")
  (move-result-object v3)
  (iput-object v3 v2 "LData;.field:LData;")

  LINE(4)
  ; v4 = v1.field
  (iget-object v1 "LData;.field:LData;")
  (move-result-pseudo-object v4)

  LINE(5)
  ; Sink.sink(v4)
  (invoke-virtual (v0 v4) "LSink;.sink:(LData;)V")
  (return-void)
 )
)
