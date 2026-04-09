(method (public) "LA;.doWork:(Ljava/lang/Object;)Ljava/lang/Object;"
 (
  (load-param-object v0)
  (load-param-object v1)
  (return-object v1)
 )
)
(method (public static) "LDispatch;.call_virtual:(LA;Ljava/lang/Object;)Ljava/lang/Object;"
 (
  (load-param-object v0)
  (load-param-object v1)
  (invoke-virtual (v0 v1) "LA;.doWork:(Ljava/lang/Object;)Ljava/lang/Object;")
  (move-result-object v2)
  (return-object v2)
 )
)
