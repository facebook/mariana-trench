(method (private) "LFlow;.flow:()V"
 (
  LINE(1)
  ; v1 = this
  (load-param v1)

  LINE(2)
  ; v2 = v1.successor
  (iget-object v1 "LFlow;.successor:LFlow;")
  (move-result-pseudo-object v2)

  LINE(3)
  ; v2.successor = Source.source()
  (invoke-virtual (v0) "LSource;.source:()LData;")
  (move-result-object v3)
  (iput-object v3 v2 "LFlow;.successor:LFlow;")

  LINE(4)
  (:enter)
  (if-eqz v1 :out)

  LINE(5)
  ; v1 = v1.successor
  ; Sink.sink(v1)
  (iget-object v1 "LFlow;.successor:LFlow;")
  (move-result-pseudo-object v1)
  (invoke-virtual (v0 v1) "LSink;.sink:(LData;)V")
  (goto :enter)

  LINE(6)
  (:out)
  (return-void)
 )
)
