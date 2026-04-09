(method (public) "LMyClass;.<init>:(Ljava/lang/Object;Ljava/lang/Object;)V"
 (
  (load-param-object v0)
  (load-param-object v1)
  (load-param-object v2)
  (iput-object v1 v0 "LMyClass;.a:Ljava/lang/Object;")
  (iput-object v2 v0 "LMyClass;.b:Ljava/lang/Object;")
  (return-void)
 )
)
(method (public static) "LMyClass;.test_new:(Ljava/lang/Object;Ljava/lang/Object;)LMyClass;"
 (
  (load-param-object v0)
  (load-param-object v1)
  (new-instance "LMyClass;")
  (move-result-pseudo-object v2)
  (invoke-direct (v2 v0 v1) "LMyClass;.<init>:(Ljava/lang/Object;Ljava/lang/Object;)V")
  (return-object v2)
 )
)
