(method (public) "LMutate;.<init>:()V"
 (
  (load-param-object v0)
  (return-void)
 )
)
(method (public) "LMutate;.setA:(Ljava/lang/Object;)V"
 (
  (load-param-object v0)
  (load-param-object v1)
  (iput-object v1 v0 "LMutate;.a:Ljava/lang/Object;")
  (return-void)
 )
)
(method (public static) "LMutate;.test:(Ljava/lang/Object;)LMutate;"
 (
  (load-param-object v0)
  (new-instance "LMutate;")
  (move-result-pseudo-object v1)
  (invoke-direct (v1) "LMutate;.<init>:()V")
  (invoke-virtual (v1 v0) "LMutate;.setA:(Ljava/lang/Object;)V")
  (return-object v1)
 )
)
