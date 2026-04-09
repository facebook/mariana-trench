(method (public) "LBuilder;.setName:(Ljava/lang/String;)LBuilder;"
 (
  (load-param-object v0)
  (load-param-object v1)
  (iput-object v1 v0 "LBuilder;.name:Ljava/lang/String;")
  (return-object v0)
 )
)
(method (public static) "LBuilder;.test:(Ljava/lang/String;Ljava/lang/String;)LBuilder;"
 (
  (load-param-object v0)
  (load-param-object v1)
  (new-instance "LBuilder;")
  (move-result-pseudo-object v2)
  (invoke-direct (v2) "LBuilder;.<init>:()V")
  (invoke-virtual (v2 v0) "LBuilder;.setName:(Ljava/lang/String;)LBuilder;")
  (move-result-object v2)
  (return-object v2)
 )
)
