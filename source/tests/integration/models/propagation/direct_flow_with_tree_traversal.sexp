(method (private) "LFlow;.flow:()V"
 (
  LINE(1)
  ; v1 = this
  (load-param v1)

  LINE(2)
  ; v2 = v1.right
  (iget-object v1 "LFlow;.right:LFlow;")
  (move-result-pseudo-object v2)

  LINE(3)
  ; v2.left = Source.source()
  (invoke-virtual (v0) "LSource;.source:()LData;")
  (move-result-object v3)
  (iput-object v3 v2 "LFlow;.left:LFlow;")

  LINE(4)
  (:enter)
  (if-eqz v1 :out)

  LINE(5)
  (if-eqz v10 :right)
  (:left)
  ; v1 = v1.left
  ; Sink.sink(v1)
  (iget-object v1 "LFlow;.left:LFlow;")
  (move-result-pseudo-object v1)
  (invoke-virtual (v0 v1) "LSink;.sink:(LData;)V")
  (goto :enter)

  LINE(6)
  (:right)
  ; v1 = v1.right
  ; Sink.sink(v1)
  (iget-object v1 "LFlow;.right:LFlow;")
  (move-result-pseudo-object v1)
  (invoke-virtual (v0 v1) "LSink;.sink:(LData;)V")
  (goto :enter)

  LINE(7)
  (:out)
  (return-void)
 )
)
