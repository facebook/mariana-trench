(method (public static) "LLoop;.reassign:(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;"
 (
  (load-param-object v0)
  (load-param-object v1)
  (move-object v2 v0)
  (:loop_head)
  (if-eqz v2 :done)
  (move-object v2 v1)
  (goto :loop_head)
  (:done)
  (return-object v2)
 )
)
