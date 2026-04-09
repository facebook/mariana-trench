(method (public) "LMyLambda;.<init>:(Ljava/lang/Object;)V"
 (
  (load-param-object v0)
  (load-param-object v1)
  (iput-object v1 v0 "LMyLambda;.captured:Ljava/lang/Object;")
  (return-void)
 )
)
(super "Lkotlin/jvm/functions/Function0;")
(method (public static) "LMyLambda;.create:(Ljava/lang/Object;)Lkotlin/jvm/functions/Function0;"
 (
  (load-param-object v0)
  (new-instance "LMyLambda;")
  (move-result-pseudo-object v1)
  (invoke-direct (v1 v0) "LMyLambda;.<init>:(Ljava/lang/Object;)V")
  (return-object v1)
 )
)
