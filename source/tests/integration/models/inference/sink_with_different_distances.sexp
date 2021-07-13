(method (private) "LFlow;.into_sink:(LData;)V"
 (
  (load-param v0)
  (load-param v1)

  LINE(1)
  (invoke-virtual (v0 v1) "LSink;.sink:(LData;)V")
  (return-void)
 )
)
(method (private) "LFlow;.into_into_sink:(LData;)V"
 (
  (load-param v0)
  (load-param v1)

  LINE(1)
  (invoke-virtual (v0 v1) "LFlow;.into_sink:(LData;)V")
  (return-void)
 )
)
(method (private) "LFlow;.different_lengths:(LData;)V"
 (
  (load-param v0)
  (load-param v1)

  LINE(1)
  (invoke-virtual (v0 v1) "LFlow;.into_sink:(LData;)V")
  (invoke-virtual (v0 v1) "LFlow;.into_into_sink:(LData;)V")
  (return-void)
 )
)
(method (private) "LFlow;.into_different_lengths:(LData;)V"
 (
  (load-param v0)
  (load-param v1)

  LINE(1)
  (invoke-virtual (v0 v1) "LFlow;.different_lengths:(LData;)V")
  (return-void)
 )
)
