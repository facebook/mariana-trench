(method (private) "LFlow;.flow:()V"
 (
  LINE(1)
  (const v0 1)
  (const v1 0)
  (:enter)
  (if-eqz v0 :out)

  LINE(2)
  (invoke-virtual (v1 v1) "LSink;.sink:(LData;)V")

  LINE(3)
  (invoke-virtual (v1) "LSource;.source:()LData;")
  (move-result-object v1)

  (goto :enter)

  LINE(4)
  (:out)

  (return-void)
 )
)
