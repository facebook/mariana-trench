(method (public) "LBase;.<init>:(Ljava/lang/Object;)V"
 (
  (load-param-object v0)
  (load-param-object v1)
  (iput-object v1 v0 "LBase;.x:Ljava/lang/Object;")
  (return-void)
 )
)
(super "LBase;")
(method (public) "LChild;.<init>:(Ljava/lang/Object;)V"
 (
  (load-param-object v0)
  (load-param-object v1)
  (invoke-direct (v0 v1) "LBase;.<init>:(Ljava/lang/Object;)V")
  (return-void)
 )
)
