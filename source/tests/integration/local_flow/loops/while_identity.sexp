(method (public static) "LLoop;.whileIdentity:(Ljava/lang/Object;)Ljava/lang/Object;"
 (
  (load-param-object v0)
  (move-object v1 v0)
  (:loop_head)
  (if-eqz v1 :done)
  (move-object v1 v0)
  (goto :loop_head)
  (:done)
  (return-object v1)
 )
)
