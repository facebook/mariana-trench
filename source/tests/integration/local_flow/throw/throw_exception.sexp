(method (public static) "LThrow;.test:(Ljava/lang/String;)V"
 (
  (load-param-object v0)
  (new-instance "Ljava/lang/Exception;")
  (move-result-pseudo-object v1)
  (invoke-direct (v1 v0) "Ljava/lang/Exception;.<init>:(Ljava/lang/String;)V")
  (throw v1)
 )
)
